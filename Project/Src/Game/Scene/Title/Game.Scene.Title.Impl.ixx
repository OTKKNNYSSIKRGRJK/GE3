module;

#include<memory>
#include<vector>
#include<string>
#include<unordered_map>

export module Game.Scene.Title : Impl;

import Lumina;
import Lumina.FirePillar;
import Lumina.MagicCircle;
import Lumina.ShowcaseCharacter;

namespace Game::Scene::Impl {
	// 火柱 1 本分の編集用データ。円柱ごとに Description を丸ごと持つ
	struct FirePillarEntry {
		char Name[32]{ "Pillar" };
		bool Enabled{ true };
		Lumina::FlamePillar::Description Desc{};
		Lumina::Float3 Offset{ 0.0f, 0.0f, 0.0f };	// キャラの足元からのオフセット
		int Seed{ 0 };
		float IntensityScale{ 1.0f };
	};

	//----	------	------	------	------	----//
	// パーティクル（本編の ParticleSystem<T> をそのまま使う）
	//----	------	------	------	------	----//

	// 本編の Particle に、タイトル用の追加情報を足したもの
	struct TitleParticle : public Particle {
		float MaxLife{ 1.0f };
		float BaseSize{ 1.0f };
		float BaseAlpha{ 1.0f };
		float Seed{ 0.0f };
		Lumina::Float3 ColorA{ 1.0f, 1.0f, 1.0f };	// 発生直後の色（リニア）
		Lumina::Float3 ColorB{ 1.0f, 1.0f, 1.0f };	// 消える直前の色（リニア）
	};

	// 本編の AmbientSparkles と同じ動き（Life・移動量はフレーム単位の係数を 60fps 換算で流用）
	// 色も本編と同じ式（HSV 由来の値をそのまま出力。リニア化しない）
	struct SparkleEmitter {
		bool Enabled{ true };
		float Rate{ 60.0f };			// 個/秒（本編は毎フレーム 1 回 ≒ 60）
		float AreaRadius{ 14.0f };		// 発生範囲の半径（本編は 100 の円 ∩ ±40 の正方形）
		float HeightMax{ 2.0f };
		float Size{ 1.5f };				// 本編 1.5
		float Alpha{ 0.1f };			// 本編 0.1
	};

	// 本編の PlayerEffects（プレイヤーのまわりを巡る光）と同じ動き
	struct AuraEmitter {
		bool Enabled{ true };
		float Rate{ 120.0f };			// 個/秒（本編は毎フレーム 2 個）
		float OrbitRadius{ 1.5f };		// 本編 1.5
		float OrbitHeight{ 1.0f };		// 上下の振れ幅（本編 1.0）
		float HeightOffset{ 0.0f };
		float Size{ 0.7f };				// 本編 0.7
		float Alpha{ 0.15f };			// 本編 0.15
		bool HueCycle{ true };			// 本編の「色相が回る」色。false なら Color（sRGB）
		Lumina::Float3 Color{ 1.0f, 0.6f, 0.3f };
	};

	// 火の粉：火柱の根元から旋回しながら昇り、速度方向に引き伸ばす（秒単位）
	struct EmberEmitter {
		bool Enabled{ true };
		float Rate{ 40.0f };			// 個/秒
		float LifeMin{ 1.0f };
		float LifeMax{ 2.2f };
		float SpawnRadius{ 2.5f };		// 足元からの距離
		float SpawnRadiusJitter{ 1.0f };
		float SpawnHeight{ 0.5f };		// 0 〜 この高さで発生
		float RiseSpeed{ 2.5f };		// 上昇速度（/秒）
		float RiseJitter{ 1.0f };
		float Swirl{ 0.8f };			// 軸まわりの旋回（rad/秒、負で逆回り）
		float Turbulence{ 3.0f };		// 横方向のランダムな加速
		float Drag{ 1.0f };				// 横速度の減衰（/秒）
		float Size{ 0.12f };
		float Stretch{ 4.0f };			// 画面上の進行方向への伸び（1 = 伸ばさない）
		float Alpha{ 1.0f };
		Lumina::Float3 ColorHot{ 1.00f, 0.85f, 0.50f };	// 発生直後（sRGB）
		Lumina::Float3 ColorCool{ 1.00f, 0.25f, 0.05f };	// 消える直前（sRGB）
		int Atlas{ 1 };					// Particles.png のセル番号（0..7）。1 = 縦長の筋
	};

