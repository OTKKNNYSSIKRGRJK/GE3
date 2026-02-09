export module Game.Player;

import Lumina.DX12;

import Lumina.Math;

namespace Game {
	struct FloatingAnimationArgument {
		float Timer{ 0.0f };
		float Amplitude{ 0.1f };
		float AngularFrequency{ 0.1f };
	};

	export class Player {
	public:
		constexpr Lumina::Vec3 const& ModelScale() const noexcept { return ModelScale_; }
		constexpr Lumina::Vec3 const& ModelRotate() const noexcept { return ModelRotate_; }
		constexpr Lumina::Vec3 const& ModelTranslate() const noexcept { return ModelTranslate_; }

		constexpr float Angle() const noexcept { return Angle_; }
		constexpr Lumina::Vec3 const& WorldPosition() const noexcept { return Position_; }

		constexpr int32_t IsTriggered_Jump() const noexcept { return Trigger_Jump_; }
		constexpr int32_t IsTriggered_Dash() const noexcept { return Trigger_Dash_; }

		constexpr int32_t IsDashing() const noexcept { return (DashStatus_ == 2); }

		constexpr int32_t IsBeingKnockedBack() const noexcept { return IsBeingKnockedBack_; }

		void Reset();

	public:
		void KnockedBack(float power_, Lumina::Vec2 const& dir_);

	private:
		template<typename...ArgTypes>
		void Move(typename ArgTypes const&...args);
		template<typename...ArgTypes>
		void Dash(typename ArgTypes const&...args);
		void ReturnToCenter();

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
		
		float Angle_{};
		float SpeedByInput_{};
		float SpeedByDash_{};

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

		FloatingAnimationArgument Floating_{};

		float SpeedByKnockBack_{};
		Lumina::Vec2 KnockBackDir_{};
		int32_t IsBeingKnockedBack_{};
		int32_t IsReturningToCenter_{};

	public:
		constexpr float MaxHP() const noexcept { return MaxHP_; }
		constexpr float Inv_MaxHP() const noexcept { return Inv_MaxHP_; }
		constexpr float HP() const noexcept { return HP_; }
		constexpr void HP(float hp_) noexcept { HP_ = hp_; }

	private:
		float MaxHP_;
		float Inv_MaxHP_;
		float HP_;
	};
}