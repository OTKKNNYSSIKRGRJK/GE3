module Game.Fighter;

//****	******	******	******	******	****//

import <algorithm>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <optional>;

import Lumina.Math;
import Lumina.Input;

import Game.PlayerInput;
import Game.PlayerCombo;

//////	//////	//////	//////	//////	//////

namespace {
	using Game::PlayerInput::Act;
	using Lumina::Input::MaskOf;

	constexpr float Pi{ 3.14159265358979f };

	// 角度の差を [-π, π] に畳む（向きを最短で回すため。Angle_ は折り返さずに足していく）
	auto WrapPi(float a_) -> float { return a_ - 2.0f * Pi * std::floor((a_ + Pi) / (2.0f * Pi)); }

	constexpr Lumina::Input::ActionMask AbilityActions{ MaskOf(Act::Tool, Act::UseItem) };

	constexpr std::int32_t NeverPressed{ 1 << 20 };		// SinceGuardPress_ の上限（ずっと押していない）

	auto GroupOf(Lumina::Input::ActionId action_) -> std::optional<Game::AbilityGroup> {
		if (action_ == Lumina::Input::ToActionId(Act::Tool)) { return Game::AbilityGroup::Tool; }
		if (action_ == Lumina::Input::ToActionId(Act::UseItem)) { return Game::AbilityGroup::Item; }
		return std::nullopt;
	}

	// 長さ 1 を超えたら 1 に収める
	auto ClampLength(Lumina::Vec2 v_) -> Lumina::Vec2 {
		float const len2{ v_.x * v_.x + v_.y * v_.y };
		if (len2 > 1.0f) {
			float const inv{ 1.0f / std::sqrt(len2) };
			return Lumina::Vec2{ v_.x * inv, v_.y * inv };
		}
		return v_;
	}
}

namespace Game {
	//====	======	======	======	======	====//
	//	初期化									//
	//====	======	======	======	======	====//

	void Fighter::Initialize(Lumina::Vec3 const& initPos_) {
		HaltAbility();
		Combo_.Reset();

		Position_ = { initPos_.x, initPos_.y, initPos_.z };
		Velocity_ = {};

		Angle_ = Setup_.InitialAngle;
		ModelRotate_ = { 0.0f, -Angle_, 0.0f };
		ModelScale_ = { Setup_.ModelScale, Setup_.ModelScale, Setup_.ModelScale };
		ModelTranslate_ = Position_;

		FloatTimer_ = 0.0f;

		SpeedByInput_ = 0.0f;
		Flag_Jump_ = 0;
		JumpSpeed_ = 0.0f;
		SecondJumpInputInterval_ = 0;
		Trigger_Jump_ = 0;

		SpeedByDash_ = 0.0f;
		DashStatus_ = -1;
		DashTimer_ = 0;
		Trigger_Dash_ = 0;
		DashDir_ = Lumina::Vec2{ std::cos(Angle_), std::sin(Angle_) };
		DashDirNow_ = DashDir_;

		ForceJump_ = false;
		ForceDash_ = false;

		SpeedByKnockBack_ = 0.0f;
		KnockFrames_ = 0;
		IsBeingKnockedBack_ = 0;
		IsReturningToCenter_ = 0;

		InputLocked_ = false;
		Guarding_ = false;
		GuardStun_ = false;
		GuardHeldPrev_ = false;
		DeflectArmed_ = false;
		SinceGuardPress_ = NeverPressed;
		ComboStepStarted_ = false;
		AbilityStarted_ = false;

		MaxHP_ = Setup_.MaxHP;
		Inv_MaxHP_ = 1.0f / MaxHP_;
		HP_ = MaxHP_;

		InitializeColliders();
	}

