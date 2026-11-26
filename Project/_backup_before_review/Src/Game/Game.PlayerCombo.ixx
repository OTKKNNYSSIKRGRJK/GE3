//////	//////	//////	//////	//////	//////
//	PlayerCombo - 通常攻撃のつながり		//
//////	//////	//////	//////	//////	//////
// 攻撃ボタンで 1 段目 → 2 段目 → 3 段目と振りをつなぐ。何段目か（Step）だけを持ち、クリップの再生は InGame が Step から選ぶ。
//
// 考え方
// - 「次の段へ移れる」のは、今の振りのクリップに置いた Cancel 窓（Allow に Attack を含む）が開いている間だけ。
//   窓は振り抜きの後半に置き、閉じたらそのまま最後まで再生して終わる（別のタイマーは持たない。時間切れ = 窓の終わり）。
// - 窓より前に押した攻撃も捨てない。先行入力（InputBuffer）に積まれていて、窓が開いた瞬間に取り出される。
// - 同じ Cancel 窓で Dodge / Jump を許せば、振りの後半から回避・ジャンプで抜けられる（抜けた行動は CancelledInto で返す）。
//   Tool / UseItem も同じ（枠の番号は CancelledArg）。
//   Move を許せば、移動の入力で抜けられる（振り抜きの最後のほうに置く）。
// - 攻撃中（Step != 0）は移動・ジャンプ・ダッシュを止める（Game.Fighter が持っていて、自分で止める）。
// - 被弾してのけぞった・入力がロックされた（ゲームオーバー）などで CanAct が false になったら、その場で攻撃をやめる。
// - 最後の段の後は、最後の段のクリップに Attack の Cancel 窓があれば 1 段目へ戻ってつながる（データで決める）。
//
// エンジンのアニメ系には依存しない（窓の状態とクリップの終わりは、呼び出し側が Context で渡す）

export module Game.PlayerCombo;

//****	******	******	******	******	****//

import <cstdint>;
import <optional>;

import Lumina.Input;
import Game.PlayerInput;

//////	//////	//////	//////	//////	//////

export namespace Game::PlayerCombo {
	inline constexpr std::int32_t MaxStep{ 3 };

	// 1 フレームぶんの外の状態（Game.Fighter が作る）
	struct Context {
		bool CanAct{ true };						// 攻撃してよい（のけぞり中でない、入力ロックなし、エディタのプレビュー中でない）
		Lumina::Input::ActionMask WindowAllow{ 0U };	// 今開いている Cancel 窓の Allow の和
		bool ClipFinished{ false };					// 今の振りのクリップが終端に着いた
		bool MoveInput{ false };					// 移動の入力がある
	};

	class Controller {
	public:
		// 毎フレーム 1 回、体の Update の中で、移動より前に呼ぶ（攻撃中かどうかで、移動を止めるため）。
		// 先行入力から Attack（攻撃中は窓が許すもの）を取り出す
		void Update(Context const& ctx_, Lumina::Input::InputBuffer& buffer_);

		// 攻撃をやめる（フェーズが変わったとき、戦闘をやり直すとき）
		void Reset() noexcept;

		auto Step() const noexcept -> std::int32_t { return Step_; }		// 0 = 攻撃していない、1..MaxStep
		auto Attacking() const noexcept -> bool { return Step_ != 0; }

		// このフレームで新しい段を始めた（同じクリップでも頭から再生し直す、振り始めの向き直り、などに使う）
		auto StepStarted() const noexcept -> bool { return StepStarted_; }

		// このフレームで、窓の許す回避・ジャンプ（・Tool / UseItem）で攻撃を抜けた（体にその行動をさせる）
		auto CancelledInto() const noexcept -> std::optional<Game::PlayerInput::Act> { return CancelledInto_; }
		// 抜けた行動の引数（Tool / UseItem の枠の番号。先行入力の BufferedInput::Arg）
		auto CancelledArg() const noexcept -> std::uint8_t { return CancelledArg_; }

	private:
		std::int32_t Step_{ 0 };
		bool StepStarted_{ false };
		std::optional<Game::PlayerInput::Act> CancelledInto_{};
		std::uint8_t CancelledArg_{ 0U };
	};
}
