//////	//////	//////	//////	//////	//////
//	PauseMenu								//
//////	//////	//////	//////	//////	//////
// InGame のポーズメニュー（仮）。ESC で開閉し、Resume / Restart / 設定 / Return to Title を選ぶ。
// 「設定」はキー割り当ての画面（Game.KeyConfigScreen）を、メニューを開いたまま上に重ねて開く（ESC か「戻る」でメニューへ戻る）。
//
// 見た目: 画面全体に黒の半透明スプライトを重ねて薄暗くし、その上にラベル画像を縦に並べる（どれも中央揃え）。
//         ラベルの表示サイズ = 画像の大きさ（PNG のヘッダから読む）× LabelScale。画像を差し替えても位置は崩れない。
//         選択中は不透明・等倍、非選択は半透明・縮小（Title のメニューと同じ考え方。追従はなめらかに）。
//         本編の SpriteRenderer / Sprite2.VS.hlsl / SpriteUI.PS.hlsl を使い、PSO だけ通常のアルファ合成で別に作る。
// 操作: ↑↓ / W S で選択、SPACE / ENTER で決定、ESC で閉じる（= Resume）。どれも押した瞬間だけ拾う。
// 止め方: 呼び出し側（InGame）が IsBlockingGame() の間ゲームの Update を丸ごと飛ばす。Render はそのまま
//         止まった状態を描き続けるので、別のキャプチャは取らない。
//         決定キー（SPACE）はゲームのジャンプ・再開と同じキーなので、決定で閉じたあとはキーを離すまで止めたままにする。
// 操作ヒント: 閉じている間は画面右下に「ESC : ポーズ」（Assets/UI/Label.PauseHint.png、Kaisei Decol Bold）を出す。
//         出してよいかは呼び出し側が Prepare に渡す（InGame は STARTUP 以外）。開いている間は出さない
// 時間: 固定の 1/60 秒で進める（ゲームの時間とは無関係）。
// 調整: ImGui「Pause Menu」（Debug のみ）。Assets/Configs/PauseMenu.json に保存し、Initialize で読み込む（Release でも読む。無いキーは初期値のまま）

export module Game.PauseMenu;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
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

import Game.KeyConfigScreen;

export namespace Game::PauseMenu {
	enum class Action : uint8_t {
		None,
		Resume,
		Restart,
		Settings,		// 設定の画面を開く（メニューの中で処理する。Update からは返さない）
		ReturnToTitle,
	};

	// 並び順 = 項目の順（上から）
	inline constexpr uint32_t NumItems{ 4U };

	struct Style {
		// 薄暗くする全画面の黒
		float DimAlpha{ 0.6f };
		float DimFadeTime{ 0.12f };		// 開いたときに DimAlpha まで上がる時間（秒）。ラベルも同じだけかけて出る

		// 並べ方（1280x720、左上原点・Y 下向き）。項目の中心を (CenterX, CenterY + (i − 1) × Spacing) に置く
		float CenterX{ 640.0f };
		float CenterY{ 360.0f };
		float Spacing{ 110.0f };
		float LabelScale{ 0.5f };		// 画像の px に掛ける（Title の Start / Exit は 720x180 を 360x90 で出している）

		// 選択の見た目
		float UnselectedAlpha{ 0.45f };
		float UnselectedScale{ 0.9f };
		float Smoothing{ 18.0f };		// 追従の速さ（/秒）

		// 操作ヒント（右下の「ESC : ポーズ」）。右下の角に合わせて置く
		bool ShowHint{ true };
		float HintMarginX{ 24.0f };		// 画面の右端から（px）
		float HintMarginY{ 16.0f };		// 画面の下端から（px）
		float HintScale{ 0.3f };		// 画像の px に掛ける（597x128 → 約 179x38）
		float HintAlpha{ 0.8f };

		// 「Q : 自害」（Assets/UI/Label.QuitHint.png）。「ESC : ポーズ」の真上に右端を揃えて積む。大きさ・濃さはヒントと同じ
		bool ShowQuitHint{ true };
		float QuitHintGap{ 0.0f };		// 「ESC : ポーズ」との間（px）。画像の余白があるので 0 でも少し空く
	};

	class Menu {
	public:
		void Initialize(
			Lumina::DX12::Context const& dxContext_,
			Lumina::AssetManager& assetMngr_
		);