	void Fighter::InitializeColliders() {
		// 体: Neki のメッシュ空間（バインドポーズで y -0.18〜2.84、z -0.31〜0.22。既定の x は腕を除いた胴の幅）
		RefreshBodyShape();
		BodyCollider_.Layer = Setup_.BodyLayer;
		BodyCollider_.Mask = Setup_.BodyMask;
		BodyCollider_.Owner = this;

		// 武器: Kinoko.obj のメッシュ空間（x -3.96〜3.90、y 0〜5.70、z -3.99〜3.91）。攻撃中だけ有効にする
		WeaponCollider_.Local = Lumina::Phys::OBB::FromCenterHalf({ 0.0f, 2.85f, 0.0f }, { 3.95f, 2.85f, 3.95f });
		WeaponCollider_.Layer = Setup_.WeaponLayer;
		WeaponCollider_.Mask = Setup_.WeaponMask;
		WeaponCollider_.Owner = this;
		WeaponCollider_.Enabled = false;
	}

	//====	======	======	======	======	====//
	//	アビリティ								//
	//====	======	======	======	======	====//

	void Fighter::SetAbility(AbilityGroup group_, std::size_t slot_, Ability* ability_) noexcept {
		auto const g{ static_cast<std::size_t>(group_) };
		if (g >= Slots_.size() || slot_ >= MaxAbilitySlots) { return; }
		if (Slots_[g][slot_] == Active_ && Active_ != nullptr && ability_ != Active_) { HaltAbility(); }	// 使用中のものを外すなら止める
		Slots_[g][slot_] = ability_;
	}

	auto Fighter::AbilityAt(AbilityGroup group_, std::size_t slot_) const noexcept -> Ability* {
		auto const g{ static_cast<std::size_t>(group_) };
		if (g >= Slots_.size() || slot_ >= MaxAbilitySlots) { return nullptr; }
		return Slots_[g][slot_];
	}

	void Fighter::HaltAbility() noexcept {
		if (Active_ == nullptr) { return; }
		Ability* const a{ Active_ };
		Active_ = nullptr;		// 先に外す（Halt の中から状態を問い合わせても使用中に見えないように）
		a->Halt(*this);
	}

	// request_ があればそれを（Cancel 窓から抜けたもの）、無ければ先行入力の Tool / UseItem のうち、いちばん新しいものを試す。
	// 使えないもの（枠が空・CanStart が false）は先行入力に残す（寿命で消える）
	auto Fighter::TryStartAbility(FighterFrame const& frame_, std::optional<Lumina::Input::BufferedInput> request_) -> bool {
		Lumina::Input::InputBuffer& buffer{ frame_.Buffer ? *frame_.Buffer : NoInput_ };
		bool const fromBuffer{ !request_.has_value() };
		if (fromBuffer) { request_ = buffer.PeekEntry(AbilityActions); }
		if (!request_) { return false; }

		auto const group{ GroupOf(request_->Action) };
		if (!group) { return false; }
		Ability* const a{ AbilityAt(*group, request_->Arg) };
		if (a == nullptr || !a->CanStart(*this)) { return false; }
		if (fromBuffer) { buffer.TakeEntry(AbilityActions); }

		++ActionSerial_;
		AbilityContext ctx{ *this, frame_ };
		if (a->Start(ctx)) {
			Active_ = a;
			AbilityStarted_ = true;
		}
		return true;
	}

	//====	======	======	======	======	====//
	//	毎フレーム								//
	//====	======	======	======	======	====//

