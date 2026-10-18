module;

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

module Game.AnimEventDemo;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれも Game.AnimEventDemo.ixx で import 済み
import <algorithm>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <format>;
import <fstream>;
import <memory>;
import <optional>;
import <span>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

import nlohmann.json;

import Lumina.AnimEvent;
import Lumina.AnimEvent.Adapter;
import Lumina.CG3D.Struct;

//////	//////	//////	//////	//////	//////

namespace Game::AnimEventDemo {
	namespace AE = Lumina::AnimEvent;
	namespace Adapter = Lumina::AnimEvent::Adapter;

	namespace {
		constexpr std::size_t MaxLogLines{ 256U };
		constexpr std::size_t MaxInstanceClips{ 16U };
		constexpr float DefaultFrameRate{ 30.0f };

		auto EdgeName(AE::EventEdge edge_) -> std::string_view {
			switch (edge_) {
				case AE::EventEdge::Instant: return "Instant";
				case AE::EventEdge::Begin: return "Begin";
				case AE::EventEdge::End: return "End";
			}
			return "?";
		}

		auto ReasonName(AE::EndReason reason_) -> std::string_view {
			switch (reason_) {
				case AE::EndReason::None: return "";
				case AE::EndReason::Completed: return "Completed";
				case AE::EndReason::Interrupted: return "Interrupted";
				case AE::EndReason::Reloaded: return "Reloaded";
			}
			return "?";
		}
	}

	//====	======	======	======	======	====//
	//	初期化・破棄							//
	//====	======	======	======	======	====//

	void PlayerAnimEvents::Initialize(ClipMap const& clips_, std::span<ClipBinding const> bindings_, float rate_) {
		Rate_ = rate_;

		// 型の登録。サイドカーに書かれている型はすべて登録しておく必要がある（未登録の型はエラーで、そのクリップ全体が読めない）。
		// 今回使うのは Hitbox だけ。残りはサンプルのサイドカーに入っている型で、読み込めるように登録しているだけ
		Registry_.Register<HitboxPayload>(AE::EventCategory::Gameplay, 0.0f,
			[](Adapter::JSON const& e_, HitboxPayload& p_) {
				return
					Adapter::ReadNameId(e_, "slot", p_.Slot) &&
					Adapter::ReadNumber(e_, "damage", p_.Damage) &&
					Adapter::ReadNumber(e_, "knockback", p_.Knockback);
			});
		Registry_.RegisterTag("SuperArmor", AE::EventCategory::Gameplay, 0.0f);
		Registry_.RegisterTag("Swing", AE::EventCategory::Presentation, 0.5f);
		Registry_.RegisterTag("Trail", AE::EventCategory::Presentation, 0.5f);

		Slots_.clear();
		for (ClipBinding const& b : bindings_) {
			auto const itr{ clips_.find(b.Name) };
			if (itr == clips_.end()) { continue; }
			auto slot{ std::make_unique<ClipSlot>() };
			slot->Name = b.Name;
			slot->Clip = &itr->second;
			slot->SidecarPath = b.SidecarPath;
			LoadSidecar(*slot);
			Rebuild(*slot);
			Slots_.push_back(std::move(slot));
		}
		// バインドされていないクリップも、空のイベントで再生できるようにしておく（時刻を持つのはこのクラスなので）
		for (auto const& [name, clip] : clips_) {
			if (Find(name) != nullptr) { continue; }
			auto slot{ std::make_unique<ClipSlot>() };
			slot->Name = name;
			slot->Clip = &clip;
			Rebuild(*slot);
			Slots_.push_back(std::move(slot));
		}
		std::ranges::sort(Slots_, {}, [](auto const& s_) -> std::string const& { return s_->Name; });

		// 焼き込み中の Rebind は再生前なので何も出ないが、念のため捨てておく
		Events_.clear();
	}

	PlayerAnimEvents::~PlayerAnimEvents() {
		// AnimEventPlayer は開いている窓を残したまま破棄できない（受け手が先に消えていることがあるため、自動では閉じない）。
		// ここで閉じて、出てきた End は捨てる（シーンの終了時なので反映する先が無い）
		Events_.clear();
		Player_.StopAllNow(AE::EndReason::Interrupted, Events_);
		Events_.clear();
	}

