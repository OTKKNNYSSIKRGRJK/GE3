module Lumina.AnimEvent;

//****	******	******	******	******	****//

// ヘッダユニットは、ビルド対象の .ixx のどこかで import されているものしか作られないらしい（GE3 の現状の設定。推測）。
// ここで import しているものは、いずれもビルド対象の .ixx で import 済み。
// .cpp でしか使わないヘッダが要るときは、グローバルモジュールフラグメント（module; の後）で #include する。
import <algorithm>;
import <array>;
import <bit>;
import <cassert>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <optional>;
import <span>;
import <utility>;
import <vector>;

//////	//////	//////	//////	//////	//////

namespace Lumina::AnimEvent {
	namespace {
		auto Bit(std::size_t index_) noexcept -> WindowMask { return WindowMask{ 1U } << index_; }

		// ループ再生では「長さちょうど」と「0」は同じ瞬間。長さちょうどのイベントは 0 として扱う。
		// 講義（前編 1.3）は焼き込みで寄せる案だったが、GE3 ではループするかを再生する側が決めるので、ここで行う。
		auto EffectiveTime(float t_, float length_, bool loop_) noexcept -> float {
			return (loop_ && t_ >= length_) ? 0.0f : t_;
		}

		auto SegmentLength(TimeSegment const& s_) noexcept -> float { return std::fabs(s_.To - s_.From); }

		// 窓に入った位置（このステップで進んだ距離の何割のところか）
		auto EnterFraction(WindowEvent const& w_, std::span<TimeSegment const> segs_, float total_) noexcept -> float {
			if (!(total_ > 0.0f)) { return 0.0f; }
			float before{ 0.0f };
			for (TimeSegment const& s : segs_) {
				if (Overlaps(s, w_.Begin, w_.End)) {
					// 順再生なら Begin から、逆再生なら End から入る。区間の始点がすでに窓の中なら 0
					float const d{ (s.From <= s.To) ? std::max(0.0f, w_.Begin - s.From) : std::max(0.0f, s.From - w_.End) };
					return std::clamp((before + d) / total_, 0.0f, 1.0f);
				}
				before += SegmentLength(s);
			}
			return 0.0f;
		}
	}

	//====	======	======	======	======	====//
	//	要求									//
	//====	======	======	======	======	====//

	auto AnimEventPlayer::Play(PlayParams const& params_) -> InstanceId {
		assert(params_.Events != nullptr && "AnimEventPlayer::Play: Events is null");
		if (params_.Events == nullptr) { return InvalidInstance; }

		InstanceId const id{ NextId_ };
		NextId_ = (NextId_ == ~InstanceId{ 0U }) ? 1U : NextId_ + 1U;	// 0 は無効値なので飛ばす
		Requests_.push_back(Request{ .What = Request::Kind::Play, .Id = id, .Params = params_ });
		return id;
	}

	void AnimEventPlayer::Stop(InstanceId id_, EndReason reason_) {
		Requests_.push_back(Request{ .What = Request::Kind::Stop, .Id = id_, .Reason = reason_ });
	}

	void AnimEventPlayer::SetWeight(InstanceId id_, float weight_) {
		Requests_.push_back(Request{ .What = Request::Kind::SetWeight, .Id = id_, .Value = weight_ });
	}

	void AnimEventPlayer::SetRate(InstanceId id_, float rate_) {
		Requests_.push_back(Request{ .What = Request::Kind::SetRate, .Id = id_, .Value = rate_ });
	}

	//====	======	======	======	======	====//
	//	1 ステップ								//
	//====	======	======	======	======	====//

