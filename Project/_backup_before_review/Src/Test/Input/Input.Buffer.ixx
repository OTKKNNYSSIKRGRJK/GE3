export module Lumina.Input : Buffer;

//****	******	******	******	******	****//

import <array>;
import <cstddef>;
import <cstdint>;
import <optional>;
import <span>;

import : Map;

//////	//////	//////	//////	//////	//////

// 先行入力
//
// 攻撃の振りの途中で次の攻撃を押したとき、その入力を捨てずに覚えておき、受付（キャンセル窓）が開いた瞬間に実行するためのもの。
// 受付の外の入力を捨てるだけだと、少し早く押した入力が消えて「押したのに出ない」操作感になる。
//
// 1 フレームの流れ（呼び出し側）
//   state.Update(map, frame);
//   buffer.Advance(isHitStop ? 0 : 1);          // 古い入力を年を取らせて捨てる。ヒットストップ中は止める
//   buffer.Capture(state, MaskOf(Act::Attack, Act::Dodge, Act::Jump));   // このフレームで押した行動を積む
//   if (auto a{ buffer.Take(allowNow) }) { /* *a を実行 */ }             // 今移れる行動のうち、いちばん新しいもの
//
// 規則
//   - 寿命: 押したフレームを年齢 0 とし、Advance で年齢が Lifetime を超えたら捨てる（押したフレームを含めて Lifetime + 1 フレーム有効）。
//   - Take: allow に入っている行動のうち、いちばん新しいもの。同じフレームに押したものどうしは ActionId の小さい方を選ぶ
//          （ゲーム側の enum の並び = 同時押しの優先順）。
//   - Take で取り出したら、その入力と、それと同時か前に押した入力はすべて捨てる（後に押したものは残る）。
//     連打したぶんが溜まって、手を離した後もコンボが続く、を防ぐため。
//   - 容量（BufferCapacity）を超えたら、いちばん古いものから捨てる。
//   - 引数（Arg）: 行動に 1 バイトの添え字を付けて積める（「どの枠のスキル・アイテムか」など）。押した時点の値が残る。
//     Take / Peek は行動だけを返す。引数も要るときは TakeEntry / PeekEntry。選び方・捨て方は引数に関係なく同じ。
//   - Drop: 指定した行動だけを捨てる（他は残す）。同じフレームに押した別々の行動を両方使うときに、Peek と組み合わせて使う。
//   - 押しっぱなしで使う行動（ガード）や、押した瞬間の時刻が大事な行動（弾き）は積まない方がよい。
//     弾きを積むと、早すぎた押下が受付の開いた時刻の弾きとして扱われ、タイミングの判定がずれる。

export namespace Lumina::Input {
	inline constexpr std::size_t BufferCapacity{ 16U };

	struct BufferedInput {
		ActionId Action{ 0U };
		std::uint32_t Age{ 0U };	// 押してから Advance したフレーム数
		std::uint8_t Arg{ 0U };		// 押した時点で付けた引数（枠の番号など）。使わない行動では 0
	};

	class InputBuffer {
	public:
		explicit InputBuffer(std::uint32_t lifetime_ = 10U) noexcept : Lifetime_{ lifetime_ } {}

		void SetLifetime(std::uint32_t lifetime_) noexcept { Lifetime_ = lifetime_; }
		auto Lifetime() const noexcept -> std::uint32_t { return Lifetime_; }

		// 年齢 0 で積む。id が範囲外なら何もしない
		void Push(ActionId id_, std::uint8_t arg_ = 0U) noexcept;

		// このフレームで Pressed になった行動のうち、mask_ に入っているものを ActionId の小さい順に積む
		void Capture(InputState const& state_, ActionMask mask_) noexcept;

		// 全員の年齢を frames_ だけ進め、寿命を過ぎたものを捨てる。0 なら何もしない（ヒットストップ・一時停止）
		void Advance(std::uint32_t frames_ = 1U) noexcept;

		// allow_ に入っている行動のうち、いちばん新しいものを取り出す（規則は上を参照）
		auto Take(ActionMask allow_) noexcept -> std::optional<ActionId>;

		// Take と同じものを選ぶが、取り出さない
		auto Peek(ActionMask allow_) const noexcept -> std::optional<ActionId>;

		// Take / Peek と同じ規則で、引数ごと返す（取り出したときの Age は取り出す直前の値）
		auto TakeEntry(ActionMask allow_) noexcept -> std::optional<BufferedInput>;
		auto PeekEntry(ActionMask allow_) const noexcept -> std::optional<BufferedInput>;

		// mask_ に入っている行動をすべて捨てる。他の行動は残す（Take と違い、前に押した別の行動を捨てない）。
		// 同じフレームに押した別々の行動を両方使いたいとき（ジャンプしながらダッシュ）に、Peek で確かめてからこれで消す
		void Drop(ActionMask mask_) noexcept;

		void Clear() noexcept { Count_ = 0U; }

		// 古い順（デバッグ表示用）
		auto Entries() const noexcept -> std::span<BufferedInput const> { return { Entries_.data(), Count_ }; }

		template<ActionKey A> void Push(A a_, std::uint8_t arg_ = 0U) noexcept { Push(ToActionId(a_), arg_); }

	private:
		auto Find(ActionMask allow_) const noexcept -> std::optional<std::size_t>;

		std::array<BufferedInput, BufferCapacity> Entries_{};	// [0, Count_) が古い順
		std::size_t Count_{ 0U };
		std::uint32_t Lifetime_{ 10U };
	};
}
