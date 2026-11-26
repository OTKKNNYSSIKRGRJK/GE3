module Game.Scene.InGame;

import : Impl;

import Lumina;
import Lumina.VsBanner;
import Game.CombatVfx;
import Game.Scene;
import Game.Scene.Title;	// 勝利演出の暗転のあと・ポーズメニューの Return to Title で、タイトルへの切り替えを頼む（ReturnToTitle）

namespace {
	// 戦闘開始時の初期配置（RestartBattle と、LOSE の巻き戻しの行き先）
	constexpr float PlayerStartX{ 10.0f };
	constexpr float PlayerStartZ{ 0.0f };
	constexpr float BossStartX{ -10.0f };
	constexpr float BossStartZ{ 0.0f };
	constexpr float Pi{ 3.14159265358979f };

	// 角度の差を [-π, π] に畳む（最短の回り方で戻すため。向きは 2π ずれても見た目は同じ）
	auto WrapAngle(float a_) -> float { return a_ - 2.0f * Pi * std::floor((a_ + Pi) / (2.0f * Pi)); }
}

namespace Game::Scene::Impl {
	void InGame::ReturnToTitle() {
		RestartBattle();		// 次に InGame に入ったときは最初から
		SceneManager_->RequestChange<Game::Scene::Title>();	// 切り替えはこのフレームの描画の後（SceneManager::ApplyChange）
	}

	void InGame::RestartBattle() {
		CurrentPhase_ = PHASE_STARTUP;
		NextPhaseCountDown_ = 90;
		Player_->Initialize(Lumina::Vec3{ PlayerStartX, 0.25f, PlayerStartZ });
		PlayerCamera_->Yaw = Player_->Angle();		// 視点もプレイヤーの後ろへ戻す
		Boss_Kinoko_->Initialize(Lumina::Vec3{ BossStartX, 0.0f, BossStartZ });
		UI_PlayerHPValue_ = Player_->HP();	// HP バーの追従を満タンから始める（LOSE からだと 0 から伸び直していた）
		playerATKFlag = 0;					// コンボ・アビリティは Player_->Initialize でやめている
		// 飛んでいる弾は消す（弾の更新は IN_BATTLE だけなので、残すと STARTUP の間その場に止まって見え、当たり判定も残る）
		PlayerBullets_->Clear();
		BossBullets_->Clear();
		VsBanner_.Reset();					// STARTUP に入ったら頭から再生される
		GameOver_.Reset();					// ゲームオーバーの色抜け・カメラワーク・倒れるモーションを元に戻す
		Victory_.Reset();					// 勝利演出も（色抜けは共用なので、どちらの Reset でも戻る）
		CombatVfx_.Clear();					// 軌跡・予兆・命中・粒子を消す
		HitPostFxTime_ = -1.0f;				// 被弾のポスエフェを切る（のけぞりの立ち上がりは次の Update で見直す）
		PPConstants_.IsEnemySuccessfullyAttacking = 0U;
	}

	template<>
	void InGame::Startup(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		Player_->Update(PlayerFrame(false));		// 動き出さない（重力だけ）

		// 弾の描画データ（RestartBattle で消したあとの数）を GPU に写す。弾が無いので更新関数は要らない
		PlayerBullets_->Update(cmdList_, Lumina::Mat4{});
		BossBullets_->Update(cmdList_, Lumina::Mat4{});

		// バトル開始の VS バナーを頭から再生し、終わったら IN_BATTLE へ（以前は 90 フレーム固定の待ち）
		if (VsBanner_.CurrentState() == Lumina::VsBanner::Presenter::State::Idle) {
			VsBanner_.Play();
		}
		VsBanner_.Update(cmdList_, 1.0f / 60.0f);	// パーティクルの描画データを写すコマンドも積む（パスの外なので OK）

		// HP バーはバナーの再生全体に合わせて伸ばす
		float const t = VsBanner_.Progress();
		float const easedT = (t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f));
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * easedT;
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * easedT;

