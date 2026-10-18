module Game.Scene.InGame;

import <string>;
import <optional>;
import <variant>;

import : Impl;

import Lumina;

import Game.CollisionLayer;
import Game.AnimEventDemo;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

namespace Game::Scene::Impl {
	namespace {
		Lumina::Mat4 LookAt(
			Lumina::Vec3 const& src_,
			Lumina::Vec3 const& dst_,
			Lumina::Vec3 const& up_
		) {
			Lumina::Vec3 const forward{ Lumina::Vec3{ dst_ - src_ }.Unit() };
			Lumina::Vec3 const right{ Lumina::Vec3::Cross(up_, forward).Unit() };
			Lumina::Vec3 const up{ Lumina::Vec3::Cross(forward, right) };

			return {
				right.x,
				up.x,
				forward.x,
				0.0f,

				right.y,
				up.y,
				forward.y,
				0.0f,

				right.z,
				up.z,
				forward.z,
				0.0f,

				-Lumina::Vec3::Dot(src_, right),
				-Lumina::Vec3::Dot(src_, up),
				-Lumina::Vec3::Dot(src_, forward),
				1.0f,
			};
		}

		Lumina::Mat4 RotateAround(
			Lumina::Vec3 const& axis_,
			float theta_
		) {
			float const cosTheta{ std::cos(theta_) };
			float const sinTheta{ std::sin(theta_) };
			return {
				axis_.x * axis_.x * (1.0f - cosTheta) + cosTheta,
				axis_.x * axis_.y * (1.0f - cosTheta) + axis_.z * sinTheta,
				axis_.x * axis_.z * (1.0f - cosTheta) - axis_.y * sinTheta,
				0.0f,

				axis_.y * axis_.x * (1.0f - cosTheta) - axis_.z * sinTheta,
				axis_.y * axis_.y * (1.0f - cosTheta) + cosTheta,
				axis_.y * axis_.z * (1.0f - cosTheta) + axis_.x * sinTheta,
				0.0f,

				axis_.z * axis_.x * (1.0f - cosTheta) + axis_.y * sinTheta,
				axis_.z * axis_.y * (1.0f - cosTheta) - axis_.x * sinTheta,
				axis_.z * axis_.z * (1.0f - cosTheta) + cosTheta,
				0.0f,

				0.0f, 0.0f, 0.0f, 1.0f,
			};
		}

		float hitBoss = 0.0f;
	}

	namespace {
		constexpr float Inv_0xFFFFFFFF{ 1.0f / static_cast<float>(0xFFFFFFFFU) };
		constexpr float Inv_900{ 1.0f / 900.0f };

		void UpdatePlayerCamera(TrackingCamera& camera_, Player const& player_) {
			camera_.Target = player_.WorldPosition();
			camera_.Position =
				camera_.Position * (1.0f - camera_.DelayFactor) +
				camera_.Target * camera_.DelayFactor;
			camera_.Position.y += camera_.Target.y * 0.175f;

			if (hitBoss > 0.05f) {
				camera_.Position.x += std::cos(hitBoss * 15.0f) * hitBoss * 3.0f;
				camera_.Position.y += std::cos(hitBoss * 20.0f) * hitBoss * 3.0f;
				camera_.Position.z += std::cos(hitBoss * 25.0f) * hitBoss * 3.0f;
			}

			Lumina::Vec4 cameraTranslateSwayingOffset{
				Lumina::Vec4{ camera_.Position } +
				Lumina::Vec4{ camera_.OffsetFromTarget } *
				RotateAround(
					camera_.Up,
					(-player_.Angle()) + std::numbers::pi_v<float> *0.5f +
					camera_.SwayAmpFactor * std::sin(camera_.SwayTimeFactor)
				)
			};
			camera_.SwayTimeFactor += camera_.SwayFreqFactor;

			camera_.View = LookAt(
				cameraTranslateSwayingOffset(),
				camera_.Target,
				camera_.Up
			);
			camera_.VP = camera_.View * camera_.Projection;
			camera_.VPUploadBuffer.Store(&camera_.VP, sizeof(Lumina::Mat4), 0LLU);
		}
	}

	namespace {
		bool UpdateAmbientSparkle(Particle& p_, void const*) {
			p_.Translate.x += (Lumina::Random::Generator()() * Inv_0xFFFFFFFF - 0.5f) * 0.05f;
			p_.Translate.y += Lumina::Random::Generator()() * Inv_0xFFFFFFFF * 0.05f;
			p_.Translate.z += (Lumina::Random::Generator()() * Inv_0xFFFFFFFF - 0.5f) * 0.025f;
			p_.RenderData.RGBA.w = (1.0f - (p_.Life - 30.0f) * (p_.Life - 30.0f) * Inv_900) * 0.1f;
			p_.Scale.x = (1.0f - (p_.Life - 30.0f) * (p_.Life - 30.0f) * Inv_900) * 1.5f;
			p_.Scale.y = (1.0f - (p_.Life - 30.0f) * (p_.Life - 30.0f) * Inv_900) * 1.5f;
			p_.Life -= 0.5f;
			return (p_.Life > 0.0f);
		}
		bool UpdatePlayerEffect(Particle& p_, void const*) {
			p_.Translate.x += p_.Velocity.x;
			p_.Translate.z += p_.Velocity.z;
			p_.RenderData.RGBA.w *= 0.95f;
			p_.Scale.x *= 0.93f;
			p_.Scale.y *= 0.93f;
			p_.Rotate.z += p_.Velocity.z * 0.01f;
			p_.Life -= 1.0f;
			return (p_.Life > 0.0f);
		}
		bool UpdateKnockEffect(Particle& p_, void const*) {
			p_.Translate.x += p_.Velocity.x;
			p_.Translate.z += p_.Velocity.z;
			p_.Scale.x *= 0.85f;
			p_.Scale.y *= 0.85f;
			p_.Rotate.x += p_.Velocity.x * 0.1f;
			p_.Rotate.y += p_.Velocity.y * 0.1f;
			p_.Rotate.z += p_.Velocity.z * 0.1f;
			p_.Life -= 1.0f;
			return (p_.Life > 0.0f);
		}

