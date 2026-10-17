module;

#include <limits>
#include <tuple>
#include <unordered_set>

module Lumina.AnimEvent;

//****	******	******	******	******	****//

import <algorithm>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <format>;
import <optional>;
import <span>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

//////	//////	//////	//////	//////	//////

namespace Lumina::AnimEvent {
	namespace {
		// 範囲外の許容量（フレームに対する割合）。
		// クリップ長はエンジン側の秒、イベントはフレーム番号なので、最終フレームに置いたイベントが
		// 丸めで長さをわずかに超えることがある。半フレーム以内なら黙ってクランプし、それを超えたら警告する。
		constexpr double OutOfRangeToleranceFrames{ 0.5 };

		class Baker {
		public:
			Baker(ClipEventSource const& src_, float clipLength_, std::vector<BakeIssue>& issues_)
				: Src_{ src_ }, Length_{ clipLength_ }, Issues_{ issues_ } {}

			auto Run() -> std::optional<ClipEvents> {
				// フレームレートとクリップ長が不正なら、以降の変換が意味をなさないので先に打ち切る
				if (!(Src_.FrameRate > 0.0f) || !std::isfinite(Src_.FrameRate)) {
					Error(std::format("frameRate が不正です（{}）", Src_.FrameRate));
					return std::nullopt;
				}
				if (!(Length_ >= 0.0f) || !std::isfinite(Length_)) {
					Error(std::format("クリップ長が不正です（{}）", Length_));
					return std::nullopt;
				}

				Out_.Length = Length_;
				BakeInstants();
				BakeWindows();
				BakeCurves();

				if (HasErrorSinceStart()) { return std::nullopt; }
				return std::move(Out_);
			}

		private:
			ClipEventSource const& Src_;
			float const Length_;
			std::vector<BakeIssue>& Issues_;
			std::size_t const IssuesAtStart_{ Issues_.size() };
			ClipEvents Out_{};

			//----	診断	------	------	------	----//

			void Error(std::string msg_) { Issues_.push_back({ BakeSeverity::Error, Prefix() + std::move(msg_) }); }
			void Warning(std::string msg_) { Issues_.push_back({ BakeSeverity::Warning, Prefix() + std::move(msg_) }); }
			auto Prefix() const -> std::string { return std::format("[{}] ", Src_.ClipName); }

			// 呼び出し前から issues_ に入っていたものは数えない（複数クリップを同じ配列に積んでもよいように）
			auto HasErrorSinceStart() const -> bool {
				return HasError(std::span{ Issues_ }.subspan(IssuesAtStart_));
			}

			//----	変換	------	------	------	----//

			// フレーム → 秒。割り算は double で行う（30fps の 1/30 などを float で積み重ねない）
			auto FrameToSec(double frame_) const -> double { return frame_ / static_cast<double>(Src_.FrameRate); }

			// [0, Length] に収める。許容量を超えてはみ出していたら警告する
			auto ClampToClip(double sec_, std::string_view what_) -> float {
				double const tol{ FrameToSec(OutOfRangeToleranceFrames) };
				double const len{ static_cast<double>(Length_) };
				if (sec_ < -tol || sec_ > len + tol) {
					Warning(std::format("{} がクリップの範囲外です（{:.4f} 秒 / 長さ {:.4f} 秒）。範囲内に収めます", what_, sec_, len));
				}
				return static_cast<float>(std::clamp(sec_, 0.0, len));
			}

			auto CheckType(std::string_view typeName_, std::string_view what_) -> EventTypeId {
				if (typeName_.empty()) {
					Error(std::format("{} の type が空です", what_));
					return InvalidTypeId;
				}
				EventTypeId const id{ TypeIdOf(typeName_) };
				if (id == InvalidTypeId) {
					Error(std::format("{} の type \"{}\" は型IDが 0（無効値）になります。名前を変えてください", what_, typeName_));
				}
				return id;
			}

			// ペイロードを詰め、(オフセット, サイズ) を返す。サイズ 0 はペイロードなし
			auto PackPayload(PayloadBytes const& p_, std::string_view what_) -> std::pair<std::uint32_t, std::uint32_t> {
				if (p_.Bytes.empty()) { return { 0U, 0U }; }

				std::size_t const align{ p_.Align };
				bool const pow2{ align != 0U && (align & (align - 1U)) == 0U };
				if (!pow2 || align > alignof(std::max_align_t)) {
					Error(std::format("{} のペイロードのアラインメントが不正です（{}）", what_, align));
					return { 0U, 0U };
				}

				std::size_t const offset{ (Out_.PayloadBlob.size() + align - 1U) & ~(align - 1U) };
				std::size_t const end{ offset + p_.Bytes.size() };
				if (end > std::numeric_limits<std::uint32_t>::max()) {
					Error(std::format("{} でペイロードの合計が 4GB を超えました", what_));
					return { 0U, 0U };
				}
				Out_.PayloadBlob.resize(end);
				std::ranges::copy(p_.Bytes, Out_.PayloadBlob.begin() + static_cast<std::ptrdiff_t>(offset));
				return { static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(p_.Bytes.size()) };
			}

			//----	瞬間イベント	------	------	----//

