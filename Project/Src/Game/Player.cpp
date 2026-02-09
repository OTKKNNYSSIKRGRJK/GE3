module Game.Player;

import Lumina.WinApp.RawInput;

import Lumina.Math;

//import Lumina.Utils.ImGui;

namespace {
	using Lumina::WinApp::KEY;
}

namespace Game {
	template<>
	void Player::Move(
		Lumina::WinApp::RawInput const& input_,
		bool const& canMove_
	) {
		auto const& keyboard{ input_.Keyboard() };

		Velocity_ = {};

		SpeedByInput_ = 0.0f;
		if (keyboard.IsPressed(KEY::ARROW_UP)) {
			SpeedByInput_ = 0.25f;
		}
		if (keyboard.IsPressed(KEY::ARROW_DOWN)) {
			SpeedByInput_ = -0.125f;
		}
		if (keyboard.IsPressed(KEY::ARROW_LEFT)) {
			Angle_ += 0.1f;
		}
		if (keyboard.IsPressed(KEY::ARROW_RIGHT)) {
			Angle_ -= 0.1f;
		}

		Trigger_Jump_ = 0;
		if (Flag_Jump_ == 0) {
			if (keyboard.IsPressed(KEY::SPACE)) {
				JumpSpeed_ = JumpInitialSpeed_ * (2.0f + (SpeedByInput_ + SpeedByDash_) * 0.75f);
				Flag_Jump_ = 1;
				Trigger_Jump_ = 1;
				SecondJumpInputInterval_ = 0;
			}
		}
		else if (Flag_Jump_ == 1) {
			if (keyboard.IsPressed(KEY::SPACE)) {
				if (SecondJumpInputInterval_ > SecondJumpInputIntervalThreshold_) {
					JumpSpeed_ = JumpInitialSpeed_ * (2.0f + (SpeedByInput_ + SpeedByDash_) * 0.75f);
					Flag_Jump_ = 2;
					Trigger_Jump_ = 1;
				}
			}
			++SecondJumpInputInterval_;
		}

		Velocity_ = Lumina::Vec3{
			(SpeedByInput_ + SpeedByDash_) * std::cos(Angle_) * canMove_,
			0.0f,
			(SpeedByInput_ + SpeedByDash_) * std::sin(Angle_) * canMove_
		};
		Position_ += Velocity_;
	}

	template<>
	void Player::Dash(
		Lumina::WinApp::RawInput const& input_
	) {
		auto const& keyboard{ input_.Keyboard() };

		Trigger_Dash_ = 0;
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
		}
		else if (DashStatus_ == 3) {
			if (keyboard.IsReleased(KEY::ARROW_UP)) {
				DashStatus_ = -1;
			}
		}
		else if (DashStatus_ == -1) {
			if (keyboard.IsPressed(KEY::ARROW_UP)) {
				DashStatus_ = 0;
			}
		}

		/*ImGui::Begin("Player");
		ImGui::Text("DashInputInterval = %d", DashInputInterval_);
		ImGui::Text("DashTimer = %d", DashTimer_);
		ImGui::Text("DashStatus = %d", DashStatus_);
		ImGui::End();*/
	}

	void Player::ReturnToCenter() {
		Position_.x *= 0.95f;
		Position_.z *= 0.95f;

		if (Lumina::Vec3::Dot(Position_, Position_) < 1.0f) {
			IsBeingKnockedBack_ = 0;
			IsReturningToCenter_ = 0;
		}
	}

	template<>
	void Player::Update(
		Lumina::WinApp::RawInput const& input_,
		bool const& canMove_
	) {
		if (IsReturningToCenter_) {
			ReturnToCenter();
		}
		else if (IsBeingKnockedBack_) {
			Position_.x += SpeedByKnockBack_ * KnockBackDir_.x;
			Position_.z += SpeedByKnockBack_ * KnockBackDir_.y;
		}
		else {
			Dash(input_);
			Move(input_, canMove_);
		}

		Position_.y += JumpSpeed_;
		JumpSpeed_ -= 0.02f;

		if (Position_.y <= 0.0f) {
			Position_.y = 0.0f;
			JumpSpeed_ = 0.0f;
			Flag_Jump_ = 0;
		}

		SpeedByKnockBack_ *= 0.95f;

		if (SpeedByKnockBack_ < 0.1f) {
			IsBeingKnockedBack_ = 0;
		}

		Position_.x = std::clamp(Position_.x, -38.5f, 37.0f);
		Position_.z = std::clamp(Position_.z, -38.5f, 37.0f);

		ModelTranslate_ = Position_;
		ModelRotate_.y = ModelRotate_.y * 0.85f + (-Angle_) * 0.15f;

		ModelTranslate_.y += 1.25f + std::sin(Floating_.Timer * Floating_.AngularFrequency) * Floating_.Amplitude;
		Floating_.Timer += 1.0f;

		if (HP_ < 0.0f) { HP_ = 0.0f; }

		/*ImGui::Begin("Player");
		ImGui::PushID("Model");
		ImGui::DragFloat3("Scale", ModelScale_(), 0.01f);
		ImGui::DragFloat3("Rotate", ModelRotate_(), 0.01f);
		ImGui::DragFloat3("Translate", ModelTranslate_(), 0.01f);
		ImGui::DragFloat("Jump Init Speed", &JumpInitialSpeed_, 0.01f, 0.0f);
		ImGui::Text("Angle = %f", Angle_);
		ImGui::PopID();
		ImGui::End();*/
	}

	template<>
	void Player::Initialize(
		Lumina::Vec3 const& initPos_
	) {
		Position_ = initPos_;

		Angle_ = std::numbers::pi_v<float> * 0.5f;
		ModelRotate_ = { 0.0f, -Angle_, 0.0f };
		ModelScale_ = { 1.0f, 1.0f, 1.0f };

		Floating_.Timer = 0.0f;
		Floating_.Amplitude = 0.1f;
		Floating_.AngularFrequency = 0.1f;

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

		IsBeingKnockedBack_ = 0;
		IsReturningToCenter_ = 0;
		
		MaxHP_ = 150.0f;
		Inv_MaxHP_ = 1.0f / MaxHP_;
		HP_ = MaxHP_;
	}

	void Player::KnockedBack(float power_, Lumina::Vec2 const& dir_) {
		SpeedByKnockBack_ = power_ * 0.2f;
		KnockBackDir_ = dir_.Unit();

		if (!std::isfinite(KnockBackDir_.x) || !std::isfinite(KnockBackDir_.y)) {
			IsReturningToCenter_ = 1;
			KnockBackDir_ = {};
		}
		else {
			IsBeingKnockedBack_ = 1;
		}
	}
}