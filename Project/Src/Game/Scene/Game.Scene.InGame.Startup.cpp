module Game.Scene.InGame;

import : Impl;

import Lumina;

namespace Game::Scene::Impl {
	template<>
	void InGame::Startup(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		bool canMove = 0;
		Player_->Update(input_, canMove);

		float const t = 1.0f - static_cast<float>(NextPhaseCountDown_) / 90.0f;
		float const easedT = (t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f));
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * easedT;
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * easedT;

		if (NextPhaseCountDown_ < 0) {
			CurrentPhase_ = PHASE_IN_BATTLE;
			NextPhaseCountDown_ = 90;
		}
		else {
			--NextPhaseCountDown_;
		}
	}
	template<>
	void InGame::Lose(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		UI_PlayerHPValue_ = UI_PlayerHPValue_ * 0.9f + Player_->HP() * 0.1f;
		if (std::abs(UI_PlayerHPValue_ - Player_->HP()) < 1.0E-2) { UI_PlayerHPValue_ = Player_->HP(); }
		UI_PlayerHPValue_ = std::max<float>(0.0f, UI_PlayerHPValue_);
		Boss_Kinoko_->HP(std::max<float>(0.0f, Boss_Kinoko_->HP()));
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * (UI_PlayerHPValue_ * Player_->Inv_MaxHP());
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * (Boss_Kinoko_->HP() * Boss_Kinoko_->Inv_MaxHP());

		auto const& keyboard{ input_.Keyboard() };

		using Lumina::WinApp::KEY;
		if (NextPhaseCountDown_ < 0 && keyboard.IsPressed(KEY::SPACE)) {
			CurrentPhase_ = PHASE_STARTUP;
			NextPhaseCountDown_ = 90;
			Player_->Initialize(Lumina::Vec3{ 10.0f, 0.25f, 0.0f });
			Boss_Kinoko_->Initialize(Lumina::Vec3{ -10.0f, 0.0f, 0.0f });
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
		Boss_Kinoko_->HP(std::max<float>(0.0f, Boss_Kinoko_->HP()));
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * (UI_PlayerHPValue_ * Player_->Inv_MaxHP());
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * (Boss_Kinoko_->HP() * Boss_Kinoko_->Inv_MaxHP());

		auto const& keyboard{ input_.Keyboard() };

		using Lumina::WinApp::KEY;
		if (NextPhaseCountDown_ < 0 && keyboard.IsPressed(KEY::SPACE)) {
			CurrentPhase_ = PHASE_STARTUP;
			NextPhaseCountDown_ = 90;
			Player_->Initialize(Lumina::Vec3{ 10.0f, 0.25f, 0.0f });
			Boss_Kinoko_->Initialize(Lumina::Vec3{ -10.0f, 0.0f, 0.0f });
		}
		else {
			--NextPhaseCountDown_;
		}
	}
}