//////	//////	//////	//////	//////	//////
//	KeyConfigScreen - 設定（キー割り当て）	//
//////	//////	//////	//////	//////	//////
// 操作方法の一覧を兼ねたキー割り当ての画面（2026-11-11）。タイトルとポーズメニューの「設定」から開く。
// 割り当ての中身は Game.KeyConfig（共有の表・保存）。この画面は表示と操作だけ。
//
// 見た目: 全画面を暗くして、中央のパネルに「設定」と表を出す。文字は Kaisei Decol Bold の文字アトラス（Game.UIFont）。
//   行: 前へ移動 / 後ろへ移動 / 左へ移動 / 右へ移動 / 攻撃 / ガード / 回避 / ジャンプ（変えられる）
//       視点 / ポーズ（変えられない。薄く表示）
//       初期設定に戻す / 戻る
// 操作: ↑↓ / W S で選択、Space / Enter で決定（割り当ての行なら「押してください」になり、次に押したキー・マウスボタンを割り当てる。
//       ESC でやめる）、ESC で閉じる。すでに別の行に使われているキーなら入れ替える。使えないキー（ESC・F1・F2・Q）は断る。
//       どれも押した瞬間だけ拾う。開いた最初のフレームは押しっぱなしを拾わない。
// 描画: 呼び出し側が Prepare（レンダーパスの外）と Render（中）を呼ぶ（SpriteRenderer の BatchEnd がコピーを積むため。PauseMenu と同じ）。
// 時間: 固定の 1/60 秒。

export module Game.KeyConfigScreen;

//****	******	******	******	******	****//

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
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

//////	//////	//////	//////	//////	//////

export namespace Game::KeyConfigScreen {
	// 配置（1280x720、左上原点・Y 下向き）と色（リニアのまま SpriteUI.PS に渡す）
	struct Style {
		float DimAlpha{ 0.75f };
		float FadeTime{ 0.15f };

		float PanelX{ 290.0f };
		float PanelY{ 48.0f };
		float PanelW{ 700.0f };
		float PanelH{ 624.0f };
		Lumina::Float4 PanelColor{ 0.05f, 0.06f, 0.09f, 0.25f };
		Lumina::Float3 Accent{ 1.0f, 0.72f, 0.40f };		// Title のラベルのグローと同じ色

		float TitleY{ 62.0f };
		float TitleSize{ 40.0f };
		float RuleY{ 122.0f };
		float SectionY{ 132.0f };
		float SectionSize{ 18.0f };

		float LabelX{ 340.0f };
		float KeyX{ 940.0f };				// 右端
		float FirstRowY{ 164.0f };
		float RowHeight{ 32.0f };
		float TextSize{ 24.0f };
		float FixedGap{ 8.0f };				// 変えられない行の前の空き
		float CommandGap{ 12.0f };			// 「初期設定に戻す」の前の空き

		float MessageY{ 586.0f };
		float MessageSize{ 20.0f };
		float MessageTime{ 2.5f };			// 秒
		float FooterY{ 626.0f };
		float FooterSize{ 18.0f };
	};

	class Screen {
	public:
		Style Look{};

		// texturePrefix_ はアセットマネージャに読み込む名前の頭（"Pause" / "Title"。同じ画像を別名で読む）
		void Initialize(Lumina::DX12::Context const& dxContext_, Lumina::AssetManager& assetMngr_, std::string_view texturePrefix_);

		void Open();
		void Close();
		auto IsOpen() const noexcept -> bool { return IsOpen_; }
		auto IsWaitingKey() const noexcept -> bool { return Waiting_; }

		// 開いている間、毎フレーム。ESC か「戻る」で閉じる（IsOpen が false になる）
		void Update(Lumina::WinApp::RawInput const& input_);

		void Prepare(Lumina::DX12::CommandList const& cmdList_);	// レンダーパスの外
		void Render();												// レンダーパスの中（RT / Viewport / DescriptorHeap は呼び出し側で設定済み）

		static constexpr char const* FontJson{ "Assets/UI/Font.KaiseiDecol.json" };
		static constexpr char const* FontImage{ "Assets/UI/Font.KaiseiDecol.png" };

	public:
		// 行（テスト・表示用に公開）
		enum class RowKind : std::uint8_t { Binding, Fixed, Reset, Back };
		struct Row {
			RowKind Kind{ RowKind::Fixed };
			Game::KeyConfig::Slot Slot{ Game::KeyConfig::Slot::Count };
			char const* Label{ "" };
			char const* Value{ "" };		// Fixed の右側
		};
		static constexpr std::size_t NumRows{ Game::KeyConfig::NumSlots + 4U };
		static auto Rows() noexcept -> std::array<Row, NumRows> const&;
		auto Selected() const noexcept -> int { return Selected_; }
		auto Message() const noexcept -> std::string const& { return Message_; }

		// 入力 1 フレームぶん（RawInput を読まずに試すため）。keys_ は仮想キー 256 個、mouse_ は左・右
		void Step(std::array<bool, 256> const& keys_, std::array<bool, 2> const& mouse_, bool blocked_);

	private:
		void MoveSelection(int dir_);
		void BuildSprites();
		auto RowY(std::size_t i_) const noexcept -> float;

		std::unique_ptr<Lumina::SpriteRenderer> Renderer_{ nullptr };
		Lumina::DX12::Shader VS_{};
		Lumina::DX12::Shader PS_{};
		Lumina::DX12::GraphicsPSO PSO_{};
		Lumina::DX12::DescriptorTable GlobalTable_Textures_{};	// [0] = Blank、[1] = 文字アトラス
		Lumina::DX12::UploadBuffer UB_Ortho_{};
		Lumina::DX12::DescriptorHeap LocalHeap_{};

		Game::UIFont::Font Font_{};
		std::vector<Lumina::Sprite> Sprites_{};

		bool IsOpen_{ false };
		bool IsPrepared_{ false };
		bool SyncNext_{ true };			// 開いた最初のフレームは、押しっぱなしを拾わない
		bool Waiting_{ false };			// キーを押されるのを待っている
		int Selected_{ 0 };
		float Timer_{ 0.0f };
		float WaitTimer_{ 0.0f };
		std::string Message_{};
		float MessageTimer_{ 0.0f };

		std::array<bool, 256> PrevKeys_{};
		std::array<bool, 2> PrevMouse_{};
	};
}
