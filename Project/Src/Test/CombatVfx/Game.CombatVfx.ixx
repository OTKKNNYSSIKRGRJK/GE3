//////	//////	//////	//////	//////	//////
//	CombatVfx - 戦闘の仮エフェクト（プロト）	//
//////	//////	//////	//////	//////	//////
// 人に触ってもらうための「戦闘が読める」仮のエフェクト一式（2026-11-11）。見た目を詰めるのは後。
//
// 出すもの
//   足元      … 移動（ダッシュ・ジャンプ以外）で地面から光る胞子が舞い上がる。プレイヤーは青緑、ボスは薔薇色。一部は点光源つき
//   ボスのオーラ … プレイヤーの体のまわりの粒子（InGame の Player effects）と同じ動きで、薔薇色。一部は点光源つき
//   ボスのダッシュ … プレイヤーのダッシュの粒子と同じ動きで、薔薇色
//   剣の軌跡  … 攻撃中（コンボ・スキル）に、刃の根元〜先端の帯を数フレームぶん残す。刃先が遅いところは薄い
//   ガード    … 構えている間、前に「防げる角度（GuardHalfAngle）」の弧の壁を出す。弾きの猶予の間は明るく（弾きの練習用）。
//               受けた瞬間にひらめく
//   命中      … 結果ごとに輪・光・火花（ボス → プレイヤーは深紅、プレイヤー → ボスは青白、ガードは橙、弾きは金＋煌めき＋地面の衝撃輪、
//               回避の無敵ですり抜けたら淡い青の輪）。InGame の Knock effects（以前からの火花）はそのまま
//   予兆      … 溜め斬り: 踏み込む範囲の帯（溜めに合わせて塗られる）＋足元の縮む輪＋刃先へ集まる光＋始まりと振り下ろし直前の煌めき
//               茨の波 : 茨が走る扇の帯（本数・角度・幅は調整値どおり）＋足元を回る花びら＋始まりと撃つ直前の煌めき
//               （帯の中を流れる矢印（山形の模様）は 2026-11-11 に外した。帯そのものは残す）
//   スタン    … のけぞり（Stagger）の間、頭の上で星がくるくる回る
//
// 仕組み
//   - 命中・ガード・弾き・回避は Game.CombatEvents で受け取る（InGame が Publish、Update の終わりの Dispatch でここに届く）。
//   - 状態で決まるもの（構え・予兆・軌跡・足元）は、毎フレーム InGame が渡す FrameInput から作る。
//   - 帯・輪・弧・煌めきは Lumina.FxMesh（ワールド空間の三角形、加算）。粒子は ParticleSystem<FxParticle>（本編の粒子と同じ RS / PSO / アトラス）。
//   - 粒子と命中の光の点光源は Lights() で返す（InGame が次のフレームの点光源に足す）。
//   - 時間の単位はフレーム（固定 60fps）。
//
// 調整: ImGui「Combat VFX」（Debug のみ）、Assets/Configs/CombatVfx.json に保存。FxMesh の色は sRGB で指定（リニアにして渡す）。
// 粒子の色は本編の粒子と同じく変換しない（見た目を本編の粒子と揃えるため）。

export module Game.CombatVfx;

//****	******	******	******	******	****//

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <span>;
import <string>;
import <vector>;
import <d3d12.h>;

import nlohmann.json;

import Lumina.Math.Numerics;
import Lumina.Math;
import Lumina.Container.List;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

import ParticleSystem;
import Lumina.FxMesh;
import Game.CombatEvents;

//////	//////	//////	//////	//////	//////

export namespace Game::CombatVfx {
	//==== 見た目の調整値（色は sRGB）====//

	struct TrailStyle {
		bool Enabled{ true };
		float BladeBase{ 1.6f };			// 武器メッシュ（Kinoko.obj）のローカル Y。刃の根元
		float BladeTip{ 5.695f };			// 刃先（AnimEvent VFX の WeaponTip と同じ）
		int Lifetime{ 9 };					// 何フレームぶん残すか
		int Subdivide{ 4 };					// サンプルの間を曲線で何分割するか（速い振りを角張らせない）
		float MinSpeed{ 0.15f };			// 刃先の速さ（ワールド/フレーム）がこれ以下なら見えない
		float FullSpeed{ 0.9f };			// これ以上で最大
		Lumina::Float3 HeadColor{ 0.85f, 0.97f, 1.0f };
		Lumina::Float3 TailColor{ 0.25f, 0.55f, 1.0f };
		float Intensity{ 1.4f };
		float AgeFalloff{ 1.4f };
		float TipBias{ 1.3f };
		float CoreWhite{ 0.55f };
		float Streak{ 0.25f };
	};

