module Lumina.AnimEvent.Tests;

//****	******	******	******	******	****//

import <algorithm>;
import <array>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <cstring>;
import <format>;
import <optional>;
import <span>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

import Lumina.AnimEvent;

//////	//////	//////	//////	//////	//////

namespace Lumina::AnimEvent::Tests {
	namespace {
		class Context {
		public:
			Context(std::vector<Failure>& out_, std::string_view name_) : Out_{ out_ }, Name_{ name_ } {}

			void Check(bool ok_, std::string_view what_) {
				if (!ok_) { Out_.push_back(Failure{ std::string{ Name_ }, std::string{ what_ } }); }
			}

		private:
			std::vector<Failure>& Out_;
			std::string_view Name_;
		};

		auto Near(float a_, float b_, float eps_ = 1e-5f) -> bool { return std::fabs(a_ - b_) <= eps_; }

		using Segments = std::array<TimeSegment, MaxSegmentsPerStep>;

		//====	======	======	======	======	====//
		//	:Time									//
		//====	======	======	======	======	====//

		// 開始直後だけ始点を含み、以後は (前回, 今回]。境界ちょうどのイベントは 1 回だけ
		void TimeForwardBoundaries(std::vector<Failure>& out_) {
			Context c{ out_, "Time.ForwardBoundaries" };
			PlaybackCursor cur{};
			Segments buf{};
			constexpr std::array<float, 4> events{ 0.0f, 0.25f, 0.5f, 1.0f };
			std::array<int, 4> fired{};
			for (int step = 0; step < 6; ++step) {
				int const n{ Advance(cur, 1.0f, 0.25f, buf) };
				for (int i = 0; i < n; ++i) {
					for (std::size_t j = 0; j < events.size(); ++j) {
						if (Contains(buf[static_cast<std::size_t>(i)], events[j])) { ++fired[j]; }
					}
				}
				if (step == 0) {
					c.Check(n == 1 && buf[0].IncludeFrom && buf[0].From == 0.0f, "1 ステップ目は [0, 0.25]");
				}
				if (step == 1) {
					c.Check(n == 1 && !buf[0].IncludeFrom, "2 ステップ目は始点を含まない");
				}
				if (step >= 4) {
					c.Check(n == 0, "非ループの終端に着いた後は区間が空");
				}
			}
			for (std::size_t j = 0; j < events.size(); ++j) {
				c.Check(fired[j] == 1, std::format("t = {} の発火回数が {}（期待 1）", events[j], fired[j]));
			}
			c.Check(cur.Finished && cur.Time == 1.0f, "非ループは長さで止まり Finished");
		}

		// ループの折り返しで区間を 2 つに分ける
		void TimeLoopSplit(std::vector<Failure>& out_) {
			Context c{ out_, "Time.LoopSplit" };
			PlaybackCursor cur{ .Time = 0.9f, .Rate = 1.0f, .Loop = true, .Fresh = false };
			Segments buf{};
			int const n{ Advance(cur, 1.0f, 0.2f, buf) };
			c.Check(n == 2, std::format("区間の数が {}（期待 2）", n));
			if (n == 2) {
				c.Check(Near(buf[0].From, 0.9f) && buf[0].To == 1.0f && !buf[0].IncludeFrom, "1 本目は (0.9, 1.0]");
				c.Check(buf[1].From == 0.0f && Near(buf[1].To, 0.1f) && buf[1].IncludeFrom, "2 本目は [0, 0.1]");
				c.Check(Contains(buf[1], 0.0f), "0 ちょうどは折り返しの区間に含まれる");
			}
			c.Check(Near(cur.Time, 0.1f), "折り返し後の時刻");
		}

		// 1 ステップで何周もする：上限まで区間を並べ、残りは時刻だけ進める
		void TimeLapCap(std::vector<Failure>& out_) {
			Context c{ out_, "Time.LapCap" };
			PlaybackCursor cur{ .Time = 0.0f, .Rate = 10.25f, .Loop = true };
			Segments buf{};
			int const n{ Advance(cur, 1.0f, 1.0f, buf) };
			c.Check(n == MaxSegmentsPerStep, std::format("区間の数が {}（期待 {}）", n, MaxSegmentsPerStep));
			c.Check(Near(cur.Time, 0.25f, 1e-4f), std::format("打ち切り後の時刻が {}（期待 0.25）", cur.Time));
		}

		// dt = 0：開始直後だけ [t, t]、以後は空
		void TimeZeroDt(std::vector<Failure>& out_) {
			Context c{ out_, "Time.ZeroDt" };
			PlaybackCursor cur{ .Time = 0.3f };
			Segments buf{};
			int n{ Advance(cur, 1.0f, 0.0f, buf) };
			c.Check(n == 1 && buf[0].From == 0.3f && buf[0].To == 0.3f && buf[0].IncludeFrom, "開始直後は [0.3, 0.3]");
			c.Check(n == 1 && Contains(buf[0], 0.3f), "開始時刻ちょうどのイベントを拾う");
			n = Advance(cur, 1.0f, 0.0f, buf);
			c.Check(n == 0, "2 回目以降の dt = 0 は区間なし");
			c.Check(cur.Time == 0.3f, "時刻は動かない");
		}

		// 逆再生：[今回, 前回)、開始直後だけ [今回, 前回]
		void TimeReverse(std::vector<Failure>& out_) {
			Context c{ out_, "Time.Reverse" };
			{
				PlaybackCursor cur{ .Time = 1.0f, .Rate = -1.0f };
				Segments buf{};
				std::array<int, 3> fired{};
				constexpr std::array<float, 3> events{ 1.0f, 0.5f, 0.0f };
				for (int step = 0; step < 6; ++step) {
					int const n{ Advance(cur, 1.0f, 0.25f, buf) };
					for (int i = 0; i < n; ++i) {
						for (std::size_t j = 0; j < events.size(); ++j) {
							if (Contains(buf[static_cast<std::size_t>(i)], events[j])) { ++fired[j]; }
						}
					}
				}
				for (std::size_t j = 0; j < events.size(); ++j) {
					c.Check(fired[j] == 1, std::format("非ループ逆再生で t = {} の発火回数が {}（期待 1）", events[j], fired[j]));
				}
				c.Check(cur.Finished && cur.Time == 0.0f, "逆再生は 0 で止まり Finished");
			}
			{
				PlaybackCursor cur{ .Time = 0.1f, .Rate = -1.0f, .Loop = true, .Fresh = false };
				Segments buf{};
				int const n{ Advance(cur, 1.0f, 0.2f, buf) };
				c.Check(n == 2, std::format("逆向きの折り返しで区間の数が {}（期待 2）", n));
				if (n == 2) {
					c.Check(Contains(buf[0], 0.0f), "1 本目 [0, 0.1) は 0 を含む");
					c.Check(!Contains(buf[1], 1.0f), "2 本目は長さちょうど（= 0 と同じ瞬間）を含まない");
				}
				c.Check(Near(cur.Time, 0.9f), "逆向きの折り返し後の時刻");
			}
		}

