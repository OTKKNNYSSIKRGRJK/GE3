//////	//////	//////	//////	//////	//////
//	VsBanner (prototype)					//
//////	//////	//////	//////	//////	//////
// バトル開始の合図。斜めのバナー 2 本が左右から滑り込んで止まり、少し置いてから抜けていく（格ゲーの VS 演出）。
// 上 = ボス（左から入る）、下 = プレイヤー（右から入る）。
//
// 中身: キャラの寄り（Lumina.ShowcaseCharacter を 2 体、それぞれ専用の Canvas（SRGB ＋ D24S8）にフォワードで描く）。
//       ライティングは本編と合わせず、撮影用の ShowcaseLighting をキャラごとに持つ。
//       バナーには「帯の中心からの画面座標（回転なし）」で貼る。帯と一緒に滑ってくるが、キャラは直立したまま。
//       RT は画面より一回り大きく（上下左右に PortraitMargin px）、帯が止まった位置で画面全体を覆うように貼る
//       （にらみ合いの間は RT の端が画面に入らない）。キャラの画面上の位置はカメラのレンズシフトで決める。
//       滑り込み・抜けの最中に RT の端が帯にかかっても目立たないよう、RT の縁 ContentFeather px は α を落とす。
//       キャラの位置（ContentOffset）は A の値だけを持ち、B は (-x, -y) を使う（点対称）。絵そのものは回さない
//
// 対称性: 配置と動きはバナー A（ボス側）だけで決め、B（プレイヤー側）は毎回「画面中心まわりに 180° 回したもの」として作る
//         （Evaluate の最後の 2 行）。サイズ・位置・動きの途中が、作りの上で必ず一致する
// 座標  : 画面ピクセル空間（原点 = 画面中心、y 上）。傾きはこの空間で決める（NDC で回すと 16:9 で角度が歪む）
// 形    : PS の BannerDistance（px 単位の符号付き距離、内側が負）。今は角の丸い長方形。
//         形を変えるときは HLSL のこの関数だけ差し替える（SDF テクスチャを読む、など）。縁の演出はこの距離から作る
//
// 描画: 1 本 = SV_VertexID から作るクアッド 1 枚（頂点バッファなし）、ルート CBV（256 バイト境界のスロット × 2）＋中身の SRV テーブル。
//       プリマルチプライド α、深度なし。InGame の UIPass（バックバッファ）に描く。
//       中身は RenderPortraits で先に描いておく（RT とビューポートを切り替えるので、パスの外で呼ぶ）
// 時計: 呼び出し側が渡す dt で進める（InGame の STARTUP は固定 1/60）
// 調整: ImGui「VS Banner」（Debug のみ）。Assets/Configs/VsBanner.json に保存し、Initialize で読み込む（Release でも読む）

export module Lumina.VsBanner;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <string>;
import <vector>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;

import Lumina.ShowcaseCharacter;

export namespace Lumina::VsBanner {
	//----	調整値							----//

	// 止まったときの配置（バナー A = ボス側。B は 180° 回転で決まるので値を持たない）。単位は px（1280x720 基準）
	struct Layout {
		float AngleDeg{ 10.0f };		// 傾き（反時計回りが正。正なら右上がり）
		float Thickness{ 240.0f };		// 太さ
		float Length{ 1700.0f };		// 長さ（画面の対角線 ≈ 1469 より長ければ、止まったとき画面の端から端まで覆う）
		float Offset{ 180.0f };			// 画面中心から A の中心線までの距離（法線方向）。2 本の隙間 = 2 × Offset − Thickness
		float Shift{ 0.0f };			// 止まった位置の、帯の向きのずらし（正 = A が右へ・B が左へ）
		float CornerRadius{ 0.0f };		// 角の丸み

		// 中身のキャラ（カメラの注視点）の画面上の位置: A の帯が止まったときの中心からのずらし（px、y 上）。B は (-x, -y)
		Float2 ContentOffset{ -330.0f, 0.0f };
	};

	// 時間（秒）
	struct Timing {
		float Delay{ 0.15f };			// 始まってから入り始めるまで
		float InTime{ 0.40f };			// 画面外 → 定位置
		float HoldTime{ 1.20f };		// にらみ合い
		float OutTime{ 0.35f };			// 定位置 → 画面外
		float Overshoot{ 1.0f };		// 入りの行き過ぎ（ease-out-back の係数。0 なら ease-out cubic）
		float Drift{ 24.0f };			// にらみ合いの間に進み続ける速さ（px/秒）。止まりきらない方が「生きている」感じになる
		bool PassThrough{ true };		// 抜け方: true = そのまま進んで反対側へ抜ける、false = 来た方へ戻る
	};

