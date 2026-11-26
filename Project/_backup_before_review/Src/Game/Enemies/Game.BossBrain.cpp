module Game.BossBrain;

//****	******	******	******	******	****//

import <array>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <memory>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

import Lumina.Math;
import Lumina.Input;
import Lumina.BT;

import Game.Fighter;
import Game.PlayerInput;
import Game.BossSkills;

//////	//////	//////	//////	//////	//////

namespace Game {
	//====	======	======	======	======	====//
	//	ブラックボード							//
	//====	======	======	======	======	====//

	auto BossBlackboard::NextRandom() noexcept -> std::uint32_t {
		std::uint32_t x{ RngState };
		x ^= x << 13U;
		x ^= x >> 17U;
		x ^= x << 5U;
		RngState = x;
		return x;
	}

	auto BossBlackboard::RandomRange(std::int32_t min_, std::int32_t max_) noexcept -> std::int32_t {
		if (max_ <= min_) { return min_; }
		auto const span{ static_cast<std::uint32_t>(max_ - min_) + 1U };
		return min_ + static_cast<std::int32_t>(NextRandom() % span);
	}

	auto BossBlackboard::RandomUnit() noexcept -> float {
		return static_cast<float>(NextRandom() >> 8U) * (1.0f / 16777216.0f);
	}

	void BossBlackboard::Press(Lumina::Input::ActionId action_, std::uint8_t arg_) const noexcept {
		if (Buffer != nullptr) { Buffer->Push(action_, arg_); }
	}
}

namespace {
	using Lumina::BT::Status;
	using Lumina::BT::Node;
	using Lumina::BT::NodePtr;
	using Lumina::BT::Make;
	using BB = Game::BossBlackboard;
	using Game::BossTuning;
	using Game::PlayerInput::Act;

	constexpr auto ActId(Act a_) noexcept -> Lumina::Input::ActionId { return Lumina::Input::ToActionId(a_); }

	auto Scaled(Lumina::Vec2 v_, float s_) -> Lumina::Vec2 { return Lumina::Vec2{ v_.x * s_, v_.y * s_ }; }
	auto Added(Lumina::Vec2 a_, Lumina::Vec2 b_) -> Lumina::Vec2 { return Lumina::Vec2{ a_.x + b_.x, a_.y + b_.y }; }
	auto Clamp(float v_, float lo_, float hi_) -> float { return (v_ < lo_) ? lo_ : ((v_ > hi_) ? hi_ : v_); }

	//==== リーフ ====//

	// 何もしない（Intent は空のまま）。ずっと Running
	class Idle : public Node<BB> {
	public:
		using Node<BB>::Node;
	protected:
		auto OnTick(BB&) -> Status override { return Status::Running; }
	};

	// 相手へ近づく（向きの指定なし = 進む方向へ旋回速度で回る）。range より近づいたら Success。長く続いたら Success（選び直す）
	class Approach : public Node<BB> {
	public:
		Approach(std::string name_, float BossTuning::* range_) : Node<BB>{ std::move(name_) }, Range_{ range_ } {}
	protected:
		void OnEnter(BB&) override { Frames_ = 0; }
		auto OnTick(BB& bb_) -> Status override {
			BossTuning const& t{ *bb_.Tuning };
			if (bb_.Distance <= t.*Range_) { return Status::Success; }
			if (++Frames_ > t.ApproachTimeout) { return Status::Success; }
			bb_.Intent.Move = Scaled(bb_.ToTarget, Clamp(t.ApproachSpeed, 0.0f, 1.0f));
			return Status::Running;
		}
	private:
		float BossTuning::* Range_;
		std::int32_t Frames_{ 0 };
	};