	//====	======	======	======	======	====//
	//	毎フレーム								//
	//====	======	======	======	======	====//

	void PlayerAnimEvents::Update(std::string const& clipName_, bool loop_, float dt_) {
		Events_.clear();

		// 1. エディタでの編集を反映する（ステップの境目なので Rebind してよい）
		for (auto& slot : Slots_) {
			if (slot->NeedsRebuild) { Rebuild(*slot); }
		}

		// 2. 再生するクリップを決める
		if (Mode_ == Mode::Live) {
			if (Live_ == nullptr || Live_->Name != clipName_ || RestartLive_) {
				if (ClipSlot* const slot{ Find(clipName_) }) {
					Live_ = slot;
					LiveId_ = Play(*slot, 0.0f, Rate_, loop_);
				}
				RestartLive_ = false;
			}
		}
		else if (RestartPreview_ && PreviewIndex_ < Slots_.size()) {
			// スクラブ・コマ送り・再生／停止・クリップの切り替えは、指定した時刻から再生し直すことで行う。
			// 止めているときは速度 0 で再生する（窓は「現在時刻を含むか」で開いたまま保たれる）
			float const rate{ PreviewPlaying_ ? (PreviewFullRate_ ? Rate_ : 1.0f) : 0.0f };
			PreviewId_ = Play(*Slots_[PreviewIndex_], PreviewTime_, rate, PreviewLoop_);
			RestartPreview_ = false;
		}

		// 3. 進める
		Player_.Step(dt_, Events_);
		++StepCount_;
		if (Mode_ == Mode::Preview) {
			if (auto const t{ Player_.Time(PreviewId_) }) { PreviewTime_ = *t; }
		}

		// 4. イベントを反映する（副作用はエッジ、状態は IsOpen で問い合わせ）
		HandleEvents();
	}

	auto PlayerAnimEvents::Play(ClipSlot& slot_, float startTime_, float rate_, bool loop_) -> AE::InstanceId {
		AE::InstanceId const id{ Player_.Play({
			.Events = slot_.Events.get(),
			.Rate = rate_,
			.StartTime = startTime_,
			.Weight = 1.0f,
			.Loop = loop_,
			.OwnsAction = true,		// 1 本ずつ再生するので、毎回持ち主にする（持ち主でないと判定系の窓が開かない）
			.StopOthers = true,		// 前のクリップの窓は Interrupted で閉じる
		}) };
		InstanceClips_.emplace_back(id, &slot_);
		if (InstanceClips_.size() > MaxInstanceClips) { InstanceClips_.erase(InstanceClips_.begin()); }
		return id;
	}

	void PlayerAnimEvents::HandleEvents() {
		for (AE::AnimEvent const& e : Events_) {
			if (AE::Is<HitboxPayload>(e)) {
				if (e.Edge == AE::EventEdge::Begin) {
					// 窓が開くたびにヒット済みの相手を空にする（同じ窓IDでも、開き直したら別の 1 振り）
					OpenHitboxes_.push_back({ e.Source, e.WindowIndex, AE::PayloadAs<HitboxPayload>(e), {} });
				}
				else if (e.Edge == AE::EventEdge::End) {
					std::erase_if(OpenHitboxes_, [&](OpenHitbox const& h_) {	// std::erase_if は C++20
						return h_.Source == e.Source && h_.Window == e.WindowIndex;
					});
				}
			}

			if (LogEnabled_ && Mode_ == Mode::Live) {
				std::string_view const clip{ ClipNameOf(e.Source) };
				ClipSlot const* const slot{ SlotOf(e.Source) };
				float const fps{ slot ? FrameRateOf(*slot) : DefaultFrameRate };
				PushLog(std::format(
					"#{:<6} {:<5} {:<10} {:<7} {:<11} t={:.3f} f={:.1f}",
					StepCount_, clip, TypeNameOf(e.Type), EdgeName(e.Edge), ReasonName(e.Reason),
					e.ClipTime, e.ClipTime * fps));
			}
		}
	}

	//====	======	======	======	======	====//
	//	問い合わせ								//
	//====	======	======	======	======	====//

