module Game.BossSkills;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれも Game.BossSkills.ixx で import 済み
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

namespace {
	constexpr float TwoPi{ 6.28318530718f };

	constexpr std::uint32_t CueLunge{ Game::CueIdOf("Lunge") };
	constexpr std::uint32_t CueThornWave{ Game::CueIdOf("ThornWave") };

	auto Facing(Game::Fighter const& f_) noexcept -> Lumina::Vec2 {
		return Lumina::Vec2{ std::cos(f_.Angle()), std::sin(f_.Angle()) };
	}

	auto Rotated(Lumina::Vec2 const& v_, float a_) noexcept -> Lumina::Vec2 {
		float const c{ std::cos(a_) };
		float const s{ std::sin(a_) };
		return Lumina::Vec2{ v_.x * c - v_.y * s, v_.x * s + v_.y * c };
	}

	// 点 p から線分 ab までの距離
	auto DistanceToSegment(Lumina::Vec2 const& p_, Lumina::Vec2 const& a_, Lumina::Vec2 const& b_) noexcept -> float {
		float const abx{ b_.x - a_.x };
		float const aby{ b_.y - a_.y };
		float const len2{ abx * abx + aby * aby };
		float t{ 0.0f };
		if (len2 > 1.0e-8f) {
			t = std::clamp(((p_.x - a_.x) * abx + (p_.y - a_.y) * aby) / len2, 0.0f, 1.0f);
		}
		float const dx{ p_.x - (a_.x + abx * t) };
		float const dy{ p_.y - (a_.y + aby * t) };
		return std::sqrt(dx * dx + dy * dy);
	}
}

namespace Game {
	//====	======	======	======	======	====//
	//	茨の列									//
	//====	======	======	======	======	====//

	void ThornWaves::Clear() noexcept {
		Lines_.clear();
		HitGroups_.clear();
		Spikes_.clear();
		Contacts_.clear();
	}

	auto ThornWaves::Spawn(Lumina::Vec2 const& origin_, Lumina::Vec2 const& dir_, ThornWaveTuning const& tuning_) -> std::uint32_t {
		std::uint32_t const group{ NextGroup_++ };
		if (NextGroup_ == 0U) { NextGroup_ = 1U; }

		float const len{ std::sqrt(dir_.x * dir_.x + dir_.y * dir_.y) };
		Lumina::Vec2 const dir{ (len > 1.0e-6f) ? Lumina::Vec2{ dir_.x / len, dir_.y / len } : Lumina::Vec2{ 1.0f, 0.0f } };
		std::int32_t const n{ std::max(tuning_.Lines, 1) };
		for (std::int32_t i = 0; i < n; ++i) {
			// 真ん中を中心に左右へ Spread ずつ（偶数本なら真ん中を挟む）
			float const a{ (static_cast<float>(i) - static_cast<float>(n - 1) * 0.5f) * tuning_.Spread };
			Line l{};
			l.Dir = Rotated(dir, a);
			l.Origin = Lumina::Vec2{ origin_.x + l.Dir.x * tuning_.StartOffset, origin_.y + l.Dir.y * tuning_.StartOffset };
			l.Group = group;
			l.Tuning = tuning_;
			Lines_.push_back(l);
		}
		return group;
	}

	auto ThornWaves::FrontAt(Line const& l_, float s_) const noexcept -> Lumina::Vec2 {
		float side{ 0.0f };
		if (l_.Tuning.SineAmplitude != 0.0f && l_.Tuning.SineWavelength > 1.0e-3f) {
			side = l_.Tuning.SineAmplitude * std::sin(TwoPi * s_ / l_.Tuning.SineWavelength);
		}
		return Lumina::Vec2{
			l_.Origin.x + l_.Dir.x * s_ - l_.Dir.y * side,
			l_.Origin.y + l_.Dir.y * s_ + l_.Dir.x * side,
		};
	}

	auto ThornWaves::IsHit(std::uint32_t group_) const noexcept -> bool {
		return std::ranges::find(HitGroups_, group_) != HitGroups_.end();
	}

	void ThornWaves::MarkHit(std::uint32_t group_) noexcept {
		if (!IsHit(group_)) { HitGroups_.push_back(group_); }
	}