		if (VsBanner_.IsFinished()) {
			VsBanner_.Reset();
			CurrentPhase_ = PHASE_IN_BATTLE;
			NextPhaseCountDown_ = 90;
		}
	}
	template<>
	void InGame::Lose(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		// ゲームプレイの入力は受け付けない（キーは読まない）。空中で倒れたら落ちて着地し、横移動と吹き飛ばしは止まる。
		// ロックは RestartBattle の Player_->Initialize で解ける
		// 巻き戻し中は、演出の時計の戻りに合わせて、プレイヤーとボスを初期配置へ連れ戻す（Update の前に決める。ボスの Update は止めてある）
		bool const rewinding{ GameOver_.IsRewinding() };
		if (rewinding) {
			float const p{ GameOver_.RewindProgress() };
			auto const mix{ [p](float from_, float to_) { return from_ + (to_ - from_) * p; } };
			constexpr float PlayerAngleEnd{ Pi * 0.5f };		// Player::Initialize の Angle_
			float const BossAngleEnd{ Boss_Kinoko_->Setup().InitialAngle };	// Boss::Initialize の向き
			Player_->SetRewindPose(
				mix(RewindPlayerFromX_, PlayerStartX),
				mix(RewindPlayerFromZ_, PlayerStartZ),
				RewindPlayerAngleFrom_ + WrapAngle(PlayerAngleEnd - RewindPlayerAngleFrom_) * p
			);
			Boss_Kinoko_->SetRewindPose(
				mix(RewindBossFromX_, BossStartX),
				mix(RewindBossFromZ_, BossStartZ),
				RewindBossRotateFrom_ + WrapAngle(BossAngleEnd - RewindBossRotateFrom_) * p
			);
			// 視点も戻す（巻き戻しの終わりで通常のカメラに戻ったとき、RestartBattle で向きが跳ばないように）
			PlayerCamera_->Yaw = RewindCameraYawFrom_ + WrapAngle(PlayerAngleEnd - RewindCameraYawFrom_) * p;
		}

		Player_->LockInput(true);
		Player_->Update(PlayerFrame(false));

		UI_PlayerHPValue_ = UI_PlayerHPValue_ * 0.9f + Player_->HP() * 0.1f;
		if (std::abs(UI_PlayerHPValue_ - Player_->HP()) < 1.0E-2) { UI_PlayerHPValue_ = Player_->HP(); }
		UI_PlayerHPValue_ = std::max<float>(0.0f, UI_PlayerHPValue_);
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * (UI_PlayerHPValue_ / Player_->MaxHP());
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * Boss_Kinoko_->HPRatio();

		auto const& keyboard{ input_.Keyboard() };

		using Lumina::WinApp::KEY;
		if (rewinding) {
			// 巻き戻しが終わったら再開（このフレームで位置は初期配置に着いている）
			if (GameOver_.IsRewindFinished()) {
				RestartBattle();
			}
		}
		else if (NextPhaseCountDown_ < 0 && GameOver_.CanRestart() && keyboard.IsPressed(KEY::SPACE)) {
			// SPACE：すぐ再開せず、演出を逆に再生する（巻き戻し）。いまの位置と向きを覚えて、初期配置へ戻していく
			RewindPlayerFromX_ = Player_->WorldPosition().x;
			RewindPlayerFromZ_ = Player_->WorldPosition().z;
			RewindPlayerAngleFrom_ = Player_->Angle();
			RewindCameraYawFrom_ = PlayerCamera_->Yaw;
			RewindBossFromX_ = Boss_Kinoko_->WorldPosition().x;
			RewindBossFromZ_ = Boss_Kinoko_->WorldPosition().z;
			RewindBossRotateFrom_ = Boss_Kinoko_->Angle();		// 体の向き（ModelRotate.y = -Angle）
			if (!GameOver_.BeginRewind()) {		// 演出が始まっていない（巻き戻すものがない）ならそのまま再開
				RestartBattle();
			}
		}
		else {
			--NextPhaseCountDown_;
		}
	}
	template<>
	void InGame::Win(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		UI_PlayerHPValue_ = UI_PlayerHPValue_ * 0.9f + Player_->HP() * 0.1f;
		if (std::abs(UI_PlayerHPValue_ - Player_->HP()) < 1.0E-2) { UI_PlayerHPValue_ = Player_->HP(); }
		UI_PlayerHPValue_ = std::max<float>(0.0f, UI_PlayerHPValue_);
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * (UI_PlayerHPValue_ / Player_->MaxHP());
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * Boss_Kinoko_->HPRatio();

		// プレイヤーは入力なしで重力だけ（勝った瞬間に空中でも落ちて着地する）。ロックは RestartBattle の Player_->Initialize で解ける
		Player_->LockInput(true);
		Player_->Update(PlayerFrame(false));

		auto const& keyboard{ input_.Keyboard() };

		using Lumina::WinApp::KEY;
		if (Victory_.IsExiting()) {
			// 暗転しきったらタイトルへ。次に InGame に入ったときは最初から（ポーズメニューの Return to Title と同じ）
			if (Victory_.IsExitFinished()) {
				ReturnToTitle();
			}
		}
		else if (NextPhaseCountDown_ < 0 && Victory_.CanRestart() && keyboard.IsPressed(KEY::SPACE)) {
			// SPACE：暗転してからタイトルへ（暗転の間も演出の時計とカメラは進む）。演出が始まっていなければすぐ
			if (!Victory_.BeginExit()) {
				ReturnToTitle();
			}
		}
		else {
			--NextPhaseCountDown_;
		}
	}
}