module Game.PauseMenu;

// ここで import しているヘッダユニットは、いずれも Game.PauseMenu.ixx で import 済み
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <memory>;
import <optional>;
import <string>;
import <vector>;
import <d3d12.h>;

import Lumina;
import nlohmann.json;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace Game::PauseMenu {
	namespace {
		constexpr float MenuDeltaTime{ 1.0f / 60.0f };
		constexpr Lumina::Float2 FallbackImageSize{ 720.0f, 180.0f };

		auto Saturate(float x_) -> float { return std::clamp(x_, 0.0f, 1.0f); }

		using JSON = nlohmann::json;

		// キーがあって数値なら上書きする（無い・型が違うときは今の値のまま）
		void Read(JSON const& obj_, char const* key_, float& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = itr->get<float>(); }
		}
		void Read(JSON const& obj_, char const* key_, bool& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_boolean()) { out_ = itr->get<bool>(); }
		}

		// PNG の大きさをヘッダ（IHDR）から読む。シグネチャ 8 バイト → チャンク長 4 → "IHDR" 4 → 幅 4 → 高さ 4（ビッグエンディアン）
		auto ReadPngSize(char const* path_) -> std::optional<Lumina::Float2> {
			std::ifstream ifs{ path_, std::ios::binary };
			if (!ifs.is_open()) { return std::nullopt; }
			std::array<unsigned char, 24> h{};
			if (!ifs.read(reinterpret_cast<char*>(h.data()), static_cast<std::streamsize>(h.size()))) { return std::nullopt; }

			constexpr std::array<unsigned char, 8> signature{ 0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU, 0x1AU, 0x0AU };
			if (!std::equal(signature.begin(), signature.end(), h.begin())) { return std::nullopt; }
			if (h[12] != 'I' || h[13] != 'H' || h[14] != 'D' || h[15] != 'R') { return std::nullopt; }

			auto const be32{
				[&h](std::size_t at_) -> uint32_t {
					return (static_cast<uint32_t>(h[at_]) << 24U) | (static_cast<uint32_t>(h[at_ + 1U]) << 16U) |
						(static_cast<uint32_t>(h[at_ + 2U]) << 8U) | static_cast<uint32_t>(h[at_ + 3U]);
				}
			};
			uint32_t const width{ be32(16U) };
			uint32_t const height{ be32(20U) };
			if (width == 0U || height == 0U) { return std::nullopt; }
			return Lumina::Float2{ static_cast<float>(width), static_cast<float>(height) };
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Initialize								//
	//////	//////	//////	//////	//////	//////

	void Menu::Initialize(
		Lumina::DX12::Context const& dxContext_,
		Lumina::AssetManager& assetMngr_
	) {
		auto const& device{ dxContext_.Device() };

		Renderer_ = std::make_unique<Lumina::SpriteRenderer>();
		Renderer_->Initialize(dxContext_, 256U);

		// * Shaders：本編の UI と同じ
		dxContext_.Compile(VS_, L"Assets/Shaders/Sprite2.VS.hlsl", L"vs_6_6", L"main", "PauseMenu.VS");
		dxContext_.Compile(PS_, L"Assets/Shaders/SpriteUI.PS.hlsl", L"ps_6_6", L"main", "PauseMenu.PS");

		// * PSO：通常のアルファ合成（黒を重ねて暗くするため。本編の UI 用 PSO は加算）・深度なし
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

			PSO_.Initialize(
				device,
				Renderer_->RootSignature(),
				VS_,
				PS_,
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
				"PauseMenu PSO"
			);
		}

		// * Textures（Title とは別名で読み込む）。[0] = Blank、[1 + i] = 項目 i
		{
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "Pause.Blank", "Assets/Img/Blank.png" },
					{ "Pause.Item0", ItemImages[0] },
					{ "Pause.Item1", ItemImages[1] },
					{ "Pause.Item2", ItemImages[2] },
					{ "Pause.Hint", HintImage },
					{ "Pause.QuitHint", QuitHintImage },
				}
			);
			GlobalTable_Textures_ = dxContext_.GlobalDescriptorHeap().Allocate(8U);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_Textures_.CPUHandle(idx),
					assetMngr_.Graphics().CPUHandle(texIDs.at(idx)),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}
		}

		// * 正射影（本編の UI と同じ：左上原点・Y 下向き・1280x720）
		{
			Lumina::Mat4 const worldToNDC{ Lumina::Mat4::Orthographic(0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 1.0f) };
			UB_Ortho_.Initialize(device, 256LLU, "PauseMenu Ortho");
			UB_Ortho_.Store(&worldToNDC, sizeof(Lumina::Mat4), 0LLU);
			LocalHeap_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1U, false);
			Lumina::DX12::CBV::Create(device, LocalHeap_.CPUHandle(0U), UB_Ortho_);
		}

		// * Sprites
		Dim_.Translate({ 0.0f, 0.0f });
		Dim_.Scale({ 1280.0f, 720.0f });
		Dim_.AnchorPoint({ 0.0f, 0.0f });
		Dim_.TextureID(0U);
		for (uint32_t idx{ 0U }; idx < NumItems; ++idx) {
			ImageSizes_[idx] = ReadPngSize(ItemImages[idx]).value_or(FallbackImageSize);
			Items_[idx].AnchorPoint({ 0.5f, 0.5f });	// 中央揃え
			Items_[idx].TextureID(1U + idx);
		}
		HintSize_ = ReadPngSize(HintImage).value_or(Lumina::Float2{ 597.0f, 128.0f });
		Hint_.AnchorPoint({ 1.0f, 1.0f });		// 右下の角に合わせる
		Hint_.TextureID(1U + NumItems);
		QuitHintSize_ = ReadPngSize(QuitHintImage).value_or(Lumina::Float2{ 387.0f, 134.0f });
		QuitHint_.AnchorPoint({ 1.0f, 1.0f });	// 右下の角に合わせる（「ESC : ポーズ」の上に積む）
		QuitHint_.TextureID(2U + NumItems);

		Close();

		// 調整値（無ければ初期値のまま）
		Load();
	}

	//////	//////	//////	//////	//////	//////
	//	Update									//
	//////	//////	//////	//////	//////	//////

	void Menu::Open() {
		IsOpen_ = true;
		WaitRelease_ = false;
		Selected_ = 0;
		OpenTimer_ = 0.0f;
		for (uint32_t idx{ 0U }; idx < NumItems; ++idx) {
			Blend_[idx] = (static_cast<int>(idx) == Selected_) ? 1.0f : 0.0f;
		}
	}

	void Menu::Close() {
		IsOpen_ = false;
		WaitRelease_ = false;
		OpenTimer_ = 0.0f;
	}

	auto Menu::Update(Lumina::WinApp::RawInput const& input_) -> Action {
		// 入力（押した瞬間だけ拾う）
		auto const& keyboard{ input_.Keyboard() };
		using Lumina::WinApp::KEY;
		bool const keyEsc{ keyboard.IsPressed(KEY::ESC) };
		bool const keyUp{ keyboard.IsPressed(KEY::ARROW_UP) || keyboard.IsPressed(KEY::W) };
		bool const keyDown{ keyboard.IsPressed(KEY::ARROW_DOWN) || keyboard.IsPressed(KEY::S) };
		bool const keyConfirm{ keyboard.IsPressed(KEY::SPACE) || keyboard.IsPressed(KEY::ENTER) };

		bool isInputBlocked{ false };
		#if defined(_DEBUG)
		// ImGui の入力欄を触っている間はメニューを動かさない
		isInputBlocked = ImGui::GetIO().WantCaptureKeyboard;
		#endif

		bool const triggeredEsc{ keyEsc && !PrevEsc_ && !isInputBlocked };
		bool const triggeredUp{ keyUp && !PrevUp_ && !isInputBlocked };
		bool const triggeredDown{ keyDown && !PrevDown_ && !isInputBlocked };
		bool const triggeredConfirm{ keyConfirm && !PrevConfirm_ && !isInputBlocked };
		PrevEsc_ = keyEsc;
		PrevUp_ = keyUp;
		PrevDown_ = keyDown;
		PrevConfirm_ = keyConfirm;

		// 決定で閉じたあと、決定キーを離すまではゲームを止めたまま（SPACE がそのままジャンプ・再開に化けないように）
		if (WaitRelease_ && !keyConfirm) { WaitRelease_ = false; }

		Action action{ Action::None };
		if (!IsOpen_) {
			if (triggeredEsc) { Open(); }
		}
		else if (triggeredEsc) {
			Close();
			action = Action::Resume;
		}
		else {
			int const count{ static_cast<int>(NumItems) };
			if (triggeredUp) { Selected_ = (Selected_ + count - 1) % count; }
			if (triggeredDown) { Selected_ = (Selected_ + 1) % count; }
			if (triggeredConfirm) {
				constexpr Action actions[NumItems]{ Action::Resume, Action::Restart, Action::ReturnToTitle };
				action = actions[Selected_];
				Close();
				WaitRelease_ = true;
			}
		}

		if (IsOpen_) {
			OpenTimer_ += MenuDeltaTime;
			UpdateSprites();
		}
		return action;
	}

	void Menu::UpdateSprites() {
		Style const& style{ Look };
		float const appear{ Saturate(OpenTimer_ / std::max(style.DimFadeTime, 0.001f)) };
		float const appearEased{ 1.0f - (1.0f - appear) * (1.0f - appear) };

		Dim_.RGBA({ 0.0f, 0.0f, 0.0f, Saturate(style.DimAlpha) * appearEased });

		float const follow{ 1.0f - std::exp(-std::max(style.Smoothing, 0.0f) * MenuDeltaTime) };
		float const middle{ static_cast<float>(NumItems - 1U) * 0.5f };
		for (uint32_t idx{ 0U }; idx < NumItems; ++idx) {
			float const target{ (static_cast<int>(idx) == Selected_) ? 1.0f : 0.0f };
			float& blend{ Blend_[idx] };
			blend += (target - blend) * follow;

			float const alpha{ (style.UnselectedAlpha + (1.0f - style.UnselectedAlpha) * blend) * appearEased };
			float const scale{ (style.UnselectedScale + (1.0f - style.UnselectedScale) * blend) * std::max(style.LabelScale, 0.0f) };

			Lumina::Sprite& item{ Items_[idx] };
			item.Translate({ style.CenterX, style.CenterY + style.Spacing * (static_cast<float>(idx) - middle) });
			item.Scale({ ImageSizes_[idx].x * scale, ImageSizes_[idx].y * scale });
			item.RGBA({ 1.0f, 1.0f, 1.0f, alpha });
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Render									//
	//////	//////	//////	//////	//////	//////

	void Menu::UpdateHint() {
		Style const& style{ Look };
		float const scale{ std::max(style.HintScale, 0.0f) };
		Hint_.Translate({ 1280.0f - style.HintMarginX, 720.0f - style.HintMarginY });
		Hint_.Scale({ HintSize_.x * scale, HintSize_.y * scale });
		Hint_.RGBA({ 1.0f, 1.0f, 1.0f, Saturate(style.HintAlpha) });

		// 「Q : 自害」は「ESC : ポーズ」の真上（右端そろえ）
		float const hintHeight{ HintSize_.y * scale };
		QuitHint_.Translate({ 1280.0f - style.HintMarginX, 720.0f - style.HintMarginY - hintHeight - style.QuitHintGap });
		QuitHint_.Scale({ QuitHintSize_.x * scale, QuitHintSize_.y * scale });
		QuitHint_.RGBA({ 1.0f, 1.0f, 1.0f, Saturate(style.HintAlpha) });
	}

	void Menu::Prepare(Lumina::DX12::CommandList const& cmdList_, bool showHint_, bool showQuitHint_) {
		IsPrepared_ = false;
		if (!Renderer_) { return; }
		bool const drawMenu{ IsOpen_ };
		bool const drawHint{ !IsOpen_ && showHint_ && Look.ShowHint };
		bool const drawQuitHint{ !IsOpen_ && showQuitHint_ && Look.ShowQuitHint };
		if (!drawMenu && !drawHint && !drawQuitHint) { return; }

		// 描画順 = Batch 順。暗幕 → ラベル（閉じているときは操作ヒントだけ）。
		// BatchEnd がアップロード → デフォルトバッファのコピーを積む（パスの外で）
		auto& renderer{ *Renderer_ };
		renderer.Begin(cmdList_);
		renderer.BatchBegin();
		if (drawMenu) {
			renderer.Batch(Dim_);
			for (auto const& item : Items_) {
				renderer.Batch(item);
			}
		}
		else {
			UpdateHint();
			if (drawHint) { renderer.Batch(Hint_); }
			if (drawQuitHint) { renderer.Batch(QuitHint_); }
		}
		renderer.BatchEnd();
		IsPrepared_ = true;
	}

	void Menu::Render() {
		if (!IsPrepared_) { return; }
		IsPrepared_ = false;

		auto& renderer{ *Renderer_ };
		renderer.Render(PSO_, GlobalTable_Textures_.GPUHandle(0U), LocalHeap_.CPUHandle(0U));
		renderer.End();
	}

	//////	//////	//////	//////	//////	//////
	//	保存・読み込み							//
	//////	//////	//////	//////	//////	//////

	auto Menu::Load() -> bool {
		std::ifstream ifs{ ConfigPath };
		if (!ifs.is_open()) { Status_ = "No config file (using defaults)."; return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { Status_ = "Config is not valid JSON."; return false; }

		Style& s{ Look };
		Read(root, "DimAlpha", s.DimAlpha);
		Read(root, "DimFadeTime", s.DimFadeTime);
		Read(root, "CenterX", s.CenterX);
		Read(root, "CenterY", s.CenterY);
		Read(root, "Spacing", s.Spacing);
		Read(root, "LabelScale", s.LabelScale);
		Read(root, "UnselectedAlpha", s.UnselectedAlpha);
		Read(root, "UnselectedScale", s.UnselectedScale);
		Read(root, "Smoothing", s.Smoothing);
		Read(root, "ShowHint", s.ShowHint);
		Read(root, "HintMarginX", s.HintMarginX);
		Read(root, "HintMarginY", s.HintMarginY);
		Read(root, "HintScale", s.HintScale);
		Read(root, "HintAlpha", s.HintAlpha);
		Read(root, "ShowQuitHint", s.ShowQuitHint);
		Read(root, "QuitHintGap", s.QuitHintGap);
		Status_ = "Loaded.";
		return true;
	}

	auto Menu::Save() -> bool {
		Style const& s{ Look };
		JSON const root{
			{ "DimAlpha", s.DimAlpha },
			{ "DimFadeTime", s.DimFadeTime },
			{ "CenterX", s.CenterX },
			{ "CenterY", s.CenterY },
			{ "Spacing", s.Spacing },
			{ "LabelScale", s.LabelScale },
			{ "UnselectedAlpha", s.UnselectedAlpha },
			{ "UnselectedScale", s.UnselectedScale },
			{ "Smoothing", s.Smoothing },
			{ "ShowHint", s.ShowHint },
			{ "HintMarginX", s.HintMarginX },
			{ "HintMarginY", s.HintMarginY },
			{ "HintScale", s.HintScale },
			{ "HintAlpha", s.HintAlpha },
			{ "ShowQuitHint", s.ShowQuitHint },
			{ "QuitHintGap", s.QuitHintGap },
		};

		std::ofstream ofs{ ConfigPath, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { Status_ = "Save failed (cannot open file)."; return false; }
		ofs << root.dump(2) << '\n';
		if (!ofs.good()) { Status_ = "Save failed (write error)."; return false; }
		Status_ = "Saved.";
		return true;
	}

	//////	//////	//////	//////	//////	//////
	//	ImGui									//
	//////	//////	//////	//////	//////	//////

	void Menu::DrawEditor() {
		#if defined(_DEBUG)
		if (!ImGui::Begin("Pause Menu")) {
			ImGui::End();
			return;
		}
		ImGui::Text("%s / Selected: %s%s", IsOpen_ ? "Open" : "Closed", ItemNames[Selected_], WaitRelease_ ? " (waiting for key release)" : "");
		ImGui::TextDisabled("ESC: open / resume, Up/Down or W/S: select, SPACE/ENTER: decide");

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { Look = Style{}; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		Style& style{ Look };
		ImGui::SeparatorText("Dim");
		ImGui::SliderFloat("Alpha", &style.DimAlpha, 0.0f, 1.0f);
		ImGui::SliderFloat("Fade time (s)", &style.DimFadeTime, 0.0f, 1.0f);

		ImGui::SeparatorText("Layout (1280x720, Y down, centered)");
		ImGui::SliderFloat("CenterX", &style.CenterX, 0.0f, 1280.0f);
		ImGui::SliderFloat("CenterY", &style.CenterY, 0.0f, 720.0f);
		ImGui::SliderFloat("Spacing", &style.Spacing, 0.0f, 300.0f);
		ImGui::SliderFloat("Label scale", &style.LabelScale, 0.05f, 2.0f);
		for (uint32_t idx{ 0U }; idx < NumItems; ++idx) {
			ImGui::TextDisabled("%s: %s (%.0fx%.0f)", ItemNames[idx], ItemImages[idx], ImageSizes_[idx].x, ImageSizes_[idx].y);
		}

		ImGui::SeparatorText("Selection");
		ImGui::SliderFloat("Unselected alpha", &style.UnselectedAlpha, 0.0f, 1.0f);
		ImGui::SliderFloat("Unselected scale", &style.UnselectedScale, 0.5f, 1.0f);
		ImGui::SliderFloat("Smoothing (/s)", &style.Smoothing, 1.0f, 40.0f);

		ImGui::SeparatorText("Hint (bottom right, hidden in STARTUP / pause)");
		ImGui::Checkbox("Show hint", &style.ShowHint);
		ImGui::SliderFloat("Hint margin X", &style.HintMarginX, 0.0f, 200.0f);
		ImGui::SliderFloat("Hint margin Y", &style.HintMarginY, 0.0f, 200.0f);
		ImGui::SliderFloat("Hint scale", &style.HintScale, 0.05f, 1.0f);
		ImGui::SliderFloat("Hint alpha", &style.HintAlpha, 0.0f, 1.0f);
		ImGui::TextDisabled("%s (%.0fx%.0f)", HintImage, HintSize_.x, HintSize_.y);
		ImGui::Checkbox("Show quit hint (Q)", &style.ShowQuitHint);
		ImGui::SliderFloat("Quit hint gap", &style.QuitHintGap, -40.0f, 100.0f);
		ImGui::TextDisabled("%s (%.0fx%.0f)", QuitHintImage, QuitHintSize_.x, QuitHintSize_.y);
		ImGui::End();
		#endif
	}
}
