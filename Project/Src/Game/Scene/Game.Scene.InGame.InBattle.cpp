module Game.Scene.InGame;

import : Impl;

import Lumina;

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

		constexpr float Inv_0xFFFFFFFF{ 1.0f / static_cast<float>(0xFFFFFFFFU) };
		Lumina::Mat4 const Mat4_I{};
	}

	template<>
	void InGame::InBattle(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		bool canMove = 1;
		Player_->Update(input_, canMove);

		Lumina::Vec3 const dPos{ Player_->WorldPosition() - Boss_Kinoko_->WorldPosition() };

		auto& rndEngine{ Lumina::Random::Generator() };

		auto emitPlayerBullets{
			[&, this] () -> void {
				auto dPos_Normalized{ dPos.Unit() };
				if (
					!std::isfinite(dPos_Normalized.x) ||
					!std::isfinite(dPos_Normalized.y) ||
					!std::isfinite(dPos_Normalized.z)
				) {
					dPos_Normalized = dPos;
				}
				for (int i = 0; i < 5; ++i) {
					PlayerBullet b{};
					{
						b.Translate = {
							Player_->WorldPosition().x,
							Player_->WorldPosition().y + 2.0f,
							Player_->WorldPosition().z,
						};

						float const angle{ Player_->Angle() + (i - 1) * 0.3f };

						b.Velocity.x = std::cos(angle) * 1.0f;
						b.Velocity.y = (-dPos_Normalized.y) * 1.0f;
						b.Velocity.z = std::sin(angle) * 1.0f;

						b.Translate.x += b.Velocity.x * 0.5f;
						b.Translate.y += b.Velocity.y * 0.5f;
						b.Translate.z += b.Velocity.z * 0.5f;

						b.Scale.x = 2.0f;
						b.Scale.y = 2.0f;

						b.Rotate.x = std::numbers::pi_v<float> * 0.5f + b.Velocity.y;
						b.Rotate.y = rndEngine() * Inv_0xFFFFFFFF * std::numbers::pi_v<float> * 2.0f;

						b.Life = 24.0f - b.Velocity.y * 12.0f;

						b.ATK = 5.0f;

						auto const rgb_Base = Lumina::Utils::Color::Convert(
							Lumina::Utils::Color::HSV{
								rndEngine() * Inv_0xFFFFFFFF * 120.0f + 60.0f,
								rndEngine() * Inv_0xFFFFFFFF * 0.7f + 0.3f,
								0.75f
							}
						);
						b.RenderData.RGBA = {
							rgb_Base.R,
							rgb_Base.G,
							rgb_Base.B,
							1.0f
						};
						b.RenderData.DiffuseID = 0U;
						b.RenderData.DiffuseAtlasID = 5U;
					}
					PlayerBullets_->Emit(std::move(b));
				}
			}
		};
		if (!Player_->IsBeingKnockedBack()) {
			emitPlayerBullets();
		}
		PlayerBullets_->Update(cmdList_, Mat4_I, UpdatePlayerBullet);

		UI_PlayerHPValue_ = UI_PlayerHPValue_ * 0.9f + Player_->HP() * 0.1f;
		if (std::abs(UI_PlayerHPValue_ - Player_->HP()) < 1.0E-2) { UI_PlayerHPValue_ = Player_->HP(); }
		UI_PlayerHPBar_.Scale_.x = MaxWidth_UI_PlayerHPBar_ * (UI_PlayerHPValue_ * Player_->Inv_MaxHP());
		UI_BossHPBar_.Scale_.x = MaxWidth_UI_BossHPBar_ * (Boss_Kinoko_->HP() * Boss_Kinoko_->Inv_MaxHP());

		if (NextPhaseCountDown_ < 0) {
			if (Boss_Kinoko_->HP() <= 0.0f) {
				CurrentPhase_ = PHASE_WIN;
			}
			else if (Player_->HP() <= 0.0f) {
				CurrentPhase_ = PHASE_LOSE;
			}
			NextPhaseCountDown_ = 90;
		}
		else {
			--NextPhaseCountDown_;
		}
	}
}