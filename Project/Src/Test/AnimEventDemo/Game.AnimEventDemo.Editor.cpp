module Game.AnimEventDemo;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれも Game.AnimEventDemo.ixx で import 済み
import <algorithm>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <format>;
import <memory>;
import <optional>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

import nlohmann.json;

import Lumina.AnimEvent;
import Lumina.AnimEvent.Adapter;
import Lumina.CG3D.Struct;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

//////	//////	//////	//////	//////	//////
// 調整用のエディタ（Debug のみ）
//
// - Live:    ゲームが再生しているクリップのタイムラインとイベントのログを見る。タイムラインをクリックするとプレビューに切り替わる
// - Preview: クリップを選んで、スクラブ・コマ送り・再生／停止。武器のコライダは窓に合わせて有効になる（Collision ウィンドウの
//            ワイヤーフレームで、どのコマで刃がどこにあるかを見られる）が、ダメージは入れない。ゲームの入力はそのまま動く
// - 窓・瞬間イベントのフレームを編集すると、次のフレームで焼き直して Rebind する（ファイルには Save を押すまで書かない）
//
// ImGui の既定フォントは日本語を表示できないので、画面の文言は英語。焼き込みの問題（日本語）は VS の出力ウィンドウにも出している

namespace Game::AnimEventDemo {
#if defined(_DEBUG)
	namespace {
		namespace AE = Lumina::AnimEvent;
		using JSON = Lumina::AnimEvent::Adapter::JSON;

		// ImU32 は 0xAABBGGRR
		constexpr ImU32 ColBackground{ 0xFF262626U };
		constexpr ImU32 ColRow{ 0xFF303030U };
		constexpr ImU32 ColTick{ 0xFF707070U };
		constexpr ImU32 ColText{ 0xFFC8C8C8U };
		constexpr ImU32 ColHitbox{ 0xFF4848E0U };
		constexpr ImU32 ColWindow{ 0xFFA87840U };
		constexpr ImU32 ColInstant{ 0xFF40D0F0U };
		constexpr ImU32 ColCursor{ 0xFFFFFFFFU };

		ImVec4 const ColError{ 1.0f, 0.4f, 0.4f, 1.0f };
		ImVec4 const ColWarning{ 1.0f, 0.85f, 0.3f, 1.0f };

		auto IntOr(JSON const& obj_, char const* key_, int fallback_) -> int {
			if (!obj_.is_object()) { return fallback_; }
			auto const itr{ obj_.find(key_) };
			return (itr != obj_.end() && itr->is_number_integer()) ? itr->get<int>() : fallback_;
		}

		auto FloatOr(JSON const& obj_, char const* key_, float fallback_) -> float {
			if (!obj_.is_object()) { return fallback_; }
			auto const itr{ obj_.find(key_) };
			return (itr != obj_.end() && itr->is_number()) ? itr->get<float>() : fallback_;
		}

		auto StringOr(JSON const& obj_, char const* key_, std::string fallback_) -> std::string {
			if (!obj_.is_object()) { return fallback_; }
			auto const itr{ obj_.find(key_) };
			return (itr != obj_.end() && itr->is_string()) ? itr->get<std::string>() : fallback_;
		}

		// 配列を返す。無い・配列でなければ nullptr
		auto ArrayOf(JSON& doc_, char const* key_) -> JSON* {
			if (!doc_.is_object()) { return nullptr; }
			auto const itr{ doc_.find(key_) };
			return (itr != doc_.end() && itr->is_array()) ? &*itr : nullptr;
		}

		// フレーム → 秒。焼き込み（Bake）と同じく double で割ってから float にする（窓の端ちょうどの判定がずれないように）
		auto FrameToTime(int frame_, float fps_) -> float {
			return static_cast<float>(static_cast<double>(frame_) / static_cast<double>(fps_));
		}

		auto TimeToFrame(float time_, float fps_) -> int {
			return static_cast<int>(std::floor(time_ * fps_ + 1.0e-3f));
		}

		auto LastFrameOf(float length_, float fps_) -> int {
			return std::max(0, static_cast<int>(std::lround(length_ * fps_)));
		}
	}