	struct WardStyle {
		bool Enabled{ true };
		float Radius{ 2.6f };				// 体の中心からの距離（XZ）
		float Bottom{ 0.6f };				// 足元（WorldPosition の y）からの高さ
		float Top{ 6.4f };
		int Segments{ 24 };
		Lumina::Float3 Color{ 1.0f, 0.62f, 0.25f };
		Lumina::Float3 DeflectColor{ 1.0f, 0.92f, 0.55f };
		float Intensity{ 0.45f };			// 構えているだけのとき
		float DeflectIntensity{ 0.9f };		// 弾きの猶予の間に足す
		float RiseRate{ 0.25f };			// 出るときの追従（毎フレームの割合）
		float FallRate{ 0.18f };			// 消えるとき
		float FlashDecay{ 0.84f };			// 受けた瞬間のひらめきの減り方（毎フレーム掛ける）
	};

	// 命中の結果 1 種類ぶん
	struct BurstStyle {
		bool Enabled{ true };
		Lumina::Float3 Color{ 1.0f, 1.0f, 1.0f };
		int Life{ 18 };						// 輪の寿命（フレーム）
		float RingSize{ 3.0f };				// 輪の最終の半径（ワールド）
		float GlowSize{ 2.2f };
		float Intensity{ 1.5f };
		float StarSize{ 0.0f };				// 0 で煌めきなし
		float GroundRing{ 0.0f };			// 0 で地面の衝撃輪なし。あれば最終の半径
		int Sparks{ 10 };
		float SparkSpeed{ 0.45f };
		float LightIntensity{ 60.0f };		// 0 で点光源なし
	};

	struct ImpactStyle {
		BurstStyle PlayerHit{ true, { 1.0f, 0.12f, 0.18f }, 20, 3.4f, 2.6f, 1.7f, 2.4f, 0.0f, 14, 0.5f, 80.0f };	// ボス → プレイヤー
		BurstStyle BossHit{ true, { 0.55f, 0.85f, 1.0f }, 16, 3.0f, 2.2f, 1.5f, 0.0f, 0.0f, 12, 0.5f, 60.0f };		// プレイヤー → ボス
		BurstStyle Guard{ true, { 1.0f, 0.55f, 0.15f }, 14, 2.2f, 1.6f, 1.2f, 0.0f, 0.0f, 10, 0.4f, 40.0f };
		BurstStyle Deflect{ true, { 1.0f, 0.85f, 0.35f }, 24, 4.5f, 3.2f, 2.2f, 4.5f, 7.0f, 22, 0.7f, 140.0f };
		BurstStyle Dodge{ true, { 0.55f, 0.75f, 1.0f }, 16, 2.6f, 0.0f, 0.8f, 0.0f, 0.0f, 0, 0.0f, 0.0f };
	};

	struct TelegraphStyle {
		bool Enabled{ true };
		Lumina::Float3 SlashColor{ 1.0f, 0.18f, 0.12f };
		Lumina::Float3 WaveColor{ 1.0f, 0.3f, 0.55f };
		float DecalHeight{ 0.08f };			// 地面に貼るものの高さ（足元の y から）
		float DecalDepthBias{ 2.0f };		// 草に埋もれないように（ワールドの長さ）
		float SlashWidth{ 3.6f };			// 溜め斬りの帯の幅
		float SlashReach{ 6.0f };			// 踏み込んだ先から刃が届く長さ（帯の長さ = 踏み込み + これ）
		float LaneIntensity{ 1.1f };		// 帯と、溜めの間に縮む足元の輪の強さ
		float FillAlpha{ 0.18f };			// 帯の塗られた部分の濃さ
		int FadeFrames{ 10 };				// 撃った・踏み込んだ後に帯が消えるまで
		float GroundRingRadius{ 6.0f };		// 溜めの間に縮む足元の輪（始まりの半径）
		float GlintSize{ 3.2f };			// 煌めき（始まり・振り下ろし直前）
		float GlintIntensity{ 2.2f };
		int GlintFrames{ 16 };
		int ConvergeRate{ 3 };				// 刃先へ集まる光の数（毎フレーム）
		float ConvergeRadius{ 5.0f };
		int PetalRate{ 3 };					// 茨の波の溜めの花びら（毎フレーム）
		float PetalRadius{ 6.5f };
	};