		int PlayerJumpEffectEmitFrameCount = 0;
		int PlayerDashEffectEmitFrameCount = 0;
	}

	struct Tag_HoldWeapon {};

	template<>
	void InGame::Update(
		Tag_HoldWeapon&&
	) {
		static auto const& animatedModel{ PlayerSkinnedInstance_ };
		static auto const& skeleton{ animatedModel->Skeleton_ };
		static uint32_t idx_LeftHand{ skeleton.IDX_Joint.find("Bone.012")->second };
		static uint32_t idx_RightHand{ skeleton.IDX_Joint.find("Bone.024")->second };
		static auto const& leftHand{ skeleton.ARR_Joint[idx_LeftHand] };
		static auto const& rightHand{ skeleton.ARR_Joint[idx_RightHand] };

		Lumina::Mat4 meshWorld{
			Lumina::Mat4::SRT(
				Player_->ModelScale() * 2.0f,
				Player_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
				Player_->ModelTranslate()
			)
		};

		World_Hands[0] = leftHand.SkeletonSpace * meshWorld;
		World_Hands[1] = rightHand.SkeletonSpace * meshWorld;

		WorldPos_Weapon_ =
			Lumina::Vec4{ 0.0f, 3.0f, 0.0f, 1.0f } *
			Lumina::Mat4::SRT(
				{ 0.125f, 0.5f, 0.125f },
				{ -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f }
			) *
			World_Hands[1];
	}

	struct Tag_UpdateColliders {};

	// コライダの World を親行列から更新する（描画と同じ行列を使う）
	template<>
	void InGame::Update(
		Tag_UpdateColliders&&
	) {
		// 右手ボーンの行列をこのフレームの値にしておく（後で同じ Tag_HoldWeapon を呼んでも結果は同じ）
		Update(Tag_HoldWeapon{});

		Lumina::Mat4 const playerMeshWorld{
			Lumina::Mat4::SRT(
				Player_->ModelScale() * 2.0f,
				Player_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
				Player_->ModelTranslate()
			)
		};
		Player_->BodyCollider().UpdateWorld(playerMeshWorld);

		// 武器 = 武器メッシュの SRT × 右手ボーン（Render.cpp の weaponWorld と同じ）
		Lumina::Mat4 const weaponWorld{
			Lumina::Mat4::SRT({ 0.125f, 0.5f, 0.125f }, { -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }) *
			World_Hands[1]
		};
		// 新方式: Hitbox の窓が開いている間だけ有効（窓の状態は、ボーン行列と同じ前フレーム終わりの時刻のもの）
		// 旧方式: 攻撃中ずっと有効
		Player_->WeaponCollider().Enabled = PlayerAnimEvents_.UseAnimEvent ? PlayerAnimEvents_.IsHitboxOpen() : (playerATKFlag != 0);
		Player_->WeaponCollider().UpdateWorld(weaponWorld);

		Lumina::Mat4 const bossMeshWorld{
			Lumina::Mat4::SRT(
				Boss_Kinoko_->ModelScale() * 2.0f,
				Boss_Kinoko_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
				Boss_Kinoko_->ModelTranslate()
			)
		};
		Boss_Kinoko_->BodyCollider().UpdateWorld(bossMeshWorld);
	}

#if defined(_DEBUG)
	struct Tag_CollisionDebugUI {};

	template<>
	void InGame::Update(
		Tag_CollisionDebugUI&&
	) {
		auto const editOBB{
			[](char const* label_, Lumina::Phys::Collider& collider_) {
				auto* const obb{ std::get_if<Lumina::Phys::OBB>(&collider_.Local) };
				if (!obb) { return; }
				ImGui::PushID(label_);
				ImGui::SeparatorText(label_);
				ImGui::Text("ID %u / %s", collider_.ID(), collider_.Enabled ? "Enabled" : "Disabled");
				ImGui::DragFloat3("Center", &obb->Center.x, 0.01f);
				ImGui::DragFloat3("HalfExtent", &obb->HalfExtent.x, 0.01f, 0.0f, 100.0f);
				ImGui::PopID();
			}
		};

		ImGui::Begin("Collision");
		ImGui::Checkbox("Show Colliders", &ShowColliders_);
		ImGui::Checkbox("Show Bounds", &CollisionDebug_.ShowBounds);
		ImGui::Checkbox("Show Contacts", &CollisionDebug_.ShowContacts);
		ImGui::Text(
			"Colliders %u / Broadphase %u / Contacts %u",
			static_cast<uint32_t>(CollisionWorld_.Colliders().size()),
			CollisionWorld_.NumBroadphasePairs(),
			static_cast<uint32_t>(CollisionWorld_.Contacts().size())
		);
		// 形状はメッシュ空間の値（Player / Boss の Initialize で入れている値を上書きする）
		editOBB("Player Body", Player_->BodyCollider());
		editOBB("Player Weapon", Player_->WeaponCollider());
		editOBB("Boss Body", Boss_Kinoko_->BodyCollider());
		ImGui::End();
	}
#endif

