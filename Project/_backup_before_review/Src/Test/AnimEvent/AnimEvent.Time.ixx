export module Lumina.AnimEvent : Time;

//****	******	******	******	******	****//

import <span>;

//////	//////	//////	//////	//////	//////

// 再生カーソルと「通過した区間」（講義 前編 1.2〜1.3）
//
// Advance は新しい時刻ではなく、このステップで通過した区間の列を返す。
// 瞬間イベントは「区間に入っているか」（Contains）、区間イベントは「区間と重なるか」（Overlaps）で判定する。
// こうしておけば dt や再生速度がいくつでも、取りこぼしと二重発火が起きない。
//
// 区間の端の規則（前編 1.3 の表）
//   通常の順再生           (前回, 今回]
//   再生開始の 1 ステップ目 [開始, 今回]          ← 開始時刻ちょうどのイベントを拾うため、ここだけ始点を含む
//   ループの折り返し        (前回, 長さ] と [0, 今回]
//   逆再生                 [今回, 前回)            ← 向きが逆になるだけ。開始直後だけ [今回, 前回]
//   非ループの終端          (前回, 長さ]、以後は空
//   一時停止（dt = 0）      空（開始直後だけ [t, t]）

export namespace Lumina::AnimEvent {
	// From → To の向きに通過したことを表す。From < To なら順再生、From > To なら逆再生。
	// From == To は「開始直後に dt = 0 だった」ときだけ現れる（IncludeFrom = true の 1 点）。
	struct TimeSegment {
		float From{ 0.0f };
		float To{ 0.0f };
		bool IncludeFrom{ false };	// 再生開始直後と、順再生でループを折り返した直後だけ true
	};

	struct PlaybackCursor {
		float Time{ 0.0f };			// クリップ時間（秒）。[0, length]
		float Rate{ 1.0f };			// 再生速度。負なら逆再生
		bool Loop{ false };
		bool Fresh{ true };			// 再生開始直後の 1 ステップだけ true。Advance が下ろす
		bool Finished{ false };		// 非ループで、再生方向の端に着いて止まっている
	};

	// 1 ステップで数える周回の上限（安全弁）。
	// 極端な再生速度や、デバッガで止めた直後の巨大な dt で、区間が際限なく増えるのを防ぐ。
	// 上限を超えた分は時刻だけ進め、その間の瞬間イベントは出さない（前編 1.2）。
	inline constexpr int MaxSegmentsPerStep{ 4 };

	// dt_ × Rate だけ進め、通過した区間を out_ に書き出す。戻り値は区間の数（out_.size() 以下）。
	// length_ <= 0 のクリップは時刻 0 に固定し、開始直後だけ [0, 0] を返す。
	auto Advance(PlaybackCursor& c_, float length_, float dt_, std::span<TimeSegment> out_) -> int;

	// 区間が時刻 t_ を含むか（瞬間イベント用）
	auto Contains(TimeSegment const& s_, float t_) noexcept -> bool;

	// 区間が窓 [begin_, end_) と少しでも重なるか（区間イベント用）。
	// 「重なったら開いている」とみなすので、1 ステップより短い窓も最低 1 ステップは開く（前編 4.4）。
	auto Overlaps(TimeSegment const& s_, float begin_, float end_) noexcept -> bool;

	// [0, length_) に折り返す。length_ <= 0 なら 0
	auto WrapTime(float t_, float length_) noexcept -> float;
}