	struct FootstepStyle {
		bool Enabled{ true };
		float StepDistance{ 1.5f };			// これだけ歩くごとに 1 回出す
		int PerStep{ 3 };
		float FootHeight{ 0.5f };			// 足元（WorldPosition の y）からの高さ
		float Spread{ 0.7f };
		float Size{ 1.0f };
		float Rise{ 0.035f };				// 舞い上がる速さ
		int Life{ 56 };
		float Alpha{ 0.35f };
		Lumina::Float3 PlayerColor{ 0.35f, 0.95f, 0.85f };
		Lumina::Float3 BossColor{ 1.0f, 0.4f, 0.6f };
		float LightChance{ 0.3f };			// 点光源をつける割合
		float LightScale{ 70.0f };			// 点光源の強さ = 不透明度 × これ
	};

	struct AuraStyle {
		bool Enabled{ true };
		float HueCenter{ 345.0f };			// 色相（度）。ゆっくり HueRange の幅で揺れる
		float HueRange{ 40.0f };
		float Saturation{ 0.75f };
		float Alpha{ 0.15f };
		float Size{ 0.7f };
		float LightScale{ 100.0f };			// 四芒星の粒だけ点光源（プレイヤーと同じ）
		bool Dash{ true };					// ダッシュの粒子
		float DashSize{ 3.0f };
	};

	// スタン（のけぞり）の間、頭の上で回る星
	struct StunStyle {
		bool Enabled{ true };
		int Count{ 5 };
		float Height{ 7.0f };				// 足元（WorldPosition の y）からの高さ
		float Radius{ 1.1f };
		float Size{ 0.45f };				// 星 1 つの大きさ（半分の幅、ワールド）
		float SpinSpeed{ 0.12f };			// 輪の回る速さ（rad/フレーム）
		float Bob{ 0.15f };					// 上下の揺れ
		Lumina::Float3 Color{ 1.0f, 0.88f, 0.3f };
		float Intensity{ 1.6f };
		float Glow{ 0.5f };					// 星のまわりのにじみ
		float RiseRate{ 0.3f };				// 出るときの追従（毎フレームの割合）
		float FallRate{ 0.15f };
	};

	struct Style {
		TrailStyle PlayerTrail{};
		TrailStyle BossTrail{ true, 1.6f, 5.695f, 9, 4, 0.15f, 0.9f, { 1.0f, 0.85f, 0.9f }, { 0.9f, 0.1f, 0.3f }, 1.4f, 1.4f, 1.3f, 0.5f, 0.25f };
		WardStyle Ward{};
		ImpactStyle Impact{};
		TelegraphStyle Telegraph{};
		FootstepStyle Footsteps{};
		AuraStyle BossAura{};
		StunStyle Stun{};
	};

	//==== 毎フレームの入力（InGame が作る）====//

	struct FighterView {
		Lumina::Float3 Position{};			// 足元（Fighter::WorldPosition）
		Lumina::Float3 Visual{};			// 見た目の中心（Fighter::ModelTranslate）
		float Angle{ 0.0f };				// 向き（前方 = (cos, sin)）
		bool Airborne{ false };
		bool Dashing{ false };
		bool DashTriggered{ false };		// このフレームでダッシュした
		bool Guarding{ false };
		bool CanDeflect{ false };			// 今受けたら弾ける
		float GuardHalfAngle{ 1.2217305f };
		bool Swinging{ false };				// 攻撃中（コンボ・スキル）。軌跡を残す
		bool Stunned{ false };				// のけぞっている（FighterAction::Stagger）。頭の上で星が回る
		Lumina::Mat4 const* WeaponWorld{ nullptr };	// 武器メッシュを描く行列（Render の weaponWorld と同じもの）
	};

	// ボスのスキルの状態（BossSkills の問い合わせをそのまま写す）
	struct BossSkillView {
		float ChargeProgress{ -1.0f };		// 溜め斬りの溜め 0..1（負 = 溜めていない）
		bool SlamWindup{ false };			// 振りかぶり（踏み込む前）
		float LungeStop{ 4.0f };			// 調整値（踏み込む距離の見積もり）
		float LungeMax{ 18.0f };
		float WindupProgress{ -1.0f };		// 茨の波の溜め 0..1
		bool WaveSlashing{ false };			// 振りの途中（撃つ前）
		int WaveLines{ 3 };
		float WaveSpread{ 0.35f };
		float WaveStart{ 2.5f };
		float WaveMaxDistance{ 45.0f };
		float WaveHalfWidth{ 1.5f };
	};