	// 見た目（色は sRGB で指定。描画時にリニアへ変換する）
	struct Look {
		Float3 ColorBoss{ 0.55f, 0.08f, 0.20f };
		Float3 ColorPlayer{ 0.10f, 0.28f, 0.62f };
		Float3 EdgeColor{ 1.0f, 0.95f, 0.85f };
		float EdgeWidth{ 4.0f };		// 縁取りの太さ（内側へ、px）。0 で無し
		float Opacity{ 0.92f };
		float ContentOpacity{ 1.0f };	// 中身の濃さ（0 = 帯の色だけ）
		float ContentFeather{ 48.0f };	// 中身の RT の縁で α を落とす幅（px）。0 で落とさない
		int DebugView{ 0 };				// 0 = 通常、1 = 中身の UV（範囲外は暗く）、2 = 距離（等高線）
	};

	// 中身のキャラ 1 体ぶん（撮影用のスタジオ。キャラは原点に立たせ、カメラをその周りに置く）
	struct Portrait {
		bool Enabled{ true };
		bool ShowWeapon{ true };
		std::string Animation{ "Idle_Drawn_H" };	// ShowcaseCharacter が読み込んだクリップの名前（ループ再生）
		float AnimationSpeed{ 1.0f };
		float Yaw{ 0.0f };				// キャラの向き（rad）
		float CameraYaw{ 0.0f };		// カメラの回り込み（rad。0 で -Z 側から +Z を向く。Title と同じ）
		float CameraPitch{ 0.05f };		// rad
		float CameraDistance{ 9.0f };
		float TargetY{ 4.2f };			// 注視点の高さ（Neki は等身大で約 5.7）
		float FovY{ 0.6f };				// 画面の高さ（720 px）に対する縦の画角（rad）。RT の大きさを変えても写る大きさは変わらない
		Float3 BodyColor{ 0.75f, 0.75f, 0.78f };	// sRGB
		ShowcaseLighting Lighting{};
	};

	struct Settings {
		Layout Placement{};
		Timing Time{};
		Look Appearance{};
		Portrait Boss{
			.Animation{ "Idle_Drawn_H" },
			.Yaw{ 0.6f },
			.BodyColor{ 0.80f, 0.62f, 0.68f },
			.Lighting{
				.FireColor{ 1.00f, 0.35f, 0.40f },
				.FireIntensity{ 1.0f },
				.RimColor{ 1.00f, 0.45f, 0.55f },
				.RimIntensity{ 1.5f },
			},
		};
		Portrait Player{
			.Animation{ "Idle_Drawn_Open_H" },
			.Yaw{ -0.6f },
			.BodyColor{ 0.50f, 0.50f, 0.72f },
			.Lighting{
				.FireColor{ 0.45f, 0.65f, 1.00f },
				.FireIntensity{ 1.0f },
				.RimColor{ 0.45f, 0.75f, 1.00f },
				.RimIntensity{ 1.5f },
			},
		};
	};

	//----	配置の計算（描画とは独立）			----//

	// 1 本ぶん。画面ピクセル空間（原点 = 画面中心、y 上）
	struct Banner {
		Float2 Center{ 0.0f, 0.0f };
		Float2 Axis{ 1.0f, 0.0f };		// 帯の向き（単位ベクトル）。法線は (-Axis.y, Axis.x)
		Float2 HalfSize{ 0.0f, 0.0f };	// (長さ, 太さ) / 2
		Float2 HoldCenter{ 0.0f, 0.0f };	// にらみ合いが始まる時点（Drift で進む前）の中心。中身はこれを基準に貼る
	};

	struct Pose {
		Banner Boss{};					// A
		Banner Player{};				// B = A を画面中心まわりに 180° 回したもの
		bool Visible{ false };
	};

	auto TotalTime(Timing const& timing_) -> float;

	// 時刻 time_（秒、0 = 始まり）の配置。screenSize_ は px
	auto Evaluate(Settings const& settings_, float time_, Float2 screenSize_) -> Pose;

	//----	描画							----//

	// バナーに貼る中身（1 本ぶん）。SRV は RS のテーブル（t0）にそのまま渡す
	struct Content {
		D3D12_GPU_DESCRIPTOR_HANDLE SRV{};
		Float2 Size{ 0.0f, 0.0f };		// 画面上の大きさ（px）。中心は「帯の中心 − HoldCenter」（止まったとき画面の中心に来る）
		bool Enabled{ false };
	};

	class Renderer {
	public:
		// rtvFormat_ / dsvFormat_ は描き込む先に合わせる（深度テストはしないので、DSV はフォーマットだけ合わせればよい）
		void Initialize(
			DX12::Context const& dxContext_,
			DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT dsvFormat_ = DX12::GraphicsPSO::DefaultDSVFormat
		);