		// 毎フレーム呼ぶ（閉じているときも。ESC を拾うため）。決定した項目のフレームだけ None 以外を返す
		auto Update(Lumina::WinApp::RawInput const& input_) -> Action;

		void Close();								// すぐ閉じる（キーの待ちも解く）
		auto IsOpen() const -> bool { return IsOpen_; }
		// 設定の画面を開いているか
		auto IsSettingsOpen() const -> bool { return Settings_.IsOpen(); }
		// ゲームの Update を止めるべきか（開いている間 ＋ 決定で閉じたあと決定キーを離すまで）
		auto IsBlockingGame() const -> bool { return IsOpen_ || WaitRelease_; }

		// 描画は 2 段に分ける（SpriteRenderer の BatchEnd がコピー（CopyBufferRegion）を積むので、レンダーパスの中では呼べない）。
		// Prepare: レンダーパスの外で呼ぶ。開いているときはメニュー、閉じているときは（showHint_ なら）操作ヒントを GPU へ写す
		// Render : レンダーパスの中で呼ぶ。Prepare したフレームだけ描く。RT / Viewport / DescriptorHeap は呼び出し側で設定済みであること
		// showQuitHint_ : 「Q : 自害」を出してよいか（Q が効く間だけ。InGame は IN_BATTLE）
		void Prepare(Lumina::DX12::CommandList const& cmdList_, bool showHint_, bool showQuitHint_ = false);
		void Render();

		void DrawEditor();							// ImGui「Pause Menu」（Debug のみ）

		auto Load() -> bool;						// Look を設定ファイルから読む（あるキーだけ上書き）
		auto Save() -> bool;

	public:
		Style Look{};

		// ラベル画像（[0] = Resume、[1] = Restart、[2] = 設定、[3] = Return to Title）
		static constexpr char const* ItemImages[NumItems]{
			"Assets/UI/Label.Resume.png",
			"Assets/UI/Label.Restart.png",
			"Assets/UI/Label.Settings.Pause.png",
			"Assets/UI/Label.ReturnToTitle.png",
		};
		static constexpr char const* ItemNames[NumItems]{ "Resume", "Restart", "Settings", "Return to Title" };
		static constexpr char const* HintImage{ "Assets/UI/Label.PauseHint.png" };
		static constexpr char const* QuitHintImage{ "Assets/UI/Label.QuitHint.png" };
		static constexpr char const* ConfigPath{ "Assets/Configs/PauseMenu.json" };

	private:
		void Open();
		void UpdateSprites();
		void UpdateHint();

	private:
		std::unique_ptr<Lumina::SpriteRenderer> Renderer_{ nullptr };
		Lumina::DX12::Shader VS_{};
		Lumina::DX12::Shader PS_{};
		Lumina::DX12::GraphicsPSO PSO_{};
		Lumina::DX12::DescriptorTable GlobalTable_Textures_{};	// [0] = Blank、[1 + i] = 項目 i、[1 + NumItems] = 操作ヒント、[2 + NumItems] = 「Q : 自害」
		Lumina::DX12::UploadBuffer UB_Ortho_{};
		Lumina::DX12::DescriptorHeap LocalHeap_{};

		Lumina::Sprite Dim_{};
		std::array<Lumina::Sprite, NumItems> Items_{};
		std::array<Lumina::Float2, NumItems> ImageSizes_{};		// 画像の px（読めなければ 720x180）
		Lumina::Sprite Hint_{};
		Lumina::Sprite QuitHint_{};
		Lumina::Float2 QuitHintSize_{};							// 「Q : 自害」の画像の px
		Lumina::Float2 HintSize_{};								// 操作ヒントの画像の px

		bool IsOpen_{ false };
		bool IsPrepared_{ false };								// このフレームで Prepare した（Render で描いて戻す）
		bool WaitRelease_{ false };
		int Selected_{ 0 };
		float OpenTimer_{ 0.0f };
		std::array<float, NumItems> Blend_{};					// 0 = 非選択の見た目、1 = 選択の見た目

		// 押した瞬間を拾うための前フレームの状態（起動直後に押しっぱなしのキーで反応しないよう true から始める）
		bool PrevEsc_{ true };
		bool PrevUp_{ true };
		bool PrevDown_{ true };
		bool PrevConfirm_{ true };

		std::string Status_{};									// エディタに出す保存・読み込みの結果

		Game::KeyConfigScreen::Screen Settings_{};				// 「設定」で開く画面（メニューの上に重ねる）
	};
}
