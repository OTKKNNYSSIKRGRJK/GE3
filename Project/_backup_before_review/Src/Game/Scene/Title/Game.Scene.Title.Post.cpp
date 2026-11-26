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
//	Title 用 Canvas とポスト処理			//
//////	//////	//////	//////	//////	//////
// シーン（キャラ → 魔法陣 → 火柱 → パーティクル → 放射光線）を Title 用 Canvas に描き、
// ポスト PS（Assets/Title/TitlePost.hlsl）でバックバッファへフルスクリーンで写す。
// その後、割れ目からの放射光線（Game.Scene.Title.Rays.cpp の枠 1）とメニュー UI をバックバッファに描く。
// 割れ目（方式 β）はポスト PS の中（Assets/Title/TitleCrack.hlsli。差し替えるときはこのファイルを入れ替える）。
//
// Canvas はスワップチェインと同じ SRGB ＋ D24S8 なので、既存の Title 用 PSO はそのまま使える。
// クリア色は黒で固定（Canvas の RT のクリア値と同じ。違う色でクリアするとデバッグレイヤーが警告する）。
// 深度は SRV として読まないので、最初のフレームに DEPTH_WRITE へ遷移したらそのまま。
// 通常時（Outro 外）もこの経路を通す。効果がすべて 0 のときポスト PS は Load 1 回で写すだけ。

namespace Game::Scene::Impl {
	namespace {
		constexpr uint32_t CanvasWidth{ 1280U };
		constexpr uint32_t CanvasHeight{ 720U };

		// TitlePost.hlsl の cbuffer Constants と同じ並び（32 の倍数に揃える）
		struct PostConstants {
			float CenterU;
			float CenterV;
			float ZoomBlur;
			float ZoomTaps;

			float Chromatic;
			float BloomIntensity;
			float BloomRadiusPx;
			float BloomThreshold;

			float BloomTaps;
			float Aspect;
			float ResolutionX;
			float ResolutionY;

			// 割れ目（TitleCrack.hlsli の CrackParams と同じ意味）
			float CrackPointU;
			float CrackPointV;
			float CrackAngle;
			float CrackGap;

			float CrackRotate;
			float CrackGlowWidth;
			float CrackGlowIntensity;
			float CrackLightIntensity;

			Lumina::Float3 CrackCoreColor;
			float CrackLineWidth;

			Lumina::Float3 CrackEdgeColor;
			float CrackLineIntensity;

			float CrackLineReveal;
			float CrackEnabled;
			float Padding0;
			float Padding1;
		};
		static_assert(sizeof(PostConstants) % 32U == 0U);

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

