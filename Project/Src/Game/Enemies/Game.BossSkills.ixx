//////	//////	//////	//////	//////	//////
//	BossSkills - ボスのスキル				//
//////	//////	//////	//////	//////	//////
// ボスのアビリティ（Game.Fighter の Ability）2 つと、茨の波の当たり判定。どちらも仮の案（2026-11-11）。見た目（予兆・茨）は別で作る。
//
// 溜め斬り（ChargeSlash、枠 0）
//   溜め（SkillCharge = ReverseCharge.gltf の構え、ChargeFrames）… 相手の方へ向き続ける。スーパーアーマー
//   振り（SkillSlam = ReverseChargeAttack.gltf）… 振りかぶる間も相手を追い、Cue "Lunge" で相手の手前まで一気に踏み込み、振り下ろす。
//                                              当たり判定はサイドカーの Hitbox の窓（通常攻撃と同じく武器のコライダ）。
//                                              振りの始めから ArmorUntil フレームまでスーパーアーマー。振り下ろした後は沈んだまま長い隙
//   中距離（MinRange〜MaxRange）でだけ使える。弾けばのけぞらせられる、回避の無敵で抜けられる。
//
// 茨の波（ThornWave、枠 1）
//   溜め（SkillDrag = Swinging.gltf の低く剣を揺らす動き、ループ）… 相手の方へ向き続ける。スーパーアーマー
//   振り（SkillWave = AirDiveAttack.gltf）… Cue "ThornWave" で、向いている方へ扇状に茨の列（Lines 本）を地面に走らせる
//   隙（RecoverFrames）
//   茨の列（ThornWaves）は地面を進み、相手の足元を通ったら 1 回だけ当たる（同じ振りで出した列のどれか 1 本だけ）。
//   跳んでいればかわせる（足の高さ > HitHeight）。回避の無敵・ガード・弾きも効く（弾いても撃った側はのけぞらない）。
//
// Cue が来なかったとき（サイドカーが無い・壊れている）は、振りの始めから FallbackFrame で踏み込む・撃つ（技が止まらないように）。
// 時間の単位はフレーム（固定 60fps）。

export module Game.BossSkills;

//****	******	******	******	******	****//

import <algorithm>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <span>;
import <string_view>;
import <vector>;

import Lumina.Math;

import Game.Fighter;

//////	//////	//////	//////	//////	//////

export namespace Game {
	// ボスのスキルの枠（AbilityGroup::Tool）
	namespace BossSkillSlot {
		inline constexpr std::size_t ChargeSlash{ 0U };
		inline constexpr std::size_t ThornWave{ 1U };
	}

	//==== 調整値（ImGui「Boss AI」→ Skills。Boss.json の "Skills" に保存）====//

	struct ChargeSlashTuning {
		float MinRange{ 8.0f };					// 相手との距離がこの範囲のときだけ使える
		float MaxRange{ 26.0f };
		std::int32_t Cooldown{ 360 };			// 使い始めてから次に使えるまで
		std::int32_t ChargeFrames{ 36 };		// 溜めの長さ（予兆）
		float ChargeTurnRate{ 0.1f };			// 溜めの間に相手を追う速さ（rad/フレーム）
		float SlamTurnRate{ 0.06f };			// 振りかぶりの間（踏み込むまで）に相手を追う速さ
		float LungeMax{ 18.0f };				// 踏み込む距離の上限
		float LungeStop{ 4.0f };				// 相手の手前これだけで止まる
		std::int32_t LungeFrames{ 8 };			// 踏み込みにかけるフレーム数（速さは等比で落とす）
		std::int32_t LungeFallbackFrame{ 14 };	// Cue "Lunge" が来なくても、振りの始めからこのフレームで踏み込む
		std::int32_t ArmorUntil{ 20 };			// 振りの始めからこのフレームまでスーパーアーマー（溜めの間はずっと）
	};

	struct ThornWaveTuning {
		float MinRange{ 10.0f };
		float MaxRange{ 70.0f };
		std::int32_t Cooldown{ 300 };
		std::int32_t WindupFrames{ 40 };		// 溜めの長さ（予兆）
		float WindupTurnRate{ 0.08f };
		std::int32_t FireFallbackFrame{ 8 };	// Cue "ThornWave" が来なくても、振りの始めからこのフレームで撃つ（クリップが先に終われば、そこで撃つ）
		std::int32_t RecoverFrames{ 24 };		// 振りのクリップが終わってからの隙

