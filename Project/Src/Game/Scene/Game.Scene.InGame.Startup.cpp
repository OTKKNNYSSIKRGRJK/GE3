module Game.Scene.InGame;

import : Impl;

import Lumina;
import Lumina.VsBanner;

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
	void InGame::RestartBattle() {
		CurrentPhase_ = PHASE_STARTUP;
		NextPhaseCountDown_ = 90;
		Player_->Initialize(Lumina::Vec3{ PlayerStartX, 0.25f, PlayerStartZ });
		Boss_Kinoko_->Initialize(Lumina::Vec3{ BossStartX, 0.0f, BossStartZ });
		UI_PlayerHPValue_ = Player_->HP_;	// HP バーの追従を満タンから始める（LOSE からだと 0 から伸び直していた）
		playerATKFlag = 0;
		// 飛んでいる弾は消す（弾の更新は IN_BATTLE だけなので、残すと STARTUP の間その場に止まって見え、当たり判定も残る）
		PlayerBullets_->Clear();
		BossBullets_->Clear();
		VsBanner_.Reset();					// STARTUP に入ったら頭から再生される
		GameOver_.Reset();					// ゲームオーバーの色抜け・カメラワーク・倒れるモーションを元に戻す
	}

	template<>
	void InGame::Startup(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		bool canMove = 0;
		Player_->Update(input_, canMove);

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
			constexpr float BossRotateEnd{ -Pi * 0.5f };		// Boss_Kinoko::Initialize の ModelRotate_.y
			Player_->SetRewindPose(
				mix(RewindPlayerFromX_, PlayerStartX),
				mix(RewindPlayerFromZ_, PlayerStartZ),
				RewindPlayerAngleFrom_ + WrapAngle(PlayerAngleEnd - RewindPlayerAngleFrom_) * p
			);
			Boss_Kinoko_->SetRewindPose(
				mix(RewindBossFromX_, BossStartX),
				mix(RewindBossFromZ_, BossStartZ),
				RewindBossRotateFrom_ + WrapAngle(BossRotateEnd - RewindBossRotateFrom_) * p
			);
		}

		bool const noMove{ false };
		Player_->LockInput(true);
		Player_->Update(input_, noMove);

		UI_PlayerHPValue_ = UI_PlayerHPValue_ * 0.9f + Player_->HP_ * 0.1f;
		if (std::abs(UI_PlayerHPValue_ - Player_->HP_) < 1.0E-2) { UI_PlayerHPValue_ = Player_->HP_; }
		UI_PlayerHPValue_ = std::max<float>(0.0f, UI_PlayerHPValue_);
		Boss_Kinoko_->HP_ = std::max<float>(0.0f, Boss_Kinoko_->HP_);
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * (UI_PlayerHPValue_ * Player_->Inv_MaxHP_);
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * (Boss_Kinoko_->HP_ * Boss_Kinoko_->Inv_MaxHP_);

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
			RewindBossFromX_ = Boss_Kinoko_->WorldPosition().x;
			RewindBossFromZ_ = Boss_Kinoko_->WorldPosition().z;
			RewindBossRotateFrom_ = Boss_Kinoko_->ModelRotate().y;
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
		UI_PlayerHPValue_ = UI_PlayerHPValue_ * 0.9f + Player_->HP_ * 0.1f;
		if (std::abs(UI_PlayerHPValue_ - Player_->HP_) < 1.0E-2) { UI_PlayerHPValue_ = Player_->HP_; }
		UI_PlayerHPValue_ = std::max<float>(0.0f, UI_PlayerHPValue_);
		Boss_Kinoko_->HP_ = std::max<float>(0.0f, Boss_Kinoko_->HP_);
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * (UI_PlayerHPValue_ * Player_->Inv_MaxHP_);
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * (Boss_Kinoko_->HP_ * Boss_Kinoko_->Inv_MaxHP_);

		auto const& keyboard{ input_.Keyboard() };

		using Lumina::WinApp::KEY;
		if (NextPhaseCountDown_ < 0 && keyboard.IsPressed(KEY::SPACE)) {
			RestartBattle();
		}
		else {
			--NextPhaseCountDown_;
		}
	}
}