	void Fighter::Update(FighterFrame const& frame_) {
		Trigger_Jump_ = 0;
		Trigger_Dash_ = 0;
		ComboStepStarted_ = false;
		AbilityStarted_ = false;

		Lumina::Input::InputBuffer& buffer{ frame_.Buffer ? *frame_.Buffer : NoInput_ };
		bool const stunned{ InputLocked_ || IsReturningToCenter_ != 0 || IsBeingKnockedBack_ != 0 };
		bool const canAct{ frame_.ControlEnabled && !stunned };
		bool const canAttack{ canAct && frame_.CanAttack };
		Lumina::Vec2 const move{ ClampLength(frame_.Intent.Move) };
		bool const hasMove{ move.x != 0.0f || move.y != 0.0f };

		// ガードを押した瞬間（弾きの猶予の起点）。攻撃中・のけぞり中に押しても数える（ガードできるようになった時点で猶予が残っていれば弾ける）。
		// 前の押下から DeflectRepressLockout フレーム以内の押し直しには猶予を付けない（連打対策）
		{
			bool const held{ frame_.Intent.GuardHeld && frame_.ControlEnabled && !InputLocked_ };
			if (held && !GuardHeldPrev_) {
				DeflectArmed_ = SinceGuardPress_ >= Setup_.DeflectRepressLockout;
				SinceGuardPress_ = 0;
			}
			else if (SinceGuardPress_ < NeverPressed) {
				++SinceGuardPress_;
			}
			GuardHeldPrev_ = held;
		}

		// 1・2. 使用中のアビリティ。動けなくなったら止める
		if (Active_ != nullptr) {
			if (!canAttack) {
				HaltAbility();
			}
			else {
				AbilityContext ctx{ *this, frame_ };
				if (!Active_->Update(ctx)) { Active_ = nullptr; }
			}
		}

		// 3. コンボ。アビリティの使用中は進めない
		std::optional<Lumina::Input::BufferedInput> cancelledAbility{};
		if (Active_ == nullptr) {
			Game::PlayerCombo::Context const ctx{
				.CanAct = canAttack,
				.WindowAllow = frame_.WindowAllow,
				.ClipFinished = frame_.ClipFinished,
				.MoveInput = hasMove,
			};
			Combo_.Update(ctx, buffer);
			if (Combo_.StepStarted()) {
				// 振り始めだけ、移動の入力の方向へすぐ向き直る（以前の Player::FaceMoveInput）
				if (hasMove) { Angle_ += WrapPi(std::atan2(move.y, move.x) - Angle_); }
				ComboStepStarted_ = true;
				++ActionSerial_;
			}
			if (auto const a{ Combo_.CancelledInto() }) {
				if (*a == Act::Jump) { ForceJump_ = true; }
				else if (*a == Act::Dodge) { ForceDash_ = true; }
				else if (*a == Act::Tool || *a == Act::UseItem) {
					cancelledAbility = Lumina::Input::BufferedInput{ .Action = Lumina::Input::ToActionId(*a), .Age = 0U, .Arg = Combo_.CancelledArg() };
				}
			}
		}

		// 4. 何もしていなければ、アビリティ → ダッシュ → ジャンプ → 移動・ガード
		bool canMove{ canAct && !Combo_.Attacking() && Active_ == nullptr };
		if (canMove && canAttack) {
			TryStartAbility(frame_, cancelledAbility);
			if (Active_ != nullptr) { canMove = false; }
		}

		// ガード硬直の間は、離していてもガードの姿勢のまま（その間の押し直しでも弾ける）
		Guarding_ = (canMove && frame_.Intent.GuardHeld && DashStatus_ != 2 && Flag_Jump_ == 0) || IsGuardStunned();

		if (InputLocked_) {
			// 入力ロック中（ゲームオーバー）：横の動き・ダッシュ・吹き飛ばしを止める（ジャンプ中なら下で落ちる）
			Velocity_ = {};
			SpeedByInput_ = 0.0f;
			SpeedByDash_ = 0.0f;
			DashStatus_ = -1;
			SpeedByKnockBack_ = 0.0f;
			IsBeingKnockedBack_ = 0;
			IsReturningToCenter_ = 0;
		}
		else if (IsReturningToCenter_) {
			ReturnToCenter();
		}
		else if (IsBeingKnockedBack_) {
			Position_.x += SpeedByKnockBack_ * KnockBackDir_.x;
			Position_.z += SpeedByKnockBack_ * KnockBackDir_.y;
		}
		else {
			Dash(frame_, canMove);
			Move(frame_, canMove);
		}
		ForceJump_ = false;		// Cancel 窓から抜けたジャンプ・ダッシュはこのフレームだけ
		ForceDash_ = false;

		Position_.y += JumpSpeed_;
		JumpSpeed_ -= Setup_.Gravity;

		if (Position_.y <= 0.0f) {
			Position_.y = 0.0f;
			JumpSpeed_ = 0.0f;
			Flag_Jump_ = 0;
		}

		// のけぞり: 速さを落とし、決まったフレーム数で明ける
		if (IsBeingKnockedBack_) {
			SpeedByKnockBack_ *= std::clamp(Setup_.KnockbackDecay, 0.0f, 0.99f);
			if (--KnockFrames_ <= 0) {
				IsBeingKnockedBack_ = 0;
				SpeedByKnockBack_ = 0.0f;
				GuardStun_ = false;
			}
		}

		Position_.x = std::clamp(Position_.x, Setup_.ArenaMin, Setup_.ArenaMax);
		Position_.z = std::clamp(Position_.z, Setup_.ArenaMin, Setup_.ArenaMax);

		ModelTranslate_ = Position_;
		ModelRotate_.y = ModelRotate_.y * (1.0f - Setup_.VisualTurnBlend) + (-Angle_) * Setup_.VisualTurnBlend;

		ModelTranslate_.y += Setup_.ModelBaseY + std::sin(FloatTimer_ * Setup_.FloatAngularFrequency) * Setup_.FloatAmplitude;
		if (!InputLocked_) {		// ロック中は止める（倒れた体が上下に揺れないように）
			FloatTimer_ += 1.0f;
		}

		if (HP_ < 0.0f) { HP_ = 0.0f; }
	}

