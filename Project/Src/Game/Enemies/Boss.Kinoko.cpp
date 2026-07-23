module Game.Boss.Kinoko;

import <memory>;

import Lumina.Math;
import BehaviorTree;
import Lumina.Utils.Color;

import Game.Player;

import ParticleSystem;

namespace Game {
	namespace {
		enum BOSS_STATE {
			IDLE,
			AIM,
			MOVE,
			JUMP,
			DASH
		};

		struct BossBlackboard : public Lumina::Behavior::Blackboard {
			// ── 観測データ（毎フレーム、ツリー実行前に外部から更新）──
			Lumina::Vec3 bossPos;
			Lumina::Vec3 bossForward;
			Lumina::Vec3 targetPos;      // プレイヤー位置
			float    distanceToTarget;
			float    deltaTime;
			int      bossId;

			// ── 内部状態（ノード自身が読み書きする）──
			float    waveLineCooldown = 0.0f;
			float    circularCooldown = 0.0f;
			int      currentPhase = 1;      // ボスのフェーズ管理
			int      lastUsedSkillId = -1;     // 連続使用防止用

			// ── 実行中スキルの一時状態 ──
			// Running状態を跨いで保持する必要があるものは
			// ノード自身のメンバに持たせるか、ここに専用フィールドを用意する
			bool     isSkillInProgress = false;

			// ── 出力（ツリーからゲーム側への書き出し）──
			ParticleSystem<Particle>* hitboxOutput;

			int isIdling = 0;
		};

		BossBlackboard BBB;

		class Condition_DistanceCheck : public Lumina::Behavior::Leaf {
			using BTStatus = Lumina::Behavior::Status;

		public:
			auto Tick(Lumina::Behavior::Blackboard& bb) -> BTStatus override {
				auto& bossBB{ reinterpret_cast<BossBlackboard&>(bb) };
				bool isNear = bossBB.distanceToTarget < m_threshold;
				bool pass = (isNear == m_wantNear);
				return pass ? BTStatus::Success : BTStatus::Failure;
			}

		public:
			Condition_DistanceCheck(float threshold, bool wantNear)
				: m_threshold(threshold), m_wantNear(wantNear) {
			}

		private:
			float m_threshold;
			bool  m_wantNear;
		};

		class Decorator_Cooldown : public Lumina::Behavior::Decorator {
			using BTStatus = Lumina::Behavior::Status;

		public:
			Decorator_Cooldown(float BossBlackboard::* cooldownField, float duration)
				: m_field(cooldownField), m_duration(duration) {}

			auto Tick(Lumina::Behavior::Blackboard& bb) -> BTStatus override {
				auto& bossBB{ reinterpret_cast<BossBlackboard&>(bb) };

				if (bossBB.*m_field > 0.0f) {
					return BTStatus::Failure; // クールダウン中は不発
				}
				BTStatus result = Child_->Tick(bb);
				if (result == BTStatus::Success) {
					bossBB.*m_field = m_duration; // 成功したらクールダウン開始
				}
				return result;
			}
		private:
			float BossBlackboard::* m_field;
			float m_duration;
		};

		class WaveLineAttack {
		public:
			void Start(const Lumina::Vec3& origin, const Lumina::Vec3& forward, int boss) {
				m_origin = origin;
				m_forward = forward; // 正規化済み前提
				m_ownerId = boss;
				m_timer = 0.0f;
				m_spawnedCount = 0;
			}

			// 毎フレーム呼ぶ
			void Update(float dt, ParticleSystem<Particle>* outHitboxes) {
				m_timer += dt;

				// 一定間隔で1つずつ生成（波が連続して押し寄せる演出）
				while (m_spawnedCount < kSegmentCount &&
					   m_timer >= m_spawnedCount * kSpawnInterval) {
					SpawnSegment(m_spawnedCount, outHitboxes);
					m_spawnedCount++;
				}
			}

			bool IsFinished() const { return m_spawnedCount >= kSegmentCount; }