	// 魔法陣が放つ光：有効なレイヤーのリング上から立ち昇る光の筋（秒単位）
	struct CircleLightEmitter {
		bool Enabled{ true };
		float Rate{ 30.0f };			// 個/秒（全レイヤー合計。円周の長さで振り分ける）
		float LifeMin{ 1.5f };
		float LifeMax{ 3.0f };
		float RiseSpeed{ 1.2f };
		float RiseJitter{ 0.6f };
		float Sway{ 0.3f };				// 横揺れの速さの振れ幅
		float Size{ 0.15f };
		float Stretch{ 6.0f };			// ワールドの上方向への伸び
		float Alpha{ 0.6f };
		float Whiten{ 0.3f };			// レイヤーの色をどれだけ白に寄せるか
		bool RespectReveal{ true };		// 描き込み途中のレイヤーは、描かれた所からだけ出す
		int Atlas{ 1 };
	};

	// 武器から放つ光：刀身の軸に沿って、動かない大きめの光の粒を置いていく（秒単位）
	// 置いた場所に留まるので、武器が動くと軌跡のように残る
	struct WeaponGlowEmitter {
		bool Enabled{ true };
		float Rate{ 60.0f };			// 個/秒
		float LifeMin{ 0.4f };
		float LifeMax{ 0.9f };
		float SpanBegin{ 0.15f };		// 軸の根元 0 → 先端 1 のどこからどこまでに出すか
		float SpanEnd{ 1.0f };
		float RadialJitter{ 0.6f };		// 軸からの散らばり（武器の太さに対する倍率）
		float SizeMin{ 0.6f };
		float SizeMax{ 1.4f };
		float Grow{ 0.3f };				// 寿命の間にどれだけ大きくなるか（0 = 変わらない）
		float Alpha{ 0.35f };
		bool HueFlow{ true };			// true: 色相を刀身方向と時間で流す（仮のタマムシ色）/ false: Color
		Lumina::Float3 Color{ 0.60f, 0.85f, 1.00f };	// sRGB
		float HueBase{ 160.0f };		// 度
		float HueRange{ 140.0f };		// 根元 → 先端での色相の変化（度）
		float HueSpeed{ 40.0f };		// 度/秒
		float Saturation{ 0.7f };
		float Value{ 1.0f };
		int Atlas{ 0 };					// 0 = ぼかし点
	};

	//----	------	------	------	------	----//
	// カメラ・TimeScale の往復（sway）
	//----	------	------	------	------	----//

	// 往復の端点（Hero / Overview）
	struct CameraPose {
		float Pitch{ 0.0f };
		float Distance{ 16.0f };
		float TargetY{ 3.0f };
		float FovY{ 0.8f };
	};

	// 1 チャンネルぶんの往復。重み 0 → 1 → 0 を Period 秒で 1 往復する
	struct SwayChannel {
		bool Enabled{ true };
		float Period{ 12.0f };			// 1 往復の秒数
		float Phase{ 0.0f };			// 周期に対する開始位置 0..1（0 = 端点 A から）
		float Dwell{ 0.5f };			// 0: 正弦のまま / 1: 両端で長めに留まる（smoothstep を重ねる）
	};

	struct CameraSway {
		bool Enabled{ false };
		CameraPose Hero{ -0.05f, 13.0f, 3.5f, 0.75f };
		CameraPose Overview{ 0.45f, 18.0f, 2.0f, 0.8f };
		// 周期をずらしておくと、組み合わせがすぐには繰り返さない
		SwayChannel Pitch{ true, 14.0f, 0.0f, 0.5f };
		SwayChannel Distance{ true, 19.0f, 0.25f, 0.5f };
		SwayChannel TargetY{ true, 11.0f, 0.5f, 0.3f };
		SwayChannel FovY{ true, 23.0f, 0.1f, 0.3f };
	};

