module Game.Scene.Title;

import <cstdint>;
import <cmath>;
import <numbers>;
import <algorithm>;
import <memory>;
import <vector>;
import <d3d12.h>;

import : Impl;

import Lumina;
import Game.Scene;
import Game.KeyConfigScreen;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

//////	//////	//////	//////	//////	//////
//	Title のメニュー（Start / 設定 / Exit）	//
//////	//////	//////	//////	//////	//////
// 本編の SpriteRenderer / Sprite2.VS.hlsl / SpriteUI.PS.hlsl をそのまま使う。
// 本編の UI 用 PSO は加算合成だが、黒のフェードを重ねるために通常のアルファ合成の PSO を別に作る。
//
// 操作：↑↓ / W S で選択、SPACE / ENTER で決定。
// ラベルのグロー：TitleLabelGlow.PS（α をぼかす）を加算で、ラベルの下に描く。グロー専用の SpriteRenderer を使う
//                 （SpriteRenderer は 1 フレームに 1 回しかバッチできないため）。明滅は CPU で色の強さに掛ける。
// タイトルロゴ：Assets/UI/Label.Title.png（今は仮画像）を少し遅れて下からフェードインさせるだけ。
// 演出：フェードイン → 選択中は不透明・等倍・少し左へ、非選択は半透明・縮小
//       → Start：Outro を開始（ロゴ・メニューはフェードインを逆再生して消え、Outro の最後に次のシーンへ。
//                Game.Scene.Title.Outro.cpp）
//       → 設定：キー割り当ての画面（Game.KeyConfigScreen）を上に重ねて開く。開いている間はメニューの入力を止める。
//              ESC か「戻る」で閉じる（ESC でタイトルが閉じないよう、Game::Scene::BlockEscapeQuit を立てる）
//       → Exit：点滅 → 黒へフェードアウト → Flag = -1（main.cpp がウィンドウを閉じる）
// 並び（2026-11-11）：Start を 1 つ上へ、元の Start の位置に設定、Exit はそのまま（項目 i の Y = FirstY + Spacing × (i − 1)）
// メニューは Pause / TimeScale の影響を受けない（固定の 1/60 秒で進める）。

namespace Game::Scene::Impl {
	namespace {
		constexpr float MenuDeltaTime{ 1.0f / 60.0f };

		float Saturate(float x_) {
			return std::clamp(x_, 0.0f, 1.0f);
		}
		float SRGBToLinear(float c_) {
			return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Initialize								//
	//////	//////	//////	//////	//////	//////

	void Title::InitializeUI(
		Lumina::DX12::Context const& dxContext_,
		Lumina::AssetManager& assetMngr_
	) {
		auto const& device{ dxContext_.Device() };

		UISpriteRenderer_ = std::make_unique<Lumina::SpriteRenderer>();
		UISpriteRenderer_->Initialize(dxContext_, 256U);
		UIGlowRenderer_ = std::make_unique<Lumina::SpriteRenderer>();
		UIGlowRenderer_->Initialize(dxContext_, 256U);

		// * Shaders：本編と同じ
		dxContext_.Compile(
			VS_UI_,
			L"Assets/Shaders/Sprite2.VS.hlsl",
			L"vs_6_6",
			L"main",
			"Title.UI.VS"
		);
		dxContext_.Compile(
			PS_UI_,
			L"Assets/Shaders/SpriteUI.PS.hlsl",
			L"ps_6_6",
			L"main",
			"Title.UI.PS"
		);

		// * PSO：通常のアルファ合成（黒のフェードで画面を暗くできるように）・深度なし
		{
			Lumina::DX12::BlendState blendState{};
			blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ONE },
				.DestBlendAlpha{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			Lumina::DX12::GraphicsPSO::InputLayout inputLayout{};
			inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);

			PSO_UI_.Initialize(
				device,
				UISpriteRenderer_->RootSignature(),
				VS_UI_,
				PS_UI_,
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
				"Title UI PSO"
			);
		}

		// * Glow PSO：加算（色はシェーダ側で「グロー色 × ぼかした α」になっている）
		{
			dxContext_.Compile(
				PS_UIGlow_,
				L"Assets/Title/TitleLabelGlow.PS.hlsl",
				L"ps_6_6",
				L"main",
				"Title.UIGlow.PS"
			);

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

			Lumina::DX12::GraphicsPSO::InputLayout inputLayout{};
			inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);

			PSO_UIGlow_.Initialize(
				device,
				UIGlowRenderer_->RootSignature(),
				VS_UI_,
				PS_UIGlow_,
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
				"Title UI Glow PSO"
			);
		}