	// 通常攻撃を n 段（入ったときに [min, max] から選ぶ）。段の頭で相手の方へ向くように、押している間は移動を相手へ向ける。
	// 最後の段を出したら押すのも移動もやめ、コンボが終わったら Success。始められなければ Failure
	class Combo : public Node<BB> {
	public:
		Combo(std::string name_, std::int32_t BossTuning::* minHits_, std::int32_t BossTuning::* maxHits_)
			: Node<BB>{ std::move(name_) }, Min_{ minHits_ }, Max_{ maxHits_ } {}
		auto Hits() const noexcept -> std::int32_t { return Hits_; }
	protected:
		void OnEnter(BB& bb_) override {
			BossTuning const& t{ *bb_.Tuning };
			std::int32_t const lo{ (t.*Min_ < 1) ? 1 : t.*Min_ };
			std::int32_t const hi{ (t.*Max_ < lo) ? lo : t.*Max_ };
			Hits_ = bb_.RandomRange(lo, hi);
			Started_ = 0;
			Waited_ = 0;
		}
		auto OnTick(BB& bb_) -> Status override {
			Game::Fighter const& self{ *bb_.Self };
			if (self.ComboStepStarted()) { ++Started_; }
			if (Started_ > 0 && self.ComboStep() == 0) { return Status::Success; }
			if (Started_ < Hits_) {
				bb_.Intent.Move = bb_.ToTarget;
				bb_.Press(ActId(Act::Attack));
			}
			if (Started_ == 0 && ++Waited_ > 10) { return Status::Failure; }
			return Status::Running;
		}
	private:
		std::int32_t BossTuning::* Min_;
		std::int32_t BossTuning::* Max_;
		std::int32_t Hits_{ 1 };
		std::int32_t Started_{ 0 };
		std::int32_t Waited_{ 0 };
	};

	// 立ち止まって相手の方へ向き直る（攻撃の後の隙）。[min, max] フレーム
	class Recover : public Node<BB> {
	public:
		Recover(std::string name_, std::int32_t BossTuning::* min_, std::int32_t BossTuning::* max_)
			: Node<BB>{ std::move(name_) }, Min_{ min_ }, Max_{ max_ } {}
	protected:
		void OnEnter(BB& bb_) override {
			Frames_ = bb_.RandomRange(bb_.Tuning->*Min_, bb_.Tuning->*Max_);
			Elapsed_ = 0;
		}
		auto OnTick(BB& bb_) -> Status override {
			bb_.Intent.Face = bb_.ToTarget;
			if (++Elapsed_ >= Frames_) { return Status::Success; }
			return Status::Running;
		}
	private:
		std::int32_t BossTuning::* Min_;
		std::int32_t BossTuning::* Max_;
		std::int32_t Frames_{ 0 };
		std::int32_t Elapsed_{ 0 };
	};

	// 相手を向いたままダッシュする（toward_ = true なら相手へ、false なら離れる = バックステップ）。
	// ダッシュが終わったら Success。始まらなければ Failure
	class DashStep : public Node<BB> {
	public:
		DashStep(std::string name_, bool toward_) : Node<BB>{ std::move(name_) }, Toward_{ toward_ } {}
	protected:
		void OnEnter(BB&) override { Pressed_ = false; Started_ = false; Waited_ = 0; }
		auto OnTick(BB& bb_) -> Status override {
			Game::Fighter const& self{ *bb_.Self };
			bb_.Intent.Face = bb_.ToTarget;
			bb_.Intent.Move = Toward_ ? bb_.ToTarget : Scaled(bb_.ToTarget, -1.0f);
			if (!Pressed_) {
				bb_.Press(ActId(Act::Dodge));
				Pressed_ = true;
				return Status::Running;
			}
			if (self.IsDashing()) { Started_ = true; }
			else if (Started_) { return Status::Success; }
			if (!Started_ && ++Waited_ > 6) { return Status::Failure; }
			return Status::Running;
		}
	private:
		bool Toward_;
		bool Pressed_{ false };
		bool Started_{ false };
		std::int32_t Waited_{ 0 };
	};