	void ThornWaves::Update(Fighter const* target_, float arenaMin_, float arenaMax_) {
		Spikes_.clear();
		Contacts_.clear();

		Lumina::Vec2 targetXZ{};
		float targetFeet{ 0.0f };
		float targetRadius{ 0.0f };
		if (target_ != nullptr) {
			Lumina::Vec3 const p{ target_->WorldPosition() };
			targetXZ = Lumina::Vec2{ p.x, p.z };
			targetFeet = p.y;
			targetRadius = target_->Setup().PushRadius * 0.5f;
		}

		for (Line& l : Lines_) {
			float const before{ l.Traveled };
			l.Traveled = std::min(l.Traveled + std::max(l.Tuning.Speed, 0.0f), l.Tuning.MaxDistance);

			// 見た目の茨: 通ったところに SpikeSpacing ごと
			float const spacing{ std::max(l.Tuning.SpikeSpacing, 0.1f) };
			while (l.NextSpike <= l.Traveled) {
				Lumina::Vec2 const at{ FrontAt(l, l.NextSpike) };
				Spikes_.push_back({ Lumina::Float3{ at.x, 0.0f, at.y }, l.Dir, l.Group });
				l.NextSpike += spacing;
			}

			// 当たり: このフレームに先端が通った線分と、相手の足元
			bool const groupTouched{ std::ranges::any_of(Contacts_, [&](Contact const& c_) { return c_.Group == l.Group; }) };
			if (target_ != nullptr && !IsHit(l.Group) && !groupTouched && targetFeet <= l.Tuning.HitHeight) {	// まとまりごとに 1 つまで
				Lumina::Vec2 const a{ FrontAt(l, before) };
				Lumina::Vec2 const b{ FrontAt(l, l.Traveled) };
				if (DistanceToSegment(targetXZ, a, b) <= l.Tuning.HitRadius + targetRadius) {
					Contacts_.push_back({ l.Group, Lumina::Float3{ targetXZ.x, 0.5f, targetXZ.y }, l.Dir, l.Tuning.Damage, l.Tuning.Knockback });
				}
			}
		}

		// 進み切った・闘技場の外へ出た列を消す
		std::erase_if(Lines_, [&](Line const& l_) {	// std::erase_if は C++20
			Lumina::Vec2 const f{ FrontAt(l_, l_.Traveled) };
			bool const outside{ f.x < arenaMin_ || f.x > arenaMax_ || f.y < arenaMin_ || f.y > arenaMax_ };
			return l_.Traveled >= l_.Tuning.MaxDistance || outside;
		});
		// もう列の残っていないまとまりの「当たった」は忘れる
		std::erase_if(HitGroups_, [&](std::uint32_t g_) {
			return std::ranges::none_of(Lines_, [g_](Line const& l_) { return l_.Group == g_; });
		});
	}

	//====	======	======	======	======	====//
	//	スキルの共通部分						//
	//====	======	======	======	======	====//

	auto BossSkill::ToTarget(Fighter const& self_, float& distance_) const noexcept -> Lumina::Vec2 {
		distance_ = 0.0f;
		if (Target_ == nullptr) { return Facing(self_); }
		Lumina::Vec3 const s{ self_.WorldPosition() };
		Lumina::Vec3 const t{ Target_->WorldPosition() };
		float const dx{ t.x - s.x };
		float const dz{ t.z - s.z };
		distance_ = std::sqrt(dx * dx + dz * dz);
		if (distance_ < 1.0e-4f) { return Facing(self_); }
		return Lumina::Vec2{ dx / distance_, dz / distance_ };
	}

	auto BossSkill::InRange(Fighter const& self_, float min_, float max_) const noexcept -> bool {
		if (Target_ == nullptr) { return false; }
		float d{};
		ToTarget(self_, d);
		return d >= min_ && d <= max_;
	}

	auto BossSkill::HasCue(AbilityContext const& ctx_, std::uint32_t cue_) noexcept -> bool {
		return std::ranges::find(ctx_.Frame.Cues, cue_) != ctx_.Frame.Cues.end();
	}

	//====	======	======	======	======	====//
	//	溜め斬り								//
	//====	======	======	======	======	====//

	auto ChargeSlash::CanStart(Fighter const& self_) const -> bool {
		return IsCooldownReady() && !self_.IsAirborne() && InRange(self_, Tuning_->MinRange, Tuning_->MaxRange);
	}

	auto ChargeSlash::Start([[maybe_unused]] AbilityContext& ctx_) -> bool {
		Phase_ = Phase::Charge;
		Frame_ = 0;
		Lunged_ = false;
		LungeLeft_ = 0;
		LungeSpeed_ = 0.0f;
		StartCooldown(Tuning_->Cooldown);
		return true;
	}

	void ChargeSlash::StartLunge(Fighter const& self_) {
		Lunged_ = true;
		float d{};
		ToTarget(self_, d);
		LungeDir_ = Facing(self_);		// 振りかぶりの間に追った向きへ（ここからは曲がらない）
		float const distance{ std::clamp(d - Tuning_->LungeStop, 0.0f, std::max(Tuning_->LungeMax, 0.0f)) };
		std::int32_t const frames{ std::max(Tuning_->LungeFrames, 1) };
		// 毎フレーム速さを Decay 倍に落とし、frames フレームの合計が distance になる初速
		constexpr float Decay{ 0.75f };
		float const sum{ (1.0f - std::pow(Decay, static_cast<float>(frames))) / (1.0f - Decay) };
		LungeSpeed_ = distance / sum;
		LungeLeft_ = frames;
	}