		// ループ再生で、どの dt・再生速度でも「1 周につき各イベントがちょうど 1 回」
		// 講義のデモで崩れる条件（ステップ数を 10 に下げる、144 に上げる、速度 2 以上、ループのまま待つ）を総当たりする
		void TimeOncePerLap(std::vector<Failure>& out_) {
			Context c{ out_, "Time.OncePerLap" };
			constexpr std::array<float, 4> stepsPerSec{ 10.0f, 30.0f, 60.0f, 144.0f };
			constexpr std::array<float, 3> rates{ 1.0f, 2.5f, 4.0f };
			constexpr std::array<float, 2> lengths{ 1.0f, 1.3333f };

			for (float const length : lengths) {
				// 0 と、途中と、終端の手前（長さちょうどはループでは 0 に寄せる前提なので入れない）
				std::array<float, 4> const events{ 0.0f, length * 0.25f, length * 0.5f, length * 0.999f };
				for (float const sps : stepsPerSec) {
					for (float const rate : rates) {
						PlaybackCursor cur{ .Rate = rate, .Loop = true };
						Segments buf{};
						std::array<int, 4> fired{};
						int laps{ 0 };
						int bad{ 0 };
						for (int step = 0; step < 3000; ++step) {
							int const n{ Advance(cur, length, 1.0f / sps, buf) };
							for (int i = 0; i < n; ++i) {
								TimeSegment const& s{ buf[static_cast<std::size_t>(i)] };
								// 折り返し直後の区間が来たら、直前の 1 周が閉じた
								bool const wrapped{ s.IncludeFrom && s.From == 0.0f && !(step == 0 && i == 0) };
								if (wrapped) {
									for (int const f : fired) { if (f != 1) { ++bad; } }
									fired = {};
									++laps;
								}
								for (std::size_t j = 0; j < events.size(); ++j) {
									if (Contains(s, events[j])) { ++fired[j]; }
								}
							}
						}
						c.Check(laps > 0, std::format("len {} / {} steps/s / rate {}: 1 周もしていない", length, sps, rate));
						c.Check(bad == 0, std::format("len {} / {} steps/s / rate {}: {} 周 中 {} 件の食い違い", length, sps, rate, laps, bad));
					}
				}
			}
		}

		// 区間の重なりの端
		void TimeOverlaps(std::vector<Failure>& out_) {
			Context c{ out_, "Time.Overlaps" };
			// 順再生
			c.Check(Overlaps({ 0.2f, 0.4f, false }, 0.4f, 0.5f), "(0.2, 0.4] と [0.4, 0.5) は 0.4 で重なる");
			c.Check(!Overlaps({ 0.4f, 0.6f, false }, 0.2f, 0.4f), "(0.4, 0.6] と [0.2, 0.4) は重ならない");
			c.Check(!Overlaps({ 0.4f, 0.6f, true }, 0.2f, 0.4f), "[0.4, 0.6] と [0.2, 0.4) は重ならない（窓の終端は含まない）");
			c.Check(Overlaps({ 0.1f, 0.9f, false }, 0.4f, 0.5f), "窓を丸ごと含む区間");
			c.Check(!Overlaps({ 0.3f, 0.3f, false }, 0.0f, 1.0f), "(t, t] は空");
			// 1 点（開始直後の dt = 0）
			c.Check(Overlaps({ 0.3f, 0.3f, true }, 0.3f, 0.5f), "[0.3, 0.3] と [0.3, 0.5)");
			c.Check(!Overlaps({ 0.3f, 0.3f, true }, 0.1f, 0.3f), "[0.3, 0.3] と [0.1, 0.3)");
			// 逆再生
			c.Check(!Overlaps({ 0.4f, 0.2f, false }, 0.4f, 0.5f), "[0.2, 0.4) と [0.4, 0.5) は重ならない");
			c.Check(Overlaps({ 0.4f, 0.2f, true }, 0.4f, 0.5f), "[0.2, 0.4] と [0.4, 0.5) は 0.4 で重なる");
			c.Check(!Overlaps({ 0.4f, 0.2f, false }, 0.1f, 0.2f), "[0.2, 0.4) と [0.1, 0.2) は重ならない");
			c.Check(Overlaps({ 0.4f, 0.2f, false }, 0.1f, 0.25f), "[0.2, 0.4) と [0.1, 0.25)");
		}

		// 1 ステップより短い窓も、少なくとも 1 ステップは開く（40ms の窓を 10 steps/s で）
		void TimeShortWindow(std::vector<Failure>& out_) {
			Context c{ out_, "Time.ShortWindow" };
			PlaybackCursor cur{};
			Segments buf{};
			int openSteps{ 0 };
			for (int step = 0; step < 12; ++step) {
				int const n{ Advance(cur, 1.0f, 0.1f, buf) };
				bool open{ false };
				for (int i = 0; i < n; ++i) { open = open || Overlaps(buf[static_cast<std::size_t>(i)], 0.43f, 0.47f); }
				if (open) { ++openSteps; }
			}
			c.Check(openSteps == 1, std::format("40ms の窓が開いたステップ数が {}（期待 1）", openSteps));
		}

		void TimeWrap(std::vector<Failure>& out_) {
			Context c{ out_, "Time.WrapTime" };
			c.Check(Near(WrapTime(2.25f, 1.0f), 0.25f), "2.25 → 0.25");
			c.Check(Near(WrapTime(-0.25f, 1.0f), 0.75f), "-0.25 → 0.75");
			c.Check(WrapTime(1.0f, 1.0f) == 0.0f, "長さちょうど → 0");
			c.Check(WrapTime(0.5f, 0.0f) == 0.0f, "長さ 0 → 0");
			float const r{ WrapTime(-1e-9f, 1.0f) };
			c.Check(r >= 0.0f && r < 1.0f, "負のごく小さい値でも [0, 長さ) に収まる");
		}

		// 長さ 0 のクリップ
		void TimeZeroLength(std::vector<Failure>& out_) {
			Context c{ out_, "Time.ZeroLength" };
			PlaybackCursor cur{};
			Segments buf{};
			int n{ Advance(cur, 0.0f, 1.0f / 60.0f, buf) };
			c.Check(n == 1 && Contains(buf[0], 0.0f), "開始直後は [0, 0]");
			n = Advance(cur, 0.0f, 1.0f / 60.0f, buf);
			c.Check(n == 0 && cur.Finished, "以後は空で Finished");
		}

		//====	======	======	======	======	====//
		//	:Data									//
		//====	======	======	======	======	====//

		struct TestPayload {
			static constexpr std::string_view TypeName{ "Test.Payload" };
			std::int32_t A;
			float B;
		};
		struct NoTypeName { int A; };
		struct NotTrivial {
			static constexpr std::string_view TypeName{ "Test.NotTrivial" };
			std::string S;
		};

		static_assert(EventPayload<TestPayload>);
		static_assert(!EventPayload<NoTypeName>);
		static_assert(!EventPayload<NotTrivial>);
		static_assert(TypeIdOf("a") == 0xE40C292CU, "FNV-1a 32bit の既知の値");
		static_assert(TypeIdOf<TestPayload>() == TypeIdOf("Test.Payload"));
		static_assert(TypeIdOf("Hitbox") != TypeIdOf("Hitbox "));

		void DataPayloadRoundTrip(std::vector<Failure>& out_) {
			Context c{ out_, "Data.PayloadRoundTrip" };
			std::array<std::byte, 16> blob{};
			TestPayload const src{ .A = -7, .B = 2.5f };
			std::memcpy(blob.data() + 4, &src, sizeof(src));	// わざと 4 バイトずらした位置に置く

			AnimEvent e{};
			e.Type = TypeIdOf<TestPayload>();
			e.Payload = blob.data() + 4;
			e.PayloadSize = static_cast<std::uint32_t>(sizeof(TestPayload));
			c.Check(Is<TestPayload>(e), "Is<T> が型IDで一致する");
			TestPayload const got{ PayloadAs<TestPayload>(e) };
			c.Check(got.A == -7 && got.B == 2.5f, "memcpy で取り出した値が一致する");
		}

		//====	======	======	======	======	====//
		//	:Bake									//
		//====	======	======	======	======	====//

		struct WidePayload {
			static constexpr std::string_view TypeName{ "Test.Wide" };
			std::uint64_t V;
		};