	auto PlayerAnimEvents::CurrentPose() const -> Pose {
		ClipSlot const* slot{ Live_ };
		AE::InstanceId id{ LiveId_ };
		if (Mode_ == Mode::Preview && PreviewIndex_ < Slots_.size()) {
			slot = Slots_[PreviewIndex_].get();
			id = PreviewId_;
		}
		if (slot == nullptr) { return {}; }
		return { slot->Clip, Player_.Time(id).value_or(0.0f) };
	}

	auto PlayerAnimEvents::IsHitboxOpen() const -> bool {
		return Player_.IsOpen<HitboxPayload>();
	}

	auto PlayerAnimEvents::TryHit(std::uint32_t targetId_) -> std::optional<HitboxPayload> {
		if (Mode_ != Mode::Live) { return std::nullopt; }
		for (OpenHitbox& h : OpenHitboxes_) {
			if (std::ranges::find(h.HitTargets, targetId_) != h.HitTargets.end()) { continue; }
			h.HitTargets.push_back(targetId_);
			if (LogEnabled_) {
				float const t{ Player_.Time(h.Source).value_or(0.0f) };
				ClipSlot const* const slot{ SlotOf(h.Source) };
				float const fps{ slot ? FrameRateOf(*slot) : DefaultFrameRate };
				PushLog(std::format(
					"#{:<6} {:<5} HIT target={} window={} damage={} t={:.3f} f={:.1f}",
					StepCount_, ClipNameOf(h.Source), targetId_, h.Window, h.Payload.Damage, t, t * fps));
			}
			return h.Payload;
		}
		return std::nullopt;
	}

	//====	======	======	======	======	====//
	//	サイドカー								//
	//====	======	======	======	======	====//