	// 相手を向いたまま横へ回る（左右は入ったときに選ぶ）。距離は入ったときの距離を保つ（CloseRange × 0.7 〜 MidRange × 0.8 に収める）。
	// abortInside_ なら、CloseRange × 0.8 より詰められたら Success（選び直して攻撃へ）
	class Strafe : public Node<BB> {
	public:
		Strafe(std::string name_, bool abortInside_) : Node<BB>{ std::move(name_) }, AbortInside_{ abortInside_ } {}
	protected:
		void OnEnter(BB& bb_) override {
			Side_ = (bb_.NextRandom() & 1U) ? 1.0f : -1.0f;
			Frames_ = bb_.RandomRange(bb_.Tuning->StrafeMin, bb_.Tuning->StrafeMax);
			Elapsed_ = 0;
			Keep_ = Clamp(bb_.Distance, bb_.Tuning->CloseRange * 0.7f, bb_.Tuning->MidRange * 0.8f);
		}
		auto OnTick(BB& bb_) -> Status override {
			BossTuning const& t{ *bb_.Tuning };
			if (AbortInside_ && bb_.Distance < t.CloseRange * 0.8f) { return Status::Success; }
			if (++Elapsed_ >= Frames_) { return Status::Success; }
			Lumina::Vec2 const tangent{ -bb_.ToTarget.y * Side_, bb_.ToTarget.x * Side_ };
			float const radial{ (Keep_ > 1.0f) ? Clamp((bb_.Distance - Keep_) / Keep_ * 4.0f, -0.5f, 0.5f) : 0.0f };
			bb_.Intent.Move = Added(Scaled(tangent, t.StrafeSpeed), Scaled(bb_.ToTarget, radial));
			bb_.Intent.Face = bb_.ToTarget;
			return Status::Running;
		}
	private:
		bool AbortInside_;
		float Side_{ 1.0f };
		float Keep_{ 0.0f };
		std::int32_t Frames_{ 0 };
		std::int32_t Elapsed_{ 0 };
	};

	// スキルを使う（先行入力に Tool を積み、引数 = 枠）。使い始めたら、終わるまで Running で Success。
	// 始まらなければ（距離・クールダウン・動けない）Failure
	class UseSkill : public Node<BB> {
	public:
		UseSkill(std::string name_, std::size_t slot_) : Node<BB>{ std::move(name_) }, Slot_{ slot_ } {}
	protected:
		void OnEnter(BB&) override { Pressed_ = false; Started_ = false; Waited_ = 0; }
		auto OnTick(BB& bb_) -> Status override {
			Game::Fighter const& self{ *bb_.Self };
			bool const using_{
				self.Action() == Game::FighterAction::Ability &&
				self.ActiveAbility() != nullptr &&
				self.ActiveAbility() == self.AbilityAt(Game::AbilityGroup::Tool, Slot_)
			};
			if (!Pressed_) {
				bb_.Intent.Face = bb_.ToTarget;
				bb_.Press(ActId(Act::Tool), static_cast<std::uint8_t>(Slot_));
				Pressed_ = true;
				return Status::Running;
			}
			if (using_) { Started_ = true; return Status::Running; }
			if (Started_) { return Status::Success; }
			bb_.Intent.Face = bb_.ToTarget;
			if (++Waited_ > 6) {
				// 始まらなかった押下は捨てる（後で寿命の間に始まると、別の行動の途中でスキルが出てしまう）
				if (bb_.Buffer != nullptr) { bb_.Buffer->Drop(Lumina::Input::MaskOf(Act::Tool)); }
				return Status::Failure;
			}
			return Status::Running;
		}
	private:
		std::size_t Slot_;
		bool Pressed_{ false };
		bool Started_{ false };
		std::int32_t Waited_{ 0 };
	};

	//==== 合成ノード ====//

	// Pick の選択肢: 重みと、要るスキルの枠（そのスキルが今使えなければ重み 0 として扱う）
	struct PickOption {
		float BossTuning::* Weight{ nullptr };
		std::size_t RequiresSkill{ static_cast<std::size_t>(-1) };	// -1 = スキルは要らない
	};