	auto ChargeSlash::Update(AbilityContext& ctx_) -> bool {
		Fighter& self{ ctx_.Self };
		ChargeSlashTuning const& t{ *Tuning_ };
		++Frame_;
		float d{};
		Lumina::Vec2 const toTarget{ ToTarget(self, d) };

		if (Phase_ == Phase::Charge) {
			self.FaceToward(toTarget, t.ChargeTurnRate);
			if (Frame_ >= t.ChargeFrames) {
				Phase_ = Phase::Slam;		// 次のフレームから SkillSlam を頭から再生（Clip() の名前が変わる）
				Frame_ = 0;
			}
			return true;
		}

		if (Phase_ != Phase::Slam) { return false; }

		if (!Lunged_) {
			self.FaceToward(toTarget, t.SlamTurnRate);
			if (HasCue(ctx_, CueLunge) || Frame_ >= t.LungeFallbackFrame) { StartLunge(self); }
		}
		if (LungeLeft_ > 0) {
			self.Translate(LungeDir_.x * LungeSpeed_, LungeDir_.y * LungeSpeed_);
			LungeSpeed_ *= 0.75f;
			--LungeLeft_;
		}

		// クリップの終わり（切り替えた直後の数フレームは、前のクリップの「終わった」が残っているので見ない）。念のため上限も
		if (Frame_ > 2 && ctx_.Frame.ClipFinished) { Phase_ = Phase::None; return false; }
		if (Frame_ > 600) { Phase_ = Phase::None; return false; }
		return true;
	}

	void ChargeSlash::Halt([[maybe_unused]] Fighter& self_) {
		Phase_ = Phase::None;
		LungeLeft_ = 0;
	}

	auto ChargeSlash::Clip() const noexcept -> std::string_view {
		switch (Phase_) {
			case Phase::Charge: return "SkillCharge";
			case Phase::Slam: return "SkillSlam";
			default: return {};
		}
	}

	auto ChargeSlash::SuperArmor() const noexcept -> bool {
		return Phase_ == Phase::Charge || (Phase_ == Phase::Slam && Frame_ < Tuning_->ArmorUntil);
	}

	auto ChargeSlash::PhaseName() const noexcept -> std::string_view {
		switch (Phase_) {
			case Phase::Charge: return "Charge";
			case Phase::Slam: return Lunged_ ? "Slam (lunged)" : "Slam (wind-up)";
			default: return "-";
		}
	}

	//====	======	======	======	======	====//
	//	茨の波									//
	//====	======	======	======	======	====//

	auto ThornWave::CanStart(Fighter const& self_) const -> bool {
		return IsCooldownReady() && Waves_ != nullptr && !self_.IsAirborne() && InRange(self_, Tuning_->MinRange, Tuning_->MaxRange);
	}

	auto ThornWave::Start([[maybe_unused]] AbilityContext& ctx_) -> bool {
		Phase_ = Phase::Windup;
		Frame_ = 0;
		Fired_ = false;
		StartCooldown(Tuning_->Cooldown);
		return true;
	}

	auto ThornWave::Update(AbilityContext& ctx_) -> bool {
		Fighter& self{ ctx_.Self };
		ThornWaveTuning const& t{ *Tuning_ };
		++Frame_;

		switch (Phase_) {
			case Phase::Windup: {
				float d{};
				self.FaceToward(ToTarget(self, d), t.WindupTurnRate);
				if (Frame_ >= t.WindupFrames) {
					Phase_ = Phase::Slash;
					Frame_ = 0;
				}
				return true;
			}
			case Phase::Slash: {
				bool const clipEnded{ Frame_ > 2 && ctx_.Frame.ClipFinished };
				// 合図・代わりのフレーム数・クリップの終わり（合図より前に終わったら、そこで撃つ）のどれか早いもの
				if (!Fired_ && (HasCue(ctx_, CueThornWave) || Frame_ >= t.FireFallbackFrame || clipEnded)) {
					Lumina::Vec3 const p{ self.WorldPosition() };
					Waves_->Spawn(Lumina::Vec2{ p.x, p.z }, Facing(self), t);
					Fired_ = true;
				}
				if (clipEnded) {
					Phase_ = Phase::Recover;
					Frame_ = 0;
				}
				else if (Frame_ > 600) {
					Phase_ = Phase::None;
					return false;
				}
				return true;
			}
			case Phase::Recover:
				if (Frame_ >= t.RecoverFrames) {
					Phase_ = Phase::None;
					return false;
				}
				return true;
			default:
				return false;
		}
	}

	void ThornWave::Halt([[maybe_unused]] Fighter& self_) {
		Phase_ = Phase::None;
	}

	auto ThornWave::Clip() const noexcept -> std::string_view {
		switch (Phase_) {
			case Phase::Windup: return "SkillDrag";
			case Phase::Slash:
			case Phase::Recover: return "SkillWave";		// 隙の間は振り終わりの姿勢のまま（同じ名前なので再生し直さない）
			default: return {};
		}
	}

	auto ThornWave::ClipLoop() const noexcept -> bool { return Phase_ == Phase::Windup; }

	auto ThornWave::SuperArmor() const noexcept -> bool { return Phase_ == Phase::Windup; }

	auto ThornWave::PhaseName() const noexcept -> std::string_view {
		switch (Phase_) {
			case Phase::Windup: return "Windup";
			case Phase::Slash: return Fired_ ? "Slash (fired)" : "Slash";
			case Phase::Recover: return "Recover";
			default: return "-";
		}
	}
}