	void Fighter::SetMaxHP(float maxHP_) noexcept {
		float const next{ std::max(maxHP_, 1.0f) };
		float const ratio{ (MaxHP_ > 0.0f) ? (HP_ / MaxHP_) : 1.0f };
		Setup_.MaxHP = next;
		MaxHP_ = next;
		Inv_MaxHP_ = 1.0f / next;
		HP_ = next * std::clamp(ratio, 0.0f, 1.0f);
	}

	void Fighter::RefreshBodyShape() noexcept {
		BodyCollider_.Local = Lumina::Phys::OBB::FromCenterHalf(Setup_.BodyCenter, Setup_.BodyHalf);
	}

	void SeparateBodies(Fighter& a_, Fighter& b_, float aShare_) noexcept {
		Lumina::Vec3 const pa{ a_.WorldPosition() };
		Lumina::Vec3 const pb{ b_.WorldPosition() };
		float dx{ pa.x - pb.x };
		float dz{ pa.z - pb.z };
		float const minDist{ a_.Setup().PushRadius + b_.Setup().PushRadius };
		float const d2{ dx * dx + dz * dz };
		if (d2 >= minDist * minDist) { return; }
		float d{ std::sqrt(d2) };
		if (d < 1.0e-4f) {
			// 真上に重なっている: a_ を向きの反対へ
			dx = -std::cos(a_.Angle());
			dz = -std::sin(a_.Angle());
			d = 1.0f;
		}
		float const push{ minDist - std::sqrt(d2) };
		float const nx{ dx / d };
		float const nz{ dz / d };
		float const share{ std::clamp(aShare_, 0.0f, 1.0f) };
		a_.Translate(nx * push * share, nz * push * share);
		b_.Translate(-nx * push * (1.0f - share), -nz * push * (1.0f - share));
	}

	void Fighter::SnapModel() noexcept {
		ModelTranslate_ = Position_;
		ModelTranslate_.y += Setup_.ModelBaseY + std::sin(FloatTimer_ * Setup_.FloatAngularFrequency) * Setup_.FloatAmplitude;
		ModelRotate_.y = -Angle_;
	}