	void PlayerAnimEvents::DrawEditor() {
		ImGui::SetNextWindowSize(ImVec2{ 620.0f, 680.0f }, ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("AnimEvent Hitbox")) {
			ImGui::End();
			return;
		}

		ImGui::Checkbox("Use AnimEvent hitbox (off = legacy: whole attack, skip while boss knocked back)", &UseAnimEvent);

		// モードの切り替え。プレビューは今のライブの位置から、止めた状態で始める
		auto const indexOf{
			[this](ClipSlot const* slot_) -> std::size_t {
				for (std::size_t idx{ 0U }; idx < Slots_.size(); ++idx) {
					if (Slots_[idx].get() == slot_) { return idx; }
				}
				return 0U;
			}
		};
		if (ImGui::RadioButton("Live", Mode_ == Mode::Live) && Mode_ != Mode::Live) {
			Mode_ = Mode::Live;
			RestartLive_ = true;		// ゲームが今選んでいるクリップを最初から再生し直す
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Preview", Mode_ == Mode::Preview) && Mode_ != Mode::Preview) {
			PreviewIndex_ = indexOf(Live_);
			PreviewTime_ = CurrentPose().Time;
			PreviewPlaying_ = false;
			Mode_ = Mode::Preview;
			RestartPreview_ = true;
		}

		ClipSlot* const shown{ Shown() };
		if (shown == nullptr || shown->Clip == nullptr) {
			ImGui::TextUnformatted("No clip.");
			ImGui::End();
			return;
		}

		float const fps{ FrameRateOf(*shown) };
		float const length{ Lumina::AnimEvent::Adapter::ClipLength(*shown->Clip) };
		int const lastFrame{ LastFrameOf(length, fps) };
		float const time{ CurrentPose().Time };
		int const frame{ TimeToFrame(time, fps) };

		//----	プレビューの操作	------	------	----//
		if (Mode_ == Mode::Preview) {
			if (ImGui::BeginCombo("Clip", shown->Name.c_str())) {
				for (std::size_t idx{ 0U }; idx < Slots_.size(); ++idx) {
					if (ImGui::Selectable(Slots_[idx]->Name.c_str(), idx == PreviewIndex_)) {
						PreviewIndex_ = idx;
						PreviewTime_ = 0.0f;
						RestartPreview_ = true;
					}
				}
				ImGui::EndCombo();
			}

			auto const seek{
				[&, this](int frame_) {
					int const f{ std::clamp(frame_, 0, lastFrame) };
					PreviewTime_ = std::min(FrameToTime(f, fps), length);
					RestartPreview_ = true;
				}
			};
			if (ImGui::Button("|<")) { seek(0); }
			ImGui::SameLine();
			if (ImGui::Button("<")) { seek(frame - 1); }
			ImGui::SameLine();
			if (ImGui::Button(PreviewPlaying_ ? "Pause" : "Play ")) {
				PreviewPlaying_ = !PreviewPlaying_;
				if (PreviewPlaying_ && !PreviewLoop_ && time >= length) { PreviewTime_ = 0.0f; }	// 終端で止まっていたら頭から
				RestartPreview_ = true;
			}
			ImGui::SameLine();
			if (ImGui::Button(">")) { seek(frame + 1); }
			ImGui::SameLine();
			if (ImGui::Button(">|")) { seek(lastFrame); }
			ImGui::SameLine();
			if (ImGui::Checkbox("Loop", &PreviewLoop_)) { RestartPreview_ = true; }
			ImGui::SameLine();
			if (ImGui::Checkbox(std::format("x{:g} speed", Rate_).c_str(), &PreviewFullRate_)) { RestartPreview_ = true; }

			int sliderFrame{ frame };
			if (ImGui::SliderInt("Frame", &sliderFrame, 0, lastFrame)) { seek(sliderFrame); }
			ImGui::TextDisabled("Preview: no damage. Weapon collider follows the Hitbox windows (see the Collision window).");
		}

		//----	状態	------	------	------	----//
		ImGui::Text("Clip %s   take \"%s\"", shown->Name.c_str(), shown->Clip->Name.c_str());
		ImGui::Text(
			"time %.3f / %.3f s   frame %d / %d (%.0f fps)   Hitbox %s",
			time, length, frame, lastFrame, fps, IsHitboxOpen() ? "OPEN" : "-"
		);

		DrawTimeline(*shown, time);
		DrawSidecarEditor(*shown, std::clamp(frame, 0, lastFrame));

		//----	ログ	------	------	------	----//
		if (ImGui::CollapsingHeader("Event log (Live)", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Record", &LogEnabled_);
			ImGui::SameLine();
			if (ImGui::Button("Clear")) { Log_.clear(); }
			ImGui::BeginChild("##log", ImVec2{ 0.0f, 200.0f }, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
			for (std::string const& line : Log_) {
				ImGui::TextUnformatted(line.c_str());
			}
			if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) { ImGui::SetScrollHereY(1.0f); }	// 末尾を見ているときだけ追従
			ImGui::EndChild();
		}

		ImGui::End();
	}

	// タイムライン：上に目盛り、窓 1 つにつき 1 行、最後の行に瞬間イベント。白い縦線が現在の時刻。
	// クリック／ドラッグでその位置へスクラブする（ライブ中ならプレビューに切り替える）
	void PlayerAnimEvents::DrawTimeline(ClipSlot& slot_, float time_) {
		float const fps{ FrameRateOf(slot_) };
		float const length{ Lumina::AnimEvent::Adapter::ClipLength(*slot_.Clip) };
		if (length <= 0.0f) {
			ImGui::TextUnformatted("Clip length is 0.");
			return;
		}
		int const lastFrame{ LastFrameOf(length, fps) };

		JSON* const windows{ ArrayOf(slot_.Doc, "windows") };
		JSON* const instants{ ArrayOf(slot_.Doc, "instants") };
		std::size_t const numWindows{ windows ? windows->size() : 0U };

		constexpr float RulerHeight{ 16.0f };
		constexpr float RowHeight{ 18.0f };
		constexpr float InstantRowHeight{ 18.0f };

		ImVec2 const p0{ ImGui::GetCursorScreenPos() };
		float const width{ std::max(ImGui::GetContentRegionAvail().x, 64.0f) };
		float const height{ RulerHeight + RowHeight * static_cast<float>(std::max<std::size_t>(numWindows, 1U)) + InstantRowHeight };

		ImGui::InvisibleButton("##timeline", ImVec2{ width, height });
		bool const hovered{ ImGui::IsItemHovered() };
		bool const active{ ImGui::IsItemActive() };

		auto const xOfTime{ [&](float t_) { return p0.x + width * std::clamp(t_ / length, 0.0f, 1.0f); } };
		auto const xOfFrame{ [&](int f_) { return xOfTime(FrameToTime(f_, fps)); } };
		auto const frameAtMouse{
			[&]() {
				float const u{ std::clamp((ImGui::GetIO().MousePos.x - p0.x) / width, 0.0f, 1.0f) };
				return std::clamp(static_cast<int>(std::lround(u * length * fps)), 0, lastFrame);
			}
		};

		ImDrawList* const dl{ ImGui::GetWindowDrawList() };
		dl->AddRectFilled(p0, ImVec2{ p0.x + width, p0.y + height }, ColBackground);

		// 目盛り：1 フレームごと（間隔が狭ければ省く）、5 フレームごとに長く、10 フレームごとに番号
		float const pxPerFrame{ width / static_cast<float>(std::max(lastFrame, 1)) };
		for (int f{ 0 }; f <= lastFrame; ++f) {
			bool const major{ f % 10 == 0 };
			bool const mid{ f % 5 == 0 };
			if (!mid && pxPerFrame < 4.0f) { continue; }
			float const x{ xOfFrame(f) };
			float const tick{ major ? RulerHeight : (mid ? 8.0f : 4.0f) };
			dl->AddLine(ImVec2{ x, p0.y + RulerHeight - tick }, ImVec2{ x, p0.y + RulerHeight }, ColTick);
			if (major) {
				std::string const label{ std::to_string(f) };
				dl->AddText(ImVec2{ x + 2.0f, p0.y }, ColText, label.c_str());
			}
		}

		// 窓
		for (std::size_t idx{ 0U }; idx < numWindows; ++idx) {
			JSON const& w{ (*windows)[idx] };
			float const y{ p0.y + RulerHeight + RowHeight * static_cast<float>(idx) };
			dl->AddRectFilled(ImVec2{ p0.x, y + 1.0f }, ImVec2{ p0.x + width, y + RowHeight - 1.0f }, ColRow);
			std::string const type{ StringOr(w, "type", "?") };
			int const b{ IntOr(w, "begin", 0) };
			int const e{ IntOr(w, "end", 0) };
			if (e > b) {
				float const x0{ xOfFrame(b) };
				float const x1{ xOfFrame(e) };
				dl->AddRectFilled(ImVec2{ x0, y + 2.0f }, ImVec2{ x1, y + RowHeight - 2.0f }, (type == "Hitbox") ? ColHitbox : ColWindow);
			}
			std::string const label{ std::format("{} [{}, {})", type, b, e) };
			dl->AddText(ImVec2{ xOfFrame(std::max(b, 0)) + 3.0f, y + 2.0f }, ColText, label.c_str());
		}

		// 瞬間イベント
		{
			float const y{ p0.y + height - InstantRowHeight };
			if (instants != nullptr) {
				for (JSON const& i : *instants) {
					int const f{ IntOr(i, "frame", 0) };
					float const x{ xOfFrame(f) };
					dl->AddLine(ImVec2{ x, y + 2.0f }, ImVec2{ x, y + InstantRowHeight - 2.0f }, ColInstant, 2.0f);
					dl->AddText(ImVec2{ x + 3.0f, y + 2.0f }, ColInstant, StringOr(i, "type", "?").c_str());
				}
			}
		}

		// 現在の時刻
		{
			float const x{ xOfTime(time_) };
			dl->AddLine(ImVec2{ x, p0.y }, ImVec2{ x, p0.y + height }, ColCursor, 2.0f);
		}

		if (hovered) { ImGui::SetTooltip("frame %d", frameAtMouse()); }

		// スクラブ
		if (active) {
			std::size_t idx{ 0U };
			for (; idx < Slots_.size(); ++idx) {
				if (Slots_[idx].get() == &slot_) { break; }
			}
			if (Mode_ != Mode::Preview) {
				Mode_ = Mode::Preview;
				PreviewPlaying_ = false;
			}
			PreviewIndex_ = idx;
			PreviewTime_ = std::min(FrameToTime(frameAtMouse(), fps), length);
			RestartPreview_ = true;
		}
	}

	// サイドカーの編集。窓の begin / end と瞬間イベントの frame、Hitbox の damage / knockback を変えられる。
	// 型の追加は Hitbox だけ（他の型のキーは残したまま書き戻す）
	void PlayerAnimEvents::DrawSidecarEditor(ClipSlot& slot_, int currentFrame_) {
		ImGui::SeparatorText("Sidecar");
		ImGui::Text("%s%s", slot_.SidecarPath.empty() ? "(no path)" : slot_.SidecarPath.c_str(), slot_.Dirty ? "  * unsaved" : "");

		if (slot_.Doc.is_null()) {
			if (slot_.SidecarPath.empty()) {
				ImGui::TextDisabled("No sidecar path is bound to this clip (see InGame Initialize).");
				return;
			}
			if (!slot_.LoadIssues.empty()) {
				ImGui::TextColored(ColError, "The file exists but could not be parsed. Create will overwrite it on Save.");
			}
			else {
				ImGui::TextDisabled("No sidecar file.");
			}
			if (ImGui::Button("Create sidecar")) { CreateSidecar(slot_); }
			return;
		}

		float const fps{ FrameRateOf(slot_) };
		int const lastFrame{ LastFrameOf(Lumina::AnimEvent::Adapter::ClipLength(*slot_.Clip), fps) };
		bool changed{ false };

		//----	窓	------	------	------	------	----//
		if (JSON* const windows{ ArrayOf(slot_.Doc, "windows") }) {
			std::optional<std::size_t> removeAt{};
			if (ImGui::BeginTable("##windows", 5, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
				ImGui::TableSetupColumn("Window");
				ImGui::TableSetupColumn("Begin / End");
				ImGui::TableSetupColumn("Set from cursor");
				ImGui::TableSetupColumn("Payload");
				ImGui::TableSetupColumn("");
				ImGui::TableHeadersRow();

				for (std::size_t idx{ 0U }; idx < windows->size(); ++idx) {
					JSON& w{ (*windows)[idx] };
					if (!w.is_object()) { continue; }
					ImGui::PushID(static_cast<int>(idx));
					ImGui::TableNextRow();

					std::string const type{ StringOr(w, "type", "?") };
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(type.c_str());

					// [begin, end) のフレーム番号
					int range[2]{ IntOr(w, "begin", 0), IntOr(w, "end", 0) };
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(110.0f);
					if (ImGui::DragInt2("##range", range, 0.2f, 0, lastFrame)) {
						w["begin"] = range[0];
						w["end"] = range[1];
						changed = true;
					}

					// 現在のコマを最初／最後に含める
					ImGui::TableNextColumn();
					if (ImGui::SmallButton("Begin")) { w["begin"] = currentFrame_; changed = true; }
					ImGui::SameLine();
					if (ImGui::SmallButton("Last")) { w["end"] = currentFrame_ + 1; changed = true; }

					ImGui::TableNextColumn();
					if (type == "Hitbox") {
						float damage{ FloatOr(w, "damage", 0.0f) };
						float knockback{ FloatOr(w, "knockback", 0.0f) };
						ImGui::SetNextItemWidth(70.0f);
						if (ImGui::DragFloat("dmg", &damage, 1.0f, 0.0f, 10000.0f, "%.0f")) { w["damage"] = damage; changed = true; }
						ImGui::SameLine();
						ImGui::SetNextItemWidth(60.0f);
						if (ImGui::DragFloat("kb", &knockback, 0.1f, 0.0f, 100.0f, "%.1f")) { w["knockback"] = knockback; changed = true; }
					}

					ImGui::TableNextColumn();
					if (ImGui::SmallButton("x")) { removeAt = idx; }

					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			if (removeAt) {
				windows->erase(*removeAt);
				changed = true;
			}
			if (ImGui::Button("+ Hitbox at cursor")) {
				JSON w = JSON::object();
				w["type"] = std::string{ "Hitbox" };
				w["begin"] = currentFrame_;
				w["end"] = std::min(currentFrame_ + 4, std::max(lastFrame, currentFrame_ + 1));
				w["slot"] = std::string{ "RightBlade" };
				w["damage"] = 100.0f;
				w["knockback"] = 5.0f;
				windows->push_back(std::move(w));
				changed = true;
			}
			ImGui::TextDisabled("Begin = first frame inside, Last = last frame inside (end is exclusive).");
		}
		else {
			ImGui::TextColored(ColError, "\"windows\" is missing or not an array.");
		}

		//----	瞬間イベント	------	------	------	----//
		if (JSON* const instants{ ArrayOf(slot_.Doc, "instants") }; instants != nullptr && !instants->empty()) {
			std::optional<std::size_t> removeAt{};
			if (ImGui::TreeNode("Instants")) {
				for (std::size_t idx{ 0U }; idx < instants->size(); ++idx) {
					JSON& i{ (*instants)[idx] };
					if (!i.is_object()) { continue; }
					ImGui::PushID(static_cast<int>(idx));
					int f{ IntOr(i, "frame", 0) };
					ImGui::SetNextItemWidth(60.0f);
					if (ImGui::DragInt(StringOr(i, "type", "?").c_str(), &f, 0.2f, 0, lastFrame)) { i["frame"] = f; changed = true; }
					ImGui::SameLine();
					if (ImGui::SmallButton("At cursor")) { i["frame"] = currentFrame_; changed = true; }
					ImGui::SameLine();
					if (ImGui::SmallButton("x")) { removeAt = idx; }
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			if (removeAt) {
				instants->erase(*removeAt);
				changed = true;
			}
		}

		//----	問題	------	------	------	------	----//
		if (!slot_.Issues.empty()) {
			ImGui::TextDisabled("Issues (full text in the VS Output window):");
			for (AE::BakeIssue const& issue : slot_.Issues) {
				ImGui::TextColored((issue.Severity == AE::BakeSeverity::Error) ? ColError : ColWarning, "%s", issue.Message.c_str());
			}
		}

		//----	保存	------	------	------	------	----//
		if (ImGui::Button(slot_.Dirty ? "Save *" : "Save")) {
			if (!SaveSidecar(slot_)) { PushLog(std::format("Save failed: {}", slot_.SidecarPath)); }
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload from file")) { LoadSidecar(slot_); }

		if (changed) {
			slot_.Dirty = true;
			slot_.NeedsRebuild = true;		// 次の Update の頭で焼き直して Rebind する
		}
	}
#else
	void PlayerAnimEvents::DrawEditor() {}
#endif
}
