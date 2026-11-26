module Game.KeyConfigScreen;

// ここで import しているヘッダユニットは、いずれも Game.KeyConfigScreen.ixx で import 済み
import <algorithm>;
import <array>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <memory>;
import <optional>;
import <string>;
import <string_view>;
import <vector>;
import <d3d12.h>;

import Lumina;
import Lumina.Input;

import Game.KeyConfig;
import Game.UIFont;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace Game::KeyConfigScreen {
	namespace {
		constexpr float DeltaTime{ 1.0f / 60.0f };

		// 仮想キーコード（GE3 の KEY と同じ値）
		constexpr std::size_t VkReturn{ 0x0DU }, VkEscape{ 0x1BU }, VkSpace{ 0x20U };
		constexpr std::size_t VkUp{ 0x26U }, VkDown{ 0x28U }, VkW{ 0x57U }, VkS{ 0x53U };

		using Game::KeyConfig::Slot;

		auto Saturate(float x_) noexcept -> float { return std::clamp(x_, 0.0f, 1.0f); }

		auto MakeRect(float x_, float y_, float w_, float h_, Lumina::Float4 const& rgba_) -> Lumina::Sprite {
			Lumina::Sprite s{};
			s.AnchorPoint({ 0.0f, 0.0f });
			s.Translate({ x_, y_ });
			s.Scale({ w_, h_ });
			s.RGBA({ rgba_.x, rgba_.y, rgba_.z, rgba_.w });
			s.TextureID(0U);		// Blank
			return s;
		}

		constexpr std::uint32_t FontTexture{ 1U };
	}

	auto Screen::Rows() noexcept -> std::array<Row, NumRows> const& {
		static std::array<Row, NumRows> const rows{ {
			{ RowKind::Binding, Slot::MoveForward, "前へ移動", "" },
			{ RowKind::Binding, Slot::MoveBack, "後ろへ移動", "" },
			{ RowKind::Binding, Slot::MoveLeft, "左へ移動", "" },
			{ RowKind::Binding, Slot::MoveRight, "右へ移動", "" },
			{ RowKind::Binding, Slot::Attack, "攻撃", "" },
			{ RowKind::Binding, Slot::Guard, "ガード", "" },
			{ RowKind::Binding, Slot::Dodge, "回避", "" },
			{ RowKind::Binding, Slot::Jump, "ジャンプ", "" },
			{ RowKind::Fixed, Slot::Count, "視点", "マウス移動" },
			{ RowKind::Fixed, Slot::Count, "ポーズ", "ESC" },
			{ RowKind::Reset, Slot::Count, "初期設定に戻す", "" },
			{ RowKind::Back, Slot::Count, "戻る", "" },
		} };
		return rows;
	}

	//////	//////	//////	//////	//////	//////
	//	Initialize								//
	//////	//////	//////	//////	//////	//////

	void Screen::Initialize(Lumina::DX12::Context const& dxContext_, Lumina::AssetManager& assetMngr_, std::string_view texturePrefix_) {
		auto const& device{ dxContext_.Device() };

		Renderer_ = std::make_unique<Lumina::SpriteRenderer>();
		Renderer_->Initialize(dxContext_, 1024U);

		// * Shaders：本編の UI と同じ
		dxContext_.Compile(VS_, L"Assets/Shaders/Sprite2.VS.hlsl", L"vs_6_6", L"main", "KeyConfig.VS");
		dxContext_.Compile(PS_, L"Assets/Shaders/SpriteUI.PS.hlsl", L"ps_6_6", L"main", "KeyConfig.PS");

		// * PSO：通常のアルファ合成・深度なし（PauseMenu と同じ）
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
				"KeyConfig PSO"
			);
		}

		// * Textures（呼び出し元ごとに別名で読み込む）。[0] = Blank、[1] = 文字アトラス
		{
			std::string const prefix{ texturePrefix_ };
			std::string const blankName{ prefix + ".KeyConfig.Blank" };
			std::string const fontName{ prefix + ".KeyConfig.Font" };
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ blankName, "Assets/Img/Blank.png" },
					{ fontName, FontImage },
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
			UB_Ortho_.Initialize(device, 256LLU, "KeyConfig Ortho");
			UB_Ortho_.Store(&worldToNDC, sizeof(Lumina::Mat4), 0LLU);
			LocalHeap_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1U, false);
			Lumina::DX12::CBV::Create(device, LocalHeap_.CPUHandle(0U), UB_Ortho_);
		}

		Font_.Load(FontJson);		// 読めなければ文字なし（パネルだけ出る）
		Sprites_.reserve(1024U);
	}

	//////	//////	//////	//////	//////	//////
	//	Update									//
	//////	//////	//////	//////	//////	//////

	void Screen::Open() {
		IsOpen_ = true;
		SyncNext_ = true;
		Waiting_ = false;
		Selected_ = 0;
		Timer_ = 0.0f;
		Message_.clear();
		MessageTimer_ = 0.0f;
		BuildSprites();
	}

	void Screen::Close() {
		IsOpen_ = false;
		Waiting_ = false;
	}

	void Screen::MoveSelection(int dir_) {
		auto const& rows{ Rows() };
		int const n{ static_cast<int>(NumRows) };
		int i{ Selected_ };
		for (int k = 0; k < n; ++k) {
			i = (i + dir_ + n) % n;
			if (rows[static_cast<std::size_t>(i)].Kind != RowKind::Fixed) {
				Selected_ = i;
				return;
			}
		}
	}

	void Screen::Update(Lumina::WinApp::RawInput const& input_) {
		if (!IsOpen_) { return; }
		std::array<bool, 256> keys{};
		auto const& keyboard{ input_.Keyboard() };
		for (std::size_t vk = 1U; vk < keys.size(); ++vk) {
			keys[vk] = keyboard.IsPressed(static_cast<Lumina::WinApp::KEY>(vk));
		}
		auto const& mouse{ input_.Mouse() };
		std::array<bool, 2> const buttons{ mouse.LeftButton() != 0, mouse.RightButton() != 0 };

		bool blocked{ false };
#if defined(_DEBUG)
		blocked = ImGui::GetIO().WantCaptureKeyboard;		// ImGui の入力欄を触っている間は動かさない
#endif
		Step(keys, buttons, blocked);
	}

	void Screen::Step(std::array<bool, 256> const& keys_, std::array<bool, 2> const& mouse_, bool blocked_) {
		if (!IsOpen_) { return; }
		Timer_ += DeltaTime;
		MessageTimer_ = std::max(MessageTimer_ - DeltaTime, 0.0f);
		if (Waiting_) { WaitTimer_ += DeltaTime; }

		if (SyncNext_) {
			PrevKeys_ = keys_;
			PrevMouse_ = mouse_;
			SyncNext_ = false;
			BuildSprites();
			return;
		}

		auto const pressed{ [&](std::size_t vk_) { return keys_[vk_] && !PrevKeys_[vk_] && !blocked_; } };
		auto const& rows{ Rows() };
		Row const& row{ rows[static_cast<std::size_t>(Selected_)] };

		auto const say{ [this](std::string text_) {
			Message_ = std::move(text_);
			MessageTimer_ = Look.MessageTime;
		} };

		if (Waiting_) {
			if (pressed(VkEscape)) {
				Waiting_ = false;
			}
			else {
				// 押した瞬間のキー・マウスボタンを 1 つ探す（キーは番号の小さい順、その後マウス）
				std::optional<Lumina::Input::Source> src{};
				for (std::size_t vk = 0x07U; vk < keys_.size() && !src; ++vk) {		// 0x01〜0x06 はマウスボタンの仮想キー（下のマウスで拾う）
					if (pressed(vk)) { src = Lumina::Input::FromKey(static_cast<std::uint16_t>(vk)); }
				}
				if (!src && !blocked_) {
					if (mouse_[0] && !PrevMouse_[0]) { src = Lumina::Input::FromMouse(Lumina::Input::MouseButton::Left); }
					else if (mouse_[1] && !PrevMouse_[1]) { src = Lumina::Input::FromMouse(Lumina::Input::MouseButton::Right); }
				}
				if (src) {
					if (Game::KeyConfig::IsReserved(*src)) {
						say("そのキーは使えません（ESC・F1・F2・Q）");
					}
					else {
						Game::KeyConfig::Bindings b{ Game::KeyConfig::Current() };
						std::optional<Slot> const swapped{ Game::KeyConfig::Assign(b, row.Slot, *src) };
						bool const saved{ Game::KeyConfig::Apply(b) };
						Waiting_ = false;
						if (!saved) { say("保存できませんでした（このあいだは有効）"); }
						else if (swapped) { say("「" + std::string{ Game::KeyConfig::SlotLabel(*swapped) } + "」と入れ替えました"); }
						else { Message_.clear(); }
					}
				}
			}
		}
		else {
			bool const up{ pressed(VkUp) || pressed(VkW) };
			bool const down{ pressed(VkDown) || pressed(VkS) };
			bool const confirm{ pressed(VkSpace) || pressed(VkReturn) };
			if (pressed(VkEscape)) {
				Close();
			}
			else {
				if (up) { MoveSelection(-1); }
				if (down) { MoveSelection(+1); }
				if (confirm) {
					Row const& r{ rows[static_cast<std::size_t>(Selected_)] };
					switch (r.Kind) {
						case RowKind::Binding:
							Waiting_ = true;
							WaitTimer_ = 0.0f;
							Message_.clear();
							break;
						case RowKind::Reset:
							Game::KeyConfig::Apply(Game::KeyConfig::Defaults());
							say("初期設定に戻しました");
							break;
						case RowKind::Back:
							Close();
							break;
						default:
							break;
					}
				}
			}
		}

		PrevKeys_ = keys_;
		PrevMouse_ = mouse_;
		BuildSprites();
	}

	//////	//////	//////	//////	//////	//////
	//	見た目									//
	//////	//////	//////	//////	//////	//////

	auto Screen::RowY(std::size_t i_) const noexcept -> float {
		float y{ Look.FirstRowY + Look.RowHeight * static_cast<float>(i_) };
		if (i_ >= Game::KeyConfig::NumSlots) { y += Look.FixedGap; }
		if (i_ >= Game::KeyConfig::NumSlots + 2U) { y += Look.CommandGap; }
		return y;
	}

	void Screen::BuildSprites() {
		Sprites_.clear();
		Style const& s{ Look };
		float const appear{ Saturate(Timer_ / std::max(s.FadeTime, 0.001f)) };
		float const a{ 1.0f - (1.0f - appear) * (1.0f - appear) };
		Lumina::Float3 const acc{ s.Accent };

		// 暗幕とパネル
		Sprites_.push_back(MakeRect(0.0f, 0.0f, 1280.0f, 720.0f, { 0.0f, 0.0f, 0.0f, s.DimAlpha * a }));
		Sprites_.push_back(MakeRect(s.PanelX, s.PanelY, s.PanelW, s.PanelH, { s.PanelColor.x, s.PanelColor.y, s.PanelColor.z, s.PanelColor.w * a }));
		Sprites_.push_back(MakeRect(s.LabelX - 20.0f, s.RuleY, s.KeyX - s.LabelX + 40.0f, 2.0f, { acc.x, acc.y, acc.z, 0.6f * a }));

		float const center{ s.PanelX + s.PanelW * 0.5f };
		Font_.Append(Sprites_, "設定", center, s.TitleY, s.TitleSize, { 1.0f, 1.0f, 1.0f, a }, FontTexture, 0.5f);
		Font_.Append(Sprites_, "操作（キーボード・マウス）", s.LabelX, s.SectionY, s.SectionSize, { 1.0f, 1.0f, 1.0f, 0.55f * a }, FontTexture);

		auto const& rows{ Rows() };
		auto const& keys{ Game::KeyConfig::Current() };
		float const lineH{ Font_.LineHeight(s.TextSize) };
		for (std::size_t i = 0U; i < NumRows; ++i) {
			Row const& r{ rows[i] };
			float const y{ RowY(i) };
			bool const selected{ static_cast<int>(i) == Selected_ };

			if (selected) {
				float const barY{ y + (lineH - s.RowHeight) * 0.5f };
				Sprites_.push_back(MakeRect(s.LabelX - 20.0f, barY, s.KeyX - s.LabelX + 40.0f, s.RowHeight, { acc.x, acc.y, acc.z, 0.14f * a }));
				Sprites_.push_back(MakeRect(s.LabelX - 20.0f, barY, 4.0f, s.RowHeight, { acc.x, acc.y, acc.z, 0.9f * a }));
			}

			float const textAlpha{ (r.Kind == RowKind::Fixed) ? 0.4f : (selected ? 1.0f : 0.75f) };
			Lumina::Float4 const labelColor{ 1.0f, 1.0f, 1.0f, textAlpha * a };
			Lumina::Float4 const keyColor{
				selected ? Lumina::Float4{ acc.x, acc.y, acc.z, a } : Lumina::Float4{ 1.0f, 1.0f, 1.0f, textAlpha * a }
			};
			Font_.Append(Sprites_, r.Label, s.LabelX, y, s.TextSize, labelColor, FontTexture);

			if (r.Kind == RowKind::Binding) {
				if (selected && Waiting_) {
					// 点滅（0.5 秒周期）
					float const blink{ (std::fmod(WaitTimer_, 0.5f) < 0.3f) ? 1.0f : 0.35f };
					Font_.Append(Sprites_, "押してください", s.KeyX, y, s.TextSize, { acc.x, acc.y, acc.z, blink * a }, FontTexture, 1.0f);
				}
				else {
					std::string const name{ Game::KeyConfig::SourceName(keys[static_cast<std::size_t>(r.Slot)]) };
					Font_.Append(Sprites_, name, s.KeyX, y, s.TextSize, keyColor, FontTexture, 1.0f);
				}
			}
			else if (r.Kind == RowKind::Fixed) {
				Font_.Append(Sprites_, r.Value, s.KeyX, y, s.TextSize, keyColor, FontTexture, 1.0f);
			}
		}

		// 知らせ（入れ替え・使えないキー）。最後の 0.5 秒で消える
		if (!Message_.empty() && MessageTimer_ > 0.0f) {
			float const m{ Saturate(MessageTimer_ / 0.5f) };
			Font_.Append(Sprites_, Message_, center, s.MessageY, s.MessageSize, { acc.x, acc.y, acc.z, m * a }, FontTexture, 0.5f);
		}

		char const* footer{
			Waiting_ ? "割り当てるキーかマウスボタンを押してください　ESC：やめる"
					 : "↑↓：選ぶ　Space / Enter：決める　ESC：戻る"
		};
		Font_.Append(Sprites_, footer, center, s.FooterY, s.FooterSize, { 1.0f, 1.0f, 1.0f, 0.6f * a }, FontTexture, 0.5f);

		if (Sprites_.size() > 1024U) { Sprites_.resize(1024U); }	// SpriteRenderer の容量
	}

	//////	//////	//////	//////	//////	//////
	//	Render									//
	//////	//////	//////	//////	//////	//////

	void Screen::Prepare(Lumina::DX12::CommandList const& cmdList_) {
		IsPrepared_ = false;
		if (!IsOpen_ || !Renderer_ || Sprites_.empty()) { return; }
		auto& renderer{ *Renderer_ };
		renderer.Begin(cmdList_);
		renderer.BatchBegin();
		for (auto const& sprite : Sprites_) {
			renderer.Batch(sprite);
		}
		renderer.BatchEnd();
		IsPrepared_ = true;
	}

	void Screen::Render() {
		if (!IsPrepared_) { return; }
		IsPrepared_ = false;
		auto& renderer{ *Renderer_ };
		renderer.Render(PSO_, GlobalTable_Textures_.GPUHandle(0U), LocalHeap_.CPUHandle(0U));
		renderer.End();
	}
}