	// 入ったときに重みで子を 1 つ選び、それを走らせる（結果はその子の結果）。重みが全部 0 なら Failure
	class Pick : public Lumina::BT::Composite<BB> {
	public:
		Pick(std::string name_, std::vector<PickOption> options_)
			: Lumina::BT::Composite<BB>{ std::move(name_) }, Options_{ std::move(options_) } {}
	protected:
		auto WeightOf(BB const& bb_, std::size_t i_) const -> float {
			PickOption const& o{ Options_[i_] };
			if (o.RequiresSkill < bb_.SkillReady.size() && !bb_.SkillReady[o.RequiresSkill]) { return 0.0f; }
			float const w{ bb_.Tuning->*o.Weight };
			return (w > 0.0f) ? w : 0.0f;
		}
		void OnEnter(BB& bb_) override {
			Chosen_ = None;
			std::size_t const n{ (Options_.size() < Children_.size()) ? Options_.size() : Children_.size() };
			float total{ 0.0f };
			for (std::size_t i = 0; i < n; ++i) { total += WeightOf(bb_, i); }
			if (total <= 0.0f) { return; }
			float r{ bb_.RandomUnit() * total };
			for (std::size_t i = 0; i < n; ++i) {
				float const w{ WeightOf(bb_, i) };
				if (w <= 0.0f) { continue; }
				Chosen_ = i;		// 丸めの誤差で最後まで来たら、最後の重みのあるもの
				if (r < w) { break; }
				r -= w;
			}
		}
		auto OnTick(BB& bb_) -> Status override {
			if (Chosen_ == None) { return Status::Failure; }
			return Children_[Chosen_]->Tick(bb_, Stamp());
		}
	private:
		static constexpr std::size_t None{ static_cast<std::size_t>(-1) };
		std::vector<PickOption> Options_;
		std::size_t Chosen_{ None };
	};

	//==== 組み立て ====//

	template<class T, class... Args>
	auto Seq(std::string name_, Args&&... children_) -> NodePtr<BB> {
		auto node{ Make<T>(std::move(name_)) };
		(node->Add(std::forward<Args>(children_)), ...);
		return node;
	}