	void Title::InitializePost(Lumina::DX12::Context const& dxContext_) {
		auto const& device{ dxContext_.Device() };

		// * Canvas：RT 1 枚（SRGB、クリア値 = 黒）＋深度（D24S8）
		{
			Canvas_.AllocateTextures(1U, true);
			Canvas_.RenderTexture(0U).Initialize(
				device,
				CanvasWidth,
				CanvasHeight,
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				Lumina::Float4{ ClearColor_[0], ClearColor_[1], ClearColor_[2], ClearColor_[3] },
				"Title.Canvas.RT0"
			);
			Canvas_.DepthTexture().Initialize(device, CanvasWidth, CanvasHeight, "Title.Canvas.Depth");
			Canvas_.TransitionResourceStates(device, dxContext_.DirectQueue());	// RT・深度とも PIXEL_SHADER_RESOURCE になる
			Canvas_.CreateViews(device);
			Canvas_.Viewport(0U) = D3D12_VIEWPORT{
				.TopLeftX{ 0.0f },
				.TopLeftY{ 0.0f },
				.Width{ static_cast<float>(CanvasWidth) },
				.Height{ static_cast<float>(CanvasHeight) },
				.MinDepth{ 0.0f },
				.MaxDepth{ 1.0f },
			};
			Canvas_.ScissorRect(0U) = D3D12_RECT{
				.left{ 0 },
				.top{ 0 },
				.right{ static_cast<LONG>(CanvasWidth) },
				.bottom{ static_cast<LONG>(CanvasHeight) },
			};
			IsCanvasDepthWritable_ = false;

			GlobalTable_PostSRV_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			Lumina::DX12::SRV<void>::Create(device, GlobalTable_PostSRV_.CPUHandle(0U), Canvas_.RenderTexture(0U));
		}

		// * Root signature：[0] ルート CBV（b0）/ [1] SRV テーブル（t0）＋静的サンプラ（バイリニア・Clamp）
		{
			auto settings{ Lumina::Utils::LoadFromFile<nlohmann::json>("TitlePost.json", "Assets/Title") };
			RS_Post_.Initialize(
				device,
				Lumina::DX12::LoadRootSignatureSetup(settings.at("RS")),
				"Title Post RS"
			);
		}

		dxContext_.Compile(
			VS_Post_,
			L"Assets/Title/TitlePost.hlsl",
			L"vs_6_6",
			L"VSMain",
			"Title.Post.VS"
		);
		dxContext_.Compile(
			PS_Post_,
			L"Assets/Title/TitlePost.hlsl",
			L"ps_6_6",
			L"PSMain",
			"Title.Post.PS"
		);

		// * PSO：上書き（ブレンドなし）・深度なし（DSV はバインドしたままなので、フォーマットだけ合わせる）
		{
			Lumina::DX12::BlendState blendState{};
			blendState.RenderTarget[0].BlendEnable = false;
			blendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			// * 三角形は VS で SV_VertexID から生成
			Lumina::DX12::GraphicsPSO::InputLayout inputLayout{};

			PSO_Post_.Initialize(
				device,
				RS_Post_,
				VS_Post_,
				PS_Post_,
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
				"Title Post PSO"
			);
		}

		UB_Post_.Initialize(device, 256LLU, "Title Post Constants");
	}

	//////	//////	//////	//////	//////	//////
	//	Update									//
	//////	//////	//////	//////	//////	//////

	// UpdateRays の後に呼ぶ（武器の先端のスクリーン位置を使う）
	void Title::UpdatePost() {
		auto const& style{ Outro_.Post };
		auto const& crack{ Outro_.Crack };

		// ズームブラーの中心：武器の先端 → 命中から割れ目へ（先端が画面に無いときは画面中央）
		float centerU{ 0.5f };
		float centerV{ 0.5f };
		OutroEffectCenter(centerU, centerV);

		float const open{ OutroCrackOpen() };

		PostConstants const constants{
			.CenterU{ centerU },
			.CenterV{ centerV },
			.ZoomBlur{ std::max(OutroPostValue(style.ZoomBlur), 0.0f) },
			.ZoomTaps{ static_cast<float>(std::clamp(style.ZoomTaps, 1, 32)) },

			.Chromatic{ std::max(OutroPostValue(style.Chromatic), 0.0f) },
			.BloomIntensity{ std::max(OutroPostValue(style.Bloom), 0.0f) },
			.BloomRadiusPx{ std::max(style.BloomRadiusPx, 0.0f) },
			.BloomThreshold{ std::max(style.BloomThreshold, 0.0f) },

			.BloomTaps{ static_cast<float>(std::clamp(style.BloomTaps, 1, 32)) },
			.Aspect{ static_cast<float>(CanvasWidth) / static_cast<float>(CanvasHeight) },
			.ResolutionX{ static_cast<float>(CanvasWidth) },
			.ResolutionY{ static_cast<float>(CanvasHeight) },

			.CrackPointU{ crack.PointU },
			.CrackPointV{ crack.PointV },
			.CrackAngle{ crack.Angle },
			.CrackGap{ std::max(crack.GapMax, 0.0f) * open },

			.CrackRotate{ crack.RotateMax * open },
			.CrackGlowWidth{ std::max(crack.GlowWidth, 0.0f) },
			.CrackGlowIntensity{ std::max(crack.GlowIntensity, 0.0f) },
			.CrackLightIntensity{ std::max(crack.LightIntensity, 0.0f) },

			.CrackCoreColor{ SRGBToLinear(crack.CoreColor) },
			.CrackLineWidth{ std::max(crack.LineWidth, 0.0f) },

			.CrackEdgeColor{ SRGBToLinear(crack.EdgeColor) },
			.CrackLineIntensity{ std::max(crack.LineIntensity, 0.0f) },

			.CrackLineReveal{ OutroCrackLineReveal() },
			.CrackEnabled{ IsOutroCrackActive() ? 1.0f : 0.0f },
			.Padding0{ 0.0f },
			.Padding1{ 0.0f },
		};
		UB_Post_.Store(&constants, sizeof(PostConstants), 0LLU);
	}

