module Game.PlayerCombo;

//****	******	******	******	******	****//

import <cstdint>;
import <optional>;

import Lumina.Input;
import Game.PlayerInput;

//////	//////	//////	//////	//////	//////

namespace Game::PlayerCombo {
	namespace {
		using Game::PlayerInput::Act;
		using Lumina::Input::MaskOf;

		// 攻撃中に窓から取り出す行動（Move は押した瞬間ではなく、入力があるかで見るので別に扱う）。
		// Tool / UseItem は、サイドカーの窓の allow に書いたときだけ取り出される（今のサイドカーには無い）
		constexpr Lumina::Input::ActionMask TakeableInAttack{ MaskOf(Act::Attack, Act::Dodge, Act::Jump, Act::Tool, Act::UseItem) };
	}

	void Controller::Update(Context const& ctx_, Lumina::Input::InputBuffer& buffer_) {
		StepStarted_ = false;
		CancelledInto_.reset();
		CancelledArg_ = 0U;

		if (!ctx_.CanAct) {
			// のけぞり・ロックで止まる。積んである入力は寿命まで残す（のけぞりが短ければ、明けた瞬間に出る）
			Step_ = 0;
			return;
		}

		if (Step_ == 0) {
			// 攻撃していない: 押された攻撃で 1 段目から
			if (buffer_.Take(MaskOf(Act::Attack))) {
				Step_ = 1;
				StepStarted_ = true;
			}
			return;
		}

		// 攻撃中: 開いている Cancel 窓が許す行動だけ取り出す。窓より前に押したものも、寿命の間は先行入力に残っている
		if (auto const e{ buffer_.TakeEntry(ctx_.WindowAllow & TakeableInAttack) }) {
			if (e->Action == Lumina::Input::ToActionId(Act::Attack)) {
				Step_ = (Step_ % MaxStep) + 1;		// 最後の段の次は 1 段目（最後の段に Attack の窓があるときだけ起きる）
				StepStarted_ = true;
				return;
			}
			Step_ = 0;
			CancelledInto_ = static_cast<Act>(e->Action);
			CancelledArg_ = e->Arg;
			return;
		}

		// 振り抜きの最後のほう（Move を許す窓）では、移動の入力で抜けられる
		if ((ctx_.WindowAllow & MaskOf(Act::Move)) != 0U && ctx_.MoveInput) {
			Step_ = 0;
			return;
		}

		// 窓が閉じたまま最後まで再生した: コンボはここで切れる。
		// 窓が閉じてから押した攻撃が残っていれば、1 段目から始め直す（Idle を 1 フレーム挟まないように、ここで続けて取り出す）
		if (ctx_.ClipFinished) {
			Step_ = 0;
			if (buffer_.Take(MaskOf(Act::Attack))) {
				Step_ = 1;
				StepStarted_ = true;
			}
		}
	}

	void Controller::Reset() noexcept {
		Step_ = 0;
		StepStarted_ = false;
		CancelledInto_.reset();
		CancelledArg_ = 0U;
	}
}