		template<class T>
		auto BytesOf(T const& v_) -> PayloadBytes {
			PayloadBytes p{};
			p.Bytes.resize(sizeof(T));
			std::memcpy(p.Bytes.data(), &v_, sizeof(T));
			p.Align = alignof(T);
			return p;
		}

		auto CountSeverity(std::vector<BakeIssue> const& issues_, BakeSeverity s_) -> int {
			int n{ 0 };
			for (BakeIssue const& i : issues_) { if (i.Severity == s_) { ++n; } }
			return n;
		}

		// フレーム → 秒、並べ替え、ペイロードの詰め方
		void BakeBasic(std::vector<Failure>& out_) {
			Context c{ out_, "Bake.Basic" };
			ClipEventSource src{};
			src.ClipName = "basic";
			src.FrameRate = 30.0f;
			src.Instants.push_back({ .TypeName = "PlaySound", .Frame = 12, .MinWeight = 0.5f, .Category = EventCategory::Presentation });
			src.Instants.push_back({ .TypeName = "Step", .Frame = 3 });
			// 1 バイトのペイロードの後に 8 バイト境界の型を置き、詰め方を確かめる
			PayloadBytes one{};
			one.Bytes = { std::byte{ 0x7F } };
			src.Windows.push_back({ .TypeName = "Hitbox", .BeginFrame = 13, .EndFrame = 16, .Payload = one });
			src.Windows.push_back({ .TypeName = "SuperArmor", .BeginFrame = 3, .EndFrame = 18, .Payload = BytesOf(WidePayload{ 0x1122334455667788ULL }) });

			std::vector<BakeIssue> issues{};
			std::optional<ClipEvents> const ev{ Bake(src, 1.0f, issues) };
			c.Check(ev.has_value(), "焼き込みが成功する");
			c.Check(issues.empty(), std::format("問題が {} 件出た（期待 0）", issues.size()));
			if (!ev) { return; }

			c.Check(ev->Length == 1.0f, "Length はエンジンから渡した値");
			c.Check(ev->Instants.size() == 2U, "瞬間イベント 2 件");
			if (ev->Instants.size() == 2U) {
				c.Check(Near(ev->Instants[0].Time, 0.1f) && ev->Instants[0].Type == TypeIdOf("Step"), "Time 昇順に並ぶ（3 フレーム目が先）");
				c.Check(Near(ev->Instants[1].Time, 0.4f) && ev->Instants[1].MinWeight == 0.5f, "12 フレーム目 = 0.4 秒、MinWeight を引き継ぐ");
			}
			c.Check(ev->Windows.size() == 2U, "窓 2 件");
			if (ev->Windows.size() == 2U) {
				WindowEvent const& w0{ ev->Windows[0] };
				WindowEvent const& w1{ ev->Windows[1] };
				c.Check(w0.Type == TypeIdOf("SuperArmor") && Near(w0.Begin, 0.1f) && Near(w0.End, 0.6f), "Begin 昇順（窓ID 0 = SuperArmor）");
				c.Check(w1.Type == TypeIdOf("Hitbox") && w1.Category == EventCategory::Gameplay, "窓ID 1 = Hitbox、既定の分類は Gameplay");
				c.Check(w1.PayloadSize == 1U && w0.PayloadSize == sizeof(WidePayload), "ペイロードのサイズ");
				c.Check(w0.PayloadOffset % alignof(WidePayload) == 0U, std::format("8 バイト境界に詰める（offset = {}）", w0.PayloadOffset));
				if (w0.PayloadOffset + w0.PayloadSize <= ev->PayloadBlob.size()) {
					WidePayload got{};
					std::memcpy(&got, ev->PayloadBlob.data() + w0.PayloadOffset, sizeof(got));
					c.Check(got.V == 0x1122334455667788ULL, "詰めたペイロードの値が保たれる");
				}
			}
		}

		// 同じ Begin の窓は JSON に書いた順を保つ
		void BakeStableOrder(std::vector<Failure>& out_) {
			Context c{ out_, "Bake.StableOrder" };
			ClipEventSource src{};
			src.Windows.push_back({ .TypeName = "B", .BeginFrame = 5, .EndFrame = 6 });
			src.Windows.push_back({ .TypeName = "A", .BeginFrame = 5, .EndFrame = 9 });
			src.Windows.push_back({ .TypeName = "C", .BeginFrame = 1, .EndFrame = 2 });
			std::vector<BakeIssue> issues{};
			auto const ev{ Bake(src, 1.0f, issues) };
			c.Check(ev && ev->Windows.size() == 3U, "焼き込みが成功する");
			if (ev && ev->Windows.size() == 3U) {
				c.Check(ev->Windows[0].Type == TypeIdOf("C") && ev->Windows[1].Type == TypeIdOf("B") && ev->Windows[2].Type == TypeIdOf("A"),
					"C（1）→ B（5, 先に書いた）→ A（5）");
			}
		}

		// エラーはすべて集めてから nullopt
		void BakeErrors(std::vector<Failure>& out_) {
			Context c{ out_, "Bake.Errors" };
			ClipEventSource src{};
			src.ClipName = "errors";
			src.Instants.push_back({ .TypeName = "", .Frame = 0 });								// type が空
			src.Windows.push_back({ .TypeName = "Zero", .BeginFrame = 4, .EndFrame = 4 });		// 長さ 0
			src.Windows.push_back({ .TypeName = "Rev", .BeginFrame = 9, .EndFrame = 2 });		// 逆転
			PayloadBytes badAlign{};
			badAlign.Bytes = { std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 } };
			badAlign.Align = 3U;
			src.Windows.push_back({ .TypeName = "Align", .BeginFrame = 1, .EndFrame = 2, .Payload = badAlign });	// アラインメント不正
			std::vector<BakeIssue> issues{};
			issues.push_back({ BakeSeverity::Error, "呼び出し前から入っていたもの" });
			auto const ev{ Bake(src, 1.0f, issues) };
			c.Check(!ev.has_value(), "エラーがあれば nullopt");
			c.Check(CountSeverity(issues, BakeSeverity::Error) == 5, std::format("エラーが {} 件（期待 既存 1 + 4）", CountSeverity(issues, BakeSeverity::Error)));
			c.Check(issues.size() >= 2U && issues[1].Message.starts_with("[errors] "), "メッセージの先頭にクリップ名");	// starts_with は C++20

			// 呼び出し前の issues にあったエラーは、今回の成否に影響しない
			ClipEventSource ok{};
			ok.Instants.push_back({ .TypeName = "Fine", .Frame = 1 });
			c.Check(Bake(ok, 1.0f, issues).has_value(), "既存のエラーがあっても、今回のソースが正しければ成功する");
		}

		void BakeTooManyWindows(std::vector<Failure>& out_) {
			Context c{ out_, "Bake.TooManyWindows" };
			ClipEventSource src{};
			for (int i = 0; i < 65; ++i) { src.Windows.push_back({ .TypeName = "W", .BeginFrame = i % 20, .EndFrame = i % 20 + 1 }); }
			std::vector<BakeIssue> issues{};
			c.Check(!Bake(src, 1.0f, issues).has_value(), "65 個はエラー");
			src.Windows.pop_back();
			issues.clear();
			c.Check(Bake(src, 1.0f, issues).has_value(), "64 個までは通る");
		}

		void BakeBadClip(std::vector<Failure>& out_) {
			Context c{ out_, "Bake.BadClip" };
			std::vector<BakeIssue> issues{};
			ClipEventSource src{};
			src.FrameRate = 0.0f;
			c.Check(!Bake(src, 1.0f, issues).has_value(), "frameRate = 0 はエラー");
			src.FrameRate = 30.0f;
			c.Check(!Bake(src, -1.0f, issues).has_value(), "負のクリップ長はエラー");
			issues.clear();
			c.Check(Bake(src, 0.0f, issues).has_value(), "長さ 0 のクリップは通る（イベントが 0 に集まるだけ）");
		}