		private:
			void SpawnSegment(int index, ParticleSystem<Particle>* out) {
				float distance = kSegmentSpacing * index;

				// 前方軸に垂直なベクトルを求めてsin揺らぎに使う
				Lumina::Vec3 right = { -m_forward.z, 0.0f, m_forward.x };

				auto atk{
					[&, this](float offset_, float phaseOffset_) -> void {
						float phase = index * kUndulationFreq + phaseOffset_;
						float lateralOffset = sinf(phase) * kUndulationAmplitude;

						Lumina::Vec3 pos = {
							m_origin.x + m_forward.x * distance + right.x * lateralOffset * 0.25f,
							m_origin.y + lateralOffset,
							m_origin.z + m_forward.z * distance + right.z * lateralOffset * 0.25f
						};

						Particle b{};
						{
							b.Translate = {
								pos.x + right.x * offset_,
								pos.y + 2.0f,
								pos.z + right.z * offset_,
							};

							b.Scale.x = kSegmentRadius;
							b.Scale.y = kSegmentRadius;

							b.Life = kSegmentLifeTime;

							b.RenderData.RGBA = {
								1.0f,
								0.5f,
								0.5f,
								1.0f
							};
							b.RenderData.DiffuseID = 0U;
							b.RenderData.DiffuseAtlasID = 5U;
						}
						out->Emit(std::move(b));
					}
				};

				atk(0.0f, 0.0f);
				atk(0.1f, 0.1f);
				atk(-0.1f, -0.1f);
			}

			static constexpr int   kSegmentCount = 100;
			static constexpr float kSegmentSpacing = 0.05f;
			static constexpr float kSpawnInterval = 0.1f;
			static constexpr float kSegmentRadius = 2.0f;
			static constexpr float kSegmentLifeTime = 30.0f;
			static constexpr float kUndulationFreq = 0.9f;
			static constexpr float kUndulationAmplitude = 0.25f;
			static constexpr float kDamage = 15.0f;

			Lumina::Vec3 m_origin{}, m_forward{};
			int m_ownerId = -1;
			float m_timer = 0.0f;
			int m_spawnedCount = 0;
		};

		class SkillNode_WaveLine : public Lumina::Behavior::Leaf {
			using BTStatus = Lumina::Behavior::Status;

		public:
			auto Tick(Lumina::Behavior::Blackboard& bb) -> BTStatus override {
				auto& bossBB{ reinterpret_cast<BossBlackboard&>(bb) };

				if (!m_started) {
					bossBB.isIdling = 0;
					m_attack.Start(bossBB.bossPos, bossBB.bossForward, bossBB.bossId);
					m_started = true;
				}
				m_attack.Update(bossBB.deltaTime, bossBB.hitboxOutput);
				if (m_attack.IsFinished()) {
					m_started = false;
					return BTStatus::Success;
				}
				return BTStatus::Running;
			}
		private:
			WaveLineAttack m_attack;
			bool m_started = false;
		};

		class CircularAttack {
		public:
			void Start() {
				m_timer = 0.0f;
			}

			// 毎フレーム呼ぶ
			void Update(float dt, Lumina::Vec3 const& pos_, ParticleSystem<Particle>* out) {
				m_timer += dt;

				auto atk{
					[&, this](float rad_) -> void {
						for (int i = 0; i < 24; ++i) {
							Particle b{};
							{
								float theta{ i / 12.0f * std::numbers::pi_v<float> };
								b.Translate = {
									pos_.x + std::cos(theta) * rad_,
									pos_.y,
									pos_.z + std::sin(theta) * rad_,
								};

								b.Velocity.y = rad_ * 0.1f;
								b.Velocity.y *= b.Velocity.y * 0.1f;

								b.Scale.x = 2.0f;
								b.Scale.y = 2.0f;

								b.Life = 10.0f;

								b.RenderData.RGBA = {
									1.0f,
									0.5f,
									0.5f,
									0.5f
								};
								b.RenderData.DiffuseID = 0U;
								b.RenderData.DiffuseAtlasID = 5U;
							}
							out->Emit(std::move(b));
						}
					}
				};

				atk(m_timer * 4.0f);
			}

			bool IsFinished() const { return m_timer >= 2.0f; }

			int m_ownerId = -1;
			float m_timer = 0.0f;
		};

		class SkillNode_CircularBurst : public Lumina::Behavior::Leaf {
			using BTStatus = Lumina::Behavior::Status;

		public:
			auto Tick(Lumina::Behavior::Blackboard& bb) -> BTStatus override {
				auto& bossBB{ reinterpret_cast<BossBlackboard&>(bb) };

				if (!m_started) {
					bossBB.isIdling = 0;
					m_attack.Start();
					m_started = true;
				}
				m_attack.Update(bossBB.deltaTime, bossBB.bossPos, bossBB.hitboxOutput);
				if (m_attack.IsFinished()) {
					m_started = false;
					return BTStatus::Success;
				}
				return BTStatus::Running;
			}
		private:
			CircularAttack m_attack;
			bool m_started = false;
		};