	void PlayerAnimEvents::LoadSidecar(ClipSlot& slot_) {
		slot_.Doc = JSON{};
		slot_.LoadIssues.clear();
		slot_.Dirty = false;
		slot_.NeedsRebuild = true;
		if (slot_.SidecarPath.empty()) { return; }

		std::ifstream ifs{ slot_.SidecarPath };
		if (!ifs.is_open()) { return; }		// ファイルが無い = サイドカーなし（エラーではない）

		JSON root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) {
			// 壊れたファイルは null のまま（エディタの Create で上書きしてしまわないよう、問題として出しておく）
			slot_.LoadIssues.push_back({ AE::BakeSeverity::Error, std::format("[{}] JSON として読めません: {}", slot_.Name, slot_.SidecarPath) });
			return;
		}
		slot_.Doc = std::move(root);
	}

	auto PlayerAnimEvents::SaveSidecar(ClipSlot& slot_) -> bool {
		if (slot_.Doc.is_null() || slot_.SidecarPath.empty()) { return false; }
		std::ofstream ofs{ slot_.SidecarPath, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { return false; }
		ofs << slot_.Doc.dump(2) << '\n';
		if (!ofs.good()) { return false; }
		slot_.Dirty = false;
		return true;
	}

	void PlayerAnimEvents::CreateSidecar(ClipSlot& slot_) {
		JSON doc = JSON::object();
		doc["version"] = Adapter::SidecarVersion;
		doc["clip"] = slot_.Name;
		if (slot_.Clip != nullptr) { doc["take"] = slot_.Clip->Name; }
		doc["frameRate"] = static_cast<int>(DefaultFrameRate);
		doc["instants"] = JSON::array();
		doc["windows"] = JSON::array();
		doc["curves"] = JSON::array();
		slot_.Doc = std::move(doc);
		slot_.Dirty = true;
		slot_.NeedsRebuild = true;
	}

	void PlayerAnimEvents::Rebuild(ClipSlot& slot_) {
		slot_.NeedsRebuild = false;
		std::vector<AE::BakeIssue> issues{ slot_.LoadIssues };
		float const length{ (slot_.Clip != nullptr) ? Adapter::ClipLength(*slot_.Clip) : 0.0f };

		std::optional<AE::ClipEvents> baked{};
		if (slot_.Doc.is_null()) {
			AE::ClipEvents empty{};
			empty.Length = length;
			baked = std::move(empty);
		}
		else {
			if (auto const src{ Adapter::ParseSidecar(slot_.Doc, slot_.Name, Registry_, issues) }) {
				baked = AE::Bake(*src, length, issues);
			}
			// 1 つの glTF に複数のテイクがあるので、サイドカーがどのテイクのものかを確かめる（Adapter::LoadClipEvents と同じ）
			if (auto const itr{ slot_.Doc.find("take") }; slot_.Clip != nullptr && itr != slot_.Doc.end() && itr->is_string()) {
				if (itr->get<std::string>() != slot_.Clip->Name) {
					issues.push_back({ AE::BakeSeverity::Warning, std::format(
						"[{}] サイドカーの take \"{}\" と、再生するクリップのテイク \"{}\" が違います",
						slot_.Name, itr->get<std::string>(), slot_.Clip->Name) });
				}
			}
		}

		slot_.Issues = std::move(issues);
		ReportIssues(slot_);
		if (!baked) { return; }		// 焼き込みに失敗したら、前のデータのまま再生を続ける

		auto next{ std::make_unique<AE::ClipEvents>(std::move(*baked)) };
		if (slot_.Events) {
			// 差し替え先は別のアドレスに作る（同じアドレスの中身を書き換えると Rebind が何もしない）
			Player_.Rebind(slot_.Events.get(), next.get(), Events_);
		}
		slot_.Events = std::move(next);
	}

	//====	======	======	======	======	====//
	//	小道具									//
	//====	======	======	======	======	====//

	auto PlayerAnimEvents::Find(std::string_view name_) -> ClipSlot* {
		for (auto& slot : Slots_) {
			if (slot->Name == name_) { return slot.get(); }
		}
		return nullptr;
	}

	auto PlayerAnimEvents::Shown() -> ClipSlot* {
		if (Mode_ == Mode::Preview) {
			return (PreviewIndex_ < Slots_.size()) ? Slots_[PreviewIndex_].get() : nullptr;
		}
		return Live_;
	}

	void PlayerAnimEvents::PushLog(std::string text_) {
		Log_.push_back(std::move(text_));
		if (Log_.size() > MaxLogLines) {
			Log_.erase(Log_.begin(), Log_.begin() + static_cast<std::ptrdiff_t>(Log_.size() - MaxLogLines));
		}
	}

	auto PlayerAnimEvents::SlotOf(AE::InstanceId id_) const -> ClipSlot const* {
		for (auto const& [id, slot] : InstanceClips_) {
			if (id == id_) { return slot; }
		}
		return nullptr;
	}

	auto PlayerAnimEvents::ClipNameOf(AE::InstanceId id_) const -> std::string_view {
		ClipSlot const* const slot{ SlotOf(id_) };
		return (slot != nullptr) ? std::string_view{ slot->Name } : std::string_view{ "?" };
	}

	auto PlayerAnimEvents::TypeNameOf(AE::EventTypeId type_) const -> std::string_view {
		auto const* const entry{ Registry_.Find(type_) };
		return (entry != nullptr) ? std::string_view{ entry->Name } : std::string_view{ "?" };
	}

	auto PlayerAnimEvents::FrameRateOf(ClipSlot const& slot_) -> float {
		if (slot_.Doc.is_object()) {
			if (auto const itr{ slot_.Doc.find("frameRate") }; itr != slot_.Doc.end() && itr->is_number()) {
				float const fps{ itr->get<float>() };
				if (std::isfinite(fps) && fps > 0.0f) { return fps; }
			}
		}
		return DefaultFrameRate;
	}

	// 焼き込みの問題を VS の出力ウィンドウに出す（ImGui の既定フォントは日本語を表示できないため）
	auto PlayerAnimEvents::ReportIssues(ClipSlot const& slot_) -> void {
#if defined(_WIN32)
		for (AE::BakeIssue const& issue : slot_.Issues) {
			std::string const line{ std::format("[AnimEventDemo] {}: {}\n",
				(issue.Severity == AE::BakeSeverity::Error) ? "Error" : "Warning", issue.Message) };
			int const len{ ::MultiByteToWideChar(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), nullptr, 0) };
			if (len <= 0) { continue; }
			std::wstring wide(static_cast<std::size_t>(len), L'\0');
			::MultiByteToWideChar(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), wide.data(), len);
			::OutputDebugStringW(wide.c_str());
		}
#else
		(void)slot_;
#endif
	}
}