	template<>
	void InGame::Update(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::WinApp::RawInput const& input_
	) {
		switch (CurrentPhase_) {
			case PHASE_STARTUP:
				Startup(dxContext_, cmdList_, input_);
				break;
			case PHASE_IN_BATTLE:
				InBattle(dxContext_, cmdList_, input_);
				break;
			case PHASE_WIN:
				Win(dxContext_, cmdList_, input_);
				break;
			case PHASE_LOSE:
				Lose(dxContext_, cmdList_, input_);
				break;
		}

		if (Player_->IsTriggered_Jump()) {
			PlayerJumpEffectEmitFrameCount = 15;
		}
		if (Player_->IsTriggered_Dash()) {
			PlayerDashEffectEmitFrameCount = 15;
		}

		hitBoss *= 0.9f;
		UpdatePlayerCamera(*PlayerCamera_, *Player_);

		Boss_Kinoko_->Update(*Player_);

		auto& rndEngine{ Lumina::Random::Generator() };

		auto emitKnockEffectParticles{
			[&, this] (Lumina::Vec3 const& contactPoint_, float hueFactor_) -> void {
				static float effectTimeFactor{ 0.0f };
				effectTimeFactor += 0.5f;

				for (int i = 0; i < 32; ++i) {
					Particle p{};
					{
						p.Translate = {
							std::cos(effectTimeFactor * 0.3f + i * 3.6f) * 1.5f,
							std::sin(effectTimeFactor * 0.4f * i) * 1.5f,
							std::sin(effectTimeFactor * 0.5f - i * 1.2f) * 1.5f
						};

						p.Velocity.x = p.Translate.x * 0.1f;
						p.Velocity.y = p.Translate.y * 0.1f;
						p.Velocity.z = p.Translate.z * 0.1f;

						p.Translate.x += contactPoint_.x;
						p.Translate.y += contactPoint_.y;
						p.Translate.z += contactPoint_.z;

						p.Scale.x = 15.0f;
						p.Scale.y = 15.0f;

						p.Rotate.z = rndEngine() * Inv_0xFFFFFFFF * std::numbers::pi_v<float> *2.0f;

						p.Life = 36.0f;

						auto const rgb_Base = Lumina::Utils::Color::Convert(
							Lumina::Utils::Color::HSV{
								rndEngine() * Inv_0xFFFFFFFF * 45.0f + hueFactor_,
								rndEngine() * Inv_0xFFFFFFFF * 0.5f + 0.5f,
								0.75f
							}
						);
						p.RenderData.RGBA = {
							rgb_Base.R,
							rgb_Base.G,
							rgb_Base.B,
							0.05f
						};
						p.RenderData.DiffuseID = 0U;
						p.RenderData.DiffuseAtlasID = (rndEngine() % 5U) + 2U;
						KnockEffects_->Emit(std::move(p));
					}
				}
			}
		};

		// Collision btw. Player and Boss

		Update(Tag_UpdateColliders{});
		CollisionWorld_.Detect();
#if defined(_DEBUG)
		Update(Tag_CollisionDebugUI{});
		PlayerAnimEvents_.DrawEditor();
#endif

		std::optional<Lumina::Phys::Hit> weaponHit{};
		std::optional<Lumina::Phys::Hit> bodyHit{};
		for (auto const& pair : CollisionWorld_.Contacts()) {
			if (auto const hit{ pair.Match(CollisionLayer::PlayerWeapon, CollisionLayer::BossBody) }) {
				weaponHit = hit;
			}
			if (auto const hit{ pair.Match(CollisionLayer::PlayerBody, CollisionLayer::BossBody) }) {
				bodyHit = hit;
			}
		}

		Lumina::Vec3 const dPos{ Player_->WorldPosition() - Boss_Kinoko_->WorldPosition() };
		// 旧方式の「攻撃が当たりうる」条件。体当たりの分岐はどちらの方式でもこれで決める（体当たりの挙動は変えないため）
		bool const legacyWeaponGate{ playerATKFlag != 0 && !Boss_Kinoko_->IsBeingKnockedBack() };
		if (PlayerAnimEvents_.UseAnimEvent) {
			// 新方式: コライダは Hitbox の窓の間だけ有効。同じ窓では同じ相手に 1 回だけ当たる（ノックバック中でも当たる）。
			// ダメージとノックバックはサイドカーの値
			if (weaponHit) {
				if (auto const hitbox{ PlayerAnimEvents_.TryHit(Boss_Kinoko_->BodyCollider().ID()) }) {
					Lumina::Vec3 const contactPoint{ weaponHit->Info.Point };
					Boss_Kinoko_->KnockedBack(hitbox->Knockback, { -dPos.x, -dPos.z });
					Boss_Kinoko_->HP_ -= hitbox->Damage;
					emitKnockEffectParticles(contactPoint, 195.0f);
					hitBoss = 1.0f;
				}
			}
		}
		else if (legacyWeaponGate) {
			if (weaponHit) {
				Lumina::Vec3 const contactPoint{ weaponHit->Info.Point };
				Boss_Kinoko_->KnockedBack(5.0f * playerATKFlag, { -dPos.x, -dPos.z });
				Boss_Kinoko_->HP_ -= 100.0f * playerATKFlag;
				emitKnockEffectParticles(contactPoint, 195.0f);
				hitBoss = 1.0f;
			}
		}
		if (!legacyWeaponGate && bodyHit) {
			Lumina::Vec3 const contactPoint{ bodyHit->Info.Point };
			/*if (Player_->IsDashing() && !Boss_Kinoko_->IsBeingKnockedBack()) {
				Boss_Kinoko_->KnockedBack(15.0f, { -dPos.x, -dPos.z });
				Boss_Kinoko_->HP_ -= 100.0f;
				emitKnockEffectParticles(contactPoint, 195.0f);
			}*/
			if (!Player_->IsDashing() && !Player_->IsBeingKnockedBack()) {
				Player_->KnockedBack(5.0f, { dPos.x, dPos.z });
				Player_->HP_ -= 5.0f;
				emitKnockEffectParticles(contactPoint, -7.5f);
			}
		}

		Lumina::Mat4 viewToWorld{ PlayerCamera_->View.Inv() };
		UB_ViewToWorld_.Store(&viewToWorld, sizeof(Lumina::Mat4), 0LLU);

		Update(Tag_HoldWeapon{});

		float playerRadius{ 1.35f };
		if (Player_->WorldPosition().y > 0.5f) {
			playerRadius = 0.0f;
		}
		float emenyRadius{ 3.2f };
		if (Boss_Kinoko_->WorldPosition().y > 2.0f) {
			emenyRadius = 0.0f;
		}
		Grassland_->Update(
			dxContext_,
			PlayerCamera_->VP,
			Player_->ModelTranslate(),
			playerRadius,
			Boss_Kinoko_->ModelTranslate(),
			emenyRadius
		);

		// Player effects
		{
			static float playerEffectTimeFactor{ 0.0f };
			playerEffectTimeFactor += 0.5f;

			auto rgb_Gaming = Lumina::Utils::Color::Convert(
				Lumina::Utils::Color::HSV{
					rndEngine() * Inv_0xFFFFFFFF * 60.0f +
					playerEffectTimeFactor +
					playerEffectTimeFactor * 0.1f * 180.0f * std::numbers::inv_pi_v<float>,
					rndEngine() * Inv_0xFFFFFFFF * 0.3f + 0.5f,
					0.95f
				}
			);

			for (int i = 0; i < 2; ++i) {
				Particle p{};
				{
					p.Translate = {
						std::cos(playerEffectTimeFactor * 0.3f + i * 3.6f) * 1.5f,
						std::sin(playerEffectTimeFactor * 0.4f * i) * 1.0f,
						std::sin(playerEffectTimeFactor * 0.5f - i * 1.2f) * 1.5f
					};

					p.Velocity.x = p.Translate.z * (-0.05f);
					p.Velocity.z = p.Translate.x * (-0.05f);

					p.Translate.x += Player_->ModelTranslate().x;
					p.Translate.y += Player_->ModelTranslate().y;
					p.Translate.z += Player_->ModelTranslate().z;

					p.Scale.x = 0.7f;
					p.Scale.y = 0.7f;

					p.Rotate.z = rndEngine() * Inv_0xFFFFFFFF * std::numbers::pi_v<float> *2.0f;

					p.Life = 64.0f;

					p.RenderData.RGBA = {
						rgb_Gaming.R + rndEngine() * Inv_0xFFFFFFFF * 0.1f,
						rgb_Gaming.G + rndEngine() * Inv_0xFFFFFFFF * 0.1f,
						rgb_Gaming.B,
						0.15f
					};
					p.RenderData.DiffuseID = 0U;
					p.RenderData.DiffuseAtlasID = (rndEngine() & 3) ? (3U) : (4U);
					PlayerEffects_->Emit(std::move(p));
				}
			}
			for (int i = 0; i < 2; ++i) {
				Particle p{};
				{
					p.Translate = {
						std::cos(playerEffectTimeFactor * 0.3f + i * 3.6f) * 0.1f,
						std::sin(playerEffectTimeFactor * 0.4f * i) * 0.1f,
						std::sin(playerEffectTimeFactor * 0.5f - i * 1.2f) * 0.1f
					};

					p.Velocity.x = p.Translate.z * (-0.05f);
					p.Velocity.z = p.Translate.x * (-0.05f);

					p.Translate.x += World_Hands[i][3][0];
					p.Translate.y += World_Hands[i][3][1];
					p.Translate.z += World_Hands[i][3][2];

					p.Scale.x = 0.7f;
					p.Scale.y = 0.7f;

					p.Rotate.z = rndEngine() * Inv_0xFFFFFFFF * std::numbers::pi_v<float> *2.0f;

					p.Life = 64.0f;

					p.RenderData.RGBA = {
						rgb_Gaming.R + rndEngine() * Inv_0xFFFFFFFF * 0.1f,
						rgb_Gaming.G + rndEngine() * Inv_0xFFFFFFFF * 0.1f,
						rgb_Gaming.B,
						0.15f
					};
					p.RenderData.DiffuseID = 0U;
					p.RenderData.DiffuseAtlasID = (rndEngine() & 3) ? (3U) : (4U);
					PlayerEffects_->Emit(std::move(p));
				}
			}

			if (PlayerJumpEffectEmitFrameCount > 0) {
				for (int i = 0; i < 2; ++i) {
					Particle p_Jump{};
					{
						p_Jump.Translate = {
							std::cos(playerEffectTimeFactor * 1.6f + std::numbers::pi_v<float> * i) * 0.5f,
							std::sin(playerEffectTimeFactor * 0.8f) * 0.1f,
							std::sin(playerEffectTimeFactor * 1.6f + std::numbers::pi_v<float> * i) * 0.5f
						};

						p_Jump.Velocity.x = p_Jump.Translate.z * (-0.05f);
						p_Jump.Velocity.z = p_Jump.Translate.x * (-0.05f);

						p_Jump.Translate.x += Player_->ModelTranslate().x;
						p_Jump.Translate.y += Player_->ModelTranslate().y - 1.0f;
						p_Jump.Translate.z += Player_->ModelTranslate().z;

						p_Jump.Scale.x = 1.0f;
						p_Jump.Scale.y = 1.0f;

						p_Jump.Rotate.z = rndEngine() * Inv_0xFFFFFFFF * std::numbers::pi_v<float> * 2.0f;

						p_Jump.Life = 32.0f;

						p_Jump.RenderData.RGBA = {
							0.1f + rgb_Gaming.R + rndEngine() * Inv_0xFFFFFFFF * 0.05f,
							0.1f + rgb_Gaming.G + rndEngine() * Inv_0xFFFFFFFF * 0.05f,
							0.1f + rgb_Gaming.B + rndEngine() * Inv_0xFFFFFFFF * 0.05f,
							1.0f
						};
						p_Jump.RenderData.DiffuseID = 0U;
						p_Jump.RenderData.DiffuseAtlasID = (rndEngine() & 3) ? (3U) : (4U);
						PlayerEffects_->Emit(std::move(p_Jump));
					}
				}
				--PlayerJumpEffectEmitFrameCount;
			}

			if (PlayerDashEffectEmitFrameCount > 0) {
				float const cos_Theta{ std::cos(Player_->Angle()) };
				float const sin_Theta{ std::sin(Player_->Angle()) };

				for (int i = 0; i < 2; ++i) {
					Particle p_Dash{};
					{
						p_Dash.Translate = {
							std::cos(playerEffectTimeFactor * 2.4f + std::numbers::pi_v<float> * i) * 0.2f +
							cos_Theta * 0.9f,
							std::sin(playerEffectTimeFactor * 0.8f) * 0.1f,
							std::sin(playerEffectTimeFactor * 2.4f + std::numbers::pi_v<float> * i) * 0.2f +
							sin_Theta * 0.9f
						};

						p_Dash.Velocity.x = p_Dash.Translate.x * (-0.1f);
						p_Dash.Velocity.z = p_Dash.Translate.z * (-0.1f);

						p_Dash.Translate.x += Player_->ModelTranslate().x;
						p_Dash.Translate.y += Player_->ModelTranslate().y;
						p_Dash.Translate.z += Player_->ModelTranslate().z;

						p_Dash.Scale.x = 3.0f;
						p_Dash.Scale.y = 3.0f;

						p_Dash.Rotate.z = rndEngine() * Inv_0xFFFFFFFF * std::numbers::pi_v<float> *2.0f;

						p_Dash.Life = 32.0f;

						p_Dash.RenderData.RGBA = {
							0.1f + rgb_Gaming.R,
							0.1f + rgb_Gaming.G,
							0.1f + rgb_Gaming.B,
							0.5f
						};
						p_Dash.RenderData.DiffuseID = 0U;
						p_Dash.RenderData.DiffuseAtlasID = (rndEngine() & 3) ? (3U) : (4U);
						PlayerEffects_->Emit(std::move(p_Dash));
					}
				}
				--PlayerDashEffectEmitFrameCount;
			}
			PlayerEffects_->Update(cmdList_, viewToWorld, UpdatePlayerEffect);
		}

		// Ambient sparkles
		{
			static float sparkleTimeFactor{ 0.0f };
			sparkleTimeFactor += 0.75f;

			float const spawnPosRad = rndEngine() * Inv_0xFFFFFFFF * 100.0f;
			float const spawnPosTheta = rndEngine() * Inv_0xFFFFFFFF * std::numbers::pi_v<float> * 2.0f;
			float const x{ spawnPosRad * std::cos(spawnPosTheta) };
			float const z{ spawnPosRad * std::sin(spawnPosTheta) };
			if (std::abs(x) < 40.0f && std::abs(z) < 40.0f) {
				Particle sparkle{};
				sparkle.Translate = {
					x,
					rndEngine() * Inv_0xFFFFFFFF * 2.0f,
					z
				};
				sparkle.Scale.x = 1.0f;
				sparkle.Scale.y = 1.0f;
				sparkle.Life = 60.0f;
				auto rgb = Lumina::Utils::Color::Convert(
					Lumina::Utils::Color::HSV{
						rndEngine() * Inv_0xFFFFFFFF * 45.0f,
						rndEngine() * Inv_0xFFFFFFFF * 0.5f + 0.5f,
						0.75f
					}
				);
				auto rgb_Gaming = Lumina::Utils::Color::Convert(
					Lumina::Utils::Color::HSV{
						rndEngine() * Inv_0xFFFFFFFF * 45.0f +
						sparkleTimeFactor +
						spawnPosTheta * 180.0f * std::numbers::inv_pi_v<float>,
						rndEngine() * Inv_0xFFFFFFFF * 0.3f + 0.5f,
						0.95f
					}
				);
				sparkle.RenderData.RGBA = {
					rgb.R * (0.7f + rgb_Gaming.R * 0.3f),
					rgb.G * (0.7f + rgb_Gaming.G * 0.3f),
					rgb.B * (0.7f + rgb_Gaming.B * 0.3f),
					0.0f
				};
				sparkle.RenderData.DiffuseID = 0U;
				sparkle.RenderData.DiffuseAtlasID = 5U;
				AmbientSparkles_->Emit(std::move(sparkle));
			}

			AmbientSparkles_->Update(cmdList_, viewToWorld, UpdateAmbientSparkle);
		}

		KnockEffects_->Update(cmdList_, viewToWorld, UpdateKnockEffect);

		List_PointLight_.Clear();
		List_LocalToWorld_LightSphere_.Clear();

		Lumina::List<PlayerBullet>::Iterator it_PlayerBullet{ PlayerBullets_->InstanceList() };
		for (it_PlayerBullet.Begin(); !it_PlayerBullet.End(); it_PlayerBullet.Next()) {
			auto& b{ *it_PlayerBullet };

			//// Homing
			//{
			//	Lumina::Vec2 const d_XZ{
			//		Boss_Kinoko_->WorldPosition().x - b.Translate.x,
			//		Boss_Kinoko_->WorldPosition().z - b.Translate.z
			//	};
			//	Lumina::Vec2 d_XZ_Normalized = d_XZ.Unit();
			//	if (!std::isfinite(d_XZ_Normalized.x) || !std::isfinite(d_XZ_Normalized.y)) {
			//		d_XZ_Normalized = { 0.0f, 0.0f };
			//	}
			//	Lumina::Vec2 vTmp{
			//		b.Velocity.x + d_XZ_Normalized.x * (0.3f - b.Velocity.y),
			//		b.Velocity.z + d_XZ_Normalized.y * (0.3f - b.Velocity.y)
			//	};
			//	vTmp = vTmp.Unit();
			//	b.Velocity.x = vTmp.x;
			//	b.Velocity.z = vTmp.y;
			//}

			if (!List_PointLight_.IsFull()) {
				auto& light{ List_PointLight_.New() };

				light.WorldPosition = {
					b.Translate.x,
					b.Translate.y,
					b.Translate.z,
					1.0f
				};
				light.RGB = {
					b.RenderData.RGBA.x,
					b.RenderData.RGBA.y,
					b.RenderData.RGBA.z
				};
				light.Intensity = (b.RenderData.RGBA.w - b.Velocity.y) * 20.0f;

				auto& lightSphere{ List_LocalToWorld_LightSphere_.New() };

				float const radius{ light.Intensity * 3.0f };
				lightSphere = {
					radius, 0.0f, 0.0f, 0.0f,
					0.0f, radius, 0.0f, 0.0f,
					0.0f, 0.0f, radius, 0.0f,
					light.WorldPosition.x,
					light.WorldPosition.y,
					light.WorldPosition.z,
					1.0f,
				};
			}

			// Collision detection
			{
				Lumina::Vec3 const d_XYZ{
					Boss_Kinoko_->WorldPosition().x - b.Translate.x,
					Boss_Kinoko_->WorldPosition().y - b.Translate.y,
					Boss_Kinoko_->WorldPosition().z - b.Translate.z
				};
				if (Lumina::Vec3::Dot(d_XYZ, d_XYZ) < 9.0f) {
					if (!Boss_Kinoko_->IsBeingKnockedBack()) {
						Boss_Kinoko_->HP_ -= (b.ATK - b.Velocity.y * 3.0f);
					}
					b.Life = 0.0f;
				}
			}
		}

		Lumina::List<Particle>::Iterator it_KnockEffect{ KnockEffects_->InstanceList() };
		for (it_KnockEffect.Begin(); !it_KnockEffect.End(); it_KnockEffect.Next()) {
			auto const& p{ *it_KnockEffect };

			if (!List_PointLight_.IsFull()) {
				auto& light{ List_PointLight_.New() };

				light.WorldPosition = {
					p.Translate.x,
					p.Translate.y,
					p.Translate.z,
					1.0f
				};
				light.RGB = {
					p.RenderData.RGBA.x,
					p.RenderData.RGBA.y,
					p.RenderData.RGBA.z
				};
				light.Intensity = p.Scale.x * 30.0f;

				auto& lightSphere{ List_LocalToWorld_LightSphere_.New() };

				float const radius{ Lumina::LightSphereRadius(1024.0f, light.Intensity * 0.5f, 1.0f, 1.0f, 0.5f) };
				lightSphere = {
					radius, 0.0f, 0.0f, 0.0f,
					0.0f, radius, 0.0f, 0.0f,
					0.0f, 0.0f, radius, 0.0f,
					light.WorldPosition.x,
					light.WorldPosition.y,
					light.WorldPosition.z,
					1.0f,
				};
			}
		}
		Lumina::List<Particle>::Iterator it_PlayerEffect{ PlayerEffects_->InstanceList() };
		for (it_PlayerEffect.Begin(); !it_PlayerEffect.End(); it_PlayerEffect.Next()) {
			auto const& p{ *it_PlayerEffect };

			if (!List_PointLight_.IsFull() && (p.RenderData.DiffuseAtlasID == 4U)) {
				auto& light{ List_PointLight_.New() };

				light.WorldPosition = {
					p.Translate.x,
					p.Translate.y,
					p.Translate.z,
					1.0f
				};
				light.RGB = {
					p.RenderData.RGBA.x,
					p.RenderData.RGBA.y,
					p.RenderData.RGBA.z
				};
				light.Intensity = p.RenderData.RGBA.w * 100.0f;

				auto& lightSphere{ List_LocalToWorld_LightSphere_.New() };

				float const radius{ Lumina::LightSphereRadius(1024.0f, light.Intensity, 1.0f, 1.0f, 0.5f) };
				lightSphere = {
					radius, 0.0f, 0.0f, 0.0f,
					0.0f, radius, 0.0f, 0.0f,
					0.0f, 0.0f, radius, 0.0f,
					light.WorldPosition.x,
					light.WorldPosition.y,
					light.WorldPosition.z,
					1.0f,
				};
			}
		}

		Lumina::List<Particle>::Iterator it_Sparkle{ AmbientSparkles_->InstanceList() };
		for (it_Sparkle.Begin(); !it_Sparkle.End(); it_Sparkle.Next()) {
			auto const& sparkle{ *it_Sparkle };

			if (!List_PointLight_.IsFull()) {
				auto& light{ List_PointLight_.New() };

				light.WorldPosition = {
					sparkle.Translate.x,
					sparkle.Translate.y,
					sparkle.Translate.z,
					1.0f
				};
				light.RGB = {
					sparkle.RenderData.RGBA.x,
					sparkle.RenderData.RGBA.y,
					sparkle.RenderData.RGBA.z
				};
				light.Intensity = sparkle.RenderData.RGBA.w * 400.0f;

				auto& lightSphere{ List_LocalToWorld_LightSphere_.New() };

				float const radius{ Lumina::LightSphereRadius(1024.0f, light.Intensity, 1.0f, 1.0f, 0.5f) };
				lightSphere = {
					radius, 0.0f, 0.0f, 0.0f,
					0.0f, radius, 0.0f, 0.0f,
					0.0f, 0.0f, radius, 0.0f,
					light.WorldPosition.x,
					light.WorldPosition.y,
					light.WorldPosition.z,
					1.0f,
				};
			}
		}

		Lumina::List<Particle>::Iterator it_BossATK{ BossBullets_->InstanceList() };
		for (it_BossATK.Begin(); !it_BossATK.End(); it_BossATK.Next()) {
			auto& atk{ *it_BossATK };
			
			// Collision detection
			{
				Lumina::Vec3 const d_XYZ{
					Player_->WorldPosition().x - atk.Translate.x,
					Player_->WorldPosition().y - atk.Translate.y,
					Player_->WorldPosition().z - atk.Translate.z
				};
				if (Lumina::Vec3::Dot(d_XYZ, d_XYZ) < 9.0f) {
					if (!Player_->IsBeingKnockedBack()) {
						Player_->KnockedBack(1.0f, { d_XYZ.x, d_XYZ.z });
						Player_->HP_ -= 0.1f;
					}
					atk.Life = 0.0f;
				}
			}

			/*if (!List_PointLight_.IsFull()) {
				auto& light{ List_PointLight_.New() };

				light.WorldPosition = {
					atk.Translate.x,
					atk.Translate.y,
					atk.Translate.z,
					1.0f
				};
				light.RGB = {
					atk.RenderData.RGBA.x,
					atk.RenderData.RGBA.y,
					atk.RenderData.RGBA.z
				};
				light.Intensity = atk.RenderData.RGBA.w * 50.0f;

				auto& lightSphere{ List_LocalToWorld_LightSphere_.New() };

				float const radius{ Lumina::LightSphereRadius(1024.0f, light.Intensity, 1.0f, 1.0f, 0.5f) };
				lightSphere = {
					radius, 0.0f, 0.0f, 0.0f,
					0.0f, radius, 0.0f, 0.0f,
					0.0f, 0.0f, radius, 0.0f,
					light.WorldPosition.x,
					light.WorldPosition.y,
					light.WorldPosition.z,
					1.0f,
				};
			}*/
		}

		Arr_Index_ActivePointLight_.clear();
		Lumina::List<Lumina::PointLight>::Iterator it_Light{ List_PointLight_ };
		for (it_Light.Begin(); !it_Light.End(); it_Light.Next()) {
			Arr_Index_ActivePointLight_.emplace_back(it_Light.Index());
		}

		DeferredLighting_->Update(
			List_PointLight_,
			List_LocalToWorld_LightSphere_,
			Arr_Index_ActivePointLight_
		);

		static Lumina::Mat4 inv_Viewport{
			1.0f / 640.0f, 0.0f, 0.0f, 0.0f,
			0.0f, -1.0f / 360.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			-1.0f, 1.0f, 0.0f, 1.0f,
		};
		Lumina::Mat4 screenToWorld{ inv_Viewport * PlayerCamera_->VP.Inv() };
		UB_ScreenToWorld_.Store(&screenToWorld, sizeof(Lumina::Mat4), 0LLU);

		{
			static float playerHPBarColorHueFactor = 0.0f;
			playerHPBarColorHueFactor += 0.01f;
			auto const rgb_Base = Lumina::Utils::Color::Convert(
				Lumina::Utils::Color::HSV{
					rndEngine() * Inv_0xFFFFFFFF * 3.0f + std::sin(playerHPBarColorHueFactor) * 5.0f + 195.0f,
					rndEngine() * Inv_0xFFFFFFFF * 0.05f + 0.5f +
					(1.0f - (Boss_Kinoko_->HP_ * Boss_Kinoko_->Inv_MaxHP_)) * 0.4f,
					0.5f +
					(1.0f - (Boss_Kinoko_->HP_ * Boss_Kinoko_->Inv_MaxHP_)) * 0.4f
				}
			);
			UI_PlayerHPBar_.RGBA_.x = rgb_Base.R;
			UI_PlayerHPBar_.RGBA_.y = rgb_Base.G;
			UI_PlayerHPBar_.RGBA_.z = rgb_Base.B;

			PlayerMaterial_.RGBA = {
				rgb_Base.R * 0.75f + 0.25f,
				rgb_Base.G * 0.75f + 0.25f,
				rgb_Base.B * 0.75f + 0.25f,
				1.0f
			};

			if (Player_->IsBeingKnockedBack()) {
				static float gamingColorHueFactor = 0.0f;
				gamingColorHueFactor += 16.0f;
				auto rgb_Gaming = Lumina::Utils::Color::Convert(
					Lumina::Utils::Color::HSV{
						rndEngine() * Inv_0xFFFFFFFF * 45.0f + gamingColorHueFactor,
						rndEngine() * Inv_0xFFFFFFFF * 0.3f + 0.5f,
						0.8f
					}
				);
				UI_PlayerHPBar_.RGBA_.x *= rgb_Gaming.R;
				UI_PlayerHPBar_.RGBA_.y *= rgb_Gaming.G;
				UI_PlayerHPBar_.RGBA_.z *= rgb_Gaming.B;

				PlayerMaterial_.RGBA.x *= rgb_Gaming.R;
				PlayerMaterial_.RGBA.y *= rgb_Gaming.G;
				PlayerMaterial_.RGBA.z *= rgb_Gaming.B;
			}
			UB_PlayerMaterial_.Store(&PlayerMaterial_, sizeof(MeshMaterial), 0LLU);
		}

		{
			static float bossHPBarColorHueFactor = 0.0f;
			bossHPBarColorHueFactor += 0.01f;
			auto const rgb_Base = Lumina::Utils::Color::Convert(
				Lumina::Utils::Color::HSV{
					rndEngine() * Inv_0xFFFFFFFF * 3.0f + std::sin(bossHPBarColorHueFactor) * 5.0f,
					rndEngine() * Inv_0xFFFFFFFF * 0.05f + 0.5f +
					(1.0f - (Boss_Kinoko_->HP_ * Boss_Kinoko_->Inv_MaxHP_)) * 0.4f,
					0.5f +
					(1.0f - (Boss_Kinoko_->HP_ * Boss_Kinoko_->Inv_MaxHP_)) * 0.4f
				}
			);
			UI_BossHPBar_.RGBA_.x = rgb_Base.R;
			UI_BossHPBar_.RGBA_.y = rgb_Base.G;
			UI_BossHPBar_.RGBA_.z = rgb_Base.B;

			BossMaterial_.RGBA = {
				rgb_Base.R * 0.75f + 0.25f,
				rgb_Base.G * 0.75f + 0.25f,
				rgb_Base.B * 0.75f + 0.25f,
				1.0f
			};

			if (Boss_Kinoko_->IsBeingKnockedBack()) {
				static float gamingColorHueFactor = 0.0f;
				gamingColorHueFactor += 16.0f;
				auto rgb_Gaming = Lumina::Utils::Color::Convert(
					Lumina::Utils::Color::HSV{
						rndEngine() * Inv_0xFFFFFFFF * 45.0f + gamingColorHueFactor,
						rndEngine() * Inv_0xFFFFFFFF * 0.3f + 0.5f,
						0.8f
					}
				);
				UI_BossHPBar_.RGBA_.x *= rgb_Gaming.R;
				UI_BossHPBar_.RGBA_.y *= rgb_Gaming.G;
				UI_BossHPBar_.RGBA_.z *= rgb_Gaming.B;

				BossMaterial_.RGBA.x *= rgb_Gaming.R;
				BossMaterial_.RGBA.y *= rgb_Gaming.G;
				BossMaterial_.RGBA.z *= rgb_Gaming.B;
			}
			UB_BossMaterial_.Store(&BossMaterial_, sizeof(MeshMaterial), 0LLU);
		}

		{
			static float time = 0.0f;
			if (Boss_Kinoko_->IsBeingKnockedBack()) {
				time += 0.01f;
				PPConstants_.Time = time;
				auto playerNDCPos = Lumina::Vec4{ Player_->WorldPosition() } * PlayerCamera_->VP;
				PPConstants_.PlayerNDCPos = { playerNDCPos.x, playerNDCPos.y };
				PPConstants_.IsEnemySuccessfullyAttacking = 1;
			}
			else {
				if (Player_->IsBeingKnockedBack()) {
					PPConstants_.IsEnemySuccessfullyAttacking = 1;
				}
				else {
					time = 0.0f;
				}
			}
			/*if (PlayerJumpEffectEmitFrameCount || PlayerDashEffectEmitFrameCount) {
				time += 0.01f;
				PPConstants_.Time = time;
				auto playerNDCPos = Lumina::Vec4{ Player_->WorldPosition() } * PlayerCamera_->VP;
				PPConstants_.PlayerNDCPos = { playerNDCPos.x, playerNDCPos.y };
				PPConstants_.IsEnemySuccessfullyAttacking = 1;
			}*/
		}
		{
			static float time = 0.0f;
			if (Player_->IsBeingKnockedBack()) {
				time += 0.01f;
				PPConstants_.Time = time;
				auto playerNDCPos = Lumina::Vec4{ Player_->WorldPosition() } * PlayerCamera_->VP;
				PPConstants_.PlayerNDCPos = { playerNDCPos.x, playerNDCPos.y };
				PPConstants_.IsEnemySuccessfullyAttacking = 1;
			}
			else {
				if (Boss_Kinoko_->IsBeingKnockedBack()) {
					PPConstants_.IsEnemySuccessfullyAttacking = 1;
				}
				else {
					time = 0.0f;
				}
			}
		}
		UB_PostProcessingConstants_.Store(&PPConstants_, sizeof(PostProcessingConstants), 0LLU);

		switch (playerATKFlag) {
		case 0:
			if (Player_->IsMoving()) {
				PlayAnimation(currentAnim_, currentAnimName_, animTimer_, IsAnimationLoop_, "Run", true);
			}
			else {
				PlayAnimation(currentAnim_, currentAnimName_, animTimer_, IsAnimationLoop_, "Idle", true);
			}
			break;

		case 1:
			PlayAnimation(currentAnim_, currentAnimName_, animTimer_, IsAnimationLoop_, "ATK1", false);
			break;

		case 2:
			PlayAnimation(currentAnim_, currentAnimName_, animTimer_, IsAnimationLoop_, "ATK2", false);
			break;

		case 3:
			PlayAnimation(currentAnim_, currentAnimName_, animTimer_, IsAnimationLoop_, "ATK3", false);
			break;
		}

		if (currentAnim_) {
			// 時刻はアニメーションイベントのプレイヤーが持つ（4 倍速・ループは折り返し・非ループは終端で止まる、は今までと同じ）。
			// クリップ名が変わったら最初から再生する。プレビュー中はエディタで選んだクリップのポーズになる
			PlayerAnimEvents_.Update(currentAnimName_, IsAnimationLoop_, 1.0f / 60.0f);
			auto const pose{ PlayerAnimEvents_.CurrentPose() };
			animTimer_ = pose.Time;		// 参考用（もう時刻の計算には使っていない）

			if (pose.Clip) {
				Lumina::CG3D::Update(
					PlayerSkinnedInstance_->SkinCluster_,
					PlayerSkinnedInstance_->Skeleton_,
					*pose.Clip,
					pose.Time
				);
			}
		}

		if (Boss_Kinoko_->IsMoving()) {
			PlayAnimation(
				currentAnim_Boss_,
				currentAnimName_Boss_,
				animTimer_Boss_,
				IsAnimationLoop_Boss_,
				"Run",
				true
			);
		}
		else {
			PlayAnimation(
				currentAnim_Boss_,
				currentAnimName_Boss_,
				animTimer_Boss_,
				IsAnimationLoop_Boss_,
				"Idle",
				true
			);
		}

		if (currentAnim_Boss_) {
			animTimer_Boss_ += 1.0f / 60.0f * 4.0f;

			if (IsAnimationLoop_Boss_) {
				animTimer_Boss_ = std::fmod(animTimer_Boss_, currentAnim_Boss_->DurationInSeconds);
			}
			else {
				// ループしない場合は Duration で止める（これなら > 判定でOK）
				if (animTimer_Boss_ > currentAnim_Boss_->DurationInSeconds) {
					animTimer_Boss_ = currentAnim_Boss_->DurationInSeconds;
				}
			}

			Lumina::CG3D::Update(
				BossSkinnedInstance_->SkinCluster_,
				BossSkinnedInstance_->Skeleton_,
				*currentAnim_Boss_,
				animTimer_Boss_
			);
		}

		SimpleFX3_->ClearBatch();
		SimpleFX3_->Batch(
			Lumina::Mat4::SRT(
				{ 3.0f, 3.0f, 3.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ Boss_Kinoko_->WorldPosition().x, Boss_Kinoko_->WorldPosition().y, Boss_Kinoko_->WorldPosition().z }
			)
		);
	}

	void InGame::PlayAnimation(
		Lumina::CG3D::MyAnimation*& curAnim_,
		std::string& curName_,
		float& timer_,
		bool& isAnimLoop_,
		std::string_view name_,
		bool isLoop_
	) {
		auto it = animDatabase_.find(name_.data());
		if (curName_ != name_) {
			timer_ = 0.0f;
		}
		isAnimLoop_ = isLoop_;
		if (it != animDatabase_.end()) {
			curAnim_ = &(it->second);
			curName_ = name_;
			return;
		}
		/*throw std::runtime_error("Motion not found: " + name);*/
		curAnim_ = &(animDatabase_.begin()->second);
	}
}

namespace Game::Scene {
	template<>
	void InGame::Update(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::WinApp::RawInput const& input_
	) {
		Impl_->Update(dxContext_, cmdList_, input_);
	}
}