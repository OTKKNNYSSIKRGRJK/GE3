module Lumina.Input;

//****	******	******	******	******	****//

import <cstddef>;
import <cstdint>;
import <optional>;

//////	//////	//////	//////	//////	//////

namespace Lumina::Input {
	void InputBuffer::Push(ActionId id_, std::uint8_t arg_) noexcept {
		if (id_ >= MaxActions) { return; }
		if (Count_ == BufferCapacity) {
			// いちばん古いものを捨てて詰める
			for (std::size_t i = 1; i < Count_; ++i) { Entries_[i - 1U] = Entries_[i]; }
			--Count_;
		}
		Entries_[Count_] = BufferedInput{ .Action = id_, .Age = 0U, .Arg = arg_ };
		++Count_;
	}

	void InputBuffer::Capture(InputState const& state_, ActionMask mask_) noexcept {
		ActionMask const pressed{ state_.PressedMask() & mask_ };
		for (std::size_t i = 0; i < MaxActions; ++i) {
			if ((pressed >> i) & 1U) { Push(static_cast<ActionId>(i)); }
		}
	}

	void InputBuffer::Advance(std::uint32_t frames_) noexcept {
		if (frames_ == 0U) { return; }
		std::size_t kept{ 0U };
		for (std::size_t i = 0; i < Count_; ++i) {
			BufferedInput e{ Entries_[i] };
			// 足し算のあふれを避ける（巨大な frames_ でも寿命切れとして捨てる）
			if (e.Age > Lifetime_ || frames_ > Lifetime_ - e.Age) { continue; }
			e.Age += frames_;
			Entries_[kept] = e;
			++kept;
		}
		Count_ = kept;
	}

	auto InputBuffer::Find(ActionMask allow_) const noexcept -> std::optional<std::size_t> {
		std::optional<std::size_t> best{};
		for (std::size_t i = 0; i < Count_; ++i) {
			BufferedInput const& e{ Entries_[i] };
			if (!((allow_ >> e.Action) & 1U)) { continue; }
			if (!best) { best = i; continue; }
			BufferedInput const& b{ Entries_[*best] };
			if (e.Age < b.Age || (e.Age == b.Age && e.Action < b.Action)) { best = i; }
		}
		return best;
	}

	auto InputBuffer::Peek(ActionMask allow_) const noexcept -> std::optional<ActionId> {
		if (auto const i{ Find(allow_) }) { return Entries_[*i].Action; }
		return std::nullopt;
	}

	auto InputBuffer::PeekEntry(ActionMask allow_) const noexcept -> std::optional<BufferedInput> {
		if (auto const i{ Find(allow_) }) { return Entries_[*i]; }
		return std::nullopt;
	}

	auto InputBuffer::Take(ActionMask allow_) noexcept -> std::optional<ActionId> {
		if (auto const e{ TakeEntry(allow_) }) { return e->Action; }
		return std::nullopt;
	}

	auto InputBuffer::TakeEntry(ActionMask allow_) noexcept -> std::optional<BufferedInput> {
		auto const i{ Find(allow_) };
		if (!i) { return std::nullopt; }
		BufferedInput const taken{ Entries_[*i] };
		std::uint32_t const age{ taken.Age };

		// 取り出したものと、それと同時か前に押したもの（年齢が age 以上）を捨てる
		std::size_t kept{ 0U };
		for (std::size_t j = 0; j < Count_; ++j) {
			if (Entries_[j].Age < age) {
				Entries_[kept] = Entries_[j];
				++kept;
			}
		}
		Count_ = kept;
		return taken;
	}

	void InputBuffer::Drop(ActionMask mask_) noexcept {
		std::size_t kept{ 0U };
		for (std::size_t j = 0; j < Count_; ++j) {
			if (!((mask_ >> Entries_[j].Action) & 1U)) {
				Entries_[kept] = Entries_[j];
				++kept;
			}
		}
		Count_ = kept;
	}
}
