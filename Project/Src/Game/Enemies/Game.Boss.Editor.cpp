module Game.Boss;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれも Game.Boss.ixx で import 済み
import <cstddef>;
import <cstdint>;
import <optional>;
import <string>;
import <string_view>;

import Lumina.Math;
import Lumina.Input;
import Lumina.BT;

import Game.Fighter;
import Game.BossBrain;
import Game.BossSkills;
import Game.PlayerInput;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

//////	//////	//////	//////	//////	//////

#if defined(_DEBUG)
namespace {
	auto NameOf(Game::FighterAction a_) -> char const* {
		switch (a_) {
		case Game::FighterAction::Locked: return "Locked";
		case Game::FighterAction::Stagger: return "Stagger";
		case Game::FighterAction::Ability: return "Ability";
		case Game::FighterAction::Attack: return "Attack";
		case Game::FighterAction::Dash: return "Dash";
		case Game::FighterAction::Guard: return "Guard";
		case Game::FighterAction::Move: return "Move";
		case Game::FighterAction::Idle: return "Idle";
		}
		return "?";
	}

	auto NameOf(Lumina::BT::LastResult r_) -> char const* {
		switch (r_) {
		case Lumina::BT::LastResult::None: return "-";
		case Lumina::BT::LastResult::Running: return "Running";
		case Lumina::BT::LastResult::Success: return "Success";
		case Lumina::BT::LastResult::Failure: return "Failure";
		case Lumina::BT::LastResult::Halted: return "Halted";
		}
		return "?";
	}

