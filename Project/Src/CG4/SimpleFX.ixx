export module Lumina.SimpleFX;

import <memory>;
import <random>;
import <vector>;
import <cstdint>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;
import Lumina.AssetManager;
import Lumina.Utils.Data;

import ParticleSystem;

namespace Lumina {
	namespace {
		bool UpdateParticle(Particle& p, void const*) {
			p.Scale.x *= 1.02f;
			p.Scale.y *= 1.05f;
			p.Scale.z *= 1.02f;

			p.Rotate.x += 0.01f;

			p.RenderData.RGBA.w *= 0.92f;
			p.Life -= 1.0f;
			return (p.Life > 0.0f);
		}
	}

	export class SimpleFX {
	public:
		auto GlobalTable() const noexcept -> DX12::DescriptorTable const& { return SRV_Textures_; }

	public:
		void Update(
			Lumina::Float3 const& translate_ = { 0.0f, 0.0f, 0.0f }
		) {
			++FrameCNT_;

			auto& rndEngine{ reinterpret_cast<std::mt19937&>(Random::Generator()) };
			static std::uniform_real_distribution<float> distScale{ 0.5f, 2.5f };
			static std::uniform_real_distribution<float> distRotate{
				-std::numbers::pi_v<float>,
				std::numbers::pi_v<float>
			};
			static std::uniform_real_distribution<float> distTranslate{ -1.5f, 1.5f };


			static Lumina::Float3 translate{
				distTranslate(rndEngine) + translate_.x,
				distTranslate(rndEngine) + translate_.y,
				distTranslate(rndEngine) + translate_.z
			};
			if ((FrameCNT_ & 0xF) == 0) {
				translate = {
					distTranslate(rndEngine) + translate_.x,
					distTranslate(rndEngine) + translate_.y,
					distTranslate(rndEngine) + translate_.z
				};
			}

			{
				Particle p;
				{
					p.Scale = { 0.1f * 0.5f, distScale(rndEngine) * 2.0f * 0.1f, 1.5f * 0.5f };
					p.Rotate = {
						distRotate(rndEngine),
						distRotate(rndEngine),
						distRotate(rndEngine)
					};
					p.Translate = translate;
					p.Velocity = { 0.0f, 0.0f, 0.0f };
					p.Life = 60.0f;
					p.RenderData.DiffuseID = 0;
					p.RenderData.DiffuseAtlasID = 0;
					p.RenderData.RGBA = { 1.0f, 1.0f, 1.0f, 1.0f };
				}
				ParticleSystem_->Emit(std::move(p));
			}
		}

		void Render(
			DX12::CommandList const& cmdList_,
			DX12::DescriptorHeap const& localHeap_CBV_,
			Mat4 const& viewToWorld_NoTranslate_
		) {
			ParticleSystem_->Update(
				cmdList_,
				viewToWorld_NoTranslate_,
				UpdateParticle
			);

			ParticleSystem_->Render(
				cmdList_,
				RS_,
				PSO_,
				LocalHeap_CBV_.CPUHandle(0U),
				localHeap_CBV_.CPUHandle(0U),
				SRV_Textures_,
				SRV_Textures_
			);
		}

		void Initialize(
			DX12::Context const& dxContext_,
			DX12::GraphicsDevice const& device_,
			AssetManager& assetMngr_
		) {
			SRV_Textures_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "FX.Particles", "Assets/Particles.png" },
				}
			);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
				device_->CopyDescriptorsSimple(
					1U,
					SRV_Textures_.CPUHandle(idx),
					assetMngr_.Graphics().CPUHandle(texIDs.at(idx)),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}

			auto settings{ Utils::LoadFromFile<nlohmann::json>("ParticleSystem.json", "Assets/Configs") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("Common RS")) };
			RS_.Initialize(device_, rsSetup, "SimpleFX RS");

			dxContext_.Compile(
				VS_,
				L"Assets/Shaders/BasicParticle.VS.hlsl",
				L"vs_6_6",
				L"main",
				"SimpleFX.VS"
			);
			dxContext_.Compile(
				PS_,
				L"Assets/Shaders/BasicParticle.PS.hlsl",
				L"ps_6_6",
				L"main",
				"SimpleFX.PS"
			);
			DX12::GraphicsPipelineState::Setup graphicsPSOSetup{};
			DX12::BlendState blendState{ .IndependentBlendEnable{ true }, };
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_ONE },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ONE },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
			DX12::RasterizerState rasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_NONE },
			};
			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ false },
			};
			DX12::GraphicsPipelineState::InputLayout inputLayout{};
			inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);
			inputLayout.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
			std::vector<DXGI_FORMAT> rtvFormats{
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			};
			graphicsPSOSetup <<
				RS_ <<
				VS_ <<
				PS_ <<
				blendState <<
				rasterizerState <<
				depthStencilState <<
				inputLayout <<
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE <<
				rtvFormats <<
				DX12::GraphicsPipelineState::DefaultDSVFormat;
			PSO_.Initialize(
				device_,
				graphicsPSOSetup,
				"CG3Eval2::GraphicsPSO"
			);

			ParticleSystem_ = std::make_unique<ParticleSystem<Particle>>();
			ParticleSystem_->Initialize(dxContext_, 4096);

			FrameCNT_ = -1;

			Dummy_.Initialize(device_, 256LLU);
			LocalHeap_CBV_.Initialize(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1U, false);
			DX12::CBV::Create(device_, LocalHeap_CBV_.CPUHandle(0U), Dummy_);
		}

	private:
		std::unique_ptr<ParticleSystem<Particle>> ParticleSystem_;

		DX12::RootSignature RS_;
		DX12::Shader VS_;
		DX12::Shader PS_;
		DX12::GraphicsPSO PSO_;

		DX12::DescriptorTable SRV_Textures_;

		DX12::DescriptorHeap LocalHeap_CBV_;
		DX12::UploadBuffer Dummy_;

		int FrameCNT_;
	};
}