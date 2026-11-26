module Game.Scene.InGame;

import : Impl;

import Lumina;
import Lumina.Input;
import Game.PlayerInput;

namespace Game::Scene::Impl {
	namespace {
		bool UpdatePlayerBullet(PlayerBullet& b_, void const*) {
			b_.Translate.x += b_.Velocity.x;
			b_.Translate.y += b_.Velocity.y;
			b_.Translate.z += b_.Velocity.z;
			b_.Rotate.y += b_.Velocity.z * 0.1f;
			b_.RenderData.RGBA.w *= 0.82f;
			b_.Life -= 1.0f;
			b_.ATK *= 0.8f;
			return (b_.Life > 0.0f);
		}
		bool UpdateBossBullet(Particle& b_, void const*) {
			b_.Translate.x += b_.Velocity.x;
			b_.Translate.y += b_.Velocity.y;
			b_.Translate.z += b_.Velocity.z;
			b_.RenderData.RGBA.w *= 0.9f;
			b_.Life -= 1.0f;
			return (b_.Life > 0.0f);
		}

		Lumina::Mat4 const Mat4_I{};
	}

	// プレイヤーの脳: カメラ基準の入力をワールド基準の Intent に直し、先行入力とアニメの状態と一緒に渡す。
	// 前 = PlayerMoveFrame_（カメラの GroundForward）、右 = 前を右へ 90° 回した向き（XZ で (f.y, -f.x)。Angle の測り方では右へ回すと角度が減る）
	auto InGame::PlayerFrame(bool controlEnabled_) -> Game::FighterFrame {
		using Game::PlayerInput::Act;
		auto const& input{ PlayerInput_.State() };
		Lumina::Input::Axis2 const move{ input.Axis(Act::Move) };	// 長さ 1 以下（InputMap で収めてある）
		Lumina::Vec2 const f{ PlayerMoveFrame_ };
		return Game::FighterFrame{
			.Intent = Game::FighterIntent{
				.Move = Lumina::Vec2{ f.x * move.Y + f.y * move.X, f.y * move.Y - f.x * move.X },
				.GuardHeld = input.Down(Act::Guard),
			},
			.Buffer = &PlayerInput_.Buffer(),
			.WindowAllow = PlayerAnimEvents_.CancelAllowMask(),
			.ClipFinished = PlayerAnimEvents_.IsLiveFinished(),
			.ControlEnabled = controlEnabled_,
			.CanAttack = !PlayerAnimEvents_.IsPreviewing(),	// エディタのプレビュー中は攻撃しない（歩くのはよい）
		};
	}

	template<>
	void InGame::InBattle(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		// プレイヤーの体（Game.Fighter）。コンボ・回避・ジャンプ・アビリティを受け付けるかは、体が自分の状態で決める。
		// 次の段へ移れるのは、今の振りのクリップの Cancel 窓（Allow に Attack）が開いている間だけ。窓の状態は前のフレームの終わりのもの
		// （当たり判定の Hitbox と同じ時刻）。窓より前に押した攻撃は先行入力に残っていて、窓が開いた瞬間に出る
		Player_->Update(PlayerFrame(!Boss_Kinoko_->Debug.Possess));	// ボスを操作している間（Debug）は動かない
		if (Player_->ComboStepStarted() || Player_->AbilityStarted() || Player_->IsTriggered_Dash()) {
			PlayerAnimEvents_.RestartLive();			// 同じクリップでも頭から（3 段目 → 1 段目、終端で始め直し、回避の連続）
		}
		playerATKFlag = Player_->ComboStep();		// アニメの選択と、旧方式の当たり判定・体当たりの分岐はこれを見る

		auto const& keyboard{ input_.Keyboard() };
		using Lumina::WinApp::KEY;

		// Q: 自害（公開デモ用。HP を 0 にするだけで、あとは本当に死んだときと同じ流れ = LOSE → ゲームオーバー演出 → SPACE で再開）
		if (keyboard.IsPressed(KEY::Q) && !Player_->IsInputLocked() && Player_->HP_ > 0.0f) {
			Player_->HP_ = 0.0f;
		}

		PlayerBullets_->Update(cmdList_, Mat4_I, UpdatePlayerBullet);
		BossBullets_->Update(cmdList_, Mat4_I, UpdateBossBullet);

		UI_PlayerHPValue_ = UI_PlayerHPValue_ * 0.9f + Player_->HP_ * 0.1f;
		if (std::abs(UI_PlayerHPValue_ - Player_->HP_) < 1.0E-2) { UI_PlayerHPValue_ = Player_->HP_; }
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * (UI_PlayerHPValue_ * Player_->Inv_MaxHP_);
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * (Boss_Kinoko_->HP_ * Boss_Kinoko_->Inv_MaxHP_);

		if (NextPhaseCountDown_ < 0) {
			if (Boss_Kinoko_->HP_ <= 0.0f) {
				CurrentPhase_ = PHASE_WIN;
				if (!Victory_.IsActive()) { Victory_.Start(); }		// 通常は Update.cpp が HP 0 になった瞬間に開始済み
			}
			else if (Player_->HP_ <= 0.0f) {
				CurrentPhase_ = PHASE_LOSE;
				GameOver_.Start();			// 色抜け・カメラ・倒れるモーション・結果表示を同じ時刻から（通常は Update.cpp が HP 0 になった瞬間に開始済み）
			}
			NextPhaseCountDown_ = 90;
		}
		else {
			--NextPhaseCountDown_;
		}
	}
}