		// RT / Viewport / DescriptorHeap は呼び出し側で設定済みであること。Pose が見えないときは何もしない。
		// contents_ は [0] = A（ボス）、[1] = B（プレイヤー）。SRV は Enabled でなくても有効なものを渡すこと（テーブルは毎回設定する）
		void Render(
			DX12::CommandList const& cmdList_,
			Pose const& pose_,
			Layout const& layout_,
			Look const& look_,
			std::array<Content, 2> const& contents_,
			Float2 screenSize_
		);

	private:
		// VsBanner.hlsl の cbuffer Constants と同じ並び。16 バイト境界をまたぐベクトルは置かない。32 の倍数に揃える（C4324 対策の方針）
		struct ShaderConstants {
			Float2 Center;			Float2 Axis;
			Float2 HalfSize;		Float2 InvHalfScreen;
			Float3 Color;			float Opacity;
			Float3 EdgeColor;		float EdgeWidth;
			float HasContent;		float DebugView;	float CornerRadius;	float Margin;
			Float2 ContentOffset;	Float2 ContentSize;
			float ContentOpacity;	float ContentFeather;	float Padding1;		float Padding2;
			float Padding3[4];		// 112 バイト → 128 バイト
		};
		static_assert(sizeof(ShaderConstants) % 32U == 0U);

		static constexpr uint64_t ConstantsStride{ (sizeof(ShaderConstants) + 0xFFLLU) & ~0xFFLLU };
		static constexpr uint32_t NumSlots{ 2U };

	private:
		DX12::RootSignature RS_{};
		DX12::Shader VS_{};
		DX12::Shader PS_{};
		DX12::GraphicsPSO PSO_{};

		DX12::UploadBuffer UB_Constants_{};
	};

	//----	再生（InGame から使う窓口）			----//

	class Presenter {
	public:
		enum class State : uint8_t { Idle, Playing, Finished };

	public:
		// 設定ファイルがあれば読み込む
		void Initialize(
			DX12::Context const& dxContext_,
			DXGI_FORMAT rtvFormat_,
			DXGI_FORMAT dsvFormat_,
			Float2 screenSize_ = { 1280.0f, 720.0f }
		);

		void Play();					// 頭から再生
		void Reset();					// Idle に戻す（描かない）
		void Update(float dt_);			// Playing なら進める。終わりに着いたら Finished。プレビュー中は進めない

		auto CurrentState() const -> State { return State_; }
		auto IsFinished() const -> bool { return State_ == State::Finished; }
		auto Time() const -> float { return Time_; }
		auto Progress() const -> float;	// 0..1（全体の長さに対する割合）

		// 中身（キャラの寄り）を専用の Canvas に描く。バナーが見えないときは何もしない。
		// RT・DSV・ビューポート・シザーを切り替えるので、パスの外で呼び、呼んだ後は呼び出し側で元に戻すこと
		void RenderPortraits(DX12::CommandList const& cmdList_);

		// RT / Viewport / DescriptorHeap は呼び出し側で設定済みであること
		void Render(DX12::CommandList const& cmdList_);

		// ImGui「VS Banner」（Debug のみ）。戻り値 true = 「Replay」が押された（呼び出し側で STARTUP に戻す）
		auto DrawEditor() -> bool;

		auto Load() -> bool;
		auto Save() -> bool;

	public:
		Settings Config{};

		static constexpr char const* ConfigPath{ "Assets/Configs/VsBanner.json" };

		// 中身の RT は「画面 ＋ 上下左右にこの px」の大きさで、等倍で貼る。
		// にらみ合いの間の Drift（既定 24 px/秒 × 1.2 秒）や入りの行き過ぎより大きくしておけば、止まっている間は RT の端が画面に入らない
		static constexpr uint32_t PortraitMargin{ 160U };
		static constexpr float FovReferenceHeight{ 720.0f };	// Portrait::FovY の基準の高さ（px）

	private:
		auto IsVisibleNow() const -> bool;
		void AdvancePortraits(float dt_);
		void DrawPortraitEditor(char const* label_, Portrait& portrait_, uint32_t index_);

	private:
		Renderer Renderer_{};
		Float2 ScreenSize_{ 1280.0f, 720.0f };

		// 中身: [0] = ボス、[1] = プレイヤー。ShowcaseCharacter はスキンのパレットと定数バッファを 1 体で 1 組しか持たないので 2 体要る
		std::array<ShowcaseCharacter, 2> Characters_{};
		uint32_t PortraitWidth_{ 0U };
		uint32_t PortraitHeight_{ 0U };
		std::array<DX12::Canvas, 2> PortraitCanvases_{};
		std::array<bool, 2> IsPortraitDepthWritable_{ false, false };
		DX12::DescriptorTable PortraitSRVs_{};		// グローバルヒープ。[0] = ボス、[1] = プレイヤー

		State State_{ State::Idle };
		float Time_{ 0.0f };

		// エディタ: 時刻を固定して見る
		bool Preview_{ false };
		float PreviewTime_{ 0.0f };
		std::string Status_{};
	};
}