	void AnimEventPlayer::Step(float dt_, std::vector<AnimEvent>& out_) {
		std::size_t const first{ out_.size() };

		ApplyRequests(out_);
		for (Instance& inst : Instances_) {
			AdvanceInstance(inst, dt_, out_);
		}

		// ステップ内の位置順。同じ位置なら End を先に出す（同時刻の窓の入れ替えで、状態が一瞬途切れたように見えないように）。
		// 安定ソートなので、同じ位置・同じ種類の中では「要求の反映 → インスタンスの再生順 → 時刻順／窓ID 順」が保たれる
		std::ranges::stable_sort(
			out_.begin() + static_cast<std::ptrdiff_t>(first), out_.end(), {},
			[](AnimEvent const& e_) { return std::pair{ e_.StepFraction, e_.Edge == EventEdge::End ? 0 : 1 }; }
		);
	}

	void AnimEventPlayer::StopAllNow(EndReason reason_, std::vector<AnimEvent>& out_) {
		Requests_.clear();
		for (Instance& inst : Instances_) {
			CloseWindows(inst, inst.Open, reason_, out_);
		}
		Instances_.clear();
	}

	void AnimEventPlayer::Rebind(ClipEvents const* from_, ClipEvents const* to_, std::vector<AnimEvent>& out_) {
		assert(from_ != nullptr && to_ != nullptr && "AnimEventPlayer::Rebind: null");
		if (from_ == nullptr || to_ == nullptr || from_ == to_) { return; }

		// 反映前の Play も差し替える（次の Step で from_ を参照しないように）
		for (Request& r : Requests_) {
			if (r.What == Request::Kind::Play && r.Params.Events == from_) { r.Params.Events = to_; }
		}

		for (Instance& inst : Instances_) {
			if (inst.Events != from_) { continue; }

			// 1. 古いデータで開いている窓をすべて閉じる。窓IDは新しいデータでは別の窓を指すかもしれないので、
			//    「同じ窓IDなら開いたまま」のような引き継ぎはしない
			CloseWindows(inst, inst.Open, EndReason::Reloaded, out_);

			// 2. 差し替えて、現在時刻で開き直す（止まっているときと同じ規則：Begin <= now < End）
			inst.Events = to_;
			inst.Cursor.Time = std::clamp(inst.Cursor.Time, 0.0f, std::max(to_->Length, 0.0f));
			float const now{ inst.Cursor.Time };
			WindowMask timeMask{ 0U };
			std::size_t const count{ std::min(to_->Windows.size(), ClipEvents::MaxWindows) };
			for (std::size_t i = 0; i < count; ++i) {
				WindowEvent const& w{ to_->Windows[i] };
				if (w.Begin <= now && now < w.End) { timeMask |= Bit(i); }
			}
			inst.Open = timeMask & EligibleMask(inst);
			for (WindowMask m = inst.Open; m != 0U; m &= m - 1U) {
				out_.push_back(MakeWindowEdge(inst, static_cast<std::size_t>(std::countr_zero(m)), EventEdge::Begin, EndReason::None, 0.0f));
			}
		}
	}

