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
// シーンのテクスチャは読まないので RT は要らない。2 枠あり、どちらも同じ PSO で描く：
//   枠 0（RaysSlot_Tip）  ：武器の先端から。Title 用 Canvas に描く（ポストのブルーム・ズームブラーが掛かる）
//   枠 1（RaysSlot_Crack）：割れ目から（区間 4）。ポストの後、バックバッファに描く（割れ目でずれないように）
// 強さ・長さは Outro のタイムラインから決める（OutroRaysIntensity / OutroRaysLength。Game.Scene.Title.Outro.cpp）。

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

		constexpr uint64_t RaysSlotStride{ 256LLU };	// 枠ごとの CB の間隔（ルート CBV は 256 バイト境界）

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

		UB_Rays_.Initialize(device, RaysSlotStride * NumRaysSlots, "Title Rays Constants");
	}

	//////	//////	//////	//////	//////	//////
	//	Update									//
	//////	//////	//////	//////	//////	//////

	// Character の姿勢とカメラが決まった後に呼ぶ（武器の先端を画面へ投影する）
	void Title::UpdateRays(Lumina::Mat4 const& viewProj_) {
		for (auto& isVisible : IsRaysVisible_) { isVisible = false; }

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
		if (!style.Enabled) { return; }

		// 見た目は共通（Rays の設定）。中心・強さ・長さ・模様の時間だけ枠ごとに変える
		auto const store{
			[this, &style](uint32_t slot_, float centerU_, float centerV_, float intensity_, float length_, float time_) {
				RaysConstants const constants{
					.CenterU{ centerU_ },
					.CenterV{ centerV_ },
					.Intensity{ intensity_ },
					.Length{ length_ },

					.Color{ SRGBToLinear(style.Color) },
					.Time{ time_ },

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
				UB_Rays_.Store(&constants, sizeof(RaysConstants), RaysSlotStride * slot_);
				IsRaysVisible_[slot_] = true;
			}
		};

		// * 枠 0：武器の先端から（命中から中心が割れ目へ移る）
		if (float const intensity{ OutroRaysIntensity() }; intensity > 0.0f && IsTipOnScreen_) {
			float centerU{ TipU_ };
			float centerV{ TipV_ };
			OutroEffectCenter(centerU, centerV);
			store(RaysSlot_Tip, centerU, centerV, intensity, OutroRaysLength(), OutroTime_);
		}

		// * 枠 1：割れ目から（区間 4。模様は先端の光線と揃わないよう時間をずらす）
		if (float const intensity{ OutroCrackRaysIntensity() }; intensity > 0.0f) {
			auto const& crack{ Outro_.Crack };
			store(RaysSlot_Crack, crack.PointU, crack.PointV, intensity, OutroCrackRaysLength(), OutroTime_ + 13.7f);
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Render									//
	//////	//////	//////	//////	//////	//////

	// RT / DSV / Viewport / DescriptorHeap は呼び出し側で設定済みであること
	void Title::RenderRays(Lumina::DX12::CommandList const& cmdList_, uint32_t slot_) {
		if (slot_ >= NumRaysSlots || !IsRaysVisible_[slot_]) { return; }

		cmdList_->SetGraphicsRootSignature(RS_Rays_.Get());
		cmdList_->SetPipelineState(PSO_Rays_.Get());
		cmdList_->SetGraphicsRootConstantBufferView(0U, UB_Rays_->GetGPUVirtualAddress() + RaysSlotStride * slot_);
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList_->DrawInstanced(3U, 1U, 0U, 0U);
	}
}