	// ダッシュ（Dodge）。仮。セキロの回避に作り直すまで。
	// 向きの指定（Intent::Face）が無ければ、向いている方向へ（走りながら曲げられる。以前の Player と同じ）。
	// あれば、始めたときの移動の入力の向き（無ければ向いている方向）に固定（敵のバックステップなど）
	void Fighter::Dash(FighterFrame const& frame_, bool canMove_) {
		if (DashStatus_ == 2) {
			if (DashTimer_ > Setup_.DashFrames) {
				DashStatus_ = 0;
				SpeedByDash_ = 0.0f;
				DashTimer_ = 0;
			}
			else {
				SpeedByDash_ *= Setup_.DashDecay;
				++DashTimer_;
			}
			return;
		}
		if (!canMove_) { return; }

		// 先行入力からは Dodge だけを消す（同じフレームに押したジャンプも出せるように。Take だと同時に押したものも捨てる）
		Lumina::Input::InputBuffer& buffer{ frame_.Buffer ? *frame_.Buffer : NoInput_ };
		if (ForceDash_ || buffer.Peek(MaskOf(Act::Dodge))) {
			buffer.Drop(MaskOf(Act::Dodge));
			Lumina::Vec2 const move{ ClampLength(frame_.Intent.Move) };
			bool const hasMove{ move.x != 0.0f || move.y != 0.0f };
			DashDir_ = hasMove ? move.Unit() : Lumina::Vec2{ std::cos(Angle_), std::sin(Angle_) };
			Trigger_Dash_ = 1;
			DashStatus_ = 2;
			SpeedByDash_ = Setup_.DashSpeed;
			DashTimer_ = 0;
			++ActionSerial_;
		}
	}

	// 移動・向き・ジャンプ。canMove_ が false の間（STARTUP、攻撃中、アビリティの使用中）は、どれも始めない
	void Fighter::Move(FighterFrame const& frame_, bool canMove_) {
		Velocity_ = {};

		FighterIntent const& intent{ frame_.Intent };
		Lumina::Vec2 const move{ ClampLength(intent.Move) };
		float const amount{ std::sqrt(move.x * move.x + move.y * move.y) };
		float const maxTurn{ (Setup_.TurnRate > 0.0f) ? Setup_.TurnRate : 4.0f * Pi };	// 0 以下ならすぐ向く

		SpeedByInput_ = 0.0f;
		Lumina::Vec2 walkDir{ std::cos(Angle_), std::sin(Angle_) };
		if (canMove_) {
			if (intent.Face) {
				// 向きの指定あり（横歩き）: 指定の向きへ回り、移動は入力の向きそのまま
				Lumina::Vec2 const f{ *intent.Face };
				if (f.x != 0.0f || f.y != 0.0f) { TurnToward(std::atan2(f.y, f.x), maxTurn); }
				if (amount > 0.0f) { walkDir = Lumina::Vec2{ move.x / amount, move.y / amount }; }
			}
			else if (amount > 0.0f && Guarding_) {
				// ガード中は向きを変えずに歩く（セキロでロックオンせずにガードしたときと同じ。後ずさりしても前を守れる）
				walkDir = Lumina::Vec2{ move.x / amount, move.y / amount };
			}
			else if (amount > 0.0f) {
				// 進む方向へ向く（見た目の向きは Update の ModelRotate_ がなめらかに追う）。移動は向いている方向へ
				TurnToward(std::atan2(move.y, move.x), maxTurn);
				walkDir = Lumina::Vec2{ std::cos(Angle_), std::sin(Angle_) };
			}
			if (amount > 0.0f) {
				SpeedByInput_ = Setup_.MoveSpeed * amount * (Guarding_ ? Setup_.GuardMoveScale : 1.0f);
			}
		}

		// ジャンプ（空中でもう一度で二段。二段目は 1 段目から SecondJumpDelay フレームを超えてから）。
		// 今跳べないときは先行入力から取り出さない（跳べるようになったときに、寿命の間なら出る）。消すのは Jump だけ（ダッシュと同時に押せる）
		Lumina::Input::InputBuffer& buffer{ frame_.Buffer ? *frame_.Buffer : NoInput_ };
		bool const canFirst{ Flag_Jump_ == 0 };
		bool const canSecond{ Flag_Jump_ == 1 && SecondJumpInputInterval_ > Setup_.SecondJumpDelay };
		if (canMove_ && (canFirst || canSecond) && (ForceJump_ || buffer.Peek(MaskOf(Act::Jump)))) {
			buffer.Drop(MaskOf(Act::Jump));
			JumpSpeed_ = Setup_.JumpInitialSpeed * (2.0f + (SpeedByInput_ + SpeedByDash_) * 0.75f);
			if (canFirst) {
				Flag_Jump_ = 1;
				SecondJumpInputInterval_ = 0;
			}
			else {
				Flag_Jump_ = 2;
			}
			Trigger_Jump_ = 1;
			++ActionSerial_;
		}
		else if (Flag_Jump_ == 1) {
			++SecondJumpInputInterval_;
		}

		if (canMove_) {
			// ダッシュの向き: 向きの指定があれば（横歩き中）始めたときの向きのまま。無ければ今向いている方向（以前どおり、走りながら曲げられる）
			Lumina::Vec2 const dashDir{ intent.Face ? DashDir_ : Lumina::Vec2{ std::cos(Angle_), std::sin(Angle_) } };
			DashDirNow_ = dashDir;
			Velocity_ = {
				SpeedByInput_ * walkDir.x + SpeedByDash_ * dashDir.x,
				0.0f,
				SpeedByInput_ * walkDir.y + SpeedByDash_ * dashDir.y,
			};
		}
		Position_.x += Velocity_.x;
		Position_.z += Velocity_.z;
	}