	struct TimeScaleSway {
		bool Enabled{ false };
		float Min{ 0.35f };
		float Max{ 1.0f };
		SwayChannel Channel{ true, 9.0f, 0.0f, 0.6f };	// 重み 0 = Max / 1 = Min（Phase 0 で通常速度から始まる）
	};

	//----	------	------	------	------	----//
	// メニュー（Start / Exit）
	//----	------	------	------	------	----//

	// 見た目と演出の調整値（画面は 1280x720、Y は下向き）
	struct TitleMenuStyle {
		float RightX{ 1240.0f };			// ラベルの右端の X（画像は文字が右寄せなので右端基準で置く）
		float FirstY{ 540.0f };				// 1 項目目の中心 Y
		float Spacing{ 80.0f };				// 項目の間隔
		float Width{ 360.0f };				// 表示サイズ（画像は 720x180）
		float Height{ 90.0f };
		float UnselectedAlpha{ 0.35f };
		float UnselectedScale{ 0.9f };
		float SelectedOffsetX{ -12.0f };	// 選択中は少し左へ出す
		float Smoothing{ 14.0f };			// 選択の切り替えの追従の速さ（/秒）
		float FadeInDuration{ 0.8f };		// 起動時の黒からのフェードイン
		float BlinkDuration{ 0.45f };		// 決定後の点滅
		float BlinkFrequency{ 10.0f };		// Hz
		float FadeOutDuration{ 0.6f };		// 点滅後の黒へのフェードアウト
	};

	// タイトルロゴ（Assets/UI/Label.Title.png。今は仮画像。名前が決まったら画像を差し替えるだけ）
	struct TitleLogoStyle {
		bool Enabled{ true };
		float X{ 80.0f };					// アンカー位置（画面 1280x720、Y は下向き）
		float Y{ 200.0f };
		float AnchorX{ 0.0f };				// 0 = 左端基準（仮画像は左寄せ）/ 0.5 = 中央 / 1 = 右端
		float AnchorY{ 0.5f };
		float Width{ 720.0f };				// 表示サイズ（仮画像は 1440x360 なので半分）
		float Height{ 180.0f };
		float Alpha{ 1.0f };
		float Delay{ 0.3f };				// メニューのフェードイン開始からの遅れ
		float FadeDuration{ 1.2f };			// 透明 → 不透明
		float RiseDistance{ 16.0f };		// 下から浮き上がる距離（px）
	};

	// ラベル（ロゴ / Start / Exit）に掛ける、明滅するグロー
	struct TitleGlowStyle {
		bool Enabled{ true };
		Lumina::Float3 Color{ 1.00f, 0.72f, 0.40f };	// sRGB
		float Intensity{ 0.8f };
		float RadiusPx{ 14.0f };			// にじみの半径（画面 px）
		float PulseDepth{ 0.6f };			// 0: 明滅しない / 1: 一番暗いときに 0 まで落ちる
		float PulseFrequency{ 0.5f };		// Hz
		float PhaseStep{ 0.15f };			// ラベルごとの位相のずれ（周期に対する割合。上から順に少しずつ遅れる）
	};

