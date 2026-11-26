module Game.Boss;

//****	******	******	******	******	****//

import <cstddef>;
import <cstdint>;
import <optional>;
import <span>;
import <string>;
import <string_view>;

import Lumina.Math;
import Lumina.Input;

import Game.CollisionLayer;
import Game.Fighter;
import Game.BossBrain;
import Game.BossSkills;

//////	//////	//////	//////	//////	//////

namespace Game {
	auto BossSetup() -> FighterSetup {
		FighterSetup s{};
		s.BodyLayer = CollisionLayer::BossBody;
		s.BodyMask = CollisionLayer::PlayerBody | CollisionLayer::PlayerWeapon;
		s.WeaponLayer = CollisionLayer::BossAttack;
		s.WeaponMask = CollisionLayer::PlayerBody;

		// やられ判定はメッシュより大きめ（胴の幅だけだと、かなり近づかないと当たらなかった）。ワールドでは × 3（大きさ 1.5 × 2）
		s.BodyHalf = { 0.8f, 1.6f, 0.8f };
		s.BodyCenter = { 0.0f, 1.4f, 0.0f };
		s.PushRadius = 1.8f;

		s.MaxHP = 25000.0f;			// 旧ボスと同じ
		s.InitialAngle = 0.0f;		// +X（初期配置でプレイヤーのいる方）を向く

		s.MoveSpeed = 0.22f;		// 仮（プレイヤーは 0.25）
		s.TurnRate = 0.08f;			// 仮（約 275°/秒）。プレイヤーはすぐ向く
		s.KnockbackScale = 0.05f;	// 仮。ATK1（5）で 0.25、ATK3（15）で 0.75 押される
		s.HitStunFrames = 12;		// 仮（プレイヤーは 20）

		s.ModelScale = 1.5f;		// 旧ボスと同じ（プレイヤーより大きい）
		s.ModelBaseY = 0.0f;		// 旧ボスは浮かせていなかった
		s.FloatAmplitude = 0.0f;
		return s;
	}

	Boss::Boss() {
		SetSetup(BossSetup());
		SetAbility(AbilityGroup::Tool, BossSkillSlot::ChargeSlash, &Slash_);
		SetAbility(AbilityGroup::Tool, BossSkillSlot::ThornWave, &Wave_);
	}

	void Boss::Initialize(Lumina::Vec3 const& initPos_) {
		Brain_.Halt();
		Buffer_.Clear();
		Fighter::Initialize(initPos_);
		Waves_.Clear();
		Slash_.ResetCooldown();
		Wave_.ResetCooldown();
	}

	void Boss::Update(BossUpdate const& update_) {
		// Debug.NoKnockBack: 押されず、のけぞらない（ガード・弾かれも含む。ダメージは入る）。エディタで切り替えたら次のフレームから効く
		SetHitReactionEnabled(!Debug.NoKnockBack);
		Buffer_.Advance();
		// のけぞっている間に積んであった押下は捨てる（明けた瞬間に、のけぞる前の攻撃が出ないように）
		if (Action() == FighterAction::Stagger) { Buffer_.Clear(); }

		// スキル: 相手（追尾・距離）とクールダウン
		Slash_.SetTarget(update_.Target);
		Wave_.SetTarget(update_.Target);
		Slash_.Tick();
		Wave_.Tick();

		FighterFrame frame{
			.WindowAllow = update_.WindowAllow,
			.ClipFinished = update_.ClipFinished,
			.ControlEnabled = update_.ControlEnabled,
			.CanAttack = update_.CanAttack,
			.Cues = update_.Cues,
		};

		bool const manual{ Debug.Possess && update_.ManualIntent.has_value() };
		bool const think{ update_.ControlEnabled && !Debug.FreezeAI && !manual && !IsInputLocked() };
		if (manual) {
			Brain_.Halt();
			Buffer_.Clear();
			frame.Intent = *update_.ManualIntent;
			frame.Buffer = update_.ManualBuffer;
		}
		else if (think) {
			Brain_.Tick(*this, update_.Target, Tuning_, Buffer_);
			frame.Intent = Brain_.Intent();
			frame.Buffer = &Buffer_;
		}
		else {
			Brain_.Halt();
			Buffer_.Clear();
			frame.Buffer = &Buffer_;
		}

		Fighter::Update(frame);

		// 茨の列を進める（撃った後は体と関係なく進む）
		Waves_.Update(update_.Target, Setup().ArenaMin, Setup().ArenaMax);
	}

	void Boss::SetRewindPose(float x_, float z_, float angle_) noexcept {
		Fighter::SetRewindPose(x_, z_, angle_);
		SnapModel();
	}
}
