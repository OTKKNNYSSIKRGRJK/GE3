module Lumina.AnimEvent;

//****	******	******	******	******	****//

import <algorithm>;
import <cmath>;
import <cstddef>;
import <span>;
import <utility>;

//////	//////	//////	//////	//////	//////

namespace Lumina::AnimEvent {
	auto WrapTime(float t_, float length_) noexcept -> float {
		if (!(length_ > 0.0f)) { return 0.0f; }
		float r{ std::fmod(t_, length_) };
		if (r < 0.0f) { r += length_; }
		// fmod の丸めで r == length_ になることがある（負の小さな値に length_ を足したときなど）
		if (r >= length_) { r = 0.0f; }
		return r;
	}

	auto Advance(PlaybackCursor& c_, float length_, float dt_, std::span<TimeSegment> out_) -> int {
		std::size_t n{ 0U };
		auto const push{
			[&](float from_, float to_, bool includeFrom_) {
				out_[n] = TimeSegment{ from_, to_, includeFrom_ };
				++n;
			}
		};

		bool inc{ std::exchange(c_.Fresh, false) };

		// 長さ 0 のクリップ：時刻は 0 に固定。開始時刻ちょうどのイベントだけ拾えるようにしておく
		if (!(length_ > 0.0f)) {
			c_.Time = 0.0f;
			c_.Finished = !c_.Loop;
			if (inc && !out_.empty()) { push(0.0f, 0.0f, true); }
			return static_cast<int>(n);
		}

		// StartTime の指定ミスなどで範囲外に置かれていても、ここで収める
		c_.Time = std::clamp(c_.Time, 0.0f, length_);

		float move{ dt_ * c_.Rate };	// 符号つきの移動量

		// 一時停止・ヒットストップ中：区間は空。開始時刻ちょうどのイベントだけは拾う（前編 1.2）
		if (move == 0.0f) {
			if (inc && !out_.empty()) { push(c_.Time, c_.Time, true); }
			return static_cast<int>(n);
		}

		if (move > 0.0f) {
			//----	順再生	------	------	------	----//
			while (n < out_.size()) {
				float const target{ c_.Time + move };
				if (!c_.Loop) {
					float const end{ std::min(target, length_) };
					// 終端で止まった後は区間が空。同じイベントを繰り返さない
					if (end > c_.Time || inc) { push(c_.Time, end, inc); }
					c_.Finished = target >= length_;
					c_.Time = end;
					return static_cast<int>(n);
				}
				if (target < length_) {
					push(c_.Time, target, inc);
					c_.Time = target;
					return static_cast<int>(n);
				}
				// 折り返し：(前回, 長さ] と [0, ...] に分ける。
				// 長さちょうどと 0 は同じ瞬間なので、ループ再生では「長さちょうど」のイベントを 0 に寄せて扱う（Player 側）。
				push(c_.Time, length_, inc);
				move -= length_ - c_.Time;
				c_.Time = 0.0f;
				inc = true;		// 次の区間は 0 ちょうどを含む
			}
		}
		else {
			//----	逆再生（順再生の鏡写し）	------	----//
			while (n < out_.size()) {
				float const target{ c_.Time + move };
				if (!c_.Loop) {
					float const end{ std::max(target, 0.0f) };
					if (end < c_.Time || inc) { push(c_.Time, end, inc); }
					c_.Finished = target <= 0.0f;
					c_.Time = end;
					return static_cast<int>(n);
				}
				if (target > 0.0f) {
					push(c_.Time, target, inc);
					c_.Time = target;
					return static_cast<int>(n);
				}
				// 逆向きの折り返し：[0, 前回) を出してから長さへ飛ぶ。
				// 0 はこの区間に含まれているので、飛んだ先の「長さちょうど」（= 同じ瞬間）は含めない（inc = false）。
				if (c_.Time > 0.0f || inc) { push(c_.Time, 0.0f, inc); }
				move += c_.Time;
				c_.Time = length_;
				inc = false;
			}
		}

		// 周回の上限に達した：残りは時刻だけ進めて打ち切る
		c_.Time = WrapTime(c_.Time + move, length_);
		return static_cast<int>(n);
	}

	auto Contains(TimeSegment const& s_, float t_) noexcept -> bool {
		if (s_.From <= s_.To) {
			// 順再生 (From, To]。開始直後だけ [From, To]
			return (s_.IncludeFrom ? t_ >= s_.From : t_ > s_.From) && t_ <= s_.To;
		}
		// 逆再生 [To, From)。開始直後だけ [To, From]
		return (s_.IncludeFrom ? t_ <= s_.From : t_ < s_.From) && t_ >= s_.To;
	}

	auto Overlaps(TimeSegment const& s_, float begin_, float end_) noexcept -> bool {
		// 講義の疑似コードは端の扱いを簡略化していたので、ここでは区間の開閉を正確に扱う。
		// 窓は [begin_, end_)。
		if (s_.From <= s_.To) {
			// (From, To] または [From, To] と [begin, end) の共通部分が空でないか
			if (!s_.IncludeFrom && !(s_.From < s_.To)) { return false; }	// (t, t] は空集合
			return s_.To >= begin_ && s_.From < end_;
		}
		// [To, From) または [To, From] と [begin, end)
		return s_.To < end_ && (s_.IncludeFrom ? s_.From >= begin_ : s_.From > begin_);
	}
}
