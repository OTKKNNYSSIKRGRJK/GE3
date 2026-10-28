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
// チャンネルスプリット：R を外へ・B を内へずらす。心臓の鼓動のように脈打ちながら、Duration 秒かけて弱まる（演出の頭に効く）。
// ぼかし：背景をぼかす（終盤。結果表示のラベルを読みやすくする）。ラベルは色抜けの後ろに描くので、ぼけない。
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

		// チャンネルスプリット（脈動）。Delay 秒までは最大の強さのまま、そこから Duration 秒かけて 0 へ（なめらかに）
		float SplitDelay{ 0.0f };
		float SplitDuration{ 2.6f };
		float SplitPixels{ 16.0f };					// 拍の頂点でのずれ（px）。キャラの近くでこの値（遠いほど弱まる）
		float SplitRadial{ 1.0f };					// 0 = 一定の向き、1 = キャラから離れる向き（放射状）
		float SplitAngle{ 0.0f };					// 度。放射状でないときのずれの向き（0 = 横）
		float PulsePeriod{ 0.8f };					// 1 拍の秒
		float PulseDecay{ 0.14f };					// 拍の減衰（1 拍に対する割合。小さいほど鋭い）
		float PulseSecond{ 0.6f };					// 2 つ目の拍（ドクン、ドクン）の強さ。0 で 1 拍だけ
		float PulseBase{ 0.08f };					// 拍の合間にも残る量（0..1）
		int PulseCount{ 4 };						// 鼓動（ドクン、ドクン）の回数。打ち終えたら止まる。0 = 包絡が消えるまで続ける
		float SplitFocusRadius{ 450.0f };			// キャラ（画面上の位置）からこの距離（px）で、ずれは SplitFarAmount まで弱まる
		float SplitFarAmount{ 0.12f };				// 遠くに残るずれの割合（0..1）
		float SplitCenterSoft{ 40.0f };			// キャラの真上ではずれの向きが定まらないので、この距離（px）以内ではずれを小さくする

		// ぼかし（終盤）。Delay 秒後から Duration 秒かけて Radius まで（なめらかに）
		float BlurDelay{ 1.6f };
		float BlurDuration{ 1.8f };
		float BlurRadius{ 10.0f };					// 完了時の半径（px）

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

		// 外の時計（Game.GameOverDirector）から時刻を指定して止める／解除する。解除すると自分の時計に戻る
		void ScrubTo(float seconds_) { PreviewMode_ = Preview::Scrub; ScrubTime_ = seconds_; }
		void ClearScrub() { PreviewMode_ = Preview::Off; }
		void SetTime(float seconds_) { if (Started_) { Time_ = seconds_; } }	// 巻き戻し用：自分の時計を直接決める（Start していなければ何もしない）

		auto Load() -> bool;			// Look を設定ファイルから読む（あるキーだけ上書き）
		auto Save() -> bool;

		// 現在の進行（0..1）。エディタの表示用
		auto GradientProgress() const -> float;
		// UI（HP バーなど）の不透明度の倍率。演出中でなければ 1、フェードが済めば 0
		auto UiAlpha() const -> float;
		// チャンネルスプリットの包絡（1 = 最大、0 = 消えた。脈動は含まない）と、今のずれ（px。脈動を含む）
		auto SplitEnvelope() const -> float;
		auto SplitPixelsNow() const -> float;
		// チャンネルスプリットの強さを 0..1 に正規化したもの（= 包絡 × 鼓動。SplitPixels は掛けない）。カメラシェークの強さに使う。演出中でなければ 0
		auto SplitIntensity() const -> float;
		// ぼかしの進行（0..1）
		auto BlurProgress() const -> float;
		// true の間、チャンネルスプリットとぼかしを掛けない（ポーズメニューが開いているときなど。時計は止めない）
		void SuppressFilters(bool suppressed_) { FiltersSuppressed_ = suppressed_; }
		// チャンネルスプリットの中心（キャラの画面上の位置。UV の 0..1）。毎フレーム、Apply の前に呼ぶ
		void SetFocus(float u_, float v_) { FocusU_ = u_; FocusV_ = v_; }
		// 回転ぼかし（巻き戻しのあいだ）：中心のまわりの振れ幅（ラジアン。0 = なし）と、中心（0 = 画面の中心、1 = SetFocus の位置）。毎フレーム、Apply の前に呼ぶ。
		// 色抜けの時計とは関係なく、呼び出し側（Director）が強さを決める。ポーズメニューが開いている間は掛からない（SuppressFilters）
		void SetSpin(float radians_, float focus_) { SpinRadians_ = radians_; SpinFocus_ = focus_; }
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
		bool FiltersSuppressed_{ false };
		float FocusU_{ 0.5f };
		float FocusV_{ 0.5f };
		float SpinRadians_{ 0.0f };
		float SpinFocus_{ 0.0f };
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
