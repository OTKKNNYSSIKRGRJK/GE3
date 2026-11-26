module Game.Scene.InGame;

import <algorithm>;
import <cmath>;
import <string>;
import <optional>;
import <variant>;

import : Impl;

import Lumina;

import Game.CollisionLayer;
import Game.AnimEventDemo;
import Game.CombatEvents;
import Game.CombatVfx;
import Game.SceneLight;
import Game.PauseMenu;
import Game.PlayerInput;
import Game.Scene;

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
	}

	namespace {
		constexpr float Inv_0xFFFFFFFF{ 1.0f / static_cast<float>(0xFFFFFFFFU) };
		constexpr float Inv_900{ 1.0f / 900.0f };

		// hitShake_ = ボスに当てたときの揺れ（InGame の HitCameraShake_。0 で揺れなし）
		void UpdatePlayerCamera(TrackingCamera& camera_, Player const& player_, float hitShake_) {
			camera_.Target = player_.WorldPosition();
			camera_.Position =
				camera_.Position * (1.0f - camera_.DelayFactor) +
				camera_.Target * camera_.DelayFactor;
			camera_.Position.y += camera_.Target.y * 0.175f;

			if (hitShake_ > 0.05f) {
				camera_.Position.x += std::cos(hitShake_ * 15.0f) * hitShake_ * 3.0f;
				camera_.Position.y += std::cos(hitShake_ * 20.0f) * hitShake_ * 3.0f;
				camera_.Position.z += std::cos(hitShake_ * 25.0f) * hitShake_ * 3.0f;
			}

			Lumina::Vec4 cameraTranslateSwayingOffset{
				Lumina::Vec4{ camera_.Position } +
				Lumina::Vec4{ camera_.OffsetFromTarget } *
				RotateAround(
					camera_.Up,
					(-camera_.Yaw) + std::numbers::pi_v<float> *0.5f +		// 以前は -player_.Angle()（プレイヤーの後ろに自動で回り込んでいた）
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

	// ボスの右手。ボスの骨格（プレイヤーと同じ Neki）とボスのメッシュの行列から作る
	struct Tag_HoldBossWeapon {};

	template<>
	void InGame::Update(
		Tag_HoldBossWeapon&&
	) {
		static auto const& skeleton{ BossSkinnedInstance_->Skeleton_ };
		static uint32_t idx_RightHand{ skeleton.IDX_Joint.find("Bone.024")->second };
		static auto const& rightHand{ skeleton.ARR_Joint[idx_RightHand] };

		Lumina::Mat4 const meshWorld{
			Lumina::Mat4::SRT(
				Boss_Kinoko_->ModelScale() * 2.0f,
				Boss_Kinoko_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
				Boss_Kinoko_->ModelTranslate()
			)
		};
		World_BossHand_ = rightHand.SkeletonSpace * meshWorld;
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
		Player_->UpdateBodyCollider(playerMeshWorld);

		// 武器 = 武器メッシュの SRT × 右手ボーン（Render.cpp の weaponWorld と同じ）
		Lumina::Mat4 const weaponWorld{
			Lumina::Mat4::SRT({ 0.125f, 0.5f, 0.125f }, { -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }) *
			World_Hands[1]
		};
		// 新方式: Hitbox の窓が開いている間だけ有効（窓の状態は、ボーン行列と同じ前フレーム終わりの時刻のもの）
		// 旧方式: 攻撃中ずっと有効
		bool const playerWeaponEnabled{ !GameOver_.IsActive() && !Victory_.IsActive() && (PlayerAnimEvents_.UseAnimEvent ? PlayerAnimEvents_.IsHitboxOpen() : (playerATKFlag != 0)) };	// 倒れている間は攻撃判定なし
		Player_->UpdateWeaponCollider(weaponWorld, playerWeaponEnabled);

		Lumina::Mat4 const bossMeshWorld{
			Lumina::Mat4::SRT(
				Boss_Kinoko_->ModelScale() * 2.0f,
				Boss_Kinoko_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
				Boss_Kinoko_->ModelTranslate()
			)
		};
		Boss_Kinoko_->UpdateBodyCollider(bossMeshWorld);

		// ボスの武器（プレイヤーと同じ武器メッシュの SRT × ボスの右手）。Hitbox の窓が開いている間だけ有効
		Update(Tag_HoldBossWeapon{});
		Lumina::Mat4 const bossWeaponWorld{
			Lumina::Mat4::SRT({ 0.125f, 0.5f, 0.125f }, { -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }) *
			World_BossHand_
		};
		bool const bossWeaponEnabled{ !GameOver_.IsActive() && !Victory_.IsActive() && BossAnimEvents_.IsHitboxOpen() };	// 勝利演出で倒れている間も判定なし
		Boss_Kinoko_->UpdateWeaponCollider(bossWeaponWorld, bossWeaponEnabled);
	}

#if defined(_DEBUG)
	struct Tag_CollisionDebugUI {};

	template<>
	void InGame::Update(
		Tag_CollisionDebugUI&&
	) {
		// コライダは const で受け取り、OBB のコピーを編集する。変わったときだけ持ち主の操作（SetBodyColliderShape など）で戻す
		auto const editOBB{
			[](char const* label_, Lumina::Phys::Collider const& collider_, auto&& apply_) {
				auto const* const src{ std::get_if<Lumina::Phys::OBB>(&collider_.Local) };
				if (!src) { return; }
				Lumina::Phys::OBB obb{ *src };
				bool changed{ false };
				ImGui::PushID(label_);
				ImGui::SeparatorText(label_);
				ImGui::Text("ID %u / %s", collider_.ID(), collider_.Enabled ? "Enabled" : "Disabled");
				changed |= ImGui::DragFloat3("Center", &obb.Center.x, 0.01f);
				changed |= ImGui::DragFloat3("HalfExtent", &obb.HalfExtent.x, 0.01f, 0.0f, 100.0f);
				ImGui::PopID();
				if (changed) { apply_(Lumina::Phys::Shape{ obb }); }
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
		editOBB("Player Body", Player_->BodyCollider(), [this](Lumina::Phys::Shape const& s_) { Player_->SetBodyColliderShape(s_); });
		editOBB("Player Weapon", Player_->WeaponCollider(), [this](Lumina::Phys::Shape const& s_) { Player_->SetWeaponColliderShape(s_); });
		editOBB("Boss Body", Boss_Kinoko_->BodyCollider(), [this](Lumina::Phys::Shape const& s_) { Boss_Kinoko_->SetBodyColliderShape(s_); });

		// ボスのデバッグ用スイッチ（ヒット判定の確認用。「Boss AI」と同じもの）
		ImGui::SeparatorText("Boss Debug");
		ImGui::Checkbox("Freeze AI (BT / Idle-Move)", &Boss_Kinoko_->Debug.FreezeAI);
		ImGui::Checkbox("No hit reaction (no push / stagger)", &Boss_Kinoko_->Debug.NoKnockBack);
		ImGui::End();
	}

	// ImGui「Player」: HP の上限、ボスに殴られたときののけぞり、ガード・弾き・回避。その場で効き、戦闘をやり直しても残る。
	// Save で Assets/Configs/Player.json に書き出す（起動時に読む）
	struct Tag_PlayerDebugUI {};

	template<>
	void InGame::Update(
		Tag_PlayerDebugUI&&
	) {
		if (!ImGui::Begin("Player")) {
			ImGui::End();
			return;
		}
		if (ImGui::Button("Save")) { Game::FighterSettings::SavePlayer(*Player_, PlayerSettingsStatus_); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Game::FighterSettings::LoadPlayer(*Player_, PlayerSettingsStatus_); }
		ImGui::SameLine();
		ImGui::TextDisabled("%s  (%s)", PlayerSettingsStatus_.c_str(), Game::FighterSettings::PlayerConfigPath);

		ImGui::Text("HP: %.0f / %.0f", Player_->HP(), Player_->MaxHP());
		float maxHP{ Player_->MaxHP() };
		if (ImGui::DragFloat("Max HP", &maxHP, 1.0f, 1.0f, 100000.0f, "%.0f")) { Player_->SetMaxHP(maxHP); }
		if (ImGui::Button("Refill HP")) { Player_->RefillHP(); }

		// のけぞり（押される距離 = ボスのサイドカーの knockback × scale。ATK1〜3 は 5 / 10 / 15）
		ImGui::SeparatorText("Hit reaction (hit by boss)");
		Game::FighterSetup s{ Player_->Setup() };
		bool changed{ false };
		changed |= ImGui::DragFloat("Knockback scale", &s.KnockbackScale, 0.005f, 0.0f, 2.0f);
		changed |= ImGui::DragFloat("Knockback decay", &s.KnockbackDecay, 0.005f, 0.0f, 0.98f);
		changed |= ImGui::DragInt("Hit stun (frames)", &s.HitStunFrames, 0.2f, 1, 120);

		// ガード（右クリック / LB）。押してから Deflect window の間に受けた攻撃は弾く
		ImGui::SeparatorText("Guard / Deflect");
		ImGui::Text("Guarding %s / Deflect ready %s / Guard stun %s",
			Player_->IsGuarding() ? "yes" : "no", Player_->CanDeflectNow() ? "yes" : "no", Player_->IsGuardStunned() ? "yes" : "no");
		float halfDeg{ s.GuardHalfAngle * 57.29578f };
		if (ImGui::DragFloat("Guard half angle (deg)", &halfDeg, 0.5f, 0.0f, 180.0f, "%.0f")) { s.GuardHalfAngle = halfDeg / 57.29578f; changed = true; }
		changed |= ImGui::DragFloat("Guard damage scale", &s.GuardDamageScale, 0.005f, 0.0f, 1.0f);
		changed |= ImGui::DragFloat("Guard knockback scale", &s.GuardKnockbackScale, 0.005f, 0.0f, 2.0f);
		changed |= ImGui::DragInt("Guard stun (frames)", &s.GuardStunFrames, 0.2f, 0, 60);
		changed |= ImGui::DragInt("Deflect window (frames)", &s.DeflectFrames, 0.2f, 0, 60);
		changed |= ImGui::DragInt("Deflect re-press lockout", &s.DeflectRepressLockout, 0.2f, 0, 120);
		changed |= ImGui::DragFloat("Move speed scale while guarding", &s.GuardMoveScale, 0.005f, 0.0f, 1.0f);
		changed |= ImGui::DragFloat("Knockback when deflected (power)", &s.DeflectedKnockback, 0.05f, 0.0f, 50.0f);
		changed |= ImGui::DragInt("Stun when deflected (frames)", &s.DeflectedStunFrames, 0.2f, 1, 180);

		// 回避（左 Shift / B）
		ImGui::SeparatorText("Dodge");
		ImGui::Text("Invincible %s", Player_->IsInvincible() ? "yes" : "no");
		changed |= ImGui::DragFloat("Dash speed", &s.DashSpeed, 0.01f, 0.0f, 10.0f);
		changed |= ImGui::DragFloat("Dash decay", &s.DashDecay, 0.005f, 0.0f, 0.99f);
		changed |= ImGui::DragInt("Dash frames", &s.DashFrames, 0.2f, 1, 120);
		changed |= ImGui::DragInt("Invincible start (frame)", &s.DodgeInvincibleStart, 0.2f, 0, 60);
		changed |= ImGui::DragInt("Invincible frames", &s.DodgeInvincibleFrames, 0.2f, 0, 60);
		if (changed) { Player_->SetSetup(s); }

		ImGui::SeparatorText("Hits taken from boss");
		ImGui::Text("Hit %u / Guarded %u / Deflected %u / Dodged %u",
			PlayerHitCounts_[0], PlayerHitCounts_[1], PlayerHitCounts_[2], PlayerHitCounts_[3]);
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset##hits")) { for (auto& c : PlayerHitCounts_) { c = 0U; } }
		ImGui::End();
	}
#endif

	template<>
	void InGame::Update(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::WinApp::RawInput const& input_
	) {
		// ポーズメニュー（ESC）。開いている間（と、決定で閉じたあと決定キーを離すまで）はゲームを丸ごと止める。
		// Render はそのまま止まった状態を描き続け、その上に暗幕とメニューを重ねる
		switch (PauseMenu_.Update(input_)) {
			case Game::PauseMenu::Action::Restart:
				RestartBattle();
				break;
			case Game::PauseMenu::Action::ReturnToTitle:
				ReturnToTitle();			// 次に InGame に入ったときは最初から。切り替えはこのフレームの描画の後
				break;
			default:
				break;
		}
#if defined(_DEBUG)
		// エディタの置き場（右の列・メニュー・F2）。ほかのウィンドウより前に。表示しないウィンドウは DrawEditor も呼ばない
		DebugDock_.Begin(input_.Keyboard().IsPressed(Lumina::WinApp::KEY::F2));
		if (DebugDock_.Visible("Pause Menu")) { PauseMenu_.DrawEditor(); }
		if (DebugDock_.Visible("GameOver Grade")) { GradeFx_.DrawEditor(); }		// ポーズ中も触れるよう、ゲームを止める判定より前で呼ぶ
		if (DebugDock_.Visible("GameOver Camera")) { CameraFx_.DrawEditor(); }
		if (DebugDock_.Visible("GameOver Shatter")) { Shatter_.DrawEditor(); }
		if (DebugDock_.Visible("GameOver Director")) { GameOver_.DrawEditor(); }	// 色抜け・カメラの後ろ（スクラブ中はこれらの時計を上書きするため）
		if (DebugDock_.Visible("Victory Camera")) { VictoryCameraFx_.DrawEditor(); }
		if (DebugDock_.Visible("Victory Grade")) { VictoryGradeFx_.DrawEditor(); }
		if (DebugDock_.Visible("Victory Director")) { Victory_.DrawEditor(); }		// 勝利演出（色抜け・カメラの後ろ）
		if (DebugDock_.Visible("Player Input")) { PlayerInput_.DrawEditor(); }
#endif
		// プレイヤーとカメラの入力を取り込む（1 フレーム 1 回）。ゲームを止めている間は Sync だけにして、
		// 止めている間に押したキー（ポーズを閉じた決定キーなど）で、再開した瞬間に行動が出ないようにする
		// 視点を回せるのは IN_BATTLE の間だけ（開始の VS バナー・勝敗の後・ゲームオーバーの演出中は止め、カーソルも解放する）
		bool const gameBlocked{ PauseMenu_.IsBlockingGame() };
		bool const cameraControl{ CurrentPhase_ == PHASE_IN_BATTLE && !GameOver_.IsActive() && !Victory_.IsActive() };
		PlayerInput_.Update(input_, gameBlocked, cameraControl);
		if (gameBlocked) { return; }

		// 視点の左右（Look の X が正 = 右へ回す = Yaw が減る）。移動の基準は回した後の向き（プレイヤーの Update より前に渡す）
		if (cameraControl) { PlayerCamera_->Yaw -= PlayerInput_.LookDelta().X; }
		PlayerMoveFrame_ = PlayerCamera_->GroundForward();

		// ゲームオーバーの序盤は、演出の時計（倒れるモーション・色抜け・カメラ・粉々の時刻）だけ遅く進める（スローモーション）。
		// 色抜けとカメラは自分の時計を持つので、Director と同じ倍率の dt を渡してそろえる。ボスや弾などの世界の動きは変えない
		// 勝利演出も同じ（色抜けとカメラは勝利演出用の 2 台目）
		float const gameOverDt{ (1.0f / 60.0f) * GameOver_.TimeScale() };
		float const victoryDt{ (1.0f / 60.0f) * Victory_.TimeScale() };
		GradeFx_.Update(gameOverDt);
		VictoryGradeFx_.Update(victoryDt);
		CameraFx_.Update(gameOverDt);
		VictoryCameraFx_.Update(victoryDt);
		GameOver_.Update(1.0f / 60.0f);		// 実時間を渡す（中で倍率を掛ける）
		Victory_.Update(1.0f / 60.0f);

		// エディタの Replay（Victory Director）で演出だけ始めた場合も、本当に勝ったときと同じ流れ（WIN フェーズ）にする（SPACE で暗転できるように）
		if (Victory_.IsRunning() && CurrentPhase_ == PHASE_IN_BATTLE) {
			CurrentPhase_ = PHASE_WIN;
			NextPhaseCountDown_ = 90;
		}

		// ゲームオーバーの演出中は、フェーズに関係なくプレイヤーの入力を受け付けない（LOSE に入る前の IN_BATTLE でも、
		// エディタの Replay / Scrub のときも）。IN_BATTLE のままでも Player::Update はキーを読まずに重力だけ進める
		Player_->LockInput(GameOver_.IsActive() || Victory_.IsActive());

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

		// 戦闘中でなければ攻撃をやめる（勝敗が決まった瞬間に振っていた場合など。WIN では体の Update を呼ばないため）
		if (CurrentPhase_ != PHASE_IN_BATTLE) {
			Player_->ResetCombo();
			playerATKFlag = 0;
		}

		if (Player_->IsTriggered_Jump()) {
			PlayerJumpEffectEmitFrameCount = 15;
		}
		if (Player_->IsTriggered_Dash()) {
			PlayerDashEffectEmitFrameCount = 15;
		}

		HitCameraShake_ *= 0.9f;
		// カメラはプレイヤーを追う（ボスを操作している間（Debug）はボスを追う）
		UpdatePlayerCamera(*PlayerCamera_, Boss_Kinoko_->Debug.Possess ? static_cast<Player const&>(*Boss_Kinoko_) : static_cast<Player const&>(*Player_), HitCameraShake_);
		CameraFx_.SetShake(GradeFx_.SplitIntensity());		// カメラシェークは、チャンネルスプリットの脈動に合わせる（包絡 × 鼓動。演出中でなければ 0）
		CameraFx_.Apply(*PlayerCamera_);		// ゲームオーバーの演出中だけ View / VP / FOV を上書き（以降の viewToWorld などはこの結果を使う）
		// 勝利演出のカメラ。「もう一人」= ボスの足元（Focus でボスへ注視点を寄せる。YawFromOther ならプレイヤーの背中側が Yaw 0）
		{
			auto const bossFeet{ Boss_Kinoko_->ModelTranslate() };
			VictoryCameraFx_.SetOther(Lumina::Float3{ bossFeet.x, bossFeet.y, bossFeet.z });
			VictoryCameraFx_.SetShake(VictoryGradeFx_.SplitIntensity());
			VictoryCameraFx_.Apply(*PlayerCamera_);
		}

		if (!GameOver_.IsRewinding()) {		// 巻き戻し中は止める（位置と向きは Lose() が決める）
			// ボスの体と脳。動けるのは IN_BATTLE の間だけ（ゲームオーバーの演出中は止める）。
			// 窓とクリップの終わりは前のフレームの終わりのもの（プレイヤーと同じ）
			Game::BossUpdate update{
				.Target = Player_.get(),
				.ControlEnabled = CurrentPhase_ == PHASE_IN_BATTLE && !GameOver_.IsActive(),
				.WindowAllow = BossAnimEvents_.CancelAllowMask(),
				.ClipFinished = BossAnimEvents_.IsLiveFinished(),
				.CanAttack = !BossAnimEvents_.IsPreviewing(),
				.Cues = BossAnimEvents_.FiredCues(),			// 前のフレームの終わりに鳴った合図（スキルの踏み込み・撃つ時刻）
			};
			if (Boss_Kinoko_->Debug.Possess) {
				// Debug: プレイヤーの入力でボスを動かす（プレイヤーの体には InBattle で何も始めさせていない）
				update.ManualIntent = PlayerFrame(true).Intent;
				update.ManualBuffer = &PlayerInput_.Buffer();
			}
			Boss_Kinoko_->Update(update);
			if (Boss_Kinoko_->ComboStepStarted() || Boss_Kinoko_->AbilityStarted() || Boss_Kinoko_->IsTriggered_Dash()) {
				BossAnimEvents_.RestartLive();		// 同じクリップでも頭から
			}

			// 体どうしが重ならないように押し分ける（ダメージ・のけぞりなし）。ボスの方が重い（プレイヤーが 3/4 動く）
			Game::SeparateBodies(*Player_, *Boss_Kinoko_, 0.75f);
		}

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

		// 当たりの結果を Game.CombatEvents に積む（演出は Game.CombatVfx が受け取って出す。配るのは Update の終わり）
		auto const publishHit{
			[](Game::CombatEvents::Side victim_, Game::HitResult result_, Game::CombatEvents::Source from_,
				Lumina::Vec3 const& point_, Lumina::Vec2 const& dir_, float damage_) -> void {
				Game::CombatEvents::Publish(Game::CombatEvents::Hit{
					.Victim = victim_,
					.Result = static_cast<Game::CombatEvents::Outcome>(result_),	// 並びは Game::HitResult と同じ
					.From = from_,
					.Point = Lumina::Float3{ point_.x, point_.y, point_.z },
					.Dir = Lumina::Float2{ dir_.x, dir_.y },
					.Damage = damage_,
				});
			}
		};
		using Game::CombatEvents::Side;
		using Game::CombatEvents::Source;

		// Collision btw. Player and Boss

		Update(Tag_UpdateColliders{});
		CollisionWorld_.Detect();
#if defined(_DEBUG)
		if (DebugDock_.Visible("Collision")) { Update(Tag_CollisionDebugUI{}); }
		if (DebugDock_.Visible("Player")) { Update(Tag_PlayerDebugUI{}); }
		if (DebugDock_.Visible("AnimEvent Hitbox")) { PlayerAnimEvents_.DrawEditor(); }
		if (DebugDock_.Visible("AnimEvent Hitbox (Boss)")) { BossAnimEvents_.DrawEditor(); }
		if (DebugDock_.Visible("Boss AI")) { Boss_Kinoko_->DrawEditor(); }
		if (DebugDock_.Visible("AnimEvent VFX")) { AnimEventVfx_.DrawEditor(); }
		if (DebugDock_.Visible("Combat VFX")) { CombatVfx_.DrawEditor(); }
		if (DebugDock_.Visible("Scene Light")) { SceneLight_.DrawEditor(); }
		if (DebugDock_.Visible("VS Banner") && VsBanner_.DrawEditor()) {
			// Replay: STARTUP に戻して頭から（プレイヤーとボスの位置・HP はそのまま）
			VsBanner_.Reset();
			CurrentPhase_ = PHASE_STARTUP;
		}
#endif

		std::optional<Lumina::Phys::Hit> weaponHit{};
		std::optional<Lumina::Phys::Hit> bossWeaponHit{};
		for (auto const& pair : CollisionWorld_.Contacts()) {
			if (auto const hit{ pair.Match(CollisionLayer::BossAttack, CollisionLayer::PlayerBody) }) {
				bossWeaponHit = hit;
			}
			if (auto const hit{ pair.Match(CollisionLayer::PlayerWeapon, CollisionLayer::BossBody) }) {
				weaponHit = hit;
			}
		}

		Lumina::Vec3 const dPos{ Player_->WorldPosition() - Boss_Kinoko_->WorldPosition() };
		// 旧方式の「攻撃が当たりうる」条件（UseAnimEvent を切ったときだけ使う）
		bool const legacyWeaponGate{ playerATKFlag != 0 && !Boss_Kinoko_->IsBeingKnockedBack() };
		if (PlayerAnimEvents_.UseAnimEvent) {
			// 新方式: コライダは Hitbox の窓の間だけ有効。同じ窓では同じ相手に 1 回だけ当たる（ノックバック中でも当たる）。
			// ダメージとノックバックはサイドカーの値
			// ボスが回避の無敵の間は TryHit しない（この窓で「当たった」にならず、無敵が明けてまだ重なっていれば当たる）
			if (weaponHit && !Boss_Kinoko_->IsInvincible()) {
				if (auto const hitbox{ PlayerAnimEvents_.TryHit(Boss_Kinoko_->BodyCollider().ID()) }) {
					Lumina::Vec3 const contactPoint{ weaponHit->Info.Point };
					Game::HitResult const bossResult{ Boss_Kinoko_->ReceiveHit({ hitbox->Damage, hitbox->Knockback, Lumina::Vec2{ -dPos.x, -dPos.z } }) };
					publishHit(Side::Boss, bossResult, Source::Weapon, contactPoint, Lumina::Vec2{ -dPos.x, -dPos.z }, hitbox->Damage);
					switch (bossResult) {
					case Game::HitResult::Hit:
						emitKnockEffectParticles(contactPoint, 195.0f);
						HitCameraShake_ = 1.0f;
						break;
					case Game::HitResult::Guarded:
						emitKnockEffectParticles(contactPoint, 35.0f);		// 橙の火花
						break;
					case Game::HitResult::Deflected:
						Player_->Deflected(Lumina::Vec2{ dPos.x, dPos.z });		// 弾かれた: プレイヤーがのけぞる
						emitKnockEffectParticles(contactPoint, 50.0f);		// 黄色の火花
						HitCameraShake_ = 0.6f;
						break;
					case Game::HitResult::Dodged:
						break;
					}
				}
			}
		}
		else if (legacyWeaponGate) {
			if (weaponHit) {
				Lumina::Vec3 const contactPoint{ weaponHit->Info.Point };
				Boss_Kinoko_->KnockedBack(5.0f * playerATKFlag, { -dPos.x, -dPos.z });
				Boss_Kinoko_->ApplyDamage(100.0f * playerATKFlag);
				publishHit(Side::Boss, Game::HitResult::Hit, Source::Weapon, contactPoint, Lumina::Vec2{ -dPos.x, -dPos.z }, 100.0f * playerATKFlag);
				emitKnockEffectParticles(contactPoint, 195.0f);
				HitCameraShake_ = 1.0f;
			}
		}
		// 体当たり（プレイヤーの体がボスの体に触れると、プレイヤーが吹き飛んで HP が減る）は外した（2026-11-10。近接で戦いづらいため）。
		// 体どうしは、ボスの更新の後で SeparateBodies が押し分けるだけ

		// ボスの武器 → プレイヤー。同じ窓では同じ相手に 1 回だけ。ダメージはサイドカーの値 × DamageScale（ImGui「Boss AI」）。
		// ガード・弾き・回避の無敵は Fighter::ReceiveHit が決める。無敵の間は TryHit しない（無敵が明けてまだ重なっていれば当たる）
		{
			bool const dodging{ bossWeaponHit.has_value() && !GameOver_.IsActive() && Player_->IsInvincible() };
			if (dodging && !PlayerDodgingHitbox_) {
				++PlayerHitCounts_[static_cast<int>(Game::HitResult::Dodged)];
				publishHit(Side::Player, Game::HitResult::Dodged, Source::Weapon, Lumina::Vec3{ bossWeaponHit->Info.Point }, Lumina::Vec2{ dPos.x, dPos.z }, 0.0f);
			}
			PlayerDodgingHitbox_ = dodging;
		}
		if (bossWeaponHit && !GameOver_.IsActive() && !Player_->IsInvincible()) {
			if (auto const hitbox{ BossAnimEvents_.TryHit(Player_->BodyCollider().ID()) }) {
				Lumina::Vec3 const contactPoint{ bossWeaponHit->Info.Point };
				Game::HitResult const result{
					Player_->ReceiveHit({ hitbox->Damage * Boss_Kinoko_->Tuning().DamageScale, hitbox->Knockback, Lumina::Vec2{ dPos.x, dPos.z } })
				};
				++PlayerHitCounts_[static_cast<int>(result)];
				publishHit(Side::Player, result, Source::Weapon, contactPoint, Lumina::Vec2{ dPos.x, dPos.z }, hitbox->Damage * Boss_Kinoko_->Tuning().DamageScale);
				switch (result) {
				case Game::HitResult::Hit:
					emitKnockEffectParticles(contactPoint, -7.5f);
					break;
				case Game::HitResult::Guarded:
					emitKnockEffectParticles(contactPoint, 35.0f);
					break;
				case Game::HitResult::Deflected:
					Boss_Kinoko_->Deflected(Lumina::Vec2{ -dPos.x, -dPos.z });	// 弾いた: ボスがのけぞる
					emitKnockEffectParticles(contactPoint, 50.0f);
					HitCameraShake_ = 0.6f;
					break;
				case Game::HitResult::Dodged:
					break;
				}
			}
		}

		// 茨の波（ボスのスキル）: 列はボスの Update で進んでいる。見た目は仮（ノックの粒子を地面から出す）。
		// 当たりは列のまとまり（1 回の振りで出した扇）ごとに 1 回。先端が足元を通るときに無敵なら、すり抜けたことにしてそのまとまりはもう当たらない。
		// 弾いても撃った側はのけぞらない（離れているので）
		{
			auto& rndThorn{ Lumina::Random::Generator() };
			// 巻き戻し中はボスの Update（列を進めて作り直す）が止まっているので、前の茨を出し続けないように見ない
			auto const spikes{ Boss_Kinoko_->Waves().Spikes() };
			for (auto const& spike : spikes) {
				if (GameOver_.IsRewinding()) { break; }
				for (int i = 0; i < 2; ++i) {
					Particle p{};
					p.Translate = { spike.Position.x, 0.3f + 0.9f * static_cast<float>(i), spike.Position.z };
					p.Velocity.x = spike.Dir.x * 0.05f;
					p.Velocity.y = 0.0f;
					p.Velocity.z = spike.Dir.y * 0.05f;
					p.Scale.x = 9.0f - 3.0f * static_cast<float>(i);
					p.Scale.y = p.Scale.x;
					p.Rotate.z = rndThorn() * Inv_0xFFFFFFFF * std::numbers::pi_v<float> * 2.0f;
					p.Life = 24.0f;
					auto const rgb{ Lumina::Utils::Color::Convert(Lumina::Utils::Color::HSV{ 340.0f + rndThorn() * Inv_0xFFFFFFFF * 20.0f, 0.85f, 0.8f }) };
					p.RenderData.RGBA = { rgb.R, rgb.G, rgb.B, 0.08f };
					p.RenderData.DiffuseID = 0U;
					p.RenderData.DiffuseAtlasID = (rndThorn() % 5U) + 2U;
					KnockEffects_->Emit(std::move(p));
				}
			}
			if (!GameOver_.IsActive() && !Victory_.IsActive()) {
				for (auto const& contact : Boss_Kinoko_->Waves().Contacts()) {
					if (Player_->IsInvincible()) {
						++PlayerHitCounts_[static_cast<int>(Game::HitResult::Dodged)];
						publishHit(Side::Player, Game::HitResult::Dodged, Source::Thorn, Lumina::Vec3{ contact.Point }, contact.Dir, 0.0f);
						Boss_Kinoko_->Waves().MarkHit(contact.Group);		// すり抜けた（同じまとまりの別の列にも当たらない）
						continue;
					}
					Lumina::Vec3 const point{ contact.Point };
					Game::HitResult const result{
						Player_->ReceiveHit({ contact.Damage * Boss_Kinoko_->Tuning().DamageScale, contact.Knockback, contact.Dir })
					};
					Boss_Kinoko_->Waves().MarkHit(contact.Group);
					++PlayerHitCounts_[static_cast<int>(result)];
					publishHit(Side::Player, result, Source::Thorn, point, contact.Dir, contact.Damage * Boss_Kinoko_->Tuning().DamageScale);
					switch (result) {
					case Game::HitResult::Hit:		emitKnockEffectParticles(point, -7.5f); break;
					case Game::HitResult::Guarded:	emitKnockEffectParticles(point, 35.0f); break;
					case Game::HitResult::Deflected: emitKnockEffectParticles(point, 50.0f); break;
					case Game::HitResult::Dodged:	break;
					}
				}
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

		// ゲームオーバーで体が砕ける粒子。ディゾルブの閾値と出す位置は Director の経過秒から決まる
		// （パレットは前フレームのポーズ。ポーズ反映は、この関数の後ろの方にあるため）
		{
			Lumina::Mat4 const shatterMeshWorld{
				Lumina::Mat4::SRT(
					Player_->ModelScale() * 2.0f,
					Player_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
					Player_->ModelTranslate()
				)
			};
			Shatter_.Update(
				cmdList_,
				viewToWorld,
				GameOver_.Elapsed(),
				GameOver_.IsActive(),
				GameOver_.IsRunning() && !GameOver_.IsRewinding(),		// 巻き戻し中は粒子を出さない（時刻が戻るので、いまある粒子も消え、体が元に戻っていく）
				shatterMeshWorld,
				PlayerSkinnedInstance_->SkinCluster_
			);
		}
		// 勝利演出でボスが砕ける粒子。見た目は GameOver Shatter と同じ（演出中だけ設定を写す。エディタはプレイヤー側の 1 つ）
		{
			if (Victory_.IsActive()) { BossShatter_.Look = Shatter_.Look; }
			Lumina::Mat4 const bossShatterMeshWorld{
				Lumina::Mat4::SRT(
					Boss_Kinoko_->ModelScale() * 2.0f,
					Boss_Kinoko_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
					Boss_Kinoko_->ModelTranslate()
				)
			};
			BossShatter_.Update(
				cmdList_,
				viewToWorld,
				Victory_.Elapsed(),
				Victory_.IsActive(),
				Victory_.IsRunning(),
				bossShatterMeshWorld,
				BossSkinnedInstance_->SkinCluster_
			);
		}

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
						Boss_Kinoko_->ApplyDamage(b.ATK - b.Velocity.y * 3.0f);
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
						Player_->ApplyDamage(0.1f);
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

		// 戦闘の仮エフェクト（足元の胞子・ボスのオーラ・命中・予兆の煌めき）の点光源。前のフレームの終わりに集めたもの
		for (auto const& l : CombatVfx_.Lights()) {
			if (List_PointLight_.IsFull() || List_LocalToWorld_LightSphere_.IsFull()) { break; }
			auto& light{ List_PointLight_.New() };
			light.WorldPosition = { l.Position.x, l.Position.y, l.Position.z, 1.0f };
			light.RGB = l.RGB;
			light.Intensity = l.Intensity;

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
					(1.0f - Boss_Kinoko_->HPRatio()) * 0.4f,
					0.5f +
					(1.0f - Boss_Kinoko_->HPRatio()) * 0.4f
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
					(1.0f - Boss_Kinoko_->HPRatio()) * 0.4f,
					0.5f +
					(1.0f - Boss_Kinoko_->HPRatio()) * 0.4f
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

		// 被弾のポスエフェ（Basic.PS）。タイマーは 1 つ。
		// プレイヤー・ボスどちらかののけぞりが始まったら 0 からやり直し、のけぞりが終わっても毎フレーム進めて、弱まりきったら切る。
		// （以前はボス用・プレイヤー用の static のタイマーが同じ Time を取り合い、のけぞりの間しか進まず、フラグも 0 に戻らなかった。
		//   そのため、のけぞりが短いとき（ガード硬直 8 フレームなど）に強いまま画面に残っていた）
		{
			bool const playerKnocked{ Player_->IsBeingKnockedBack() != 0 };
			bool const bossKnocked{ Boss_Kinoko_->IsBeingKnockedBack() != 0 };
			if ((playerKnocked && !PrevPlayerKnockedBack_) || (bossKnocked && !PrevBossKnockedBack_)) {
				HitPostFxTime_ = 0.0f;
			}
			PrevPlayerKnockedBack_ = playerKnocked;
			PrevBossKnockedBack_ = bossKnocked;

			if (HitPostFxTime_ >= 0.0f) {
				HitPostFxTime_ += HitPostFxStep;		// 以前と同じく、最初のフレームが 0.01
				PPConstants_.Time = HitPostFxTime_;
				auto playerNDCPos = Lumina::Vec4{ Player_->WorldPosition() } * PlayerCamera_->VP;	// TODO: w で割っていない（クリップ座標のまま）
				PPConstants_.PlayerNDCPos = { playerNDCPos.x, playerNDCPos.y };
				PPConstants_.IsEnemySuccessfullyAttacking = 1U;
				if (HitPostFxTime_ >= HitPostFxEndTime) { HitPostFxTime_ = -1.0f; }	// 次のフレームから外す
			}
			else {
				PPConstants_.Time = 0.0f;
				PPConstants_.IsEnemySuccessfullyAttacking = 0U;
			}
			// （以前の書きかけ：ジャンプ・ダッシュでもかける案。シチュごとのポスエフェを作るときの参考に残す）
			/*if (PlayerJumpEffectEmitFrameCount || PlayerDashEffectEmitFrameCount) {
				time += 0.01f;
				PPConstants_.Time = time;
				auto playerNDCPos = Lumina::Vec4{ Player_->WorldPosition() } * PlayerCamera_->VP;
				PPConstants_.PlayerNDCPos = { playerNDCPos.x, playerNDCPos.y };
				PPConstants_.IsEnemySuccessfullyAttacking = 1;
			}*/
		}
		UB_PostProcessingConstants_.Store(&PPConstants_, sizeof(PostProcessingConstants), 0LLU);

		// HP が 0 になったその瞬間にゲームオーバー演出を始める（InBattle の判定は 91 フレームごとで、最大 1.5 秒ほど遅れる）。
		// ボスが先に倒れている場合は勝ち（InBattle の判定に任せる）
		if (CurrentPhase_ == PHASE_IN_BATTLE && Player_->IsDefeated() && !Boss_Kinoko_->IsDefeated()) {
			CurrentPhase_ = PHASE_LOSE;
			NextPhaseCountDown_ = 90;
			GameOver_.Start();
		}
		// ボスの HP が 0 になったその瞬間に勝利演出を始める（両方 0 なら勝ち。InBattle の判定は予備）
		if (CurrentPhase_ == PHASE_IN_BATTLE && Boss_Kinoko_->IsDefeated()) {
			CurrentPhase_ = PHASE_WIN;
			NextPhaseCountDown_ = 90;
			Victory_.Start();
		}

		switch (playerATKFlag) {
		case 0:
			if (auto const abilityClip{ Player_->AbilityClip() }; !abilityClip.empty()) {
				PlayAnimation(currentAnim_, currentAnimName_, animTimer_, IsAnimationLoop_, abilityClip, Player_->AbilityClipLoop());	// スキルの使用中
			}
			else if (Player_->IsDashingForward()) {
				PlayAnimation(currentAnim_, currentAnimName_, animTimer_, IsAnimationLoop_, "Dodge", false);	// 回避（前方宙返り）
			}
			else if (Player_->IsGuarding()) {
				PlayAnimation(currentAnim_, currentAnimName_, animTimer_, IsAnimationLoop_, "Guard", true);
			}
			else if (Player_->IsMoving()) {
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

		if (GameOver_.HasDyingPose()) {
			// 倒れるモーション：時刻は演出の時計が決める（クリップの終端で止まる）。武器は骨に付いてくる
			Lumina::CG3D::Update(
				PlayerSkinnedInstance_->SkinCluster_,
				PlayerSkinnedInstance_->Skeleton_,
				*GameOver_.DyingClip(),
				GameOver_.DyingTime()
			);
		}
		else if (currentAnim_) {
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

		// 武器の行列を、このフレームのポーズで作り直す（Render の weaponWorld・刃先の光はこれを使う）。
		// 以前は骨の更新より前（上の Tag_HoldWeapon）の値のまま描いていたので、武器が手より 1 フレーム遅れていた（速い振りで刃が手から外れて見える）。
		// 当たり判定は次のフレームの Tag_UpdateColliders でもう一度作る（そのときの骨もこのポーズ）
		Update(Tag_HoldWeapon{});

		// ボスのアニメ（プレイヤーと同じ選び方）。時刻はボスのアニメーションイベントが持つ（4 倍速・ループは折り返し・非ループは終端で止まる）
		switch (Boss_Kinoko_->ComboStep()) {
		case 1:
			PlayAnimation(currentAnim_Boss_, currentAnimName_Boss_, animTimer_Boss_, IsAnimationLoop_Boss_, "ATK1", false);
			break;
		case 2:
			PlayAnimation(currentAnim_Boss_, currentAnimName_Boss_, animTimer_Boss_, IsAnimationLoop_Boss_, "ATK2", false);
			break;
		case 3:
			PlayAnimation(currentAnim_Boss_, currentAnimName_Boss_, animTimer_Boss_, IsAnimationLoop_Boss_, "ATK3", false);
			break;
		default:
			if (auto const abilityClip{ Boss_Kinoko_->AbilityClip() }; !abilityClip.empty()) {
				// スキル（溜め → 振り、のように途中でクリップが変わる。名前が変われば頭から再生される）
				PlayAnimation(currentAnim_Boss_, currentAnimName_Boss_, animTimer_Boss_, IsAnimationLoop_Boss_, abilityClip, Boss_Kinoko_->AbilityClipLoop());
			}
			else if (Boss_Kinoko_->IsDashingForward()) {
				// 前への踏み込みだけ宙返り（バックステップは今までどおり Run。前転で後ろへ下がると変なので）
				PlayAnimation(currentAnim_Boss_, currentAnimName_Boss_, animTimer_Boss_, IsAnimationLoop_Boss_, "Dodge", false);
			}
			else if (Boss_Kinoko_->IsGuarding()) {
				PlayAnimation(currentAnim_Boss_, currentAnimName_Boss_, animTimer_Boss_, IsAnimationLoop_Boss_, "Guard", true);
			}
			else if (Boss_Kinoko_->IsMoving()) {
				PlayAnimation(currentAnim_Boss_, currentAnimName_Boss_, animTimer_Boss_, IsAnimationLoop_Boss_, "Run", true);
			}
			else {
				PlayAnimation(currentAnim_Boss_, currentAnimName_Boss_, animTimer_Boss_, IsAnimationLoop_Boss_, "Idle", true);
			}
			break;
		}

		if (Victory_.HasDyingPose()) {
			// 勝利演出：ボスが倒れるモーション（ゲームオーバーと同じクリップ。時刻は Victory_ の時計）。武器は骨に付いてくる
			Lumina::CG3D::Update(
				BossSkinnedInstance_->SkinCluster_,
				BossSkinnedInstance_->Skeleton_,
				*Victory_.DyingClip(),
				Victory_.DyingTime()
			);
		}
		else if (currentAnim_Boss_) {
			BossAnimEvents_.Update(currentAnimName_Boss_, IsAnimationLoop_Boss_, 1.0f / 60.0f);
			auto const bossPose{ BossAnimEvents_.CurrentPose() };
			animTimer_Boss_ = bossPose.Time;		// 参考用
			if (bossPose.Clip) {
				Lumina::CG3D::Update(
					BossSkinnedInstance_->SkinCluster_,
					BossSkinnedInstance_->Skeleton_,
					*bossPose.Clip,
					bossPose.Time
				);
			}
		}
		// ボスの武器の行列も、このフレームのボスの姿勢で作り直す（Render の武器はこれを使う）
		Update(Tag_HoldBossWeapon{});

		SimpleFX3_->ClearBatch();
		SimpleFX3_->Batch(
			Lumina::Mat4::SRT(
				{ 3.0f, 3.0f, 3.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ Boss_Kinoko_->WorldPosition().x, Boss_Kinoko_->WorldPosition().y, Boss_Kinoko_->WorldPosition().z }
			)
		);

		// 戦闘の仮エフェクト。このフレームに積んだ出来事（命中）を配ってから、
		// 状態で決まるもの（軌跡・構え・予兆・足元）を作る。武器の行列は、このフレームのポーズで作り直した右手から
		// Render の weaponWorld / bossWeaponWorld と同じ式で作る（軌跡が描いた刃とずれないように）
		{
			Game::CombatEvents::Dispatch();

			Lumina::Mat4 const weaponSRT{
				Lumina::Mat4::SRT({ 0.125f, 0.5f, 0.125f }, { -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f })
			};
			Lumina::Mat4 const playerWeapon{ weaponSRT * World_Hands[1] };
			Lumina::Mat4 const bossWeapon{ weaponSRT * World_BossHand_ };

			auto const viewOf{
				[](Game::Fighter const& f_, Lumina::Mat4 const* weapon_) -> Game::CombatVfx::FighterView {
					Lumina::Vec3 const p{ f_.WorldPosition() };
					Lumina::Vec3 const v{ f_.ModelTranslate() };
					return Game::CombatVfx::FighterView{
						.Position = { p.x, p.y, p.z },
						.Visual = { v.x, v.y, v.z },
						.Angle = f_.Angle(),
						.Airborne = f_.IsAirborne(),
						.Dashing = f_.IsDashing() != 0,
						.DashTriggered = f_.IsTriggered_Dash() != 0,
						.Guarding = f_.IsGuarding(),
						.CanDeflect = f_.CanDeflectNow(),
						.GuardHalfAngle = f_.Setup().GuardHalfAngle,
						.Swinging = f_.Attacking() || f_.ActiveAbility() != nullptr,	// コンボ・スキルの間（刃先の速さで薄くなる）
						.Stunned = f_.Action() == Game::FighterAction::Stagger,		// のけぞり（ガード硬直は Guard なので入らない）
						.WeaponWorld = weapon_,
					};
				}
			};

			auto const& slash{ Boss_Kinoko_->SlashSkill() };
			auto const& wave{ Boss_Kinoko_->WaveSkill() };
			auto const& slashTuning{ Boss_Kinoko_->SlashTuning() };
			auto const& waveTuning{ Boss_Kinoko_->WaveTuning() };

			Game::CombatVfx::FrameInput frame{};
			frame.Player = viewOf(*Player_, &playerWeapon);
			frame.Boss = viewOf(*Boss_Kinoko_, &bossWeapon);
			frame.Skills = Game::CombatVfx::BossSkillView{
				.ChargeProgress = slash.ChargeProgress(),
				.SlamWindup = slash.IsSlamWindup(),
				.LungeStop = slashTuning.LungeStop,
				.LungeMax = slashTuning.LungeMax,
				.WindupProgress = wave.WindupProgress(),
				.WaveSlashing = wave.IsSlashing(),
				.WaveLines = waveTuning.Lines,
				.WaveSpread = waveTuning.Spread,
				.WaveStart = waveTuning.StartOffset,
				.WaveMaxDistance = waveTuning.MaxDistance,
				.WaveHalfWidth = waveTuning.HitRadius,
			};
			frame.ViewToWorld = &viewToWorld;
			frame.ArenaMin = Player_->Setup().ArenaMin;
			frame.ArenaMax = Player_->Setup().ArenaMax;
			frame.Suspended = GameOver_.IsActive() || Victory_.IsActive();
			CombatVfx_.Update(cmdList_, frame);
		}
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
	void InGame::Update() {
		Impl_->Update(Frame_.DX12, Frame_.CmdList, Frame_.Input);
	}
}