	void AnimEventPlayer::ApplyRequests(std::vector<AnimEvent>& out_) {
		// 反映中に Requests_ が増えることはない（このクラスはコールバックを呼ばない）が、念のため取り出してから回す
		std::vector<Request> requests{ std::exchange(Requests_, {}) };

		for (Request const& r : requests) {
			switch (r.What) {
			case Request::Kind::Play: {
				PlayParams const& p{ r.Params };
				if (p.StopOthers) {
					for (Instance& inst : Instances_) { CloseWindows(inst, inst.Open, EndReason::Interrupted, out_); }
					Instances_.clear();
				}
				if (p.OwnsAction) {
					// 持ち主は 1 本だけ。前の持ち主の判定系の窓は中断扱いで閉じる（前編 4.6）。
					// 前の持ち主はブレンドで抜けていく途中かもしれないので、インスタンス自体は止めない（演出系は出し続ける）。
					// ※ここで閉じなくても、OwnsAction を下ろせば同じ Step の AdvanceInstance が同じ End（Interrupted）を出す。
					//   結果は同じだが、「持ち主の交代で閉じる」ことをコード上で明示するために、要求の反映の時点で閉じておく
					for (Instance& inst : Instances_) {
						if (!inst.OwnsAction) { continue; }
						CloseWindows(inst, inst.Open & GameplayMask(*inst.Events), EndReason::Interrupted, out_);
						inst.OwnsAction = false;
					}
				}
				Instance inst{};
				inst.Id = r.Id;
				inst.Events = p.Events;
				inst.Cursor = PlaybackCursor{ .Time = p.StartTime, .Rate = p.Rate, .Loop = p.Loop, .Fresh = true, .Finished = false };
				inst.Weight = p.Weight;
				inst.OwnsAction = p.OwnsAction;
				Instances_.push_back(inst);
				break;
			}
			case Request::Kind::Stop: {
				auto const itr{ std::ranges::find(Instances_, r.Id, &Instance::Id) };
				if (itr == Instances_.end()) { break; }		// 止め済み・知らない ID は無視
				CloseWindows(*itr, itr->Open, r.Reason, out_);
				Instances_.erase(itr);		// 再生順を保つ（出力の並びに使うため）
				break;
			}
			case Request::Kind::SetWeight:
				if (Instance* inst{ Find(r.Id) }) { inst->Weight = r.Value; }
				break;
			case Request::Kind::SetRate:
				if (Instance* inst{ Find(r.Id) }) { inst->Cursor.Rate = r.Value; }
				break;
			}
		}
	}

	void AnimEventPlayer::AdvanceInstance(Instance& inst_, float dt_, std::vector<AnimEvent>& out_) {
		ClipEvents const& ev{ *inst_.Events };
		assert(ev.Windows.size() <= ClipEvents::MaxWindows && "ClipEvents: too many windows (use Bake)");

		std::array<TimeSegment, MaxSegmentsPerStep> buf{};
		int const n{ Advance(inst_.Cursor, ev.Length, dt_, buf) };
		std::span<TimeSegment const> const segs{ buf.data(), static_cast<std::size_t>(n) };

		float total{ 0.0f };
		for (TimeSegment const& s : segs) { total += SegmentLength(s); }

		//----	瞬間イベント（前編 4.3）	------	----//
		{
			float before{ 0.0f };
			for (TimeSegment const& s : segs) {
				for (InstantEvent const& e : ev.Instants) {
					float const t{ EffectiveTime(e.Time, ev.Length, inst_.Cursor.Loop) };
					if (!Contains(s, t)) { continue; }
					bool const eligible{
						(e.Category == EventCategory::Gameplay) ? inst_.OwnsAction : (inst_.Weight >= e.MinWeight)
					};
					if (!eligible) { continue; }

					AnimEvent a{};
					a.Type = e.Type;
					a.Edge = EventEdge::Instant;
					a.Category = e.Category;
					a.StepFraction = (total > 0.0f) ? std::clamp((before + std::fabs(t - s.From)) / total, 0.0f, 1.0f) : 0.0f;
					a.Weight = inst_.Weight;
					a.ClipTime = t;
					a.Source = inst_.Id;
					a.Payload = PayloadPtr(ev, e.PayloadOffset, e.PayloadSize);
					a.PayloadSize = e.PayloadSize;
					out_.push_back(a);
				}
				before += SegmentLength(s);
			}
		}

		//----	区間イベント（前編 4.4）	------	----//
		// 「開いている」の定義
		//   通過区間があるステップ：区間と少しでも重なった窓（短い窓も最低 1 ステップは開く）
		//   通過区間が空のステップ（一時停止・ヒットストップ・非ループの終端）：現在時刻を含む窓（開いたまま保つ）
		WindowMask timeMask{ 0U };
		std::size_t const count{ std::min(ev.Windows.size(), ClipEvents::MaxWindows) };
		float const now{ inst_.Cursor.Time };
		for (std::size_t i = 0; i < count; ++i) {
			WindowEvent const& w{ ev.Windows[i] };
			bool const open{
				segs.empty()
					? (w.Begin <= now && now < w.End)
					: std::ranges::any_of(segs, [&](TimeSegment const& s_) { return Overlaps(s_, w.Begin, w.End); })
			};
			if (open) { timeMask |= Bit(i); }
		}

		WindowMask const nowOpen{ timeMask & EligibleMask(inst_) };

		// 閉じた窓。時間としてはまだ開いているのに絞り込みで外れたもの（ウェイトが閾値を割った）は Interrupted
		for (WindowMask m = inst_.Open & ~nowOpen; m != 0U; m &= m - 1U) {
			auto const idx{ static_cast<std::size_t>(std::countr_zero(m)) };
			EndReason const reason{ (timeMask & Bit(idx)) != 0U ? EndReason::Interrupted : EndReason::Completed };
			// 窓は「重なったステップまで開いている」ので、閉じるのは常にステップの頭（前のステップで窓を抜けている）
			out_.push_back(MakeWindowEdge(inst_, idx, EventEdge::End, reason, 0.0f));
		}
		// 開いた窓
		for (WindowMask m = nowOpen & ~inst_.Open; m != 0U; m &= m - 1U) {
			auto const idx{ static_cast<std::size_t>(std::countr_zero(m)) };
			out_.push_back(MakeWindowEdge(inst_, idx, EventEdge::Begin, EndReason::None, EnterFraction(ev.Windows[idx], segs, total)));
		}
		inst_.Open = nowOpen;
	}