	struct FrameInput {
		FighterView Player{};
		FighterView Boss{};
		BossSkillView Skills{};
		Lumina::Mat4 const* ViewToWorld{ nullptr };		// 0 行目 = カメラの右、1 行目 = 上
		float ArenaMin{ -38.5f };
		float ArenaMax{ 37.0f };
		bool Suspended{ false };			// ゲームオーバーの演出中など。軌跡・構え・予兆・足元を止める（出ている粒子・命中はそのまま消えていく）
	};

	struct LightRequest {
		Lumina::Float3 Position{};
		Lumina::Float3 RGB{};
		float Intensity{ 0.0f };
	};

	// 粒子（本編の Particle に、動きの係数を足したもの）
	struct FxParticle : Particle {
		float MaxLife{ 1.0f };
		float BaseAlpha{ 1.0f };			// 毎フレーム AlphaMul を掛ける
		float AlphaMul{ 1.0f };
		float ScaleMul{ 1.0f };
		float FadeIn{ 0.0f };				// 出てからこのフレーム数で不透明度 0 → 1
		float Drag{ 1.0f };					// 速さに毎フレーム掛ける
		float Gravity{ 0.0f };				// 毎フレーム速さの y から引く（負なら浮く）
		float Spin{ 0.0f };					// Rotate.z に毎フレーム足す。SpinByVelocityZ が true なら Velocity.z × これ
		bool SpinByVelocityZ{ false };
		Lumina::Float3 Attractor{};			// Pull > 0 のとき、ここへ引かれる
		float Pull{ 0.0f };
		float Swirl{ 0.0f };				// Attractor のまわりを XZ で回る強さ
		float LightScale{ 0.0f };			// 0 より大きければ点光源（強さ = 不透明度 × これ）
	};

	//==== 本体 ====//

	class Presenter {
	public:
		Style Look{};

		// rtvFormat_ / dsvFormat_ は MergePass に合わせる。設定ファイルを読み、Game.CombatEvents に登録する（1 回だけ呼ぶこと）
		void Initialize(Lumina::DX12::Context const& dxContext_, DXGI_FORMAT rtvFormat_, DXGI_FORMAT dsvFormat_);

		// 出ているものを全部消す（戦闘のやり直し）
		void Clear();

		// 毎フレーム 1 回（Game.CombatEvents::Dispatch の後）。粒子の描画データを写すコマンドも積む（レンダーパスの外で呼ぶ）
		void Update(Lumina::DX12::CommandList const& cmdList_, FrameInput const& in_);

		// MergePass の中。粒子は本編の粒子と同じ RS / PSO / テクスチャ（InGame の値をそのまま渡す）
		void RenderParticles(
			Lumina::DX12::CommandList const& cmdList_,
			Lumina::DX12::RootSignature const& rs_,
			Lumina::DX12::GraphicsPSO const& pso_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_SceneVars_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_VP_,
			Lumina::DX12::DescriptorTable const& textures_,
			Lumina::DX12::DescriptorTable const& textures2_
		);
		// MergePass の中。depthSRV_ = G-buffer の深度の SRV。nearZ_ / farZ_ はカメラの射影と同じ値
		void RenderMesh(
			Lumina::DX12::CommandList const& cmdList_,
			Lumina::Mat4 const& viewProjection_,
			float nearZ_,
			float farZ_,
			D3D12_GPU_DESCRIPTOR_HANDLE depthSRV_
		);

		// このフレームの点光源（粒子と命中の光）。InGame が点光源の列に足す
		auto Lights() const noexcept -> std::span<LightRequest const> { return Lights_; }

		// ImGui「Combat VFX」（Debug のみ）
		void DrawEditor();

		static constexpr char const* ConfigPath{ "Assets/Configs/CombatVfx.json" };
		auto Save() -> bool;
		auto Load() -> bool;

