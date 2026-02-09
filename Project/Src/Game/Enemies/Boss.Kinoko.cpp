module Game.Boss.Kinoko;

import Lumina.Math;

import Game.Player;

namespace Game {
	namespace {
		enum BOSS_STATE {
			IDLE,
			AIM,
			MOVE,
			JUMP,
			DASH
		};

		constexpr float	XMin = -38.5f;
		constexpr float XMax = 37.0f;
		constexpr float ZMin = -38.5f;
		constexpr float ZMax = 37.0f;
	}

	void Boss_Kinoko::Move() {
		float speedFactor{
			Lumina::Vec3::Dot(RelPos_SelfToPlayer_, RelPos_SelfToPlayer_) * 0.0078125f * 0.25f
		};
		speedFactor = std::clamp(speedFactor, 1.0f, 3.0f);
		speedFactor = 4.0f - speedFactor;
		if (HP_ * Inv_MaxHP_ < 0.8f) { speedFactor += 0.5f; }
		if (HP_ * Inv_MaxHP_ < 0.5f) { speedFactor += 0.5f; }
		if (HP_ * Inv_MaxHP_ < 0.3f) { speedFactor += 0.5f; }
		if (HP_ * Inv_MaxHP_ < 0.1f) { speedFactor += 0.5f; }
		SpeedByMove_ = SpeedByMove_ * 0.95f + SpeedByMove_Default_ * speedFactor * 0.05f;

		auto const relPosUnit{ RelPos_SelfToPlayer_.Unit() };
		Position_.x += SpeedByMove_ * relPosUnit.x;
		Position_.z += SpeedByMove_ * relPosUnit.z;

		ModelTranslate_.y += std::sin(Boing_.Timer * 0.2f) * 0.02f;
	}

	void Boss_Kinoko::Dash() {
		/*Trigger_Dash_ = 0;
		if (DashStatus_ == 0) {
			if (keyboard.IsReleased(KEY::ARROW_UP)) {
				DashStatus_ = 1;
				DashInputInterval_ = 0;
			}
		}
		else if (DashStatus_ == 1) {
			if (keyboard.IsPressed(KEY::ARROW_UP)) {
				if (DashInputInterval_ <= DashInputIntervalThreshold_) {
					Trigger_Dash_ = 1;
					DashStatus_ = 2;
					SpeedByDash_ = 2.0f;
					DashTimer_ = 0;
					DashInputInterval_ = 0;
				}
				else {
					DashStatus_ = -1;
				}
			}
			else {
				++DashInputInterval_;
			}
		}
		else if (DashStatus_ == 2) {
			if (DashTimer_ > DashMaxDuration_) {
				DashStatus_ = 3;
				SpeedByDash_ = 0.0f;
				DashTimer_ = 0;
			}
			else {
				SpeedByDash_ *= 0.85f;
				++DashTimer_;
			}
		}*/
	}

	void Boss_Kinoko::Idle() {
		Position_.y += std::sin(Boing_.Timer * 0.2f) * 0.02f;

		if (FrameCountDown_ToNextState_ <= 0) {
			CurrentState_ = &Boss_Kinoko::Move;
			FrameCountDown_ToNextState_ = (Lumina::Random::Generator()()) % 60 + 120;
		}
	}

	void Boss_Kinoko::KnockedBack() {
		Position_.x += SpeedByKnockBack_ * KnockBackDir_.x;
		Position_.z += SpeedByKnockBack_ * KnockBackDir_.y;

		SpeedByKnockBack_ *= 0.95f;

		if (SpeedByKnockBack_ < 0.1f) {
			IsBeingKnockedBack_ = 0;

			CurrentState_ = &Boss_Kinoko::Idle;
			FrameCountDown_ToNextState_ = (Lumina::Random::Generator()()) % 15 + 15;
		}
	}

	void Boss_Kinoko::ReturnToCenter() {
		Position_.x *= 0.95f;
		Position_.z *= 0.95f;

		if (Lumina::Vec3::Dot(Position_, Position_) < 1.0f) {
			IsBeingKnockedBack_ = 0;

			CurrentState_ = &Boss_Kinoko::Idle;
			FrameCountDown_ToNextState_ = (Lumina::Random::Generator()()) % 30 + 60;
		}
	}

