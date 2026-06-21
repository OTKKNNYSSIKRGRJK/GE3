export module Game.FX1;

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

using namespace Lumina;

namespace Game {
	namespace {
		constexpr float ThetaV{ 0.075f };
		float const SIN_ThetaV{ std::sin(ThetaV) };
		float const COS_ThetaV{ std::cos(ThetaV) };

		bool UpdateParticle(Particle& p, void const*) {
			p.Scale.x *= 1.02f;
			p.Scale.y *= 1.02f;
			p.Scale.z *= 1.02f;

			float vx = p.Velocity.x;
			float vy = p.Velocity.y;
			p.Velocity.x = vx * COS_ThetaV - vy * SIN_ThetaV;
			p.Velocity.y = vx * SIN_ThetaV + vy * COS_ThetaV;
			
			p.Translate.x += p.Velocity.x;
			p.Translate.y += p.Velocity.y;
			p.Translate.z += p.Velocity.z;

			p.RenderData.RGBA.w *= 0.95f;
			p.Life -= 1.0f;
			return (p.Life > 0.0f);
		}
	}

	export class FX1 {
	public:
		void Update(
			Lumina::Float3 const& translate_ = { 0.0f, 0.0f, 0.0f }
		) {
			++FrameCNT_;

			auto& rndEngine{ reinterpret_cast<std::mt19937&>(Random::Generator()) };
			static std::uniform_real_distribution<float> distRotate{
				-std::numbers::pi_v<float>,
				std::numbers::pi_v<float>
			};
			static std::uniform_real_distribution<float> distTranslate{ -2.5f, 2.5f };

			Lumina::Float3 translate = {
				distTranslate(rndEngine) + translate_.x,
				distTranslate(rndEngine) + translate_.y,
				distTranslate(rndEngine) + translate_.z
			};

			constexpr static Float4 particleColor[3]{
				{ 1.0f, 0.5f, 0.5f, 1.0f },
				{ 0.5f, 1.0f, 0.5f, 1.0f },
				{ 0.5f, 0.5f, 1.0f, 1.0f },
			};
			Float3 const rot{
				distRotate(rndEngine),
				distRotate(rndEngine),
				distRotate(rndEngine)
			};
			for (int i = 0; i < 3; ++i) {
				Particle p;
				{
					p.Scale = { 0.75f, 0.75f, 0.75f };
					p.Rotate = rot;
					p.Translate = translate;
					p.Velocity = { translate.y * 0.01f, -translate.x * 0.01f, -2.0f + i * 0.2f };
					p.Life = 45.0f;
					p.RenderData.DiffuseID = 0;
					p.RenderData.DiffuseAtlasID = 5;
					p.RenderData.RGBA = particleColor[i];
				}
				ParticleSystem_->Emit(std::move(p));
			}

			for (int i = 0; i < 3; ++i) {
				Particle p;
				{
					p.Scale = { 0.5f, 0.5f, 0.5f };
					p.Rotate = rot;
					p.Translate = { translate.x * 3.0f, translate.y * 3.0f, translate.z };
					p.Velocity = {
						-p.Translate.y * (0.2f - i * 0.005f),
						p.Translate.x * (0.2f - i * 0.005f),
						-0.125f
					};
					p.Life = 60.0f;
					p.RenderData.DiffuseID = 0;
					p.RenderData.DiffuseAtlasID = 5;
					p.RenderData.RGBA = particleColor[i];
				}
				ParticleSystem_->Emit(std::move(p));
			}

			static int speedLineTimer{ 0 };
			if ((speedLineTimer & 0x01) == 0) {
				Float3 const translate_SpeedLine = {
					distTranslate(rndEngine) + translate_.x,
					distTranslate(rndEngine) + translate_.y,
					distTranslate(rndEngine) + translate_.z + 25.0f
				};
				for (int i = 0; i < 3; ++i) {
					Particle p;
					{
						p.Scale = { 12.5f, 0.0625f, 1.0f };
						p.Rotate = { 0.0f, std::numbers::pi_v<float> * 0.5f, distRotate(rndEngine) };
						p.Translate = translate_SpeedLine;
						p.Translate.x *= (1.0f - i * 0.05f);
						p.Translate.y *= (1.0f - i * 0.05f);
						p.Velocity = { 0.0f, 0.0f, -2.5f };
						p.Life = 45.0f;
						p.RenderData.DiffuseID = 0;
						p.RenderData.DiffuseAtlasID = 0;
						p.RenderData.RGBA = particleColor[i];
					}
					ParticleSystem_->Emit(std::move(p));
				}
			}
			++speedLineTimer;
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
					{ "FX1.Particles", "Assets/Particles.png" },
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
			RS_.Initialize(device_, rsSetup, "FX1 RS");

			dxContext_.Compile(
				VS_,
				L"Assets/Shaders/BasicParticle.VS.hlsl",
				L"vs_6_6",
				L"main",
				"FX1.VS"
			);
			dxContext_.Compile(
				PS_,
				L"Assets/Shaders/BasicParticle.PS.hlsl",
				L"ps_6_6",
				L"main",
				"FX1.PS"
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