	auto BuildTree() -> NodePtr<BB> {
		using namespace Lumina::BT;
		using Game::FighterAction;

		auto isStaggered{ Make<Condition<BB>>("IsStaggered", [](BB const& bb_) {
			return bb_.Self->Action() == FighterAction::Stagger;
		}) };
		auto closer{ [](std::string name_, float BossTuning::* range_) {
			return Make<Condition<BB>>(std::move(name_), [range_](BB const& bb_) { return bb_.Distance < bb_.Tuning->*range_; });
		} };

		constexpr std::size_t SlashSlot{ Game::BossSkillSlot::ChargeSlash };
		constexpr std::size_t WaveSlot{ Game::BossSkillSlot::ThornWave };

		auto closePick{ Make<Pick>("CloseChoice", std::vector<PickOption>{
			{ &BossTuning::CloseAttackWeight }, { &BossTuning::CloseRetreatWeight }, { &BossTuning::CloseCircleWeight } }) };
		closePick->Add(Seq<Sequence<BB>>("Attack",
			Make<Combo>("Combo", &BossTuning::ComboMinCloseHits, &BossTuning::ComboMaxCloseHits),
			Make<Recover>("Recover", &BossTuning::RecoverMin, &BossTuning::RecoverMax)));
		closePick->Add(Seq<Sequence<BB>>("Retreat",
			Make<DashStep>("Backstep", false),
			Make<Recover>("Recover", &BossTuning::RecoverMin, &BossTuning::RecoverMax)));
		closePick->Add(Make<Strafe>("Circle", false));

		auto midPick{ Make<Pick>("MidChoice", std::vector<PickOption>{
			{ &BossTuning::MidRushWeight }, { &BossTuning::MidWalkWeight }, { &BossTuning::MidCircleWeight },
			{ &BossTuning::MidSlashWeight, SlashSlot }, { &BossTuning::MidWaveWeight, WaveSlot } }) };
		midPick->Add(Seq<Sequence<BB>>("Rush",
			Make<DashStep>("DashIn", true),
			Make<Combo>("Combo", &BossTuning::ComboMinRushHits, &BossTuning::ComboMaxRushHits),
			Make<Recover>("Recover", &BossTuning::RecoverMin, &BossTuning::RecoverMax)));
		midPick->Add(Make<Approach>("Walk", &BossTuning::CloseRange));
		midPick->Add(Make<Strafe>("Circle", true));
		midPick->Add(Seq<Sequence<BB>>("Slash",
			Make<UseSkill>("ChargeSlash", SlashSlot),
			Make<Recover>("Recover", &BossTuning::RecoverMin, &BossTuning::RecoverMax)));
		midPick->Add(Seq<Sequence<BB>>("Wave",
			Make<UseSkill>("ThornWave", WaveSlot),
			Make<Recover>("Recover", &BossTuning::RecoverMin, &BossTuning::RecoverMax)));

		auto farPick{ Make<Pick>("FarChoice", std::vector<PickOption>{
			{ &BossTuning::FarWalkWeight }, { &BossTuning::FarWaveWeight, WaveSlot } }) };
		farPick->Add(Make<Approach>("Walk", &BossTuning::MidRange));
		farPick->Add(Seq<Sequence<BB>>("Wave",
			Make<UseSkill>("ThornWave", WaveSlot),
			Make<Recover>("Recover", &BossTuning::RecoverMin, &BossTuning::RecoverMax)));

		auto combat{ Seq<Selector<BB>>("Combat",
			Seq<Sequence<BB>>("Close", closer("Distance < Close", &BossTuning::CloseRange), std::move(closePick)),
			Seq<Sequence<BB>>("Mid", closer("Distance < Mid", &BossTuning::MidRange), std::move(midPick)),
			Seq<Sequence<BB>>("Far", std::move(farPick))) };

		return Seq<ReactiveSelector<BB>>("Root",
			Seq<ReactiveSequence<BB>>("Stagger", std::move(isStaggered), Make<Idle>("Idle")),
			std::move(combat));
	}
}

namespace Game {
	//====	======	======	======	======	====//
	//	脳										//
	//====	======	======	======	======	====//

	BossBrain::BossBrain() {
		Tree_.SetRoot(BuildTree());
	}

	void BossBrain::Tick(Fighter const& self_, Fighter const* target_, BossTuning const& tuning_, Lumina::Input::InputBuffer& buffer_) {
		BB_.Intent = {};
		BB_.Self = &self_;
		BB_.Target = target_;
		BB_.Tuning = &tuning_;
		BB_.Buffer = &buffer_;
		for (std::size_t i = 0; i < BB_.SkillReady.size(); ++i) {
			Ability const* const a{ self_.AbilityAt(AbilityGroup::Tool, i) };
			BB_.SkillReady[i] = (a != nullptr) && a->CanStart(self_);
		}
		if (target_ == nullptr) {
			Tree_.Halt(BB_);
			return;
		}

		Lumina::Vec3 const s{ self_.WorldPosition() };
		Lumina::Vec3 const t{ target_->WorldPosition() };
		float const dx{ t.x - s.x };
		float const dz{ t.z - s.z };
		float const dist{ std::sqrt(dx * dx + dz * dz) };
		BB_.Distance = dist;
		BB_.ToTarget = (dist > 1.0e-4f)
			? Lumina::Vec2{ dx / dist, dz / dist }
			: Lumina::Vec2{ std::cos(self_.Angle()), std::sin(self_.Angle()) };

		Tree_.Tick(BB_);
	}

	void BossBrain::Halt() {
		Tree_.Halt(BB_);
		BB_.Intent = {};
	}
}
