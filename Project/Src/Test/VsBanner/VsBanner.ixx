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
// 背景のパーティクル: 本編・Title と同じ ParticleSystem<T>（BasicParticle.VS ＋ Title の TitleParticle.PS、Particles.png）。
//       中身の RT に「奥・中の層 → キャラ → 手前の層」の順で描く（加算、深度なし）。
//       カメラはキャラ用とは別の透視カメラ（+Z を向く。ワールドの x = 帯の向き、y = 帯の法線）。帯の傾きは射影の後に回す。
//       ParallaxDepth の距離にある粒は帯と一緒に動き、遠い粒ほど帯の動きに遅れて付いてくる（Parallax）。
//       流れる向きは帯の向き（B は逆向き、点対称）。ParticleSystem は 1 つにつき 1 フレーム 1 回しか描けない
//       （描画データとカメラの CBV を 1 組しか持たない）ので、バナー 2 本 × 層の数だけ持つ
//
// 対称性: 配置と動きはバナー A（ボス側）だけで決め、B（プレイヤー側）は毎回「画面中心まわりに 180° 回したもの」として作る
//         （Evaluate の最後の 2 行）。サイズ・位置・動きの途中が、作りの上で必ず一致する
// 座標  : 画面ピクセル空間（原点 = 画面中心、y 上）。傾きはこの空間で決める（NDC で回すと 16:9 で角度が歪む）
// 形    : PS の BannerDistance（px 単位の符号付き距離、内側が負）。縁の演出はこの距離から作る。
//         Shape.Image に画像（透明 PNG に形を塗ったもの）を指定すると、起動時に Lumina.SDF で符号付き距離場に変換し、
//         R8 のテクスチャにして使う（画像の右端 = 進行方向の先頭、上端 = 帯の法線の向き。B は 180° 回るので点対称のまま）。
//         画像が無い・読めないときは角の丸い長方形（CornerRadius）。LockAspect なら太さ = 長さ × 画像の縦横比。
//         SDF は内外の 2 値なので、半透明・濃淡は別に持つ: テクスチャは R8G8 で R = 距離、G = 元の α（濃さ）。
//         しきい値を低くして（既定 0.02）薄い所も形に含め、UseOpacity なら塗りの α に G を掛ける
// グロー: 形の外側に、距離 d から光をにじませる（加算。α は増やさない）。SDF に詰めた距離の範囲（Shape.Range）より外には伸ばせないので、
//         幅はその範囲までに抑える（クアッドもグローの幅だけ広げる）
// 背景  : ベタ塗りの代わりにシェーダアート（ユーザーが以前書いた Fancy.CS の移植。最初の Test2.PS は合わなかったので差し替えた）。
//         帯の中心を模様の中心にして帯のローカル座標で描くので、帯と一緒に傾いて滑る。
//         渡すのは時間とパレット（色 = Bias + Amplitude · cos(2π · (Frequency · t + Phase))）。パレットはボス / プレイヤーで別
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
import <memory>;
import <optional>;
import <random>;
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
import Lumina.AssetManager;
import Lumina.SDF;
import Lumina.SDF.Adapter;
import ParticleSystem;

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

	// 背景のパーティクル 1 グループ（層）。大きさ・速さは Depth = 1 の面での px。画面上では Depth で割られる
	// 背景のパーティクル 1 グループ（層）。距離・大きさ・速さはパーティクル用カメラのワールド単位
	struct ParticleGroup {
		std::string Name{ "Layer" };
		bool Enabled{ true };
		bool Front{ false };			// true = キャラの手前に描く
		int Count{ 50 };				// バナー 1 本あたり（最大 MaxParticlesPerGroup）
		float DepthMin{ 10.0f };		// カメラからの距離
		float DepthMax{ 20.0f };
		float SizeMin{ 0.3f };			// 粒の大きさ（ParticleSystem の Scale。クアッドの一辺）
		float SizeMax{ 0.6f };
		float SpeedMin{ 8.0f };			// 帯の向きに流れる速さ（/秒）。負なら逆向き
		float SpeedMax{ 12.0f };
		float CrossSpeed{ 0.5f };		// 帯に垂直な向きのばらつき（± /秒）
		float Stretch{ 0.0f };			// 流れる向きに伸ばす長さ = 速さ × これ（秒）。0 で伸ばさない
		float Spin{ 0.0f };				// 伸ばさないときの回転（rad/秒、粒ごとに向きはランダム）
		int Atlas{ 0 };					// Particles.png のセル（0 ぼかし点 / 1 縦の筋 / 2 星 / 3 ぼかし星 / 4 四芒星 / 5 小四芒星 / 6 煙 / 7 円）
		Float3 Color{ 1.0f, 0.95f, 0.85f };	// sRGB
		float Alpha{ 1.0f };
		float LifeMin{ 1.0f };			// 寿命（秒）。始めと終わりはフェード
		float LifeMax{ 2.0f };
	};

	struct BackgroundParticles {
		bool Enabled{ true };
		float FovY{ 1.0471976f };		// パーティクル用カメラの画角（画面の高さ 720 px に対して、rad。既定 60°）
		float Parallax{ 1.0f };			// 1 = 遠い粒ほど帯の動きに遅れる（物理的に正しい量）。0 = 全部帯と一緒に動く
		float ParallaxDepth{ 10.0f };	// この距離にある粒は帯と一緒に動く（これより近い粒は帯より速く動く）
		std::vector<ParticleGroup> Groups{
			ParticleGroup{
				.Name{ "Far" }, .Count{ 160 }, .DepthMin{ 30.0f }, .DepthMax{ 60.0f },
				.SizeMin{ 0.3f }, .SizeMax{ 0.6f }, .SpeedMin{ 8.0f }, .SpeedMax{ 16.0f }, .CrossSpeed{ 0.5f },
				.Atlas{ 0 }, .Color{ 0.85f, 0.85f, 1.0f }, .Alpha{ 0.6f },
				.LifeMin{ 1.0f }, .LifeMax{ 2.5f },
			},
			ParticleGroup{
				.Name{ "Mid" }, .Count{ 60 }, .DepthMin{ 14.0f }, .DepthMax{ 24.0f },
				.SizeMin{ 0.4f }, .SizeMax{ 0.8f }, .SpeedMin{ 7.0f }, .SpeedMax{ 11.0f }, .CrossSpeed{ 0.5f },
				.Stretch{ 0.05f }, .Atlas{ 0 }, .Color{ 1.0f, 0.95f, 0.85f }, .Alpha{ 0.8f },
				.LifeMin{ 0.8f }, .LifeMax{ 2.0f },
			},
			ParticleGroup{
				.Name{ "Near" }, .Front{ true }, .Count{ 10 }, .DepthMin{ 5.0f }, .DepthMax{ 8.0f },
				.SizeMin{ 1.0f }, .SizeMax{ 1.8f }, .SpeedMin{ 5.0f }, .SpeedMax{ 7.0f }, .CrossSpeed{ 0.3f },
				.Atlas{ 0 }, .Color{ 1.0f, 0.95f, 0.9f }, .Alpha{ 0.25f },
				.LifeMin{ 0.8f }, .LifeMax{ 1.6f },
			},
		};
	};

	// ParticleSystem に入れる粒
	struct BannerParticle : public Particle {
		float MaxLife{ 1.0f };
		float BaseSize{ 1.0f };
		float BaseAlpha{ 1.0f };
		float Spin{ 0.0f };
	};

	// バナーの形（画像 → SDF）
	struct ShapeSettings {
		std::string Image{ "Assets/Img/VsBanner.Shape.png" };	// 空なら角の丸い長方形。α を形に使う
		bool LockAspect{ true };		// 太さ = 長さ × 画像の縦横比（画像が伸び縮みしないように）
		int Resolution{ 2048 };			// SDF テクスチャの幅（高さは画像の縦横比から）
		float Range{ 32.0f };			// SDF に詰める距離の範囲（テクセル）。グローなど縁の演出はこれより太くできない
		float Threshold{ 0.02f };		// α がこれ以上の所を形に含める（輪郭の位置）。低いほど薄い所まで形になる
		bool UseOpacity{ true };		// 塗りの α に元の α（濃さ）を掛ける（半透明・濃淡）
		bool EdgeUsesOpacity{ true };	// 縁取りにも濃さを掛ける（false なら、薄い所でも縁取りはくっきり）
	};

	// コサインパレット: 色(t) = Bias + Amplitude · cos(2π · (Frequency · t + Phase))（各チャンネルごと）
	struct CosinePalette {
		Float3 Bias{ 0.5f, 0.5f, 0.7f };			// 中心の色（元の a）
		Float3 Amplitude{ 0.1f, 0.3f, 0.3f };		// 振れ幅（元の b）
		Float3 Frequency{ 1.0f, 1.0f, 1.0f };		// t に対する周波数（元の c）
		Float3 Phase{ 0.263f, 0.416f, 0.557f };	// 位相（元の d）
	};

	// 背景のシェーダアート
	struct BackgroundArt {
		bool Enabled{ true };				// false ならベタ塗り（Look の色）
		float Speed{ 1.0f };				// 時間の進み（1 = 実時間の秒）
		float Scale{ 64.0f };				// 模様の 1 単位（元のシェーダの「16 texel」）が帯の何 px か。元は 320x180 を画面に引き伸ばしていたとみなして 64
		Float2 Origin{ 0.0f, 0.0f };		// 模様の中心の、帯の中心からのずらし（px、x = 帯の向き、y = 下向き）
		bool OutputIsGamma{ true };			// 元の出力をガンマのままの値とみなして 0..1 に収め、リニアに直す（false ならそのまま出す）
		CosinePalette Boss{
			.Bias{ 0.7f, 0.4f, 0.5f },
			.Amplitude{ 0.3f, 0.15f, 0.2f },
			.Frequency{ 1.0f, 1.0f, 1.0f },
			.Phase{ 0.0f, 0.1f, 0.2f },
		};
		CosinePalette Player{};				// 元のシェーダの値
	};

	// 形の外側のグロー（加算）
	struct GlowSettings {
		bool Enabled{ true };
		Float3 Color{ 1.0f, 0.85f, 0.7f };	// sRGB
		float Intensity{ 0.8f };
		float Width{ 24.0f };				// 縁からの幅（px）。Shape.Range（px 換算）を超えた分は切る
		float Falloff{ 2.0f };				// 減り方（(1 − d / 幅)^Falloff）
	};

	struct Settings {
		Layout Placement{};
		ShapeSettings Shape{};
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
		BackgroundParticles Particles{};
		BackgroundArt Background{};
		GlowSettings Glow{};
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
	// バナーの形の SDF（両方のバナーで共通）
	struct ShapeInput {
		D3D12_GPU_DESCRIPTOR_HANDLE SRV{};	// Enabled でなくても有効なものを渡すこと（テーブルは毎回設定する）
		Float2 Texels{ 1.0f, 1.0f };		// テクスチャの大きさ
		float Range{ 1.0f };
		bool Enabled{ false };
		bool UseOpacity{ true };			// 塗りに G（濃さ）を掛ける
		bool EdgeUsesOpacity{ true };		// 縁取りに G を掛ける
	};

	// 背景とグロー（Render に渡すもの。時間は Presenter が進める）
	struct Decoration {
		BackgroundArt const* Background{ nullptr };
		GlowSettings const* Glow{ nullptr };
		float Time{ 0.0f };
		float RangePx{ 0.0f };				// SDF の距離の範囲（px 換算）。形が無いときは 0（制限なし）
	};

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
			ShapeInput const& shape_,
			Decoration const& decoration_,
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
			float ContentOpacity;	float ContentFeather;	float HasShape;		float ShapeRange;
			Float2 ShapeTexels;		float ShapeUseOpacity;	float EdgeUseOpacity;
			Float3 PaletteBias;		float ArtTime;
			Float3 PaletteAmplitude; float ArtScale;
			Float3 PaletteFrequency; float ArtEnabled;
			Float3 PalettePhase;	float GlowWidth;
			Float3 GlowColor;		float GlowIntensity;
			Float2 ArtOrigin;		float GlowFalloff;	float ArtOutputIsGamma;	// 224 バイト
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

	// 背景のパーティクルの描画（中身の RT に描く。加算・深度なし）。粒の計算は Presenter が CPU で行い、ここは並べて描くだけ
	class Presenter {
	public:
		enum class State : uint8_t { Idle, Playing, Finished };

	public:
		// 設定ファイルがあれば読み込む
		void Initialize(
			DX12::Context const& dxContext_,
			AssetManager& assetMngr_,		// Particles.png の読み込みに使う
			DXGI_FORMAT rtvFormat_,
			DXGI_FORMAT dsvFormat_,
			Float2 screenSize_ = { 1280.0f, 720.0f }
		);

		void Play();					// 頭から再生
		void Reset();					// Idle に戻す（描かない）
		// Playing なら進める。終わりに着いたら Finished。プレビュー中は進めない。
		// パーティクルの描画データを GPU へ写すコマンドを積むので、パスの外（InGame の Update）で呼ぶこと
		void Update(DX12::CommandList const& cmdList_, float dt_);

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
		static constexpr float FovReferenceHeight{ 720.0f };	// Portrait::FovY / BackgroundParticles::FovY の基準の高さ（px）

		static constexpr uint32_t MaxParticleGroups{ 6U };		// 層の数の上限（ParticleSystem を起動時に確保しておく）
		static constexpr uint32_t MaxParticlesPerGroup{ 1024U };

	private:
		auto IsVisibleNow() const -> bool;
		void AdvancePortraits(float dt_);

		// 背景のパーティクル
		struct ParticleCamera {
			float Pan{ 0.0f };			// カメラの x（ワールド）
			float Focal{ 1.0f };		// 焦点距離（画面の px）
		};
		auto MakeParticleCamera(Banner const& banner_, Mat4& viewProj_, Mat4& viewToWorld_) const -> ParticleCamera;
		void UpdateParticles(DX12::CommandList const& cmdList_, float dt_);
		void ResetParticles();		// 次の UpdateParticles で全部撒き直す
		void DrawPortraitEditor(char const* label_, Portrait& portrait_, uint32_t index_);
		void DrawParticleEditor();
		void DrawShapeEditor();
		void DrawDecorationEditor();

		// バナーの形: Config.Shape から SDF を作り直す（失敗したら角の丸い長方形に戻す）
		void LoadShape();
		void ApplyShapeAspect();	// LockAspect なら太さを長さ × 縦横比にそろえる

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

		// バナーの形
		DX12::Context const* Context_{ nullptr };
		std::unique_ptr<DX12::DefaultTexture2D> ShapeTexture_{};	// 読めなかったときは 1×1 の代わり
		DX12::DescriptorTable ShapeSRV_{};							// グローバルヒープに 1 つ（作り直しても同じ場所に書く）
		Float2 ShapeTexels_{ 1.0f, 1.0f };
		float ShapeAspect_{ 0.0f };									// 画像の 高さ / 幅（読めていないときは 0）
		float ShapeBakedRange_{ 1.0f };
		float ArtTime_{ 0.0f };										// 背景のシェーダアートの時間（見えている間だけ進める）								// テクスチャを作ったときの Range（シェーダにはこちらを渡す）
		bool HasShape_{ false };
		std::string ShapeStatus_{};

		// [バナー][層]。層の数は MaxParticleGroups まで（起動時に全部確保する。ParticleSystem は作るたびにグローバルヒープを消費するため）
		std::array<std::array<std::unique_ptr<ParticleSystem<BannerParticle>>, MaxParticleGroups>, 2> ParticleSystems_{};
		std::array<std::array<int, MaxParticleGroups>, 2> ParticleCounts_{};		// 今撒いてある数（設定と違えば撒き直す）
		bool ParticlesNeedReset_{ true };
		std::mt19937 Random_{ 0x5EEDu };								// 粒の撒き方（更新関数にポインタで渡す）
		DX12::RootSignature RS_Particle_{};
		DX12::Shader VS_Particle_{};
		DX12::Shader PS_Particle_{};
		DX12::GraphicsPSO PSO_Particle_{};
		DX12::DescriptorTable GlobalTable_ParticleTextures_{};		// 32 枠（RS の Textures に合わせる）。0 番 = Particles.png
		std::array<DX12::UploadBuffer, 2> UB_ParticleViewToWorld_{};
		std::array<DX12::UploadBuffer, 2> UB_ParticleVP_{};
		DX12::DescriptorHeap LocalHeap_Particle_{};					// [バナー × 2 + 0] = ViewToWorld、[+ 1] = VP

		State State_{ State::Idle };
		float Time_{ 0.0f };

		// エディタ: 時刻を固定して見る
		bool Preview_{ false };
		float PreviewTime_{ 0.0f };
		std::string Status_{};
	};
}
