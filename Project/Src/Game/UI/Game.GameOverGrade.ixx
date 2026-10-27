//////	//////	//////	//////	//////	//////
//	GameOverGrade							//
//////	//////	//////	//////	//////	//////
// ゲームオーバー（プレイヤーの HP がゼロ）の色抜け演出：グラデーションマップ（Clip Studio 風）＋ビネット。
//
// 描画の流れ（InGame::Render）：
//   通常   … UIPass はこれまでどおりバックバッファへ直接描く（このクラスは何もしない）。
//   演出中 … IsActive() が true の間だけ、UIPass を BeginSource() が返す中間 RT に描き、
//            UIPass の後ろで Apply() が中間 RT をバックバッファへ写す（このとき色抜けとビネットを掛ける）。
//            HP バーやメニューなど UI も色抜けの対象。ImGui はバックバッファに直接描かれるので対象外。
// 時間：Start() からの秒で、グラデーションマップとビネットがそれぞれ Delay 秒後から Duration 秒かけて（smootherstep で）効く。
//       進み方は固定の dt（InGame の Update から Update(dt) を呼ぶ。ポーズ中は進まない）。Reset() で元の絵に戻る。
// 色：ストップの色は sRGB で持つ（ImGui のカラーピッカーの値がそのまま見た目になる）。計算の空間は GameOverGrade.hlsl の冒頭を参照。
// 調整：ImGui「GameOver Grade」（Debug のみ）。グラデーションバー（クリックで追加・ドラッグで移動・右クリックで削除）、
//       選択中のストップのカラーピッカー、プリセット、プレビュー（常に表示 / 時間をスクラブ / 左右比較 / 輝度だけ表示）。
//       Assets/Configs/GameOverGrade.json に保存し、Initialize で読み込む（Release でも読む。無いキーは初期値のまま）

export module Game.GameOverGrade;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <string>;
import <d3d12.h>;

import Lumina;

export namespace Game::GameOverGrade {
	inline constexpr uint32_t MaxStops{ 8U };		// GameOverGrade.hlsl の MaxStops と同じ
	inline constexpr uint32_t MinStops{ 2U };

	struct Stop {
		float Position{ 0.0f };						// 0..1
		Lumina::Float3 Color{ 0.0f, 0.0f, 0.0f };	// sRGB（0..1）
	};

	struct Style {
		// 時間（Start からの秒）
		float GradientDelay{ 0.0f };
		float GradientDuration{ 1.6f };
		float VignetteDelay{ 0.4f };
		float VignetteDuration{ 2.0f };
		float UiFadeDelay{ 0.0f };					// HP バーなどの UI を消す（線形のフェードアウト）
		float UiFadeDuration{ 0.5f };

		// グラデーションマップ
		float GradientAmount{ 1.0f };				// 完了時のブレンド量（0 = 元の絵のまま）
		float BlackPoint{ 0.0f };					// この輝度（sRGB）以下は t = 0
		float WhitePoint{ 1.0f };					// この輝度以上は t = 1
		float Gamma{ 1.0f };						// t = pow(t, 1 / Gamma)。> 1 で明るい側へ寄る
		uint32_t NumStops{ 3U };
		std::array<Stop, MaxStops> Stops{			// 位置の昇順
			Stop{ 0.0f, { 0.03f, 0.04f, 0.07f } },
			Stop{ 0.5f, { 0.50f, 0.47f, 0.44f } },
			Stop{ 1.0f, { 0.96f, 0.92f, 0.82f } },
		};

		// ビネット
		float VignetteStrength{ 0.85f };			// 完了時の強さ（0..1）
		float VignetteRadius{ 0.3f };				// ここまでは変化なし（画面の角が 1）
		float VignetteSoftness{ 0.7f };				// Radius からこの幅で色が寄る
		float VignetteRoundness{ 0.6f };			// 0 = 画面に沿った楕円、1 = 真円
		Lumina::Float3 VignetteColor{ 0.0f, 0.0f, 0.0f };	// sRGB
	};

	// エディタのプレビュー（Debug のみ。ゲームの進行とは無関係に演出を出す）
	enum class Preview : int {
		Off,		// ゲームの進行に任せる
		Hold,		// 完了後の見た目で固定
		Scrub,		// 時間をスライダーで動かす
	};

	class Effect {
	public:
		// Canvas（中間 RT）・シェーダ・PSO を作り、設定ファイルを読む
		void Initialize(Lumina::DX12::Context const& dxContext_);

		void Start();					// 演出を頭から始める（すでに始まっていても頭から）
		void Reset();					// 演出なし（元の絵）に戻す
		void Update(float deltaTime_);	// 秒。Start していなければ何もしない

		// true の間、InGame は UIPass を中間 RT に描き、UIPass の後に Apply を呼ぶ
		auto IsActive() const -> bool { return Started_ || PreviewMode_ != Preview::Off; }

		// UIPass の前（レンダーパスの外）で呼ぶ。定数を GPU に写し、中間 RT を描画可能にして、その RTV を返す
		auto BeginSource(Lumina::DX12::CommandList const& cmdList_) -> D3D12_CPU_DESCRIPTOR_HANDLE;
		// UIPass の後（レンダーパスの外）で呼ぶ。中間 RT をバックバッファへ写す。終わった時点で RT = バックバッファ（深度なし）
		void Apply(Lumina::DX12::Context const& dxContext_, Lumina::DX12::CommandList const& cmdList_);

		void DrawEditor();				// ImGui「GameOver Grade」（Debug のみ）

		auto Load() -> bool;			// Look を設定ファイルから読む（あるキーだけ上書き）
		auto Save() -> bool;

		// 現在の進行（0..1）。エディタの表示用
		auto GradientProgress() const -> float;
		// UI（HP バーなど）の不透明度の倍率。演出中でなければ 1、フェードが済めば 0
		auto UiAlpha() const -> float;
		auto VignetteProgress() const -> float;
		auto TotalTime() const -> float;

	public:
		Style Look{};
		static constexpr char const* ConfigPath{ "Assets/Configs/GameOverGrade.json" };

	private:
		auto Elapsed() const -> float;	// 演出の経過秒（プレビューを含む）

	private:
		Lumina::DX12::Canvas Canvas_{};
		Lumina::DX12::DescriptorTable GlobalTable_SRV_{};
		Lumina::DX12::RootSignature RS_{};
		Lumina::DX12::Shader VS_{};
		Lumina::DX12::Shader PS_{};
		Lumina::DX12::GraphicsPSO PSO_{};
		Lumina::DX12::UploadBuffer UB_{};

		bool Started_{ false };
		float Time_{ 0.0f };

		// エディタの状態（保存しない）
		Preview PreviewMode_{ Preview::Off };
		float ScrubTime_{ 0.0f };
		bool SplitView_{ false };
		float SplitPosition_{ 0.5f };
		bool ViewLuma_{ false };
		int Selected_{ 0 };
		int DraggingStop_{ -1 };		// グラデーションバーでドラッグ中のストップ（無ければ -1）
		std::string Status_{};
	};
}