		// * Textures（Sprite.json の Textures は 128 枠。使うのは先頭 4 つ）
		{
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "Title.Label.Start", "Assets/UI/Label.Start.png" },
					{ "Title.Label.Exit", "Assets/UI/Label.Exit.png" },
					{ "Title.Blank", "Assets/Img/Blank.png" },
					{ "Title.Label.Title", "Assets/UI/Label.Title.png" },
					{ "Title.Label.Settings", "Assets/UI/Label.Settings.png" },
				}
			);
			GlobalTable_UITextures_ = dxContext_.GlobalDescriptorHeap().Allocate(8U);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_UITextures_.CPUHandle(idx),
					assetMngr_.Graphics().CPUHandle(texIDs.at(idx)),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}
		}

		// * 正射影（本編の UI と同じ：左上原点・Y 下向き・1280x720）
		{
			Lumina::Mat4 const worldToNDC{ Lumina::Mat4::Orthographic(0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 1.0f) };
			UB_UIOrtho_.Initialize(device, 256LLU, "Title UI Ortho");
			UB_UIOrtho_.Store(&worldToNDC, sizeof(Lumina::Mat4), 0LLU);
			LocalHeap_UI_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1U, false);
			Lumina::DX12::CBV::Create(device, LocalHeap_UI_.CPUHandle(0U), UB_UIOrtho_);
		}

		// * Sprites
		constexpr uint32_t itemTextures[NumMenuItems]{ 0U, 4U, 1U };		// Start / 設定 / Exit
		for (int idx{ 0 }; idx < NumMenuItems; ++idx) {
			auto& item{ UI_Items_[idx] };
			item.AnchorPoint({ 1.0f, 0.5f });	// 右端・縦中央
			item.TextureID(itemTextures[idx]);
		}
		UI_Fade_.Translate({ 0.0f, 0.0f });
		UI_Fade_.Scale({ 1280.0f, 720.0f });
		UI_Fade_.AnchorPoint({ 0.0f, 0.0f });
		UI_Fade_.TextureID(2U);
		UI_Logo_.TextureID(3U);

		// 「設定」の画面（キー割り当て）
		SettingsScreen_.Initialize(dxContext_, assetMngr_, "Title");

		ResetMenu();
	}

	void Title::ResetMenu() {
		MenuPhase_ = MenuPhase::FadeIn;
		MenuTimer_ = 0.0f;
		LogoTimer_ = 0.0f;
		GlowTimer_ = 0.0f;
		MenuSelected_ = 0;
		for (int idx{ 0 }; idx < NumMenuItems; ++idx) {
			MenuBlend_[idx] = (idx == MenuSelected_) ? 1.0f : 0.0f;
		}
		FadeAlpha_ = 1.0f;
		FadeOutFrom_ = 0.0f;
		IntroFadeColor_ = Lumina::Float3{ 0.0f, 0.0f, 0.0f };	// Outro から戻ったときは、呼び出し側でその色に置き換える
		// 起動直後に押しっぱなしのキーで反応しないよう「押されていた」扱いから始める
		PrevKeyUp_ = true;
		PrevKeyDown_ = true;
		PrevKeyConfirm_ = true;
	}

	//////	//////	//////	//////	//////	//////
	//	Update									//
	//////	//////	//////	//////	//////	//////

	void Title::UpdateUI(Lumina::WinApp::RawInput const& input_) {
		constexpr float dt{ MenuDeltaTime };
		auto const& style{ MenuStyle_ };

		//----	------	------	------	------	----//
		// 入力（押した瞬間だけ拾う）

		auto const& keyboard{ input_.Keyboard() };
		using Lumina::WinApp::KEY;
		bool const keyUp{ keyboard.IsPressed(KEY::ARROW_UP) || keyboard.IsPressed(KEY::W) };
		bool const keyDown{ keyboard.IsPressed(KEY::ARROW_DOWN) || keyboard.IsPressed(KEY::S) };
		bool const keyConfirm{ keyboard.IsPressed(KEY::SPACE) || keyboard.IsPressed(KEY::ENTER) };

		bool isInputBlocked{ false };
		#if defined(_DEBUG)
		// ImGui の入力欄を触っている間はメニューを動かさない
		isInputBlocked = ImGui::GetIO().WantCaptureKeyboard;
		#endif

		bool const triggeredUp{ keyUp && !PrevKeyUp_ && !isInputBlocked };
		bool const triggeredDown{ keyDown && !PrevKeyDown_ && !isInputBlocked };
		bool const triggeredConfirm{ keyConfirm && !PrevKeyConfirm_ && !isInputBlocked };
		PrevKeyUp_ = keyUp;
		PrevKeyDown_ = keyDown;
		PrevKeyConfirm_ = keyConfirm;

		// 設定の画面：開いている間はそちらに入力を渡し、メニューは動かさない（押した瞬間の前の状態は上で更新済み）。
		// ESC でタイトルが閉じないよう、開いている間と、閉じたあと ESC を離すまでは main.cpp に知らせる
		bool const settingsOpen{ SettingsScreen_.IsOpen() };
		if (settingsOpen) {
			SettingsScreen_.Update(input_);
		}
		Game::Scene::BlockEscapeQuit = SettingsScreen_.IsOpen() || (Game::Scene::BlockEscapeQuit && keyboard.IsPressed(KEY::ESC));

		//----	------	------	------	------	----//
		// 状態遷移

		MenuTimer_ += dt;

		switch (MenuPhase_) {
		case MenuPhase::FadeIn:
		case MenuPhase::Idle:
			if (MenuPhase_ == MenuPhase::FadeIn) {
				FadeAlpha_ = 1.0f - Saturate(MenuTimer_ / std::max(style.FadeInDuration, 0.001f));
				if (FadeAlpha_ <= 0.0f) {
					MenuPhase_ = MenuPhase::Idle;
					MenuTimer_ = 0.0f;
				}
			}

			if (settingsOpen) { break; }		// 設定の画面を開いている間は選ばない（フェードインはそのまま進む）
			if (triggeredUp) {
				MenuSelected_ = (MenuSelected_ + NumMenuItems - 1) % NumMenuItems;
			}
			if (triggeredDown) {
				MenuSelected_ = (MenuSelected_ + 1) % NumMenuItems;
			}
			if (triggeredConfirm) {
				if (MenuSelected_ == 0) {
					StartOutro(false);	// Start：Outro へ（MenuPhase_ も Outro になる）
				}
				else if (MenuSelected_ == 1) {
					SettingsScreen_.Open();		// 設定：メニューの上に重ねて開く
					Game::Scene::BlockEscapeQuit = true;
				}
				else {
					MenuPhase_ = MenuPhase::Decided;	// Exit
					MenuTimer_ = 0.0f;
				}
			}
			break;

		case MenuPhase::Outro:
			// フェードイン途中で決めた場合も、黒はそのまま明けていく
			FadeAlpha_ = std::max(FadeAlpha_ - dt / std::max(style.FadeInDuration, 0.001f), 0.0f);
			break;

		case MenuPhase::Decided:
			// フェードイン途中で決めた場合も、黒はそのまま明けていく
			FadeAlpha_ = std::max(FadeAlpha_ - dt / std::max(style.FadeInDuration, 0.001f), 0.0f);
			if (MenuTimer_ >= style.BlinkDuration) {
				MenuPhase_ = MenuPhase::FadeOut;
				MenuTimer_ = 0.0f;
				FadeOutFrom_ = FadeAlpha_;
				IntroFadeColor_ = Lumina::Float3{ 0.0f, 0.0f, 0.0f };	// 終了時の暗転は黒
			}
			break;

		case MenuPhase::FadeOut:
			{
				float const t{ Saturate(MenuTimer_ / std::max(style.FadeOutDuration, 0.001f)) };
				FadeAlpha_ = FadeOutFrom_ + (1.0f - FadeOutFrom_) * t;
				if (t >= 1.0f) {
					MenuPhase_ = MenuPhase::Done;
					MenuTimer_ = 0.0f;
					// 0: Start → InGame へ / 1: Exit → main.cpp がウィンドウを閉じる
					Game::Scene::Flag = (MenuSelected_ == 0) ? 1 : -1;
				}
			}
			break;

		case MenuPhase::Done:
		default:
			break;
		}

		//----	------	------	------	------	----//
		// 見た目

		// Outro の頭でロゴ・メニューを消す（1 → 0。ロゴのフェードインと同じ曲線を逆にたどる）
		float const outroVisibility{ OutroUIVisibility() };
		float const outroEased{ 1.0f - (1.0f - outroVisibility) * (1.0f - outroVisibility) * (1.0f - outroVisibility) };

		float const follow{ 1.0f - std::exp(-std::max(style.Smoothing, 0.0f) * dt) };
		for (int idx{ 0 }; idx < NumMenuItems; ++idx) {
			float const target{ (idx == MenuSelected_) ? 1.0f : 0.0f };
			float& blend{ MenuBlend_[idx] };
			blend += (target - blend) * follow;

			float alpha{ style.UnselectedAlpha + (1.0f - style.UnselectedAlpha) * blend };
			float const scale{ style.UnselectedScale + (1.0f - style.UnselectedScale) * blend };

			// 決定した項目を点滅
			bool const isDecidedItem{ (idx == MenuSelected_) && (MenuPhase_ == MenuPhase::Decided) };
			if (isDecidedItem) {
				float const wave{
					std::cos(std::numbers::pi_v<float> * 2.0f * style.BlinkFrequency * MenuTimer_)
				};
				alpha *= (wave > 0.0f) ? 1.0f : 0.25f;
			}
			alpha *= outroEased;

			auto& item{ UI_Items_[idx] };
			item.Translate({
				style.RightX + style.SelectedOffsetX * blend,
				style.FirstY + style.Spacing * static_cast<float>(idx - 1),		// 設定（1）が FirstY。Start はその 1 つ上
			});
			item.Scale({ style.Width * scale, style.Height * scale });
			item.RGBA({ 1.0f, 1.0f, 1.0f, alpha });
		}

		//----	------	------	------	------	----//
		// タイトルロゴ（遅れて、下から浮き上がりながらフェードイン。決定後の暗転は黒のフェードに任せる）

		{
			auto const& logo{ LogoStyle_ };
			LogoTimer_ += dt;
			// Outro では今の位置から 0 へ逆再生する（浮き上がりも逆に沈む）
			float const t{
				std::min(
					Saturate((LogoTimer_ - logo.Delay) / std::max(logo.FadeDuration, 0.001f)),
					outroVisibility
				)
			};
			float const eased{ 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t) };	// 終わりでゆっくり止まる

			UI_Logo_.AnchorPoint({ logo.AnchorX, logo.AnchorY });
			UI_Logo_.Translate({ logo.X, logo.Y + logo.RiseDistance * (1.0f - eased) });
			UI_Logo_.Scale({ logo.Width, logo.Height });
			UI_Logo_.RGBA({ 1.0f, 1.0f, 1.0f, logo.Enabled ? (logo.Alpha * eased) : 0.0f });
		}

		//----	------	------	------	------	----//
		// ラベルのグロー：元のラベルと同じ場所に、にじむ分（RadiusPx）だけ大きいクアッドを置き、
		// UV も同じだけ 0..1 の外へ伸ばす。強さ = Intensity × 明滅 × 元ラベルの不透明度
		//（非選択は弱く、決定時の点滅やロゴのフェードインにも自然に追従する）

		GlowTimer_ += dt;
		{
			auto const& glow{ GlowStyle_ };
			float const r{ SRGBToLinear(glow.Color.x) };
			float const g{ SRGBToLinear(glow.Color.y) };
			float const b{ SRGBToLinear(glow.Color.z) };
			float const margin{ std::max(glow.RadiusPx, 0.0f) };

			Lumina::Sprite const* const sources[NumGlowTargets]{ &UI_Logo_, &UI_Items_[0], &UI_Items_[1], &UI_Items_[2] };
			for (int idx{ 0 }; idx < NumGlowTargets; ++idx) {
				auto const& source{ *sources[idx] };
				auto& target{ UI_Glows_[idx] };

				float const phase{ GlowTimer_ * glow.PulseFrequency + glow.PhaseStep * static_cast<float>(idx) };
				float const pulse{
					1.0f - Saturate(glow.PulseDepth) * (0.5f + 0.5f * std::cos(std::numbers::pi_v<float> * 2.0f * phase))
				};
				// Outro の消え始めに、選んだ項目のグローを強める（消えるにつれて元に戻る）
				bool const isOutroSelected{ (MenuPhase_ == MenuPhase::Outro) && (idx == 1 + MenuSelected_) };
				float const boost{ isOutroSelected ? (1.0f + Outro_.UI.SelectedGlowBoost * outroVisibility) : 1.0f };
				float const strength{ glow.Enabled ? (glow.Intensity * pulse * source.RGBA().w * boost) : 0.0f };

				Lumina::Vec2 const scale{ source.Scale() };
				Lumina::Vec2 const anchor{ source.AnchorPoint() };
				Lumina::Vec2 const translate{ source.Translate() };
				float const marginU{ (scale.x > 0.0f) ? (margin / scale.x) : 0.0f };
				float const marginV{ (scale.y > 0.0f) ? (margin / scale.y) : 0.0f };

				target.TextureID(source.TextureID());
				target.AnchorPoint({ anchor.x, anchor.y });
				target.Scale({ scale.x + margin * 2.0f, scale.y + margin * 2.0f });
				target.Translate({
					translate.x + margin * (anchor.x * 2.0f - 1.0f),
					translate.y + margin * (anchor.y * 2.0f - 1.0f),
				});
				target.UV(0U, { -marginU, -marginV });
				target.UV(1U, { 1.0f + marginU, -marginV });
				target.UV(2U, { -marginU, 1.0f + marginV });
				target.UV(3U, { 1.0f + marginU, 1.0f + marginV });
				// α はにじみの半径（px）として PS に渡す（TitleLabelGlow.PS.hlsl 参照）
				target.RGBA({ r * strength, g * strength, b * strength, margin });
			}
		}

		// 一番手前の全画面スプライト：Outro の白フラッシュ・フェードアウトがあればその色、無ければ黒のフェード
		Lumina::Float3 overlayColor{};
		float overlayAlpha{ 0.0f };
		OutroOverlay(overlayColor, overlayAlpha);
		if (overlayAlpha > 0.0f) {
			UI_Fade_.RGBA({ overlayColor.x, overlayColor.y, overlayColor.z, overlayAlpha });
		}
		else {
			UI_Fade_.RGBA({ IntroFadeColor_.x, IntroFadeColor_.y, IntroFadeColor_.z, Saturate(FadeAlpha_) });
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Render									//
	//////	//////	//////	//////	//////	//////

	void Title::RenderUI(Lumina::DX12::CommandList const& cmdList_) {
		// * 1. グロー（加算・ラベルの下）
		{
			auto& glowRenderer{ *UIGlowRenderer_ };
			glowRenderer.Begin(cmdList_);
			glowRenderer.BatchBegin();
			for (auto const& glow : UI_Glows_) {
				glowRenderer.Batch(glow);
			}
			glowRenderer.BatchEnd();
			glowRenderer.Render(
				PSO_UIGlow_,
				GlobalTable_UITextures_.GPUHandle(0U),
				LocalHeap_UI_.CPUHandle(0U)
			);
			glowRenderer.End();
		}

		// * 2. ラベルとフェード（アルファ合成）
		auto& renderer{ *UISpriteRenderer_ };

		// * 描画順 = Batch 順。ロゴ → メニュー → フェード（一番手前）
		renderer.Begin(cmdList_);
		renderer.BatchBegin();
		renderer.Batch(UI_Logo_);
		for (auto const& item : UI_Items_) {
			renderer.Batch(item);
		}
		renderer.Batch(UI_Fade_);
		renderer.BatchEnd();

		renderer.Render(
			PSO_UI_,
			GlobalTable_UITextures_.GPUHandle(0U),
			LocalHeap_UI_.CPUHandle(0U)
		);
		renderer.End();

		// * 3. 設定の画面（開いている間だけ。一番手前。Title はレンダーパスを使っていないので、続けて写して描ける）
		SettingsScreen_.Prepare(cmdList_);
		SettingsScreen_.Render();
	}

	//////	//////	//////	//////	//////	//////
	//	ImGui									//
	//////	//////	//////	//////	//////	//////

	void Title::EditUI() {
		#if defined(_DEBUG)
		ImGui::Begin("Menu");

		constexpr char const* phaseNames[]{ "FadeIn", "Idle", "Decided", "FadeOut", "Done", "Outro" };
		constexpr char const* itemNames[NumMenuItems]{ "Start", "Settings", "Exit" };
		ImGui::Text("Phase: %s  /  Selected: %s%s", phaseNames[static_cast<int>(MenuPhase_)], itemNames[MenuSelected_], SettingsScreen_.IsOpen() ? "  (settings open)" : "");
		ImGui::BeginDisabled(IsOutroActive());	// Outro 中は「Outro」ウィンドウの Rewind で戻す
		if (ImGui::Button("Replay (fade in)")) {
			ResetMenu();
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled("Up/Down or W/S to select, SPACE/ENTER to decide (ignored while ImGui has keyboard focus)");

		auto& style{ MenuStyle_ };
		ImGui::SeparatorText("Layout (1280x720, Y down)");
		ImGui::SliderFloat("RightX", &style.RightX, 0.0f, 1280.0f);
		ImGui::SliderFloat("FirstY", &style.FirstY, 0.0f, 720.0f);
		ImGui::SliderFloat("Spacing", &style.Spacing, 0.0f, 200.0f);
		ImGui::SliderFloat("Width", &style.Width, 0.0f, 720.0f);
		ImGui::SliderFloat("Height", &style.Height, 0.0f, 180.0f);

		ImGui::SeparatorText("Selection");
		ImGui::SliderFloat("UnselectedAlpha", &style.UnselectedAlpha, 0.0f, 1.0f);
		ImGui::SliderFloat("UnselectedScale", &style.UnselectedScale, 0.5f, 1.0f);
		ImGui::SliderFloat("SelectedOffsetX", &style.SelectedOffsetX, -100.0f, 100.0f);
		ImGui::SliderFloat("Smoothing (/s)", &style.Smoothing, 1.0f, 40.0f);

		ImGui::SeparatorText("Transitions");
		ImGui::SliderFloat("FadeInDuration", &style.FadeInDuration, 0.0f, 3.0f);
		ImGui::SliderFloat("BlinkDuration", &style.BlinkDuration, 0.0f, 2.0f);
		ImGui::SliderFloat("BlinkFrequency (Hz)", &style.BlinkFrequency, 1.0f, 30.0f);
		ImGui::SliderFloat("FadeOutDuration", &style.FadeOutDuration, 0.0f, 3.0f);

		if (ImGui::CollapsingHeader("Label Glow", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID("Glow");
			auto& glow{ GlowStyle_ };
			ImGui::Checkbox("Enabled", &glow.Enabled);
			ImGui::ColorEdit3("Color", &glow.Color.x);
			ImGui::SliderFloat("Intensity", &glow.Intensity, 0.0f, 4.0f);
			ImGui::SliderFloat("RadiusPx", &glow.RadiusPx, 0.0f, 64.0f);
			ImGui::SliderFloat("PulseDepth", &glow.PulseDepth, 0.0f, 1.0f);
			ImGui::SliderFloat("PulseFrequency (Hz)", &glow.PulseFrequency, 0.0f, 4.0f);
			ImGui::SliderFloat("PhaseStep", &glow.PhaseStep, 0.0f, 1.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Title Logo", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID("Logo");
			auto& logo{ LogoStyle_ };
			ImGui::TextDisabled("Texture: Assets/UI/Label.Title.png (placeholder, white on transparent)");
			ImGui::Checkbox("Enabled", &logo.Enabled);
			ImGui::SameLine();
			if (ImGui::Button("Replay")) { LogoTimer_ = 0.0f; }
			ImGui::SliderFloat("X", &logo.X, 0.0f, 1280.0f);
			ImGui::SliderFloat("Y", &logo.Y, 0.0f, 720.0f);
			ImGui::SliderFloat("AnchorX", &logo.AnchorX, 0.0f, 1.0f);
			ImGui::SliderFloat("AnchorY", &logo.AnchorY, 0.0f, 1.0f);
			ImGui::SliderFloat("Width", &logo.Width, 0.0f, 1280.0f);
			ImGui::SliderFloat("Height", &logo.Height, 0.0f, 720.0f);
			ImGui::SliderFloat("Alpha", &logo.Alpha, 0.0f, 1.0f);
			ImGui::SliderFloat("Delay", &logo.Delay, 0.0f, 3.0f);
			ImGui::SliderFloat("FadeDuration", &logo.FadeDuration, 0.0f, 3.0f);
			ImGui::SliderFloat("RiseDistance", &logo.RiseDistance, -100.0f, 100.0f);
			ImGui::PopID();
		}

		ImGui::End();
		#endif
	}
}