		// 茨の列
		std::int32_t Lines{ 3 };				// 扇の本数
		float Spread{ 0.35f };					// 隣の列との角度（rad）
		float StartOffset{ 2.5f };				// 自分の前のこの距離から出る
		float Speed{ 0.7f };					// 1 フレームに進む距離
		float MaxDistance{ 45.0f };				// ここまで進んだら消える（闘技場の外へ出ても消える）
		float HitRadius{ 1.5f };				// 列の先端と相手（足元の XZ）の距離がこれ以下なら当たる
		float HitHeight{ 1.2f };				// 相手の足の高さがこれより上なら当たらない（跳んでかわせる）
		float SpikeSpacing{ 1.5f };				// 見た目の茨を出す間隔（Spikes() に積む）
		float SineAmplitude{ 0.0f };			// 列を横に揺らす幅（0 で直線。旧「波濤」の揺れ）
		float SineWavelength{ 8.0f };
		float Damage{ 250.0f };					// サイドカーの Hitbox と同じ単位（InGame が DamageScale を掛ける）
		float Knockback{ 8.0f };
	};

	//==== 茨の列 ====//

	class ThornWaves {
	public:
		// 見た目の茨を出す位置（このフレームに列が通ったところ。InGame がエフェクトを出す）
		struct Spike {
			Lumina::Float3 Position{};
			Lumina::Vec2 Dir{};
			std::uint32_t Group{ 0U };
		};
		// 相手（の足元）に先端が触れている列（InGame が無敵を見て ReceiveHit し、当てたら MarkHit）
		struct Contact {
			std::uint32_t Group{ 0U };
			Lumina::Float3 Point{};
			Lumina::Vec2 Dir{};			// 列の進む向き（押す向き）
			float Damage{ 0.0f };
			float Knockback{ 0.0f };
		};

		void Clear() noexcept;

		// origin_（XZ）から dir_ の向きへ、tuning_ の本数・扇で列を出す。出した列のまとまりの番号を返す
		auto Spawn(Lumina::Vec2 const& origin_, Lumina::Vec2 const& dir_, ThornWaveTuning const& tuning_) -> std::uint32_t;

		// 列を進め、見た目の茨と、相手との接触を作り直す（target_ が null なら接触なし）
		void Update(Fighter const* target_, float arenaMin_, float arenaMax_);

		// このまとまりは当たった（もう当たらない）
		void MarkHit(std::uint32_t group_) noexcept;

		auto Spikes() const noexcept -> std::span<Spike const> { return Spikes_; }
		auto Contacts() const noexcept -> std::span<Contact const> { return Contacts_; }
		auto ActiveLines() const noexcept -> std::size_t { return Lines_.size(); }

	private:
		struct Line {
			Lumina::Vec2 Origin{};
			Lumina::Vec2 Dir{};
			std::uint32_t Group{ 0U };
			float Traveled{ 0.0f };
			float NextSpike{ 0.0f };
			ThornWaveTuning Tuning{};	// 出したときの値（途中で ImGui を触っても、出ている列は変わらない）
		};
		auto FrontAt(Line const& l_, float s_) const noexcept -> Lumina::Vec2;
		auto IsHit(std::uint32_t group_) const noexcept -> bool;

		std::vector<Line> Lines_{};
		std::vector<std::uint32_t> HitGroups_{};
		std::vector<Spike> Spikes_{};
		std::vector<Contact> Contacts_{};
		std::uint32_t NextGroup_{ 1U };
	};

	//==== スキル ====//

	// ボスのスキルの共通部分: 相手（追尾・距離の判定用）とクールダウン
	class BossSkill : public Ability {
	public:
		void SetTarget(Fighter const* target_) noexcept { Target_ = target_; }
		void Tick() noexcept { if (CooldownLeft_ > 0) { --CooldownLeft_; } }		// 毎フレーム 1 回（使用中も進む）
		void ResetCooldown() noexcept { CooldownLeft_ = 0; }
		auto CooldownLeft() const noexcept -> std::int32_t { return CooldownLeft_; }
		virtual auto PhaseName() const noexcept -> std::string_view = 0;			// ImGui の表示用

	protected:
		// 自分 → 相手（XZ、長さ 1）と距離。相手がいなければ向いている方向と 0
		auto ToTarget(Fighter const& self_, float& distance_) const noexcept -> Lumina::Vec2;
		auto InRange(Fighter const& self_, float min_, float max_) const noexcept -> bool;
		static auto HasCue(AbilityContext const& ctx_, std::uint32_t cue_) noexcept -> bool;