		class ActionNode_Idle : public Lumina::Behavior::Leaf {
			using BTStatus = Lumina::Behavior::Status;

		public:
			auto Tick(Lumina::Behavior::Blackboard& bb) -> BTStatus override {
				auto& bossBB{ reinterpret_cast<BossBlackboard&>(bb) };
				bossBB.isIdling = 1;
				return BTStatus::Running;
			}
		};
	}

	auto Boss_Kinoko::BuildBT(void* bossParticleSystem_) -> void {
		using namespace Lumina::Behavior;
		BTRoot_ = std::make_unique<Selector>();

		// 円形範囲攻撃の枝
		auto circularSeq{ std::make_unique<Sequence>() };
		circularSeq->AddChild(std::make_unique<Condition_DistanceCheck>(10.0f, true));
		{
			auto cd{ std::make_unique<Decorator_Cooldown>(&BossBlackboard::circularCooldown, 8.0f) };
			cd->SetChild(std::make_unique<SkillNode_CircularBurst>());
			circularSeq->AddChild(std::move(cd));
		}
		BTRoot_->AddChild(std::move(circularSeq));

		// 波濤攻撃の枝
		auto waveSeq = std::make_unique<Sequence>();
		waveSeq->AddChild(std::make_unique<Condition_DistanceCheck>(10.0f, false));
		{
			auto cd = std::make_unique<Decorator_Cooldown>(&BossBlackboard::waveLineCooldown, 6.0f);
			cd->SetChild(std::make_unique<SkillNode_WaveLine>());
			waveSeq->AddChild(std::move(cd));
		}
		BTRoot_->AddChild(std::move(waveSeq));

		// フォールバック（Idle）
		BTRoot_->AddChild(std::make_unique<ActionNode_Idle>());

		BBB.hitboxOutput = static_cast<ParticleSystem<Particle>*>(bossParticleSystem_);
	}

	void Boss_Kinoko::Move() {
		if (!BBB.isIdling) { return; }

		float speedFactor{
			Lumina::Vec3::Dot(RelPos_SelfToPlayer_, RelPos_SelfToPlayer_) * 0.0078125f * 0.25f
		};
		speedFactor = std::clamp(speedFactor, 1.0f, 1.5f);
		speedFactor = 2.0f - speedFactor;
		if (HP_ * Inv_MaxHP_ < 0.5f) { speedFactor += 0.5f; }
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
		if (BBB.isIdling) {
			Position_.y += std::sin(Boing_.Timer * 0.2f) * 0.02f;

			if (FrameCountDown_ToNextState_ <= 0) {
				CurrentState_ = &Boss_Kinoko::Move;
				FrameCountDown_ToNextState_ = (Lumina::Random::Generator()()) % 60 + 120;
			}
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

		Position_.x = std::clamp(Position_.x, -38.5f, 37.0f);
		Position_.z = std::clamp(Position_.z, -38.5f, 37.0f);

		Position_.y = 0.0f;

		ModelScale_.x = 1.5f + std::sin(Boing_.Timer * 0.12f) * 0.02f;
		ModelScale_.z = 1.5f + std::sin(Boing_.Timer * 0.15f) * 0.02f;
		ModelScale_.y = 1.5f + std::sin(Boing_.Timer * 0.1f) * 0.05f;

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

		static float dt = 0.0667f;

		BBB.bossPos = Position_;
		BBB.bossForward = { RelPos_SelfToPlayer_.x, RelPos_SelfToPlayer_.y, RelPos_SelfToPlayer_.z };
		BBB.distanceToTarget = RelPos_SelfToPlayer_.Norm();
		BBB.deltaTime = dt;
		BBB.waveLineCooldown -= dt;
		BBB.circularCooldown -= dt;

		BTRoot_->Tick(BBB);
	}

	template<>
	void Boss_Kinoko::Initialize(
		Lumina::Vec3 const& initPos_
	) {
		State_ = BOSS_STATE::IDLE;
		Position_ = initPos_;

		Angle_ = std::numbers::pi_v<float> * 0.5f;
		ModelRotate_ = { 0.0f, -Angle_, 0.0f };
		ModelScale_ = { 1.0f, 1.0f, 1.0f };
		ModelTranslate_.y = 0.0f;

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