			void BakeInstants() {
				Out_.Instants.reserve(Src_.Instants.size());
				for (std::size_t i = 0; i < Src_.Instants.size(); ++i) {
					InstantSource const& s{ Src_.Instants[i] };
					std::string const what{ std::format("instants[{}] \"{}\"", i, s.TypeName) };

					InstantEvent e{};
					e.Type = CheckType(s.TypeName, what);
					e.Time = ClampToClip(FrameToSec(s.Frame), what);
					e.MinWeight = s.MinWeight;
					e.Category = s.Category;
					std::tie(e.PayloadOffset, e.PayloadSize) = PackPayload(s.Payload, what);
					Out_.Instants.push_back(e);
				}
				// 判定は区間との包含で行うので順序は必須ではないが、昇順にしておくと
				// 1 ステップ内の出力が時刻順にそろい、デバッグ表示も読みやすい
				std::ranges::stable_sort(Out_.Instants, {}, &InstantEvent::Time);
			}

			//----	区間イベント	------	------	----//

			void BakeWindows() {
				if (Src_.Windows.size() > ClipEvents::MaxWindows) {
					// 開いている窓の集合を 64bit マスクで持つため（前編 4.4）
					Error(std::format("窓が {} 個あります。1 クリップの上限は {} 個です", Src_.Windows.size(), ClipEvents::MaxWindows));
				}

				Out_.Windows.reserve(Src_.Windows.size());
				for (std::size_t i = 0; i < Src_.Windows.size(); ++i) {
					WindowSource const& s{ Src_.Windows[i] };
					std::string const what{ std::format("windows[{}] \"{}\"", i, s.TypeName) };

					// 長さ 0 の区間はエラー。瞬間イベントの書き間違いであることがほとんど（前編 4.2）
					if (s.EndFrame == s.BeginFrame) {
						Error(std::format("{} の長さが 0 です（begin = end = {}）。瞬間イベントなら instants に置いてください", what, s.BeginFrame));
						continue;
					}
					if (s.EndFrame < s.BeginFrame) {
						Error(std::format("{} の begin（{}）が end（{}）より後ろです", what, s.BeginFrame, s.EndFrame));
						continue;
					}

					WindowEvent e{};
					e.Type = CheckType(s.TypeName, what);
					e.Begin = ClampToClip(FrameToSec(s.BeginFrame), what + " の begin");
					e.End = ClampToClip(FrameToSec(s.EndFrame), what + " の end");
					if (!(e.Begin < e.End)) {
						Error(std::format("{} がクリップの範囲に収めると長さ 0 になります", what));
						continue;
					}
					e.MinWeight = s.MinWeight;
					e.Category = s.Category;
					std::tie(e.PayloadOffset, e.PayloadSize) = PackPayload(s.Payload, what);
					Out_.Windows.push_back(e);
				}
				// Begin 昇順。同じ Begin なら元の順（JSON に書いた順）を保つ。並べ替え後の添字が窓ID
				std::ranges::stable_sort(Out_.Windows, {}, &WindowEvent::Begin);
			}

			//----	カーブ	------	------	------	----//

			void BakeCurves() {
				std::unordered_set<CurveId> seen{};
				Out_.Curves.reserve(Src_.Curves.size());

				for (std::size_t i = 0; i < Src_.Curves.size(); ++i) {
					CurveSource const& s{ Src_.Curves[i] };
					std::string const what{ std::format("curves[{}] \"{}\"", i, s.Name) };

					CurveId const id{ CheckType(s.Name, what) };
					if (id == InvalidTypeId) { continue; }
					if (!seen.insert(id).second) {
						Error(std::format("{} と同じ名前のカーブが既にあります", what));
						continue;
					}
					if (s.Keys.empty()) {
						Warning(std::format("{} にキーがありません。読み飛ばします", what));
						continue;
					}

					// キーをフレーム順に並べる（同じフレームなら後に書いたものが勝つように、安定ソートしてから後ろを採る）
					std::vector<std::pair<float, float>> keys{ s.Keys };
					std::ranges::stable_sort(keys, {}, &std::pair<float, float>::first);

					// 等間隔に焼き直す。間隔 = 1 フレーム。Sample が [i, i + 1] を読むので最低 2 点
					CurveTrack track{};
					track.Id = id;
					track.KeyInterval = 1.0f / Src_.FrameRate;
					double const frames{ static_cast<double>(Length_) * static_cast<double>(Src_.FrameRate) };
					std::size_t const count{ std::max<std::size_t>(2U, static_cast<std::size_t>(std::ceil(frames)) + 1U) };
					track.Values.resize(count);
					for (std::size_t k = 0; k < count; ++k) {
						track.Values[k] = EvalKeys(keys, static_cast<float>(k));
					}
					Out_.Curves.push_back(std::move(track));
				}
			}

			// 区分線形。範囲外は端の値
			static auto EvalKeys(std::vector<std::pair<float, float>> const& keys_, float frame_) -> float {
				// 先頭と同じフレームに複数キーがあれば、後に書いたものを採る（下の upper_bound の経路に任せる）
				if (frame_ < keys_.front().first) { return keys_.front().second; }
				if (frame_ >= keys_.back().first) { return keys_.back().second; }
				// frame_ より大きい最初のキー。同じフレームのキーが並んでいれば、その最後のものの直後を指す
				auto const hi{ std::ranges::upper_bound(keys_, frame_, {}, &std::pair<float, float>::first) };
				auto const lo{ hi - 1 };
				float const span{ hi->first - lo->first };
				float const t{ span > 0.0f ? (frame_ - lo->first) / span : 1.0f };
				return std::lerp(lo->second, hi->second, t);
			}
		};
	}

	auto HasError(std::span<BakeIssue const> issues_) noexcept -> bool {
		return std::ranges::any_of(issues_, [](BakeIssue const& i_) { return i_.Severity == BakeSeverity::Error; });
	}

	auto Bake(ClipEventSource const& src_, float clipLength_, std::vector<BakeIssue>& issues_) -> std::optional<ClipEvents> {
		return Baker{ src_, clipLength_, issues_ }.Run();
	}
}