	void AnimEventPlayer::CloseWindows(Instance& inst_, WindowMask mask_, EndReason reason_, std::vector<AnimEvent>& out_) {
		WindowMask const closing{ inst_.Open & mask_ };
		for (WindowMask m = closing; m != 0U; m &= m - 1U) {
			out_.push_back(MakeWindowEdge(inst_, static_cast<std::size_t>(std::countr_zero(m)), EventEdge::End, reason_, 0.0f));
		}
		inst_.Open &= ~closing;
	}

	auto AnimEventPlayer::GameplayMask(ClipEvents const& ev_) noexcept -> WindowMask {
		WindowMask m{ 0U };
		std::size_t const count{ std::min(ev_.Windows.size(), ClipEvents::MaxWindows) };
		for (std::size_t i = 0; i < count; ++i) {
			if (ev_.Windows[i].Category == EventCategory::Gameplay) { m |= Bit(i); }
		}
		return m;
	}

	auto AnimEventPlayer::EligibleMask(Instance const& inst_) noexcept -> WindowMask {
		ClipEvents const& ev{ *inst_.Events };
		WindowMask m{ 0U };
		std::size_t const count{ std::min(ev.Windows.size(), ClipEvents::MaxWindows) };
		for (std::size_t i = 0; i < count; ++i) {
			WindowEvent const& w{ ev.Windows[i] };
			bool const eligible{
				(w.Category == EventCategory::Gameplay) ? inst_.OwnsAction : (inst_.Weight >= w.MinWeight)
			};
			if (eligible) { m |= Bit(i); }
		}
		return m;
	}

	auto AnimEventPlayer::MakeWindowEdge(Instance const& inst_, std::size_t index_, EventEdge edge_, EndReason reason_, float stepFraction_) -> AnimEvent {
		WindowEvent const& w{ inst_.Events->Windows[index_] };
		AnimEvent a{};
		a.Type = w.Type;
		a.Edge = edge_;
		a.Reason = reason_;
		a.Category = w.Category;
		a.WindowIndex = static_cast<std::uint16_t>(index_);
		a.StepFraction = stepFraction_;
		a.Weight = inst_.Weight;
		a.ClipTime = inst_.Cursor.Time;
		a.Source = inst_.Id;
		a.Payload = PayloadPtr(*inst_.Events, w.PayloadOffset, w.PayloadSize);
		a.PayloadSize = w.PayloadSize;
		return a;
	}

	//====	======	======	======	======	====//
	//	問い合わせ								//
	//====	======	======	======	======	====//