		// 派生クラスはメンバを直接触らず、ここから読む・頼む（2026-11-26：Target_ / CooldownLeft_ を private へ）
		auto Target() const noexcept -> Fighter const* { return Target_; }
		auto IsCooldownReady() const noexcept -> bool { return CooldownLeft_ <= 0; }
		void StartCooldown(std::int32_t frames_) noexcept { CooldownLeft_ = (frames_ > 0) ? frames_ : 0; }

	private:
		Fighter const* Target_{ nullptr };
		std::int32_t CooldownLeft_{ 0 };
	};

	class ChargeSlash final : public BossSkill {
	public:
		explicit ChargeSlash(ChargeSlashTuning const* tuning_) noexcept : Tuning_{ tuning_ } {}

		auto Name() const noexcept -> std::string_view override { return "ChargeSlash"; }
		auto CanStart(Fighter const& self_) const -> bool override;
		auto Start(AbilityContext& ctx_) -> bool override;
		auto Update(AbilityContext& ctx_) -> bool override;
		void Halt(Fighter& self_) override;
		auto Clip() const noexcept -> std::string_view override;
		auto SuperArmor() const noexcept -> bool override;
		auto PhaseName() const noexcept -> std::string_view override;

		auto Lunged() const noexcept -> bool { return Lunged_; }

		// 予兆の演出用（Game.CombatVfx が毎フレーム見る。2026-11-11）
		// 溜めの進み 0..1（溜めの間だけ。それ以外は負）
		auto ChargeProgress() const noexcept -> float {
			if (Phase_ != Phase::Charge) { return -1.0f; }
			return std::clamp(static_cast<float>(Frame_) / static_cast<float>(std::max(Tuning_->ChargeFrames, 1)), 0.0f, 1.0f);
		}
		auto IsSlamWindup() const noexcept -> bool { return Phase_ == Phase::Slam && !Lunged_; }	// 振りかぶりの間（踏み込む前）
		auto IsSlamming() const noexcept -> bool { return Phase_ == Phase::Slam; }
		auto SlamFrame() const noexcept -> std::int32_t { return (Phase_ == Phase::Slam) ? Frame_ : -1; }

	private:
		enum class Phase : std::uint8_t { None, Charge, Slam };
		void StartLunge(Fighter const& self_);

		ChargeSlashTuning const* Tuning_;
		Phase Phase_{ Phase::None };
		std::int32_t Frame_{ 0 };
		bool Lunged_{ false };
		std::int32_t LungeLeft_{ 0 };
		float LungeSpeed_{ 0.0f };
		Lumina::Vec2 LungeDir_{ 1.0f, 0.0f };
	};

	class ThornWave final : public BossSkill {
	public:
		ThornWave(ThornWaveTuning const* tuning_, ThornWaves* waves_) noexcept : Tuning_{ tuning_ }, Waves_{ waves_ } {}

		auto Name() const noexcept -> std::string_view override { return "ThornWave"; }
		auto CanStart(Fighter const& self_) const -> bool override;
		auto Start(AbilityContext& ctx_) -> bool override;
		auto Update(AbilityContext& ctx_) -> bool override;
		void Halt(Fighter& self_) override;
		auto Clip() const noexcept -> std::string_view override;
		auto ClipLoop() const noexcept -> bool override;
		auto SuperArmor() const noexcept -> bool override;
		auto PhaseName() const noexcept -> std::string_view override;

		auto Fired() const noexcept -> bool { return Fired_; }

		// 予兆の演出用（Game.CombatVfx が毎フレーム見る。2026-11-11）
		// 溜めの進み 0..1（溜めの間だけ。それ以外は負）
		auto WindupProgress() const noexcept -> float {
			if (Phase_ != Phase::Windup) { return -1.0f; }
			return std::clamp(static_cast<float>(Frame_) / static_cast<float>(std::max(Tuning_->WindupFrames, 1)), 0.0f, 1.0f);
		}
		auto IsSlashing() const noexcept -> bool { return Phase_ == Phase::Slash && !Fired_; }	// 振りの途中（撃つ前）

	private:
		enum class Phase : std::uint8_t { None, Windup, Slash, Recover };

		ThornWaveTuning const* Tuning_;
		ThornWaves* Waves_;
		Phase Phase_{ Phase::None };
		std::int32_t Frame_{ 0 };
		bool Fired_{ false };
	};
}