		// 範囲外：半フレーム以内は黙ってクランプ、超えたら警告してクランプ、窓が長さ 0 になればエラー
		void BakeOutOfRange(std::vector<Failure>& out_) {
			Context c{ out_, "Bake.OutOfRange" };
			{
				// 31 フレーム = 1.0333… 秒、クリップ長 1.0333 秒（丸めでわずかに短い）→ 警告なし
				ClipEventSource src{};
				src.Instants.push_back({ .TypeName = "End", .Frame = 31 });
				std::vector<BakeIssue> issues{};
				auto const ev{ Bake(src, 1.0333f, issues) };
				c.Check(ev && issues.empty(), "半フレーム以内のはみ出しは警告しない");
				c.Check(ev && ev->Instants.size() == 1U && ev->Instants[0].Time == 1.0333f, "長さにクランプされる");
			}
			{
				ClipEventSource src{};
				src.Instants.push_back({ .TypeName = "Late", .Frame = 40 });
				src.Windows.push_back({ .TypeName = "Tail", .BeginFrame = 25, .EndFrame = 45 });
				std::vector<BakeIssue> issues{};
				auto const ev{ Bake(src, 1.0f, issues) };
				c.Check(ev.has_value(), "はみ出しは警告だけで成功する");
				c.Check(CountSeverity(issues, BakeSeverity::Warning) == 2, std::format("警告が {} 件（期待 2）", CountSeverity(issues, BakeSeverity::Warning)));
				c.Check(ev && ev->Instants[0].Time == 1.0f && ev->Windows[0].End == 1.0f, "長さにクランプされる");
			}
			{
				ClipEventSource src{};
				src.Windows.push_back({ .TypeName = "Outside", .BeginFrame = 40, .EndFrame = 45 });
				std::vector<BakeIssue> issues{};
				c.Check(!Bake(src, 1.0f, issues).has_value(), "クランプすると長さ 0 になる窓はエラー");
			}
		}

		// カーブ：1 フレーム間隔に焼き直す、範囲外は端の値、同じ名前はエラー
		void BakeCurves(std::vector<Failure>& out_) {
			Context c{ out_, "Bake.Curves" };
			ClipEventSource src{};
			src.Curves.push_back({ .Name = "TurnRate", .Keys = { { 12.0f, 0.0f }, { 0.0f, 1.0f }, { 9.0f, 1.0f } } });	// わざと順不同
			src.Curves.push_back({ .Name = "Step", .Keys = { { 5.0f, 0.0f }, { 5.0f, 1.0f } } });						// 同じフレームのキー
			src.Curves.push_back({ .Name = "Empty", .Keys = {} });
			std::vector<BakeIssue> issues{};
			auto const ev{ Bake(src, 0.5f, issues) };
			c.Check(ev.has_value(), "焼き込みが成功する");
			c.Check(CountSeverity(issues, BakeSeverity::Warning) == 1, "キーなしのカーブは警告して読み飛ばす");
			if (!ev || ev->Curves.size() != 2U) {
				c.Check(false, "カーブが 2 本できる");
				return;
			}
			CurveTrack const& t{ ev->Curves[0] };
			c.Check(t.Id == TypeIdOf("TurnRate") && Near(t.KeyInterval, 1.0f / 30.0f), "ID と間隔");
			c.Check(t.Values.size() == 16U, std::format("0.5 秒 × 30fps → 16 点（実際 {}）", t.Values.size()));
			if (t.Values.size() == 16U) {
				c.Check(t.Values[0] == 1.0f && t.Values[9] == 1.0f, "9 フレーム目まで 1");
				c.Check(Near(t.Values[10], 2.0f / 3.0f) && Near(t.Values[11], 1.0f / 3.0f), "9〜12 フレームは線形");
				c.Check(t.Values[12] == 0.0f && t.Values[15] == 0.0f, "12 フレーム目以降は最後のキーの値");
			}
			CurveTrack const& s{ ev->Curves[1] };
			c.Check(s.Values.size() == 16U && s.Values[4] == 0.0f && s.Values[5] == 1.0f && s.Values[6] == 1.0f,
				"同じフレームに 2 つキーがあれば、後に書いたほうの値を採る");

			ClipEventSource dup{};
			dup.Curves.push_back({ .Name = "X", .Keys = { { 0.0f, 1.0f } } });
			dup.Curves.push_back({ .Name = "X", .Keys = { { 0.0f, 2.0f } } });
			issues.clear();
			c.Check(!Bake(dup, 1.0f, issues).has_value(), "同じ名前のカーブはエラー");
		}

		//====	======	======	======	======	====//
		//	:Player									//
		//====	======	======	======	======	====//

		// テスト用クリップ。frameRate = 100 にして「フレーム = 1/100 秒」で書く
		auto MakeClip(Context& c_, ClipEventSource src_, float length_) -> ClipEvents {
			src_.FrameRate = 100.0f;
			std::vector<BakeIssue> issues{};
			std::optional<ClipEvents> ev{ Bake(src_, length_, issues) };
			c_.Check(ev.has_value(), "テスト用クリップの焼き込み");
			return ev ? std::move(*ev) : ClipEvents{};
		}

		auto Win(std::string_view type_, int begin_, int end_, EventCategory cat_ = EventCategory::Gameplay, float minWeight_ = 0.0f) -> WindowSource {
			return WindowSource{ .TypeName = std::string{ type_ }, .BeginFrame = begin_, .EndFrame = end_, .MinWeight = minWeight_, .Category = cat_ };
		}

		auto Inst(std::string_view type_, int frame_, EventCategory cat_ = EventCategory::Presentation, float minWeight_ = 0.0f) -> InstantSource {
			return InstantSource{ .TypeName = std::string{ type_ }, .Frame = frame_, .MinWeight = minWeight_, .Category = cat_ };
		}

		auto Count(std::vector<AnimEvent> const& v_, std::string_view type_, EventEdge edge_) -> int {
			EventTypeId const t{ TypeIdOf(type_) };
			return static_cast<int>(std::ranges::count_if(v_, [&](AnimEvent const& e_) { return e_.Type == t && e_.Edge == edge_; }));
		}

		auto FindEdge(std::vector<AnimEvent> const& v_, std::string_view type_, EventEdge edge_) -> AnimEvent const* {
			EventTypeId const t{ TypeIdOf(type_) };
			auto const itr{ std::ranges::find_if(v_, [&](AnimEvent const& e_) { return e_.Type == t && e_.Edge == edge_; }) };
			return itr == v_.end() ? nullptr : &*itr;
		}

		// 1 ステップより短い窓（40ms を 10 steps/s で）：Begin と End が 1 回ずつ。Begin は入った位置を持つ
		void PlayerShortWindow(std::vector<Failure>& out_) {
			Context c{ out_, "Player.ShortWindow" };
			ClipEventSource src{};
			src.Windows.push_back(Win("Hitbox", 43, 47));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };

