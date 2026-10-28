export module Game.Player;

import Lumina.DX12;

import Lumina.Math;
import Lumina.Phys.Collider;

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

		inline int32_t IsMoving() const noexcept { return Lumina::Vec3::Dot(Velocity_, Velocity_) > 0.0f; }
		constexpr int32_t IsDashing() const noexcept { return (DashStatus_ == 2); }

		constexpr int32_t IsBeingKnockedBack() const noexcept { return IsBeingKnockedBack_; }

		// 入力を受け付けない状態にする（ゲームオーバー中）。キーは読まず、横の移動・ダッシュ・吹き飛ばしは止まる。
		// 空中なら落ちて着地する（Update は呼び続けること）。Initialize で解除される
		void LockInput(bool locked_) noexcept { InputLocked_ = locked_; }
		constexpr bool IsInputLocked() const noexcept { return InputLocked_; }

		// 巻き戻し（ゲームオーバーのあと、再開の前）：水平の位置と向きを決める。次の Update で ModelTranslate / ModelRotate に反映される
		void SetRewindPose(float x_, float z_, float angle_) noexcept { Position_.x = x_; Position_.z = z_; Angle_ = angle_; }

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
		bool InputLocked_{ false };

	public:
		// コライダ（形状はメッシュ空間で持つ。World の更新は親行列を知っている InGame が行う）
		Lumina::Phys::Collider& BodyCollider() noexcept { return BodyCollider_; }
		Lumina::Phys::Collider const& BodyCollider() const noexcept { return BodyCollider_; }
		Lumina::Phys::Collider& WeaponCollider() noexcept { return WeaponCollider_; }
		Lumina::Phys::Collider const& WeaponCollider() const noexcept { return WeaponCollider_; }

	private:
		void InitializeColliders();

		Lumina::Phys::Collider BodyCollider_{};
		Lumina::Phys::Collider WeaponCollider_{};

	public:
		float MaxHP_;
		float Inv_MaxHP_;
		float HP_;
	};
}