	// ツリーの表示。最後の tick で通ったノードは明るく、走っているものは緑、通らなかったものは暗く
	void DrawNode(Lumina::BT::Node<Game::BossBlackboard> const& node_, std::uint32_t stamp_, int& id_) {
		bool const visited{ node_.LastStamp() == stamp_ && stamp_ != 0U };
		ImVec4 const color{
			!visited ? ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f } :
			node_.IsRunning() ? ImVec4{ 0.4f, 1.0f, 0.4f, 1.0f } :
			(node_.Last() == Lumina::BT::LastResult::Failure) ? ImVec4{ 1.0f, 0.5f, 0.4f, 1.0f } :
			ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f }
		};
		ImGui::PushID(id_++);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		std::size_t const n{ node_.ChildCount() };
		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth };
		if (n == 0U) { flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; }
		std::string_view const name{ node_.Name() };
		bool const open{ ImGui::TreeNodeEx("##node", flags, "%.*s  [%s]", static_cast<int>(name.size()), name.data(), NameOf(node_.Last())) };
		ImGui::PopStyleColor();
		if (open && n != 0U) {
			for (std::size_t i = 0; i < n; ++i) {
				if (auto const* child{ node_.Child(i) }) { DrawNode(*child, stamp_, id_); }
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}
#endif

namespace Game {
	void Boss::DrawEditor() {
#if defined(_DEBUG)
		ImGui::SetNextWindowSize(ImVec2{ 420.0f, 640.0f }, ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Boss AI")) {
			ImGui::End();
			return;
		}

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		ImGui::Checkbox("Freeze AI", &Debug.FreezeAI);
		ImGui::SameLine();
		ImGui::Checkbox("No hit reaction", &Debug.NoKnockBack);
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Player hits do not push or stagger the boss (damage still applies)"); }
		ImGui::Checkbox("Possess (keyboard / pad drives the boss, player stops)", &Debug.Possess);

		ImGui::SeparatorText("State");
		BossBlackboard const& bb{ Brain_.Blackboard() };
		ImGui::Text("Action: %s   Combo step: %d   HP: %.0f / %.0f", NameOf(Action()), ComboStep(), HP(), MaxHP());
		ImGui::Text("Distance: %.2f   (close < %.1f, mid < %.1f)", bb.Distance, Tuning_.CloseRange, Tuning_.MidRange);
		FighterIntent const& intent{ Brain_.Intent() };
		ImGui::Text("Intent move: (%.2f, %.2f)   face: %s", intent.Move.x, intent.Move.y, intent.Face ? "target" : "-");
		ImGui::Text("Buffered inputs: %d", static_cast<int>(Buffer_.Entries().size()));

		if (ImGui::CollapsingHeader("Body", ImGuiTreeNodeFlags_DefaultOpen)) {
			FighterSetup s{ Setup() };
			bool changed{ false };
			changed |= ImGui::DragFloat("Move speed", &s.MoveSpeed, 0.005f, 0.0f, 1.0f);
			changed |= ImGui::DragFloat("Turn rate (rad/frame)", &s.TurnRate, 0.002f, 0.0f, 1.0f);
			if (changed) { SetSetup(s); }		// その場で効く

			// のけぞり（押される距離 = プレイヤーのサイドカーの knockback × scale。ATK1〜3 は 5 / 10 / 15）
			changed = false;
			changed |= ImGui::DragFloat("Knockback scale", &s.KnockbackScale, 0.005f, 0.0f, 2.0f);
			changed |= ImGui::DragFloat("Knockback decay", &s.KnockbackDecay, 0.005f, 0.0f, 0.98f);
			changed |= ImGui::DragInt("Hit stun (frames)", &s.HitStunFrames, 0.2f, 1, 120);
			// プレイヤーに弾かれたとき・回避（ダッシュ）の無敵。ボスのガードはまだ BT に無い（Possess でなら使える）
			changed |= ImGui::DragFloat("Knockback when deflected (power)", &s.DeflectedKnockback, 0.05f, 0.0f, 50.0f);
			changed |= ImGui::DragInt("Stun when deflected (frames)", &s.DeflectedStunFrames, 0.2f, 1, 180);
			changed |= ImGui::DragInt("Dash invincible start (frame)", &s.DodgeInvincibleStart, 0.2f, 0, 60);
			changed |= ImGui::DragInt("Dash invincible frames", &s.DodgeInvincibleFrames, 0.2f, 0, 60);
			if (changed) { SetSetup(s); }

			float maxHP{ MaxHP() };
			if (ImGui::DragFloat("Max HP", &maxHP, 10.0f, 1.0f, 1000000.0f, "%.0f")) { SetMaxHP(maxHP); s.MaxHP = Setup().MaxHP; }

			// やられ判定（メッシュ空間。ワールドでは × 3）。Collision の窓の Boss Body と違い、戦闘をやり直しても残る
			bool shape{ false };
			shape |= ImGui::DragFloat3("Hurtbox half (mesh)", &s.BodyHalf.x, 0.01f, 0.05f, 5.0f);
			shape |= ImGui::DragFloat3("Hurtbox center (mesh)", &s.BodyCenter.x, 0.01f, -5.0f, 5.0f);
			shape |= ImGui::DragFloat("Push radius (world)", &s.PushRadius, 0.01f, 0.0f, 10.0f);
			if (shape) {
				SetSetup(s);
				RefreshBodyShape();
			}
		}

		// スキル。Use で今すぐ使わせる（先行入力に積むだけ。距離・クールダウンが合わなければ出ない）
		if (ImGui::CollapsingHeader("Skills", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto const status{ [this](BossSkill& s_, std::size_t slot_) {
				ImGui::Text("%-12.*s  phase: %-15.*s  cooldown: %3d  ready: %s",
					static_cast<int>(s_.Name().size()), s_.Name().data(),
					static_cast<int>(s_.PhaseName().size()), s_.PhaseName().data(),
					s_.CooldownLeft(), s_.CanStart(*this) ? "yes" : "no");
				ImGui::SameLine();
				ImGui::PushID(static_cast<int>(slot_));
				if (ImGui::SmallButton("Use")) {
					Buffer_.Push(Lumina::Input::ToActionId(PlayerInput::Act::Tool), static_cast<std::uint8_t>(slot_));
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset CD")) { s_.ResetCooldown(); }
				ImGui::PopID();
			} };
			status(Slash_, BossSkillSlot::ChargeSlash);
			status(Wave_, BossSkillSlot::ThornWave);
			ImGui::Text("Thorn lines active: %d", static_cast<int>(Waves_.ActiveLines()));

			if (ImGui::TreeNode("Charge slash")) {
				ChargeSlashTuning& s{ SlashTuning_ };
				ImGui::DragFloatRange2("Range", &s.MinRange, &s.MaxRange, 0.1f, 0.0f, 100.0f);
				ImGui::DragInt("Cooldown (frames)", &s.Cooldown, 1.0f, 0, 3600);
				ImGui::DragInt("Charge (frames)", &s.ChargeFrames, 0.5f, 1, 300);
				ImGui::DragFloat("Charge turn rate", &s.ChargeTurnRate, 0.002f, 0.0f, 1.0f);
				ImGui::DragFloat("Wind-up turn rate", &s.SlamTurnRate, 0.002f, 0.0f, 1.0f);
				ImGui::DragFloat("Lunge max", &s.LungeMax, 0.1f, 0.0f, 60.0f);
				ImGui::DragFloat("Lunge stop short", &s.LungeStop, 0.05f, 0.0f, 20.0f);
				ImGui::DragInt("Lunge (frames)", &s.LungeFrames, 0.2f, 1, 60);
				ImGui::DragInt("Lunge fallback frame", &s.LungeFallbackFrame, 0.2f, 0, 120);
				ImGui::DragInt("Super armor until", &s.ArmorUntil, 0.2f, 0, 120);
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Thorn wave")) {
				ThornWaveTuning& w{ WaveTuning_ };
				ImGui::DragFloatRange2("Range", &w.MinRange, &w.MaxRange, 0.1f, 0.0f, 150.0f);
				ImGui::DragInt("Cooldown (frames)", &w.Cooldown, 1.0f, 0, 3600);
				ImGui::DragInt("Wind-up (frames)", &w.WindupFrames, 0.5f, 1, 300);
				ImGui::DragFloat("Wind-up turn rate", &w.WindupTurnRate, 0.002f, 0.0f, 1.0f);
				ImGui::DragInt("Fire fallback frame", &w.FireFallbackFrame, 0.2f, 0, 120);
				ImGui::DragInt("Recover (frames)", &w.RecoverFrames, 0.5f, 0, 300);
				ImGui::SeparatorText("Lines");
				ImGui::DragInt("Lines", &w.Lines, 0.05f, 1, 9);
				ImGui::DragFloat("Spread (rad)", &w.Spread, 0.005f, 0.0f, 1.5f);
				ImGui::DragFloat("Start offset", &w.StartOffset, 0.05f, 0.0f, 20.0f);
				ImGui::DragFloat("Speed", &w.Speed, 0.01f, 0.01f, 5.0f);
				ImGui::DragFloat("Max distance", &w.MaxDistance, 0.5f, 1.0f, 150.0f);
				ImGui::DragFloat("Hit radius", &w.HitRadius, 0.02f, 0.0f, 10.0f);
				ImGui::DragFloat("Hit height (jump over)", &w.HitHeight, 0.02f, 0.0f, 10.0f);
				ImGui::DragFloat("Spike spacing", &w.SpikeSpacing, 0.02f, 0.1f, 10.0f);
				ImGui::DragFloat("Sine amplitude", &w.SineAmplitude, 0.02f, 0.0f, 10.0f);
				ImGui::DragFloat("Sine wavelength", &w.SineWavelength, 0.05f, 0.5f, 50.0f);
				ImGui::DragFloat("Damage (x damage scale)", &w.Damage, 1.0f, 0.0f, 5000.0f);
				ImGui::DragFloat("Knockback", &w.Knockback, 0.1f, 0.0f, 100.0f);
				ImGui::TreePop();
			}
		}

		if (ImGui::CollapsingHeader("Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (auto const* root{ Brain_.Tree().Root() }) {
				int id{ 0 };
				DrawNode(*root, Brain_.Tree().Stamp(), id);
			}
		}

		if (ImGui::CollapsingHeader("Tuning")) {
			BossTuning& t{ Tuning_ };
			ImGui::DragFloat("Close range", &t.CloseRange, 0.1f, 0.0f, 100.0f);
			ImGui::DragFloat("Mid range", &t.MidRange, 0.1f, 0.0f, 100.0f);
			ImGui::DragFloat("Approach speed", &t.ApproachSpeed, 0.01f, 0.0f, 1.0f);
			ImGui::DragInt("Approach timeout (frames)", &t.ApproachTimeout, 1.0f, 1, 1200);
			ImGui::DragIntRange2("Combo hits (close)", &t.ComboMinCloseHits, &t.ComboMaxCloseHits, 0.1f, 1, 3);
			ImGui::DragIntRange2("Combo hits (rush)", &t.ComboMinRushHits, &t.ComboMaxRushHits, 0.1f, 1, 3);
			ImGui::DragIntRange2("Recover (frames)", &t.RecoverMin, &t.RecoverMax, 0.5f, 0, 300);
			ImGui::DragIntRange2("Strafe (frames)", &t.StrafeMin, &t.StrafeMax, 0.5f, 1, 600);
			ImGui::DragFloat("Strafe speed", &t.StrafeSpeed, 0.01f, 0.0f, 1.0f);
			ImGui::SeparatorText("Weights");
			ImGui::DragFloat("Close: attack", &t.CloseAttackWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Close: retreat", &t.CloseRetreatWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Close: circle", &t.CloseCircleWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Mid: rush", &t.MidRushWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Mid: walk", &t.MidWalkWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Mid: circle", &t.MidCircleWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Mid: charge slash", &t.MidSlashWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Mid: thorn wave", &t.MidWaveWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Far: walk", &t.FarWalkWeight, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Far: thorn wave", &t.FarWaveWeight, 0.05f, 0.0f, 20.0f);
			ImGui::SeparatorText("Damage");
			ImGui::DragFloat("Damage scale (x sidecar)", &t.DamageScale, 0.005f, 0.0f, 2.0f);
		}

		ImGui::End();
#endif
	}
}