	// 向きを target へ、最大 maxStep_ だけ回す（最短の回り方。Angle_ は折り返さずに差だけ足す。
	// ModelRotate_ の補間が ±π の境目で逆回りしないように）
	void Fighter::TurnToward(float targetAngle_, float maxStep_) noexcept {
		float const diff{ WrapPi(targetAngle_ - Angle_) };
		Angle_ += std::clamp(diff, -maxStep_, maxStep_);
	}

	void Fighter::ReturnToCenter() {
		Position_.x *= 0.95f;
		Position_.z *= 0.95f;

		if (Position_.x * Position_.x + Position_.y * Position_.y + Position_.z * Position_.z < 1.0f) {
			IsBeingKnockedBack_ = 0;
			IsReturningToCenter_ = 0;
		}
	}

	void Fighter::KnockedBack(float power_, Lumina::Vec2 const& dir_, std::int32_t stunFrames_) {
		if (!HitReactionEnabled_) { return; }
		// 速さの和（等比級数）が押す距離になるように、初速 = 距離 × (1 − 減衰)
		float const decay{ std::clamp(Setup_.KnockbackDecay, 0.0f, 0.99f) };
		SpeedByKnockBack_ = std::max(0.0f, power_ * Setup_.KnockbackScale) * (1.0f - decay);
		KnockFrames_ = std::max<std::int32_t>((stunFrames_ < 0) ? Setup_.HitStunFrames : stunFrames_, 1);
		KnockBackDir_ = dir_.Unit();
		GuardStun_ = false;		// ガードの硬直にするなら ReceiveHit が後で立てる
		// ダッシュは打ち切る（以前は止まったまま残り、のけぞりが明けると続きを走っていた）
		if (DashStatus_ == 2) {
			DashStatus_ = 0;
			SpeedByDash_ = 0.0f;
			DashTimer_ = 0;
		}

		if (!std::isfinite(KnockBackDir_.x) || !std::isfinite(KnockBackDir_.y)) {
			IsReturningToCenter_ = 1;
			KnockBackDir_ = {};
		}
		else {
			IsBeingKnockedBack_ = 1;
		}
	}

