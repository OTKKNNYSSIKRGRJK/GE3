export module Game.Boss.Kinoko;

import Lumina.DX12;

import Lumina.Math;

namespace Game {
	struct BoingBoingAnimationArgument {
		float Timer{ 0.0f };
	};

	export class Boss_Kinoko {
	public:
		constexpr Lumina::Vec3 const& ModelScale() const noexcept { return ModelScale_; }
		constexpr Lumina::Vec3 const& ModelRotate() const noexcept { return ModelRotate_; }
		constexpr Lumina::Vec3 const& ModelTranslate() const noexcept { return ModelTranslate_; }
		constexpr Lumina::Vec3 const& ModelShearOnXZPlane() const noexcept { return ModelShearOnXZPlane_; }

		constexpr float Angle() const noexcept { return Angle_; }
		constexpr Lumina::Vec3 const& WorldPosition() const noexcept { return Position_; }

		constexpr int32_t IsTriggered_Jump() const noexcept { return Trigger_Jump_; }
		constexpr int32_t IsTriggered_Dash() const noexcept { return Trigger_Dash_; }

		constexpr int32_t IsBeingKnockedBack() const noexcept { return IsBeingKnockedBack_; }

		void Reset(Lumina::Vec3 const& initPos_);

	public:
		void KnockedBack(float power_, Lumina::Vec2 const& dir_);

	private:
		void Idle();
		void Move();
		void Dash();
		void KnockedBack();
		void ReturnToCenter();

		using State = void(Boss_Kinoko::*)();

	public:
		template<typename...ArgTypes>
		void Update(typename ArgTypes const&...args);

	public:
		template<typename...ArgTypes>
		void Initialize(typename ArgTypes const&...args);

	private:
		Lumina::Vec3 Position_{};
		Lumina::Vec3 Velocity_{};

		Lumina::Vec3 ModelScale_{};
		Lumina::Vec3 ModelRotate_{};
		Lumina::Vec3 ModelTranslate_{};
		Lumina::Vec3 ModelShearOnXZPlane_{};

		Lumina::Vec3 RelPos_SelfToPlayer_{};

		float Angle_{};
		float SpeedByMove_{};
		float SpeedByMove_Default_{};
		float SpeedByDash_{};

		int32_t State_{};

		int32_t Trigger_Jump_{};
		int32_t Flag_Jump_{};
		int32_t SecondJumpInputInterval_{};
		int32_t SecondJumpInputIntervalThreshold_{};
		float JumpSpeed_{};
		float JumpInitialSpeed_{};

		int32_t Trigger_Dash_{};
		int32_t DashStatus_{};
		int32_t DashTimer_{};
		int32_t DashMaxDuration_{};
		int32_t DashInputInterval_{};
		int32_t DashInputIntervalThreshold_{};

		BoingBoingAnimationArgument Boing_{};

		float SpeedByKnockBack_{};
		Lumina::Vec2 KnockBackDir_{};
		int32_t IsBeingKnockedBack_{};

		State CurrentState_{ nullptr };
		int32_t FrameCountDown_ToNextState_{};

	public:
		float MaxHP_{};
		float Inv_MaxHP_{};
		float HP_{};
	};
}