			AnimEventPlayer p{};
			p.Play({ .Events = &clip, .OwnsAction = true });
			std::vector<AnimEvent> all{};
			int openSteps{ 0 };
			for (int step = 0; step < 12; ++step) {
				std::vector<AnimEvent> ev{};
				p.Step(0.1f, ev);
				all.insert(all.end(), ev.begin(), ev.end());
				if (p.IsOpen(TypeIdOf("Hitbox"))) { ++openSteps; }
			}
			c.Check(Count(all, "Hitbox", EventEdge::Begin) == 1 && Count(all, "Hitbox", EventEdge::End) == 1, "Begin / End が 1 回ずつ");
			c.Check(openSteps == 1, std::format("開いていたステップ数が {}（期待 1）", openSteps));
			if (AnimEvent const* b{ FindEdge(all, "Hitbox", EventEdge::Begin) }) {
				c.Check(Near(b->StepFraction, 0.3f, 1e-3f), std::format("Begin の StepFraction が {}（期待 0.3：0.4→0.5 の 0.43 で入る）", b->StepFraction));
			}
			if (AnimEvent const* e{ FindEdge(all, "Hitbox", EventEdge::End) }) {
				c.Check(e->Reason == EndReason::Completed, "最後まで通過したら Completed");
			}
			p.StopAllNow(EndReason::Interrupted, all);
		}

		// 窓の途中で止める：次の Step の頭で Interrupted の End（前編 4.5）
		void PlayerInterrupt(std::vector<Failure>& out_) {
			Context c{ out_, "Player.Interrupt" };
			ClipEventSource src{};
			src.Windows.push_back(Win("Hitbox", 20, 60));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };

			AnimEventPlayer p{};
			InstanceId const id{ p.Play({ .Events = &clip, .OwnsAction = true }) };
			std::vector<AnimEvent> ev{};
			for (int i = 0; i < 3; ++i) { p.Step(0.1f, ev); }
			c.Check(p.IsOpen(TypeIdOf("Hitbox")), "0.3 秒時点で開いている");

			p.Stop(id);
			c.Check(p.IsOpen(TypeIdOf("Hitbox")), "Stop は要求なので、Step までは開いたまま");
			ev.clear();
			p.Step(0.1f, ev);
			c.Check(ev.size() == 1U && ev[0].Edge == EventEdge::End && ev[0].Reason == EndReason::Interrupted && ev[0].StepFraction == 0.0f,
				"中断の End（Interrupted）が 1 件だけ");
			c.Check(!p.IsOpen(TypeIdOf("Hitbox")) && !p.IsPlaying(id) && !p.Time(id).has_value(), "止めた後は開いていない・再生していない");
		}

		// 新しい持ち主が来たら、前の持ち主の判定系だけ閉じる。演出系は出し続ける（前編 4.6）
		void PlayerOwnerTransfer(std::vector<Failure>& out_) {
			Context c{ out_, "Player.OwnerTransfer" };
			ClipEventSource a{};
			a.Windows.push_back(Win("Hitbox", 0, 100));
			a.Windows.push_back(Win("Trail", 0, 100, EventCategory::Presentation));
			a.Instants.push_back(Inst("Release", 50, EventCategory::Gameplay));
			a.Instants.push_back(Inst("Swing", 50, EventCategory::Presentation));
			ClipEvents const clipA{ MakeClip(c, a, 1.0f) };
			ClipEventSource b{};
			b.Windows.push_back(Win("Guard", 0, 100));
			ClipEvents const clipB{ MakeClip(c, b, 1.0f) };

			AnimEventPlayer p{};
			InstanceId const idA{ p.Play({ .Events = &clipA, .OwnsAction = true }) };
			std::vector<AnimEvent> ev{};
			p.Step(0.1f, ev);
			c.Check(p.IsOpen(TypeIdOf("Hitbox")) && p.IsOpen(TypeIdOf("Trail")), "A の窓が両方開く");

			InstanceId const idB{ p.Play({ .Events = &clipB, .OwnsAction = true }) };
			ev.clear();
			p.Step(0.1f, ev);
			AnimEvent const* end{ FindEdge(ev, "Hitbox", EventEdge::End) };
			c.Check(end != nullptr && end->Reason == EndReason::Interrupted && end->Source == idA, "A の Hitbox は Interrupted で閉じる");
			c.Check(FindEdge(ev, "Trail", EventEdge::End) == nullptr && p.IsOpen(TypeIdOf("Trail")), "A の Trail（演出系）は開いたまま");
			c.Check(p.IsOpen(TypeIdOf("Guard")) && p.ActionOwner() == idB, "持ち主は B");

			std::vector<AnimEvent> rest{};
			for (int i = 0; i < 6; ++i) { p.Step(0.1f, rest); }
			c.Check(Count(rest, "Release", EventEdge::Instant) == 0, "持ち主でなくなった A の判定系の瞬間イベントは出ない");
			c.Check(Count(rest, "Swing", EventEdge::Instant) == 1, "A の演出系の瞬間イベントは出る");
			c.Check(Count(rest, "Hitbox", EventEdge::Begin) == 0, "A の Hitbox は二度と開かない");
			p.StopAllNow(EndReason::Interrupted, rest);
		}

		// 持ち主でないインスタンスの判定系は、最初から一切出ない
		void PlayerNonOwner(std::vector<Failure>& out_) {
			Context c{ out_, "Player.NonOwner" };
			ClipEventSource src{};
			src.Windows.push_back(Win("Hitbox", 0, 50));
			src.Instants.push_back(Inst("Release", 10, EventCategory::Gameplay));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };
			AnimEventPlayer p{};
			p.Play({ .Events = &clip });
			std::vector<AnimEvent> ev{};
			for (int i = 0; i < 12; ++i) { p.Step(0.1f, ev); }
			c.Check(ev.empty() && !p.IsOpen(TypeIdOf("Hitbox")), "イベントなし");
			p.StopAllNow(EndReason::Interrupted, ev);
		}

		// 演出系はウェイトの閾値で採る。窓の途中で閾値を割ったら Interrupted で閉じる
		void PlayerWeight(std::vector<Failure>& out_) {
			Context c{ out_, "Player.Weight" };
			ClipEventSource src{};
			src.Instants.push_back(Inst("Sound", 25, EventCategory::Presentation, 0.5f));
			src.Windows.push_back(Win("Trail", 0, 100, EventCategory::Presentation, 0.5f));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };

			AnimEventPlayer p{};
			InstanceId const id{ p.Play({ .Events = &clip, .Weight = 0.3f, .Loop = true }) };
			std::vector<AnimEvent> ev{};
			for (int i = 0; i < 4; ++i) { p.Step(0.1f, ev); }
			c.Check(ev.empty(), "ウェイト 0.3 では何も出ない");

			p.SetWeight(id, 0.8f);
			ev.clear();
			p.Step(0.1f, ev);
			c.Check(Count(ev, "Trail", EventEdge::Begin) == 1, "閾値を超えたら Trail が開く");

			p.SetWeight(id, 0.2f);
			ev.clear();
			p.Step(0.1f, ev);
			AnimEvent const* end{ FindEdge(ev, "Trail", EventEdge::End) };
			c.Check(end != nullptr && end->Reason == EndReason::Interrupted, "閾値を割ったら Interrupted で閉じる");

			p.SetWeight(id, 1.0f);
			ev.clear();
			for (int i = 0; i < 10; ++i) { p.Step(0.1f, ev); }
			c.Check(Count(ev, "Sound", EventEdge::Instant) == 1, "ウェイト 1 なら 1 周で Sound が 1 回");
			p.StopAllNow(EndReason::Interrupted, ev);
		}

		// 一時停止（dt = 0）：窓は開いたまま、瞬間イベントは出ない
		void PlayerPause(std::vector<Failure>& out_) {
			Context c{ out_, "Player.Pause" };
			ClipEventSource src{};
			src.Windows.push_back(Win("Armor", 20, 80));
			src.Instants.push_back(Inst("Tick", 30));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };
			AnimEventPlayer p{};
			p.Play({ .Events = &clip, .OwnsAction = true });
			std::vector<AnimEvent> ev{};
			for (int i = 0; i < 3; ++i) { p.Step(0.1f, ev); }	// 0.3 秒
			ev.clear();
			for (int i = 0; i < 30; ++i) { p.Step(0.0f, ev); }
			c.Check(ev.empty(), "止まっている間はイベントなし");
			c.Check(p.IsOpen(TypeIdOf("Armor")), "窓は開いたまま");
			p.StopAllNow(EndReason::Interrupted, ev);
		}

		// ループの「長さちょうど」のイベントは 0 として扱う（1 周に 1 回）。非ループなら終端で 1 回
		void PlayerLoopEnd(std::vector<Failure>& out_) {
			Context c{ out_, "Player.LoopEnd" };
			ClipEventSource src{};
			src.Instants.push_back(Inst("Edge", 100));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };
			{
				AnimEventPlayer p{};
				p.Play({ .Events = &clip, .Loop = true });
				std::vector<AnimEvent> ev{};
				for (int i = 0; i < 10; ++i) { p.Step(0.25f, ev); }	// 0 → 2.5 秒
				int const n{ Count(ev, "Edge", EventEdge::Instant) };
				c.Check(n == 3, std::format("ループで 0, 1, 2 秒の 3 回のはずが {} 回", n));
				bool allZero{ true };
				for (AnimEvent const& e : ev) { allZero = allZero && e.ClipTime == 0.0f; }
				c.Check(allZero, "ClipTime は 0 として報告される");
			}
			{
				AnimEventPlayer p{};
				InstanceId const id{ p.Play({ .Events = &clip }) };
				std::vector<AnimEvent> ev{};
				for (int i = 0; i < 10; ++i) { p.Step(0.25f, ev); }
				c.Check(Count(ev, "Edge", EventEdge::Instant) == 1, "非ループなら終端で 1 回");
				c.Check(p.IsFinished(id) && p.IsPlaying(id) && p.Time(id) == 1.0f, "終端で止まり、インスタンスは残る（最後のコマを保持）");
			}
		}

		// 非ループの終端に掛かる窓は、終端に着いた次のステップで Completed で閉じる
		void PlayerFinishCloses(std::vector<Failure>& out_) {
			Context c{ out_, "Player.FinishCloses" };
			ClipEventSource src{};
			src.Windows.push_back(Win("Recovery", 80, 100));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };
			AnimEventPlayer p{};
			p.Play({ .Events = &clip, .OwnsAction = true });
			std::vector<AnimEvent> ev{};
			for (int i = 0; i < 12; ++i) { p.Step(0.1f, ev); }
			AnimEvent const* end{ FindEdge(ev, "Recovery", EventEdge::End) };
			c.Check(Count(ev, "Recovery", EventEdge::Begin) == 1 && end != nullptr && end->Reason == EndReason::Completed, "Begin 1 回、End は Completed");
			c.Check(!p.IsOpen(TypeIdOf("Recovery")), "止まった後は閉じている");
		}

		// 同じ型の窓が隣り合っていても、状態は途切れない
		void PlayerAdjacentWindows(std::vector<Failure>& out_) {
			Context c{ out_, "Player.AdjacentWindows" };
			ClipEventSource src{};
			// 2 進で割り切れる時刻を使う（0.1 刻みだと積み重ねの丸めで境界の判定がぶれる）
			src.Windows.push_back(Win("Invincible", 25, 50));
			src.Windows.push_back(Win("Invincible", 50, 75));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };
			AnimEventPlayer p{};
			p.Play({ .Events = &clip, .OwnsAction = true });
			std::vector<AnimEvent> ev{};
			std::string open{};
			for (int i = 0; i < 8; ++i) {
				p.Step(0.125f, ev);
				open += p.IsOpen(TypeIdOf("Invincible")) ? '1' : '0';
			}
			// (0, 0.125] (0.125, 0.25] … (0.875, 1.0]：0.25 で開き、0.75 の手前を含むステップまで開いている
			c.Check(open == "01111100", std::format("開いていたステップの並びが {}（期待 01111100）", open));
			p.StopAllNow(EndReason::Interrupted, ev);
		}

		// 同じステップの中では End が先（StopOthers で入れ替えた瞬間など）
		void PlayerOrdering(std::vector<Failure>& out_) {
			Context c{ out_, "Player.Ordering" };
			ClipEventSource a{};
			a.Windows.push_back(Win("Old", 0, 100));
			ClipEvents const clipA{ MakeClip(c, a, 1.0f) };
			ClipEventSource b{};
			b.Windows.push_back(Win("New", 0, 100));
			b.Instants.push_back(Inst("Start", 0));
			b.Instants.push_back(Inst("Mid", 5));
			ClipEvents const clipB{ MakeClip(c, b, 1.0f) };

			AnimEventPlayer p{};
			p.Play({ .Events = &clipA, .OwnsAction = true });
			std::vector<AnimEvent> ev{};
			p.Step(0.1f, ev);
			p.Play({ .Events = &clipB, .OwnsAction = true, .StopOthers = true });
			ev.clear();
			p.Step(0.1f, ev);
			c.Check(ev.size() == 4U, std::format("4 件のはずが {} 件", ev.size()));
			if (ev.size() == 4U) {
				c.Check(ev[0].Type == TypeIdOf("Old") && ev[0].Edge == EventEdge::End, "1 件目：Old の End");
				c.Check(ev[1].Edge != EventEdge::End && ev[2].Edge != EventEdge::End, "End の後に Begin / 瞬間");
				c.Check(ev[3].Type == TypeIdOf("Mid") && Near(ev[3].StepFraction, 0.5f, 1e-3f), "最後は 0.05 秒（ステップの半ば）の Mid");
			}
			p.StopAllNow(EndReason::Interrupted, ev);
		}

		// 途中から始める：反映前でも Time が引ける。開始時刻ちょうどのイベントを拾う
		void PlayerStartTime(std::vector<Failure>& out_) {
			Context c{ out_, "Player.StartTime" };
			ClipEventSource src{};
			src.Instants.push_back(Inst("At50", 50));
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };
			AnimEventPlayer p{};
			InstanceId const id{ p.Play({ .Events = &clip, .Rate = 2.0f, .StartTime = 0.5f }) };
			c.Check(p.Time(id) == 0.5f && !p.IsPlaying(id), "反映前の Time は StartTime");
			std::vector<AnimEvent> ev{};
			p.Step(0.1f, ev);
			c.Check(Count(ev, "At50", EventEdge::Instant) == 1, "開始時刻ちょうどのイベントを拾う");
			c.Check(p.Time(id).has_value() && Near(*p.Time(id), 0.7f), "Rate = 2 で 0.2 進む");
			p.SetRate(id, 0.0f);
			p.Step(0.1f, ev);
			c.Check(p.Time(id).has_value() && Near(*p.Time(id), 0.7f), "SetRate(0) で止まる");
		}

		// ペイロードは焼き込みデータを指す。ForEachOpenWindow からも読める
		void PlayerPayload(std::vector<Failure>& out_) {
			Context c{ out_, "Player.Payload" };
			ClipEventSource src{};
			WindowSource w{ Win("Test.Wide", 0, 50) };
			w.Payload = BytesOf(WidePayload{ 42U });
			src.Windows.push_back(w);
			ClipEvents const clip{ MakeClip(c, src, 1.0f) };
			AnimEventPlayer p{};
			p.Play({ .Events = &clip, .OwnsAction = true });
			std::vector<AnimEvent> ev{};
			p.Step(0.1f, ev);
			c.Check(ev.size() == 1U && Is<WidePayload>(ev[0]) && PayloadAs<WidePayload>(ev[0]).V == 42U, "Begin のペイロード");
			int seen{ 0 };
			p.ForEachOpenWindow([&](OpenWindowInfo const& info_) {
				++seen;
				WidePayload v{};
				if (info_.Payload != nullptr) { std::memcpy(&v, info_.Payload, sizeof(v)); }
				c.Check(v.V == 42U && info_.OwnsAction && info_.Window != nullptr && info_.Window->Type == TypeIdOf<WidePayload>(), "ForEachOpenWindow の中身");
			});
			c.Check(seen == 1, "開いている窓は 1 つ");
			c.Check(p.IsOpen<WidePayload>(), "IsOpen<T>");
			p.StopAllNow(EndReason::Interrupted, ev);
		}

		//====	======	======	======	======	====//
		//	カーブ・Rebind							//
		//====	======	======	======	======	====//

		void CurveSample(std::vector<Failure>& out_) {
			Context c{ out_, "Curve.Sample" };
			CurveTrack t{};
			t.KeyInterval = 0.5f;
			t.Values = { 0.0f, 1.0f, 3.0f };
			c.Check(SampleCurve(t, 0.25f) == 0.5f, "キーの間は線形");
			c.Check(SampleCurve(t, 0.75f) == 2.0f, "2 区間目");
			c.Check(SampleCurve(t, -1.0f) == 0.0f && SampleCurve(t, 99.0f) == 3.0f, "範囲外は端の値");
			c.Check(SampleCurve(t, 1.0f) == 3.0f, "最後のキーちょうど");
			c.Check(SampleCurve(t, std::nanf("")) == 0.0f, "NaN は先頭の値");
			c.Check(SampleCurve(CurveTrack{}, 0.5f) == 0.0f, "値が無ければ 0");
		}

		// インスタンスのウェイトで加重平均。持っていないインスタンスは数えない
		void CurveBlend(std::vector<Failure>& out_) {
			Context c{ out_, "Curve.Blend" };
			ClipEventSource a{};
			a.Curves.push_back({ .Name = "TurnRate", .Keys = { { 0.0f, 0.0f }, { 10.0f, 1.0f } } });
			ClipEvents const clipA{ MakeClip(c, a, 1.0f) };
			ClipEventSource b{};
			b.Curves.push_back({ .Name = "TurnRate", .Keys = { { 0.0f, 3.0f } } });
			ClipEvents const clipB{ MakeClip(c, b, 1.0f) };
			ClipEventSource none{};
			ClipEvents const clipN{ MakeClip(c, none, 1.0f) };

			AnimEventPlayer p{};
			InstanceId const idA{ p.Play({ .Events = &clipA, .Weight = 0.25f }) };
			std::vector<AnimEvent> ev{};
			p.Step(0.05f, ev);
			c.Check(Near(p.Curve(TypeIdOf("TurnRate"), -1.0f), 0.5f), "1 本だけなら、その値（0.05 秒で 0.5）");
			c.Check(p.Curve(TypeIdOf("Missing"), -1.0f) == -1.0f, "持っていなければ fallback");

			InstanceId const idB{ p.Play({ .Events = &clipB, .Weight = 0.75f }) };
			p.Play({ .Events = &clipN, .Weight = 1.0f });
			p.Step(0.0f, ev);
			// (0.25 × 0.5 + 0.75 × 3) / (0.25 + 0.75)。カーブを持たない clipN は数えない
			c.Check(Near(p.Curve(TypeIdOf("TurnRate"), -1.0f), 2.375f), std::format("加重平均（実際 {}）", p.Curve(TypeIdOf("TurnRate"), -1.0f)));

			p.SetWeight(idA, 0.0f);
			p.SetWeight(idB, 0.0f);
			p.Step(0.0f, ev);
			c.Check(p.Curve(TypeIdOf("TurnRate"), -1.0f) == -1.0f, "ウェイトの合計が 0 なら fallback");
		}

		// 差し替え：古い窓を Reloaded で閉じ、新しいデータで現在時刻を含む窓を開き直す
		void RebindBasic(std::vector<Failure>& out_) {
			Context c{ out_, "Rebind.Basic" };
			ClipEventSource oldSrc{};
			oldSrc.Windows.push_back(Win("Hitbox", 20, 50));
			ClipEvents const oldClip{ MakeClip(c, oldSrc, 1.0f) };
			ClipEventSource newSrc{};
			newSrc.Windows.push_back(Win("Armor", 10, 40));
			newSrc.Windows.push_back(Win("Hitbox", 60, 80));
			newSrc.Instants.push_back(Inst("Tick", 30));
			ClipEvents const newClip{ MakeClip(c, newSrc, 1.0f) };

			AnimEventPlayer p{};
			InstanceId const id{ p.Play({ .Events = &oldClip, .OwnsAction = true }) };
			std::vector<AnimEvent> ev{};
			for (int i = 0; i < 3; ++i) { p.Step(0.1f, ev); }	// 0.3 秒、Hitbox が開いている
			c.Check(p.IsOpen(TypeIdOf("Hitbox")), "差し替え前は Hitbox が開いている");

			ev.clear();
			p.Rebind(&oldClip, &newClip, ev);
			c.Check(ev.size() == 2U, std::format("End 1 件 + Begin 1 件のはずが {} 件", ev.size()));
			if (ev.size() == 2U) {
				c.Check(ev[0].Type == TypeIdOf("Hitbox") && ev[0].Edge == EventEdge::End && ev[0].Reason == EndReason::Reloaded, "古い Hitbox は Reloaded で閉じる");
				c.Check(ev[1].Type == TypeIdOf("Armor") && ev[1].Edge == EventEdge::Begin && ev[1].Source == id, "新しいデータで 0.3 秒を含む Armor が開く");
			}
			c.Check(!p.IsOpen(TypeIdOf("Hitbox")) && p.IsOpen(TypeIdOf("Armor")), "問い合わせも新しいデータ");
			c.Check(Count(ev, "Tick", EventEdge::Instant) == 0, "瞬間イベントは出さない");

			ev.clear();
			for (int i = 0; i < 5; ++i) { p.Step(0.1f, ev); }	// 0.3 → 0.8
			c.Check(Count(ev, "Armor", EventEdge::End) == 1 && Count(ev, "Hitbox", EventEdge::Begin) == 1, "差し替え後は新しいデータで進む");
			p.StopAllNow(EndReason::Interrupted, ev);

			// 開き直すときも採用の方針（前編 4.6）を守る：持ち主でなければ判定系は開かない、閾値未満なら演出系は開かない
			ClipEventSource mixed{};
			mixed.Windows.push_back(Win("Armor", 0, 100));
			mixed.Windows.push_back(Win("Trail", 0, 100, EventCategory::Presentation, 0.5f));
			ClipEvents const mixedClip{ MakeClip(c, mixed, 1.0f) };
			AnimEventPlayer q{};
			q.Play({ .Events = &oldClip, .Weight = 0.3f });
			q.Step(0.3f, ev);
			ev.clear();
			q.Rebind(&oldClip, &mixedClip, ev);
			c.Check(ev.empty() && !q.IsOpen(TypeIdOf("Armor")) && !q.IsOpen(TypeIdOf("Trail")), "持ち主でない・ウェイト不足のインスタンスでは開き直さない");
			q.StopAllNow(EndReason::Interrupted, ev);
		}

		// 反映前の Play も差し替わる。短くなったら現在時刻を収める
		void RebindPendingAndShorter(std::vector<Failure>& out_) {
			Context c{ out_, "Rebind.PendingAndShorter" };
			ClipEventSource src{};
			src.Instants.push_back(Inst("Start", 0));
			ClipEvents const longClip{ MakeClip(c, src, 1.0f) };
			ClipEvents const shortClip{ MakeClip(c, src, 0.5f) };
			{
				AnimEventPlayer p{};
				InstanceId const id{ p.Play({ .Events = &longClip }) };
				std::vector<AnimEvent> ev{};
				p.Rebind(&longClip, &shortClip, ev);
				c.Check(ev.empty(), "反映前なのでイベントなし");
				p.Step(0.1f, ev);
				c.Check(Count(ev, "Start", EventEdge::Instant) == 1 && p.IsPlaying(id), "Play が新しいデータで反映される");
				for (int i = 0; i < 10; ++i) { p.Step(0.1f, ev); }
				c.Check(p.IsFinished(id) && p.Time(id) == 0.5f, "新しい長さ（0.5 秒）で終わる");
			}
			{
				AnimEventPlayer p{};
				InstanceId const id{ p.Play({ .Events = &longClip }) };
				std::vector<AnimEvent> ev{};
				for (int i = 0; i < 8; ++i) { p.Step(0.1f, ev); }	// 0.8 秒
				p.Rebind(&longClip, &shortClip, ev);
				c.Check(p.Time(id) == 0.5f, "差し替え先が短ければ、現在時刻を長さに収める");
			}
		}

		// ランダムな操作（Rebind を含む）を続けても、窓ごとに Begin / End が必ず交互に出て、最後は全部閉じる。
		// イベント列から復元した「開いている集合」が、常に ForEachOpenWindow と一致する
		void PlayerRandomInvariant(std::vector<Failure>& out_) {
			Context c{ out_, "Player.RandomInvariant" };
			std::vector<ClipEvents> clips{};
			for (int k = 0; k < 3; ++k) {
				ClipEventSource src{};
				for (int i = 0; i < 6; ++i) {
					int const b{ (i * 17 + k * 11) % 90 };
					src.Windows.push_back(Win(i % 2 == 0 ? "G" : "P", b, b + 3 + (i * 7) % 20, i % 2 == 0 ? EventCategory::Gameplay : EventCategory::Presentation, static_cast<float>(i % 3) * 0.3f));
				}
				src.Instants.push_back(Inst("I", (k * 23) % 100, EventCategory::Presentation, 0.4f));
				clips.push_back(MakeClip(c, src, 1.0f));
			}
			// ホットリロードの差し替え先：窓をずらし、長さも変えたもの
			std::vector<ClipEvents> alts{};
			for (int k = 0; k < 3; ++k) {
				ClipEventSource src{};
				for (int i = 0; i < 5; ++i) {
					int const b{ (i * 13 + k * 7) % 70 };
					src.Windows.push_back(Win(i % 2 == 0 ? "G" : "P", b, b + 5 + i * 3, i % 2 == 0 ? EventCategory::Gameplay : EventCategory::Presentation));
				}
				alts.push_back(MakeClip(c, src, 0.8f));
			}
			std::array<bool, 3> swapped{};

			std::uint32_t rng{ 12345U };
			auto const next{ [&](std::uint32_t mod_) { rng = rng * 1664525U + 1013904223U; return (rng >> 8U) % mod_; } };

			AnimEventPlayer p{};
			std::vector<InstanceId> ids{};
			std::vector<std::pair<InstanceId, std::uint16_t>> open{};	// イベント列から復元した開いている窓
			int violations{ 0 };
			int mismatches{ 0 };
			int ineligible{ 0 };

			auto const consume{
				[&](std::vector<AnimEvent> const& ev_) {
					for (AnimEvent const& e : ev_) {
						if (e.Edge == EventEdge::Instant) { continue; }
						std::pair const key{ e.Source, e.WindowIndex };
						auto const itr{ std::ranges::find(open, key) };
						if (e.Edge == EventEdge::Begin) {
							if (itr != open.end()) { ++violations; } else { open.push_back(key); }
						}
						else {
							if (itr == open.end()) { ++violations; } else { open.erase(itr); }
						}
					}
				}
			};

			for (int step = 0; step < 3000; ++step) {
				switch (next(10U)) {
				case 0: {
					ClipEvents const& clip{ clips[next(3U)] };
					ids.push_back(p.Play({ .Events = &clip, .Rate = 0.5f + static_cast<float>(next(30U)) * 0.1f,
						.Weight = static_cast<float>(next(11U)) * 0.1f, .Loop = next(2U) == 0U, .OwnsAction = next(2U) == 0U, .StopOthers = next(5U) == 0U }));
					break;
				}
				case 1:
					if (!ids.empty()) { p.Stop(ids[next(static_cast<std::uint32_t>(ids.size()))], next(2U) == 0U ? EndReason::Completed : EndReason::Interrupted); }
					break;
				case 2:
					if (!ids.empty()) { p.SetWeight(ids[next(static_cast<std::uint32_t>(ids.size()))], static_cast<float>(next(11U)) * 0.1f); }
					break;
				case 3:
					if (!ids.empty()) { p.SetRate(ids[next(static_cast<std::uint32_t>(ids.size()))], static_cast<float>(next(40U)) * 0.1f - 1.0f); }
					break;
				case 4:
					if (next(4U) == 0U) {
						std::size_t const k{ next(3U) };
						std::vector<AnimEvent> ev{};
						if (swapped[k]) { p.Rebind(&alts[k], &clips[k], ev); }
						else { p.Rebind(&clips[k], &alts[k], ev); }
						swapped[k] = !swapped[k];
						consume(ev);
					}
					break;
				default:
					break;
				}
				float const dt{ next(8U) == 0U ? 0.0f : 1.0f / static_cast<float>(10U + next(140U)) };
				std::vector<AnimEvent> ev{};
				p.Step(dt, ev);
				consume(ev);

				std::size_t n{ 0U };
				bool same{ true };
				p.ForEachOpenWindow([&](OpenWindowInfo const& info_) {
					++n;
					same = same && std::ranges::find(open, std::pair{ info_.Source, info_.WindowIndex }) != open.end();
					// 採用の方針（前編 4.6）：判定系は持ち主だけ、演出系はウェイトが閾値以上のときだけ開いている
					bool const eligible{
						info_.Window->Category == EventCategory::Gameplay ? info_.OwnsAction : info_.Weight >= info_.Window->MinWeight
					};
					if (!eligible) { ++ineligible; }
				});
				if (!same || n != open.size()) { ++mismatches; }
			}

			std::vector<AnimEvent> last{};
			p.StopAllNow(EndReason::Interrupted, last);
			consume(last);
			c.Check(violations == 0, std::format("Begin / End が交互にならなかった回数 {}", violations));
			c.Check(mismatches == 0, std::format("復元した集合と ForEachOpenWindow が食い違ったステップ数 {}", mismatches));
			c.Check(open.empty(), std::format("StopAllNow の後に {} 個の窓が開いたまま", open.size()));
			c.Check(ineligible == 0, std::format("採用の方針に反して開いていた窓がのべ {} 個", ineligible));
		}
	}

	auto RunAll() -> std::vector<Failure> {
		std::vector<Failure> out{};
		TimeForwardBoundaries(out);
		TimeLoopSplit(out);
		TimeLapCap(out);
		TimeZeroDt(out);
		TimeReverse(out);
		TimeOncePerLap(out);
		TimeOverlaps(out);
		TimeShortWindow(out);
		TimeWrap(out);
		TimeZeroLength(out);
		DataPayloadRoundTrip(out);
		BakeBasic(out);
		BakeStableOrder(out);
		BakeErrors(out);
		BakeTooManyWindows(out);
		BakeBadClip(out);
		BakeOutOfRange(out);
		BakeCurves(out);
		PlayerShortWindow(out);
		PlayerInterrupt(out);
		PlayerOwnerTransfer(out);
		PlayerNonOwner(out);
		PlayerWeight(out);
		PlayerPause(out);
		PlayerLoopEnd(out);
		PlayerFinishCloses(out);
		PlayerAdjacentWindows(out);
		PlayerOrdering(out);
		PlayerStartTime(out);
		PlayerPayload(out);
		CurveSample(out);
		CurveBlend(out);
		RebindBasic(out);
		RebindPendingAndShorter(out);
		PlayerRandomInvariant(out);
		return out;
	}
}
