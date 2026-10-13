module Game.Scene.Title;

import <cstdint>;
import <cmath>;
import <algorithm>;
import <vector>;
import <d3d12.h>;

import nlohmann.json;

import : Impl;

import Lumina;

#undef max
#undef min

//////	//////	//////	//////	//////	//////
//	Title の放射光線（Outro 用）			//
//////	//////	//////	//////	//////	//////
// 画面上の 1 点から放射状に伸びる光線を、フルスクリーン三角形 1 枚・加算で描く（Assets/Title/TitleRays.hlsl）。
// シーンのテクスチャは読まないので RT は要らない。中心は今は武器の先端のスクリーン位置。
// 強さ・長さは Outro のタイムラインから決める（OutroRaysIntensity / OutroRaysLength。Game.Scene.Title.Outro.cpp）。
// 描く順番：パーティクルの後（Title 用 Canvas の中。ポストのブルーム・ズームブラーが掛かる）。

namespace Game::Scene::Impl {
	namespace {
		// TitleRays.hlsl の cbuffer Constants と同じ並び（32 の倍数に揃える）
		struct RaysConstants {
			float CenterU;
			float CenterV;
			float Intensity;
			float Length;

			Lumina::Float3 Color;
			float Time;

			float RayCount;
			float Sharpness;
			float RotateSpeed;
			float FlickerSpeed;

			Lumina::Float3 CoreColor;
			float CoreRadius;

			float CoreIntensity;
			float Aspect;
			float Padding0;
			float Padding1;

			float Padding2[12];		// 80 バイト → 128 バイト
		};
		static_assert(sizeof(RaysConstants) % 32U == 0U);

		float SRGBToLinear(float c_) {
			return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
		}
		Lumina::Float3 SRGBToLinear(Lumina::Float3 const& c_) {
			return { SRGBToLinear(c_.x), SRGBToLinear(c_.y), SRGBToLinear(c_.z) };
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Initialize								//
	//////	//////	//////	//////	//////	//////

	void Title::InitializeRays(Lumina::DX12::Context const& dxContext_) {
		auto const& device{ dxContext_.Device() };

		// * Root signature：ルート CBV 1 つだけ
		{
			auto settings{ Lumina::Utils::LoadFromFile<nlohmann::json>("TitleRays.json", "Assets/Title") };
			RS_Rays_.Initialize(
				device,
				Lumina::DX12::LoadRootSignatureSetup(settings.at("RS")),
				"Title Rays RS"
			);
		}

		dxContext_.Compile(
			VS_Rays_,
			L"Assets/Title/TitleRays.hlsl",
			L"vs_6_6",
			L"VSMain",
			"Title.Rays.VS"
		);
		dxContext_.Compile(
			PS_Rays_,
			L"Assets/Title/TitleRays.hlsl",
			L"ps_6_6",
			L"PSMain",
			"Title.Rays.PS"
		);

		// * PSO：加算・深度なし（DSV はバインドされたままなので、フォーマットだけ合わせる）
		{
			Lumina::DX12::BlendState blendState{};
			blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_ONE },
				.DestBlend{ D3D12_BLEND_ONE },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ZERO },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			// * 三角形は VS で SV_VertexID から生成
			Lumina::DX12::GraphicsPSO::InputLayout inputLayout{};

			PSO_Rays_.Initialize(
				device,
				RS_Rays_,
				VS_Rays_,
				PS_Rays_,
				blendState,
				Lumina::DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_NONE },
				},
				Lumina::DX12::DepthStencilState{
					.DepthEnable{ false },
					.StencilEnable{ false },
				},
				inputLayout,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, },
				Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
				"Title Rays PSO"
			);
		}

		UB_Rays_.Initialize(device, 256LLU, "Title Rays Constants");
	}

	//////	//////	//////	//////	//////	//////
	//	Update									//
	//////	//////	//////	//////	//////	//////

	// Character の姿勢とカメラが決まった後に呼ぶ（武器の先端を画面へ投影する）
	void Title::UpdateRays(Lumina::Mat4 const& viewProj_) {
		IsRaysVisible_ = false;

		// * 武器の先端のスクリーン位置（光線とズームブラーの中心。カメラの後ろにあるときは無効）
		{
			IsTipOnScreen_ = false;
			Lumina::Float3 base{};
			Lumina::Float3 tip{};
			float radius{ 0.0f };
			if (Character_.WeaponAxis(base, tip, radius)) {
				Lumina::Vec4 const clip{ Lumina::Vec4{ tip.x, tip.y, tip.z, 1.0f } * viewProj_ };
				if (clip.w > 1e-4f) {
					TipU_ = (clip.x / clip.w) * 0.5f + 0.5f;
					TipV_ = 0.5f - (clip.y / clip.w) * 0.5f;
					IsTipOnScreen_ = true;
				}
			}
		}

		auto const& style{ Outro_.Rays };
		float const intensity{ OutroRaysIntensity() };
		if (!style.Enabled || intensity <= 0.0f || !IsTipOnScreen_) { return; }

		float const centerU{ TipU_ };
		float const centerV{ TipV_ };

		RaysConstants const constants{
			.CenterU{ centerU },
			.CenterV{ centerV },
			.Intensity{ intensity },
			.Length{ OutroRaysLength() },

			.Color{ SRGBToLinear(style.Color) },
			.Time{ OutroTime_ },

			.RayCount{ static_cast<float>(std::max(style.RayCount, 1)) },
			.Sharpness{ std::max(style.Sharpness, 0.01f) },
			.RotateSpeed{ style.RotateSpeed },
			.FlickerSpeed{ std::max(style.FlickerSpeed, 0.0f) },

			.CoreColor{ SRGBToLinear(style.CoreColor) },
			.CoreRadius{ std::max(style.CoreRadius, 0.0001f) },

			.CoreIntensity{ std::max(style.CoreIntensity, 0.0f) },
			.Aspect{ 1280.0f / 720.0f },
			.Padding0{ 0.0f },
			.Padding1{ 0.0f },

			.Padding2{},
		};
		UB_Rays_.Store(&constants, sizeof(RaysConstants), 0LLU);
		IsRaysVisible_ = true;
	}

	//////	//////	//////	//////	//////	//////
	//	Render									//
	//////	//////	//////	//////	//////	//////

	// RT / DSV / Viewport / DescriptorHeap は呼び出し側で設定済みであること
	void Title::RenderRays(Lumina::DX12::CommandList const& cmdList_) {
		if (!IsRaysVisible_) { return; }

		cmdList_->SetGraphicsRootSignature(RS_Rays_.Get());
		cmdList_->SetPipelineState(PSO_Rays_.Get());
		cmdList_->SetGraphicsRootConstantBufferView(0U, UB_Rays_->GetGPUVirtualAddress());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList_->DrawInstanced(3U, 1U, 0U, 0U);
	}
}