	private:
		struct TrailSample {
			Lumina::Float3 Base{};
			Lumina::Float3 Tip{};
			float Alpha{ 0.0f };
			int Age{ 0 };
			bool Break{ false };			// 前のサンプルとつながない（振りの始まり）
		};
		struct Trail {
			std::vector<TrailSample> Samples{};	// 古い順
			bool WasSwinging{ false };
		};
		struct Ward {
			float Level{ 0.0f };
			float Deflect{ 0.0f };
			float Flash{ 0.0f };
		};
		struct Burst {
			Lumina::Float3 Point{};
			Lumina::Float3 Ground{};		// 地面の衝撃輪の中心
			int Age{ 0 };
			BurstStyle Look{};
			float Spin{ 0.0f };
		};
		struct Glint {
			Lumina::Float3 Point{};
			int Age{ 0 };
			int Life{ 16 };
			float Size{ 3.0f };
			Lumina::Float3 Color{};
			float Spin{ 0.0f };
		};
		struct LaneSet {			// 予兆の帯（消えていく途中も含む）
			Lumina::Float3 Origin{};
			float Angle{ 0.0f };
			float Length{ 0.0f };			// 溜め斬りの帯の長さ（踏み込み + 届く長さ）
			float Fill{ 0.0f };
			float Pulse{ 0.0f };
			float Fade{ 0.0f };				// 1 = 出ている、0 = 消えた
			bool Active{ false };
		};
		struct Stun {
			float Level{ 0.0f };			// 0..1（出る・消えるをなめらかに）
			float Angle{ 0.0f };			// 輪の回り具合
		};
		struct Footsteps {
			float Accum{ 0.0f };
			Lumina::Float3 Prev{};
			bool HasPrev{ false };
		};

		static void OnHit(void* self_, Game::CombatEvents::Hit const& e_);

		void UpdateTrail(Trail& t_, TrailStyle const& s_, FighterView const& f_, bool suspended_);
		void UpdateWard(Ward& w_, FighterView const& f_, bool suspended_);
		void UpdateTelegraph(FrameInput const& in_);
		void UpdateFootsteps(Footsteps& fs_, FighterView const& f_, Lumina::Float3 const& color_, bool suspended_);
		void EmitBossAura(FrameInput const& in_);
		void EmitSparks(Lumina::Float3 const& p_, Lumina::Float2 const& dir_, BurstStyle const& s_);

		void BuildTrail(Trail const& t_, TrailStyle const& s_);
		void BuildWard(Ward const& w_, FighterView const& f_);
		void BuildBursts();
		void BuildTelegraph(FrameInput const& in_);
		void BuildBillboard(Lumina::FxMesh::Shape shape_, Lumina::Float3 const& center_, float size_, Lumina::Float3 const& rgb_, float intensity_, Lumina::Float4 const& param_, float depthBias_);
		void BuildFlat(Lumina::FxMesh::Shape shape_, Lumina::Float3 const& center_, float size_, Lumina::Float3 const& rgb_, float intensity_, Lumina::Float4 const& param_);
		void BuildLane(Lumina::Float3 const& origin_, float angle_, float start_, float length_, float width_, Lumina::Float3 const& rgb_, float intensity_, float fill_);
		void UpdateStun(Stun& s_, FighterView const& f_, bool suspended_);
		void BuildStun(Stun const& s_, FighterView const& f_);

		void CollectLights();
		auto Random01() noexcept -> float;

		Lumina::FxMesh::Renderer Mesh_{};
		ParticleSystem<FxParticle> Particles_{};
		std::vector<LightRequest> Lights_{};

		Trail PlayerTrail_{};
		Trail BossTrail_{};
		Ward PlayerWard_{};
		Ward BossWard_{};
		std::vector<Burst> Bursts_{};
		std::vector<Glint> Glints_{};
		LaneSet SlashLane_{};
		LaneSet WaveLanes_{};
		Stun PlayerStun_{};
		Stun BossStun_{};
		Footsteps PlayerSteps_{};
		Footsteps BossSteps_{};

		// 予兆の前のフレームの状態（始まり・切り替わりを見る）
		bool PrevCharging_{ false };
		bool PrevSlamWindup_{ false };
		bool PrevWaveWindup_{ false };
		bool PrevWaveSlashing_{ false };

		int BossDashFrames_{ 0 };
		float AuraTime_{ 0.0f };
		float Time_{ 0.0f };				// 秒（模様の流れ）
		Lumina::Float3 CamRight_{ 1.0f, 0.0f, 0.0f };
		Lumina::Float3 CamUp_{ 0.0f, 1.0f, 0.0f };
		Lumina::Mat4 const* ViewToWorld_{ nullptr };
		FrameInput LastInput_{};			// 命中のハンドラが受けた側の構えを光らせるのに使う

		std::uint32_t Rng_{ 0x9E3779B9U };
		bool Subscribed_{ false };
		std::string Status_{};

		// 表示用
		std::uint32_t NumBurstsTotal_{ 0U };
	};
}