	export class Title {
	private:
		template<typename...ArgTypes>
		void Startup(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void InBattle(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Win(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Lose(typename ArgTypes const&...args_);

	public:
		template<typename...ArgTypes>
		void Update(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Render(typename ArgTypes const&...args_);

	public:
		template<typename...ArgTypes>
		void Initialize(typename ArgTypes const&...args_);

		Title();
		virtual ~Title();

	private:
		//----	------	------	------	------	----//
		// Settings（ImGui での調整結果の保存・読み込み。実装は Game.Scene.Title.Settings.cpp）

		static constexpr char const* SettingsFilePath{ "Assets/Configs/TitleScene.json" };

		bool SaveSettings() const;
		bool LoadSettings();

		// 保存と読み込みで同じキー一覧を共有する（Archive = Writer / Reader、Self = Title / Title const）
		template<typename Archive, typename Self>
		static void VisitSettings(Archive& ar_, Self& self_);

		std::string SettingsStatus_{};		// ImGui に出す直近の結果

	private:
		//----	------	------	------	------	----//
		// Time

		bool IsPaused_{ false };
		float TimeScale_{ 1.0f };
		float Time_{ 0.0f };			// 魔法陣の回転・明滅に使う
		float SwayClock_{ 0.0f };		// 往復用の時計（TimeScale の影響を受けない実時間。Pause では止まる）
		TimeScaleSway TimeScaleSway_{};

		//----	------	------	------	------	----//
		// FirePillar prototype（キャラの足元基準。並び順 = 描画順で、外側 → 内側に並べる）

		Lumina::FlamePillar::Renderer FirePillarRenderer_{};
		std::vector<FirePillarEntry> FirePillars_{};
		std::vector<Lumina::FlamePillar::Instance> FirePillarInstances_{};	// 毎フレーム FirePillars_ から作り直す
		float FirePillarAge_{ 0.0f };

		//----	------	------	------	------	----//
		// Character（プレイヤー＋武器、フォワード）

		Lumina::ShowcaseCharacter Character_{};
		Lumina::ShowcaseLighting Lighting_{};
		bool LinkFireToPillar_{ true };		// 照り返しの色・高さを、有効な最初の火柱の Ramp.Mid / Height に合わせる
		float FireLightIntensity_{ 1.6f };

		//----	------	------	------	------	----//
		// MagicCircle prototype

		Lumina::MagicCircle::Renderer MagicCircleRenderer_{};
		std::vector<Lumina::MagicCircle::Layer> MagicCircleLayers_{};
		float MagicCircleIntensity_{ 1.0f };

		// 描き込み演出（Replay で全レイヤーの Reveal を 0 から順に伸ばす）
		bool IsRevealPlaying_{ false };
		float RevealTimer_{ 0.0f };
		float RevealDuration_{ 1.2f };		// 1 レイヤーの描き込みにかかる秒数
		float RevealStagger_{ 0.35f };		// レイヤー間の開始のずれ

		//----	------	------	------	------	----//
		// Camera（Update と Render の両方で使う。実装は Render.cpp）

		void BuildCamera(Lumina::Mat4& view_, Lumina::Mat4& viewProj_, Lumina::Vec3& eye_) const;

		//----	------	------	------	------	----//
		// Particles（実装は Game.Scene.Title.Particles.cpp）

		void InitializeParticles(Lumina::DX12::Context const& dxContext_, Lumina::AssetManager& assetMngr_);
		void UpdateParticles(
			Lumina::DX12::CommandList const& cmdList_,
			float deltaTime_,
			Lumina::Mat4 const& view_,
			Lumina::Mat4 const& viewProj_
		);
		void RenderParticles(Lumina::DX12::CommandList const& cmdList_);
		void EditParticles();		// ImGui（Release では何もしない）

		Lumina::DX12::RootSignature RS_Particle_{};
		Lumina::DX12::Shader VS_Particle_{};
		Lumina::DX12::Shader PS_Particle_{};
		Lumina::DX12::GraphicsPSO PSO_Particle_{};
		Lumina::DX12::UploadBuffer UB_ParticleViewToWorld_{};
		Lumina::DX12::UploadBuffer UB_ParticleVP_{};
		Lumina::DX12::DescriptorHeap LocalHeap_Particle_{};			// 0: ViewToWorld / 1: VP（non-shader-visible）
		Lumina::DX12::DescriptorTable GlobalTable_ParticleTextures_{};	// 0: Particles.png

		std::unique_ptr<ParticleSystem<TitleParticle>> Sparkles_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> Aura_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> Embers_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> CircleLights_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> WeaponGlow_{ nullptr };

		SparkleEmitter SparkleParams_{};
		AuraEmitter AuraParams_{};
		EmberEmitter EmberParams_{};
		CircleLightEmitter CircleLightParams_{};
		WeaponGlowEmitter WeaponGlowParams_{};

		// 発生数の端数の持ち越し・本編の色計算用の時間
		float SparkleAccum_{ 0.0f };
		float AuraAccum_{ 0.0f };
		float EmberAccum_{ 0.0f };
		float CircleLightAccum_{ 0.0f };
		float WeaponGlowAccum_{ 0.0f };
		float SparkleTimeFactor_{ 0.0f };
		float AuraTimeFactor_{ 0.0f };
		int AuraSpawnIndex_{ 0 };

		//----	------	------	------	------	----//
		// Menu UI（実装は Game.Scene.Title.UI.cpp）。本編の SpriteRenderer を流用

		void InitializeUI(Lumina::DX12::Context const& dxContext_, Lumina::AssetManager& assetMngr_);
		void ResetMenu();
		void UpdateUI(Lumina::WinApp::RawInput const& input_);
		void RenderUI(Lumina::DX12::CommandList const& cmdList_);
		void EditUI();		// ImGui（Release では何もしない）

		enum class MenuPhase : int {
			FadeIn,		// 黒から明ける（この間も選択・決定は受け付ける）
			Idle,
			Decided,	// 選んだ項目が点滅
			FadeOut,	// 黒へ
			Done,		// シーン切り替え / 終了を要求済み
		};
		static constexpr int NumMenuItems{ 2 };		// 0: Start / 1: Exit

		std::unique_ptr<Lumina::SpriteRenderer> UISpriteRenderer_{ nullptr };
		std::unique_ptr<Lumina::SpriteRenderer> UIGlowRenderer_{ nullptr };	// グロー専用（1 フレームに 1 回しかバッチできないので分ける）
		Lumina::DX12::Shader VS_UI_{};
		Lumina::DX12::Shader PS_UI_{};
		Lumina::DX12::GraphicsPSO PSO_UI_{};
		Lumina::DX12::Shader PS_UIGlow_{};
		Lumina::DX12::GraphicsPSO PSO_UIGlow_{};		// 加算・TitleLabelGlow.PS
		Lumina::DX12::UploadBuffer UB_UIOrtho_{};
		Lumina::DX12::DescriptorHeap LocalHeap_UI_{};			// 0: 正射影の CBV（non-shader-visible）
		Lumina::DX12::DescriptorTable GlobalTable_UITextures_{};	// 0: Start / 1: Exit / 2: Blank（フェード用）/ 3: Title

		Lumina::Sprite UI_Items_[NumMenuItems]{};
		Lumina::Sprite UI_Fade_{};
		Lumina::Sprite UI_Logo_{};

		TitleLogoStyle LogoStyle_{};
		float LogoTimer_{ 0.0f };

		static constexpr int NumGlowTargets{ 3 };		// 0: ロゴ / 1: Start / 2: Exit
		Lumina::Sprite UI_Glows_[NumGlowTargets]{};
		TitleGlowStyle GlowStyle_{};
		float GlowTimer_{ 0.0f };

		TitleMenuStyle MenuStyle_{};
		MenuPhase MenuPhase_{ MenuPhase::FadeIn };
		float MenuTimer_{ 0.0f };
		int MenuSelected_{ 0 };
		float MenuBlend_[NumMenuItems]{ 1.0f, 0.0f };	// 選択の度合い 0..1（見た目用に遅れて追従）
		float FadeAlpha_{ 1.0f };						// 画面全体の黒の濃さ
		float FadeOutFrom_{ 0.0f };
		bool PrevKeyUp_{ false };
		bool PrevKeyDown_{ false };
		bool PrevKeyConfirm_{ false };

		//----	------	------	------	------	----//
		// オービットカメラ（注視点のまわりを回る）

		bool AutoOrbit_{ false };
		float OrbitSpeed_{ 0.15f };		// rad/s
		float CameraYaw_{ 0.0f };
		float CameraPitch_{ 0.2f };
		float CameraDistance_{ 16.0f };
		float CameraTargetY_{ 3.0f };
		float CameraFovY_{ 0.8f };
		CameraSway CameraSway_{};

		float ClearColor_[4]{ 0.03f, 0.03f, 0.05f, 1.0f };
	};
}