	template<>
	void Boss_Kinoko::Update(Player const& player_) {
		RelPos_SelfToPlayer_ = player_.WorldPosition() - Position_;
		RelPos_SelfToPlayer_.y = 0.0f;
		Angle_ = std::atan2(RelPos_SelfToPlayer_.z, RelPos_SelfToPlayer_.x);

		if (CurrentState_ != nullptr) {
			(this->*CurrentState_)();
		}
		--FrameCountDown_ToNextState_;

		if (!std::isfinite(Position_.x) || !std::isfinite(Position_.z)) {
			Position_.x = player_.WorldPosition().x;
			Position_.z = player_.WorldPosition().z;
			CurrentState_ = &Boss_Kinoko::ReturnToCenter;
		}

		Position_.x = std::clamp(Position_.x, XMin, XMax);
		Position_.z = std::clamp(Position_.z, ZMin, ZMax);

		Position_.y = 0.0f;

		ModelScale_.x = 2.0f + std::sin(Boing_.Timer * 0.12f) * 0.02f;
		ModelScale_.z = 2.0f + std::sin(Boing_.Timer * 0.15f) * 0.02f;
		ModelScale_.y = 2.0f + std::sin(Boing_.Timer * 0.1f) * 0.05f;

		ModelTranslate_.x = Position_.x;
		ModelTranslate_.z = Position_.z;

		ModelRotate_.y = ModelRotate_.y * 0.98f + (-Angle_) * 0.02f;

		static Lumina::PerlinNoise shearNoise{ 0.75f };

		ModelShearOnXZPlane_.x = shearNoise(
			Boing_.Timer * 0.05f,
			Boing_.Timer * 0.05f,
			Boing_.Timer * 0.05f
		) - 0.5f;
		ModelShearOnXZPlane_.x *= 0.1f;
		ModelShearOnXZPlane_.x += std::cos(Boing_.Timer * 0.12f) * 0.1f;
		ModelShearOnXZPlane_.z = shearNoise(
			Boing_.Timer * 0.05f,
			Boing_.Timer * 0.05f + 0.5f,
			Boing_.Timer * 0.05f + 0.5f
		) - 0.5f;
		ModelShearOnXZPlane_.z *= 0.1f;
		ModelShearOnXZPlane_.z += std::sin(Boing_.Timer * 0.12f) * 0.1f;

		Boing_.Timer += 1.0f;

		if (HP_ < 0.0f) { HP_ = 0.0f; }
	}

	template<>
	void Boss_Kinoko::Initialize(
		Lumina::Vec3 const& initPos_
	) {
		Reset(initPos_);
	}

	void Boss_Kinoko::KnockedBack(float power_, Lumina::Vec2 const& dir_) {
		SpeedByKnockBack_ = power_ * 0.1f;
		KnockBackDir_ = dir_.Unit();

		CurrentState_ = &Boss_Kinoko::KnockedBack;
		IsBeingKnockedBack_ = 1;
	}

	void Boss_Kinoko::Reset(
		Lumina::Vec3 const& initPos_
	) {
		State_ = BOSS_STATE::IDLE;
		Position_ = initPos_;

		Angle_ = std::numbers::pi_v<float> *0.5f;
		ModelRotate_ = { 0.0f, -Angle_, 0.0f };
		ModelScale_ = { 1.0f, 1.0f, 1.0f };

		Boing_.Timer = 0.0f;

		Flag_Jump_ = 0;
		JumpSpeed_ = 0.0f;
		JumpInitialSpeed_ = 0.25f;
		SecondJumpInputInterval_ = 0;
		SecondJumpInputIntervalThreshold_ = 15;

		SpeedByDash_ = 0.0f;
		DashStatus_ = -1;
		DashTimer_ = 0;
		DashMaxDuration_ = 15;
		DashInputInterval_ = 0;
		DashInputIntervalThreshold_ = 10;

		SpeedByKnockBack_ = 0.0f;

		CurrentState_ = &Boss_Kinoko::Idle;
		FrameCountDown_ToNextState_ = (Lumina::Random::Generator()()) % 60 + 240;

		SpeedByMove_Default_ = 0.1f;

		MaxHP_ = 25000.0f;
		Inv_MaxHP_ = 1.0f / MaxHP_;
		HP_ = MaxHP_;
	}
}