	//////	//////	//////	//////	//////	//////
	//	Render									//
	//////	//////	//////	//////	//////	//////

	// シーンを描く前に呼ぶ：Canvas を RT にして、黒と深度 1.0 でクリア
	void Title::BeginSceneCanvas(Lumina::DX12::CommandList const& cmdList_) {
		D3D12_RESOURCE_BARRIER barriers[2]{};
		uint32_t num_Barriers{ 0U };
		barriers[num_Barriers++] = Lumina::DX12::Barrier::Transition(
			Canvas_.RenderTexture(0U),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		if (!IsCanvasDepthWritable_) {
			// 深度は SRV として読まないので、最初の 1 回だけ遷移してそのまま
			barriers[num_Barriers++] = Lumina::DX12::Barrier::Transition(
				Canvas_.DepthTexture(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_DEPTH_WRITE
			);
			IsCanvasDepthWritable_ = true;
		}
		cmdList_->ResourceBarrier(num_Barriers, barriers);

		D3D12_CPU_DESCRIPTOR_HANDLE const rtv{ Canvas_.RTV(0U) };
		D3D12_CPU_DESCRIPTOR_HANDLE const dsv{ Canvas_.DSV() };
		cmdList_->OMSetRenderTargets(1U, &rtv, false, &dsv);
		cmdList_->RSSetViewports(1U, &Canvas_.Viewport(0U));
		cmdList_->RSSetScissorRects(1U, &Canvas_.ScissorRect(0U));
		cmdList_->ClearRenderTargetView(rtv, ClearColor_, 0U, nullptr);
		cmdList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0U, 0U, nullptr);
	}

	// シーンを描き終えたら呼ぶ：Canvas を SRV にして、ポスト PS でバックバッファへ写す
	// 終わった時点で RT = バックバッファ、DSV = スワップチェインの深度（メニュー UI はこの上に描く）
	void Title::EndSceneCanvasAndPost(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_
	) {
		D3D12_RESOURCE_BARRIER const barrier{
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(0U),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			)
		};
		cmdList_->ResourceBarrier(1U, &barrier);

		auto const& swapChain{ dxContext_.SwapChain() };
		D3D12_CPU_DESCRIPTOR_HANDLE const rtv{ swapChain.BackBufferRTVCPUHandle() };
		D3D12_CPU_DESCRIPTOR_HANDLE const dsv{ swapChain.DSVCPUHandle() };
		cmdList_->OMSetRenderTargets(1U, &rtv, false, &dsv);

		// 全画面を上書きするのでバックバッファのクリアは要らない
		cmdList_->SetGraphicsRootSignature(RS_Post_.Get());
		cmdList_->SetPipelineState(PSO_Post_.Get());
		cmdList_->SetGraphicsRootConstantBufferView(0U, UB_Post_->GetGPUVirtualAddress());
		cmdList_->SetGraphicsRootDescriptorTable(1U, GlobalTable_PostSRV_.GPUHandle(0U));
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList_->DrawInstanced(3U, 1U, 0U, 0U);
	}
}