	auto AnimEventPlayer::HasOpenWindows() const noexcept -> bool {
		return std::ranges::any_of(Instances_, [](Instance const& i_) { return i_.Open != 0U; });
	}

	auto AnimEventPlayer::Find(InstanceId id_) -> Instance* {
		auto const itr{ std::ranges::find(Instances_, id_, &Instance::Id) };
		return itr == Instances_.end() ? nullptr : &*itr;
	}

	auto AnimEventPlayer::Find(InstanceId id_) const -> Instance const* {
		auto const itr{ std::ranges::find(Instances_, id_, &Instance::Id) };
		return itr == Instances_.end() ? nullptr : &*itr;
	}

	auto AnimEventPlayer::FindPendingPlay(InstanceId id_) const -> Request const* {
		auto const itr{
			std::ranges::find_if(Requests_, [&](Request const& r_) { return r_.What == Request::Kind::Play && r_.Id == id_; })
		};
		return itr == Requests_.end() ? nullptr : &*itr;
	}

	auto AnimEventPlayer::Time(InstanceId id_) const -> std::optional<float> {
		if (Instance const* inst{ Find(id_) }) { return inst->Cursor.Time; }
		if (Request const* r{ FindPendingPlay(id_) }) {
			float const len{ r->Params.Events->Length };
			return std::clamp(r->Params.StartTime, 0.0f, std::max(len, 0.0f));	// Advance と同じ収め方
		}
		return std::nullopt;
	}

	auto AnimEventPlayer::IsFinished(InstanceId id_) const -> bool {
		Instance const* inst{ Find(id_) };
		return inst != nullptr && inst->Cursor.Finished;
	}

	auto AnimEventPlayer::IsPlaying(InstanceId id_) const -> bool { return Find(id_) != nullptr; }

	auto AnimEventPlayer::ActionOwner() const noexcept -> InstanceId {
		auto const itr{ std::ranges::find_if(Instances_, &Instance::OwnsAction) };
		return itr == Instances_.end() ? InvalidInstance : itr->Id;
	}

	auto SampleCurve(CurveTrack const& c_, float t_) noexcept -> float {
		std::size_t const n{ c_.Values.size() };
		if (n == 0U) { return 0.0f; }
		if (n == 1U || !(c_.KeyInterval > 0.0f)) { return c_.Values.front(); }
		// 範囲外は端の値。size_t への変換の前に収めておく（巨大な値や NaN をそのままキャストしない）
		float const last{ static_cast<float>(n - 1U) };
		float const x{ std::clamp(t_ / c_.KeyInterval, 0.0f, last) };
		if (!(x >= 0.0f)) { return c_.Values.front(); }		// NaN
		std::size_t const i{ std::min(static_cast<std::size_t>(x), n - 2U) };
		return std::lerp(c_.Values[i], c_.Values[i + 1U], x - static_cast<float>(i));	// std::lerp は C++20
	}

	auto AnimEventPlayer::Curve(CurveId id_, float fallback_) const -> float {
		float sum{ 0.0f };
		float weight{ 0.0f };
		for (Instance const& inst : Instances_) {
			// 1 クリップのカーブは数本なので線形探索で十分
			auto const itr{ std::ranges::find(inst.Events->Curves, id_, &CurveTrack::Id) };
			if (itr == inst.Events->Curves.end() || !(inst.Weight > 0.0f)) { continue; }
			sum += inst.Weight * SampleCurve(*itr, inst.Cursor.Time);
			weight += inst.Weight;
		}
		return (weight > 0.0f) ? sum / weight : fallback_;
	}

	auto AnimEventPlayer::IsOpen(EventTypeId type_) const -> bool {
		for (Instance const& inst : Instances_) {
			for (WindowMask m = inst.Open; m != 0U; m &= m - 1U) {
				if (inst.Events->Windows[static_cast<std::size_t>(std::countr_zero(m))].Type == type_) { return true; }
			}
		}
		return false;
	}
}