	auto Fighter::ReceiveHit(HitRequest const& hit_) -> HitResult {
		if (IsInvincible()) { return HitResult::Dodged; }

		Lumina::Vec2 const push{ hit_.PushDir };
		float const len{ std::sqrt(push.x * push.x + push.y * push.y) };
		bool front{ false };
		if (Guarding_ && len > 1.0e-6f) {
			// 攻撃の来た向き = 押す向きの反対。向いている方向との角度が GuardHalfAngle 以内なら前
			float const c{ (std::cos(Angle_) * -push.x + std::sin(Angle_) * -push.y) / len };
			front = c >= std::cos(std::clamp(Setup_.GuardHalfAngle, 0.0f, Pi));
		}

		if (front) {
			if (CanDeflectNow()) { return HitResult::Deflected; }
			HP_ -= std::max(0.0f, hit_.Damage) * std::max(0.0f, Setup_.GuardDamageScale);
			KnockedBack(hit_.Knockback * std::max(0.0f, Setup_.GuardKnockbackScale), push, Setup_.GuardStunFrames);
			GuardStun_ = (IsBeingKnockedBack_ != 0);
			return HitResult::Guarded;
		}

		HP_ -= std::max(0.0f, hit_.Damage);
		if (Active_ == nullptr || !Active_->SuperArmor()) { KnockedBack(hit_.Knockback, push); }	// スーパーアーマー中は押されない
		return HitResult::Hit;
	}

	void Fighter::FaceToward(Lumina::Vec2 const& dir_, float maxStep_) noexcept {
		if (dir_.x == 0.0f && dir_.y == 0.0f) { return; }
		TurnToward(std::atan2(dir_.y, dir_.x), (maxStep_ > 0.0f) ? maxStep_ : 4.0f * Pi);
	}

	void Fighter::Deflected(Lumina::Vec2 const& pushDir_) {
		KnockedBack(Setup_.DeflectedKnockback, pushDir_, Setup_.DeflectedStunFrames);
	}

	//====	======	======	======	======	====//
	//	問い合わせ								//
	//====	======	======	======	======	====//

	auto Fighter::CanDeflectNow() const noexcept -> bool {
		return Guarding_ && DeflectArmed_ && SinceGuardPress_ < Setup_.DeflectFrames;
	}

	auto Fighter::IsInvincible() const noexcept -> bool {
		if (DashStatus_ != 2) { return false; }
		// DashTimer_ は始めたフレームの Update の後で 0、次のフレームの後で 1…（当たり判定は Update の後）
		return DashTimer_ >= Setup_.DodgeInvincibleStart && DashTimer_ < Setup_.DodgeInvincibleStart + Setup_.DodgeInvincibleFrames;
	}

	auto Fighter::IsDashingForward(float minCos_) const noexcept -> bool {
		if (DashStatus_ != 2) { return false; }
		return DashDirNow_.x * std::cos(Angle_) + DashDirNow_.y * std::sin(Angle_) >= minCos_;
	}

	auto Fighter::Action() const noexcept -> FighterAction {
		if (InputLocked_) { return FighterAction::Locked; }
		if (IsGuardStunned()) { return FighterAction::Guard; }	// ガードの硬直（のけぞりではない。BT からはガード中に見える）
		if (IsBeingKnockedBack_ || IsReturningToCenter_) { return FighterAction::Stagger; }
		if (Active_ != nullptr) { return FighterAction::Ability; }
		if (Combo_.Attacking()) { return FighterAction::Attack; }
		if (DashStatus_ == 2) { return FighterAction::Dash; }
		if (Guarding_) { return FighterAction::Guard; }
		if (IsMoving()) { return FighterAction::Move; }
		return FighterAction::Idle;
	}
}
