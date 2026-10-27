module Game.Scene.InGame;

import : Impl;

import Lumina;
import Lumina.VsBanner;

namespace Game::Scene::Impl {
	void InGame::RestartBattle() {
		CurrentPhase_ = PHASE_STARTUP;
		NextPhaseCountDown_ = 90;
		Player_->Initialize(Lumina::Vec3{ 10.0f, 0.25f, 0.0f });
		Boss_Kinoko_->Initialize(Lumina::Vec3{ -10.0f, 0.0f, 0.0f });
		UI_PlayerHPValue_ = Player_->HP_;	// HP バーの追従を満タンから始める（LOSE からだと 0 から伸び直していた）
		playerATKFlag = 0;
		// 飛んでいる弾は消す（弾の更新は IN_BATTLE だけなので、残すと STARTUP の間その場に止まって見え、当たり判定も残る）
		PlayerBullets_->Clear();
		BossBullets_->Clear();
		VsBanner_.Reset();					// STARTUP に入ったら頭から再生される
		GradeFx_.Reset();					// ゲームオーバーの色抜けを元に戻す
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