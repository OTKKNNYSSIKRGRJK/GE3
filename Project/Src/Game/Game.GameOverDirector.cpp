module Game.GameOverDirector;

// ここで import しているヘッダユニットは、いずれも Game.GameOverDirector.ixx で import 済み
import <algorithm>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <map>;
import <memory>;
import <optional>;
import <string>;
import <utility>;
import <vector>;

import Lumina;
import Lumina.CG3D.Struct;
import Game.GameOverGrade;
import Game.GameOverCamera;
import Game.GameOverShatter;
import nlohmann.json;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace Game::GameOverDirector {
	namespace {
		auto Saturate(float x_) -> float { return std::clamp(x_, 0.0f, 1.0f); }

		using JSON = nlohmann::json;

		void Read(JSON const& obj_, char const* key_, float& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = itr->get<float>(); }
		}
		void Read(JSON const& obj_, char const* key_, std::string& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_string()) { out_ = itr->get<std::string>(); }
		}

		// スローモーションの速さ（Start からの実時間 real_ 秒のとき）。Hold の間は SlowScale、そのあと Recover かけて 1 へ（smoothstep）
		auto SlowFactor(Style const& style_, float real_) -> float {
			float const slow{ std::clamp(style_.SlowScale, 0.01f, 1.0f) };
			float const hold{ std::max(style_.SlowHold, 0.0f) };
			float const recover{ std::max(style_.SlowRecover, 0.0f) };
			if (slow >= 1.0f || hold + recover <= 0.0f) { return 1.0f; }
			if (real_ < hold) { return slow; }
			if (recover <= 0.0f || real_ >= hold + recover) { return 1.0f; }
			float const u{ (real_ - hold) / recover };
			return slow + (1.0f - slow) * (u * u * (3.0f - 2.0f * u));
		}

		// PNG の大きさをヘッダ（IHDR）から読む。シグネチャ 8 バイト → チャンク長 4 → "IHDR" 4 → 幅 4 → 高さ 4（ビッグエンディアン）
		auto ReadPngSize(char const* path_) -> std::optional<Lumina::Float2> {
			std::ifstream ifs{ path_, std::ios::binary };
			if (!ifs.is_open()) { return std::nullopt; }
			unsigned char h[24]{};
			if (!ifs.read(reinterpret_cast<char*>(h), 24)) { return std::nullopt; }

			constexpr unsigned char signature[8]{ 0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU, 0x1AU, 0x0AU };
			if (!std::equal(signature, signature + 8, h)) { return std::nullopt; }
			if (h[12] != 'I' || h[13] != 'H' || h[14] != 'D' || h[15] != 'R') { return std::nullopt; }

			auto const be32{
				[&h](int at_) -> uint32_t {
					return (static_cast<uint32_t>(h[at_]) << 24U) | (static_cast<uint32_t>(h[at_ + 1]) << 16U) |
						(static_cast<uint32_t>(h[at_ + 2]) << 8U) | static_cast<uint32_t>(h[at_ + 3]);
				}
			};
			uint32_t const width{ be32(16) };
			uint32_t const height{ be32(20) };
			if (width == 0U || height == 0U) { return std::nullopt; }
			return Lumina::Float2{ static_cast<float>(width), static_cast<float>(height) };
		}

		// プレイヤーの骨格の骨のうち、クリップが動かす（名前が一致する）数
		auto CountDriven(Lumina::CG3D::MyAnimation const& clip_, Lumina::CG3D::Skeleton const& skeleton_) -> uint32_t {
			uint32_t count{ 0U };
			for (auto const& joint : skeleton_.ARR_Joint) {
				if (clip_.Nodes.contains(joint.Name)) { ++count; }
			}
			return count;
		}

		// from_ の idFrom_ 以下と to_ の idTo_ 以下を、子の順に突き合わせる（子の数が 1 か所でも違えば false）
		auto MatchSubtree(
			Lumina::CG3D::Skeleton const& from_,
			uint32_t idFrom_,
			Lumina::CG3D::Skeleton const& to_,
			uint32_t idTo_,
			std::map<std::string, std::string>& table_
		) -> bool {
			table_.clear();
			std::vector<std::pair<uint32_t, uint32_t>> stack{ { idFrom_, idTo_ } };
			while (!stack.empty()) {
				auto const [idFrom, idTo]{ stack.back() };
				stack.pop_back();
				auto const& jointFrom{ from_.ARR_Joint[idFrom] };
				auto const& jointTo{ to_.ARR_Joint[idTo] };
				if (jointFrom.IDs_Child.size() != jointTo.IDs_Child.size()) {
					table_.clear();
					return false;
				}
				table_[jointFrom.Name] = jointTo.Name;
				for (std::size_t idx{ 0U }; idx < jointFrom.IDs_Child.size(); ++idx) {
					stack.emplace_back(jointFrom.IDs_Child[idx], jointTo.IDs_Child[idx]);
				}
			}
			return true;
		}

		// クリップの骨の名前を、to_ の骨格の名前に付け替える。from_ はクリップの骨格。
		// from_ では「クリップが動かす骨のうち、親が動かされていないもの」をルートにし、to_ では全ての骨を候補に、階層が一致する所を探す
		// （シーンのルート直下の並びに頼らない。Dying.gltf のように別の骨格が同居していても探せる）
		auto RetargetByHierarchy(
			Lumina::CG3D::MyAnimation& clip_,
			Lumina::CG3D::Skeleton const& from_,
			Lumina::CG3D::Skeleton const& to_
		) -> bool {
			std::optional<uint32_t> fromRoot{};
			for (auto const& joint : from_.ARR_Joint) {
				if (joint.IDs_Child.empty() || !clip_.Nodes.contains(joint.Name)) { continue; }
				if (joint.ID_Parent && clip_.Nodes.contains(from_.ARR_Joint[*joint.ID_Parent].Name)) { continue; }
				fromRoot = joint.ID;
				break;
			}
			if (!fromRoot) { return false; }

			std::map<std::string, std::string> table{};
			bool matched{ false };
			for (auto const& candidate : to_.ARR_Joint) {
				if (candidate.IDs_Child.empty()) { continue; }
				if (MatchSubtree(from_, *fromRoot, to_, candidate.ID, table)) { matched = true; break; }
			}
			if (!matched) { return false; }

			std::map<std::string, Lumina::CG3D::MyAnimation::Node> renamed{};
			for (auto& [name, node] : clip_.Nodes) {
				auto const itr{ table.find(name) };
				renamed.emplace(itr != table.end() ? itr->second : name, std::move(node));
			}
			clip_.Nodes = std::move(renamed);
			return true;
		}

#if defined(_DEBUG)
		// 全トラックを並べた概観（読み取り専用）。1 行 = 1 トラック：区間（Begin..End）と印（Marks）
		struct Row {
			char const* Label;
			float Begin;
			float End;
			ImU32 Colour;
			std::vector<float> Marks;
		};

		// クリック／ドラッグした時刻を返す（触っていなければ nullopt）
		auto OverviewBar(std::vector<Row> const& rows_, float range_, float now_, bool showNow_) -> std::optional<float> {
			constexpr float LabelWidth{ 66.0f };
			constexpr float RowHeight{ 18.0f };
			constexpr float RulerHeight{ 16.0f };

			ImVec2 const origin{ ImGui::GetCursorScreenPos() };
			float const totalWidth{ std::max(ImGui::GetContentRegionAvail().x - 4.0f, 240.0f) };
			float const barWidth{ totalWidth - LabelWidth };
			float const height{ RulerHeight + RowHeight * static_cast<float>(rows_.size()) };

			ImGui::InvisibleButton("##GameOverOverview", ImVec2{ totalWidth, height });
			bool const held{ ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) };
			ImVec2 const mouse{ ImGui::GetIO().MousePos };

			std::optional<float> picked{};
			if (held) {
				picked = std::clamp((mouse.x - origin.x - LabelWidth) / barWidth * range_, 0.0f, range_);
			}

			auto const xOf{ [&origin, barWidth, range_](float t_) -> float { return origin.x + LabelWidth + t_ / range_ * barWidth; } };
			ImDrawList* const drawList{ ImGui::GetWindowDrawList() };
			drawList->AddRectFilled(origin, ImVec2{ origin.x + totalWidth, origin.y + height }, 0xFF262626U);

			// 目盛り（0.5 秒ごと、1 秒ごとに数字）
			for (int tick{ 0 }; static_cast<float>(tick) * 0.5f <= range_; ++tick) {
				float const x{ xOf(static_cast<float>(tick) * 0.5f) };
				bool const whole{ tick % 2 == 0 };
				drawList->AddLine(ImVec2{ x, origin.y + RulerHeight - (whole ? 8.0f : 4.0f) }, ImVec2{ x, origin.y + RulerHeight }, 0xFF707070U);
				if (whole) {
					std::string const label{ std::to_string(tick / 2) };
					drawList->AddText(ImVec2{ x + 2.0f, origin.y }, 0xFFC8C8C8U, label.c_str());
				}
			}

			for (std::size_t idx{ 0U }; idx < rows_.size(); ++idx) {
				Row const& row{ rows_[idx] };
				float const top{ origin.y + RulerHeight + RowHeight * static_cast<float>(idx) };
				float const bottom{ top + RowHeight };
				if (idx % 2U == 0U) {
					drawList->AddRectFilled(ImVec2{ origin.x, top }, ImVec2{ origin.x + totalWidth, bottom }, 0xFF2C2C2CU);
				}
				drawList->AddText(ImVec2{ origin.x + 4.0f, top + 2.0f }, 0xFFC8C8C8U, row.Label);

				float const x0{ xOf(row.Begin) };
				float const x1{ std::max(xOf(row.End), x0 + 2.0f) };
				drawList->AddRectFilled(ImVec2{ x0, top + 3.0f }, ImVec2{ x1, bottom - 3.0f }, row.Colour);

				for (float const mark : row.Marks) {
					float const x{ xOf(mark) };
					float const centreY{ (top + bottom) * 0.5f };
					drawList->AddQuadFilled(
						ImVec2{ x, centreY - 5.0f }, ImVec2{ x + 5.0f, centreY },
						ImVec2{ x, centreY + 5.0f }, ImVec2{ x - 5.0f, centreY }, 0xFFFFFFFFU
					);
				}
			}

			if (showNow_) {
				float const x{ xOf(std::clamp(now_, 0.0f, range_)) };
				drawList->AddLine(ImVec2{ x, origin.y }, ImVec2{ x, origin.y + height }, 0xFF4040FFU, 2.0f);
			}
			return picked;
		}
#endif
	}

	//////	//////	//////	//////	//////	//////
	//	Initialize / Time						//
	//////	//////	//////	//////	//////	//////

	void Director::Initialize(
		std::vector<Lumina::CG3D::MyAnimation>& clips_,
		Lumina::CG3D::Skeleton const& clipSkeleton_,
		Lumina::CG3D::Skeleton const& playerSkeleton_,
		Game::GameOverGrade::Effect& grade_,
		Game::GameOverCamera::Rig& camera_,
		Game::GameOverShatter::Effect& shatter_
	) {
		Grade_ = &grade_;
		Camera_ = &camera_;
		Shatter_ = &shatter_;
		Load();

		// ラベル画像の大きさ（表示の大きさ = 画像の px × 倍率）。読めなければ既定の値のまま
		LabelImageSize_ = ReadPngSize(LabelImage).value_or(LabelImageSize_);
		PromptImageSize_ = ReadPngSize(PromptImage).value_or(PromptImageSize_);

		// 倒れるモーション。全クリップを持ち、骨の名前が違うもの（0 本）は階層の突き合わせで付け替えてみて、動かす骨の数を数える
		Clips_ = std::move(clips_);
		Driven_.assign(Clips_.size(), 0U);
		ClipIndex_ = 0U;
		if (Clips_.empty()) {
			ClipStatus_ = "No clips in the animation file.";
			return;
		}
		std::size_t renamedClips{ 0U };
		for (std::size_t idx{ 0U }; idx < Clips_.size(); ++idx) {
			uint32_t driven{ CountDriven(Clips_[idx], playerSkeleton_) };
			if (driven == 0U && RetargetByHierarchy(Clips_[idx], clipSkeleton_, playerSkeleton_)) {
				driven = CountDriven(Clips_[idx], playerSkeleton_);
				++renamedClips;
			}
			Driven_[idx] = driven;
		}

		// 選ぶ：ClipName と同じ名前で 1 本以上動かすもの。無ければ、動かす骨が一番多いもの（同数なら先のもの）
		std::optional<std::size_t> chosen{};
		for (std::size_t idx{ 0U }; idx < Clips_.size(); ++idx) {
			if (!Look.ClipName.empty() && Clips_[idx].Name == Look.ClipName && Driven_[idx] > 0U) { chosen = idx; break; }
		}
		bool const byName{ chosen.has_value() };
		if (!chosen) {
			std::size_t best{ 0U };
			for (std::size_t idx{ 1U }; idx < Clips_.size(); ++idx) {
				if (Driven_[idx] > Driven_[best]) { best = idx; }
			}
			chosen = best;
		}
		ClipIndex_ = *chosen;

		ClipStatus_ = "Using '" + Clips_[ClipIndex_].Name + "' (drives " + std::to_string(Driven_[ClipIndex_]) + " bones)";
		if (!byName && !Look.ClipName.empty()) { ClipStatus_ += "; '" + Look.ClipName + "' was not found or drives no bones"; }
		ClipStatus_ += ". " + std::to_string(Clips_.size()) + " clips, " + std::to_string(renamedClips) + " renamed by hierarchy.";
		if (Driven_[ClipIndex_] == 0U) {
			ClipStatus_ += " NO clip drives any bone of the player skeleton: the pose will not change.";
		}
	}

	void Director::Start() {
		Started_ = true;
		Time_ = 0.0f;
		RealTime_ = 0.0f;
		Rewinding_ = false;
		RewindElapsed_ = 0.0f;
		if (Grade_) { Grade_->Start(); }
		if (Camera_) { Camera_->Start(); }
		if (Shatter_) { Shatter_->Reset(); }		// 前の粒子を消す
	}

	void Director::Reset() {
		Started_ = false;
		Time_ = 0.0f;
		RealTime_ = 0.0f;
		Rewinding_ = false;
		RewindElapsed_ = 0.0f;
		if (Grade_) { Grade_->Reset(); }
		if (Camera_) { Camera_->Reset(); }
		if (Shatter_) { Shatter_->Reset(); }
	}

	void Director::Update(float deltaTime_) {
		// deltaTime_ は実時間。演出の時計は、スローの間だけ遅く進む（色抜けとカメラには、呼び出し側が同じ倍率を掛けた dt を渡す）
		if (!Started_) { return; }
		float const dt{ std::max(deltaTime_, 0.0f) };
		if (Rewinding_) {
			// 巻き戻し：時計を戻す。色抜けとカメラの時計も同じ秒に合わせる（TimeScale() が 0 なので、自分では進んでいない）
			RewindElapsed_ += dt;
			Time_ = RewindFromTime_ * (1.0f - RewindProgress());
			if (Grade_) { Grade_->SetTime(Time_); }
			if (Camera_) { Camera_->SetTime(Time_); }
			return;
		}
		Time_ += dt * TimeScale();
		RealTime_ += dt;
	}

	auto Director::Elapsed() const -> float {
		if (PreviewMode_ == Preview::Scrub) { return ScrubTime_; }
		return Started_ ? Time_ : 0.0f;
	}

	auto Director::TimeScale() const -> float {
		if (Rewinding_) { return 0.0f; }		// 巻き戻し中は、色抜けとカメラが自分では進まない（Update が時計を決める）
		if (!IsRunning()) { return 1.0f; }
		return SlowFactor(Look, RealTime_);
	}

	auto Director::SlowEndTime() const -> float {
		float const slow{ std::clamp(Look.SlowScale, 0.01f, 1.0f) };
		float const hold{ std::max(Look.SlowHold, 0.0f) };
		float const recover{ std::max(Look.SlowRecover, 0.0f) };
		if (slow >= 1.0f || hold + recover <= 0.0f) { return 0.0f; }
		// Hold の間は slow 倍、Recover の間は slow → 1 の smoothstep（平均 slow + (1 - slow) / 2）
		return slow * hold + recover * (slow + (1.0f - slow) * 0.5f);
	}

	auto Director::DyingTime() const -> float {
		auto const* const clip{ DyingClip() };
		if (!clip) { return 0.0f; }
		return std::clamp(
			Look.DyingStartOffset + Elapsed() * Look.DyingRate,
			0.0f,
			clip->DurationInSeconds
		);
	}

	auto Director::ResultAlpha() const -> float {
		if (ForceLabels || !IsActive()) { return 1.0f; }
		return Saturate((Elapsed() - Look.ResultDelay) / std::max(Look.ResultFadeDuration, 1.0e-4f)) * LabelRewindFade();
	}

	auto Director::PromptAlpha() const -> float {
		if (ForceLabels || !IsActive()) { return 1.0f; }
		return Saturate((Elapsed() - Look.PromptDelay) / std::max(Look.PromptFadeDuration, 1.0e-4f)) * LabelRewindFade();
	}

	auto Director::CanRestart() const -> bool {
		if (!Started_) { return true; }
		return Time_ >= Look.RestartDelay;
	}

	auto Director::BeginRewind() -> bool {
		if (!Started_ || PreviewMode_ != Preview::Off) { return false; }
		if (Look.RewindDuration <= 0.0f) { return false; }
		Rewinding_ = true;
		RewindElapsed_ = 0.0f;
		RewindFromTime_ = Time_;
		return true;
	}

	auto Director::RewindProgress() const -> float {
		if (!Rewinding_) { return 0.0f; }
		float const u{ Saturate(RewindElapsed_ / std::max(Look.RewindDuration, 1.0e-3f)) };
		return std::pow(u, std::max(Look.RewindPower, 0.1f));
	}

	// 巻き戻しが始まったら、ラベルはすぐ（RewindLabelFade 秒で）消える。時計の戻りに任せると、ゆっくり始まる分だけ残ってしまう。巻き戻し中でなければ 1
	auto Director::LabelRewindFade() const -> float {
		if (!Rewinding_) { return 1.0f; }
		return 1.0f - Saturate(RewindElapsed_ / std::max(Look.RewindLabelFade, 1.0e-3f));
	}

	// 回転ぼかしの振れ幅。巻き戻しの頭で SpinIn の割合かけて立ち上がり、終わりの SpinOut の割合で消える（smoothstep）
	auto Director::SpinRadians() const -> float {
		if (!Rewinding_ || Look.RewindDuration <= 0.0f) { return 0.0f; }
		float const u{ Saturate(RewindElapsed_ / Look.RewindDuration) };
		auto const smooth{ [](float x_) { float const t{ Saturate(x_) }; return t * t * (3.0f - 2.0f * t); } };
		float const amount{ smooth(u / std::max(Look.SpinIn, 1.0e-3f)) * smooth((1.0f - u) / std::max(Look.SpinOut, 1.0e-3f)) };
		return Look.SpinAngleDeg * (3.14159265f / 180.0f) * amount;
	}

	auto Director::IsRewindFinished() const -> bool {
		return Rewinding_ && RewindElapsed_ >= Look.RewindDuration;
	}

	auto Director::TotalTime() const -> float {
		float total{ std::max({ Look.ResultDelay + Look.ResultFadeDuration, Look.PromptDelay + Look.PromptFadeDuration, Look.RestartDelay }) };
		if (Grade_) { total = std::max(total, Grade_->TotalTime()); }
		if (Camera_) { total = std::max(total, Camera_->TotalTime()); }
		if (Shatter_) { total = std::max(total, Shatter_->TotalTime()); }
		if (auto const* const clip{ DyingClip() }) {
			float const rate{ std::max(Look.DyingRate, 1.0e-3f) };
			total = std::max(total, (clip->DurationInSeconds - Look.DyingStartOffset) / rate);
		}
		return total;
	}

	//////	//////	//////	//////	//////	//////
	//	Save / Load								//
	//////	//////	//////	//////	//////	//////

	auto Director::Load() -> bool {
		std::ifstream ifs{ ConfigPath };
		if (!ifs.is_open()) { Status_ = "No config file (using defaults)."; return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { Status_ = "Config is not valid JSON."; return false; }

		Style& style{ Look };
		Read(root, "ClipName", style.ClipName);
		Read(root, "DyingRate", style.DyingRate);
		Read(root, "DyingStartOffset", style.DyingStartOffset);
		Read(root, "ResultDelay", style.ResultDelay);
		Read(root, "ResultFadeDuration", style.ResultFadeDuration);
		Read(root, "RestartDelay", style.RestartDelay);
		Read(root, "PromptDelay", style.PromptDelay);
		Read(root, "PromptFadeDuration", style.PromptFadeDuration);
		Read(root, "GameOverY", style.GameOverY);
		Read(root, "GameOverScale", style.GameOverScale);
		Read(root, "PromptY", style.PromptY);
		Read(root, "PromptScale", style.PromptScale);
		Read(root, "SlowScale", style.SlowScale);
		Read(root, "SlowHold", style.SlowHold);
		Read(root, "SlowRecover", style.SlowRecover);
		Read(root, "RewindDuration", style.RewindDuration);
		Read(root, "RewindPower", style.RewindPower);
		Read(root, "RewindLabelFade", style.RewindLabelFade);
		Read(root, "SpinAngleDeg", style.SpinAngleDeg);
		Read(root, "SpinFocus", style.SpinFocus);
		Read(root, "SpinIn", style.SpinIn);
		Read(root, "SpinOut", style.SpinOut);
		Status_ = "Loaded.";
		return true;
	}

	auto Director::Save() -> bool {
		Style const& style{ Look };
		JSON root = JSON::object();
		root["ClipName"] = style.ClipName;
		root["DyingRate"] = style.DyingRate;
		root["DyingStartOffset"] = style.DyingStartOffset;
		root["ResultDelay"] = style.ResultDelay;
		root["ResultFadeDuration"] = style.ResultFadeDuration;
		root["RestartDelay"] = style.RestartDelay;
		root["PromptDelay"] = style.PromptDelay;
		root["PromptFadeDuration"] = style.PromptFadeDuration;
		root["GameOverY"] = style.GameOverY;
		root["GameOverScale"] = style.GameOverScale;
		root["PromptY"] = style.PromptY;
		root["PromptScale"] = style.PromptScale;
		root["SlowScale"] = style.SlowScale;
		root["SlowHold"] = style.SlowHold;
		root["SlowRecover"] = style.SlowRecover;
		root["RewindDuration"] = style.RewindDuration;
		root["RewindPower"] = style.RewindPower;
		root["RewindLabelFade"] = style.RewindLabelFade;
		root["SpinAngleDeg"] = style.SpinAngleDeg;
		root["SpinFocus"] = style.SpinFocus;
		root["SpinIn"] = style.SpinIn;
		root["SpinOut"] = style.SpinOut;

		std::ofstream ofs{ ConfigPath, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { Status_ = "Save failed (cannot open file)."; return false; }
		ofs << root.dump(2) << '\n';
		if (!ofs.good()) { Status_ = "Save failed (write error)."; return false; }
		Status_ = "Saved.";
		return true;
	}

	//////	//////	//////	//////	//////	//////
	//	ImGui									//
	//////	//////	//////	//////	//////	//////

	void Director::DrawEditor() {
#if defined(_DEBUG)
		if (!ImGui::Begin("GameOver Director")) {
			ImGui::End();
			return;
		}
		Style& style{ Look };
		float const total{ TotalTime() };

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { std::string const keep{ style.ClipName }; Look = Style{}; Look.ClipName = keep; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		//----	Preview							----//
		ImGui::SeparatorText("Preview (all tracks at once)");
		{
			int mode{ static_cast<int>(PreviewMode_) };
			ImGui::RadioButton("Game", &mode, static_cast<int>(Preview::Off));
			ImGui::SameLine();
			ImGui::RadioButton("Scrub", &mode, static_cast<int>(Preview::Scrub));
			PreviewMode_ = static_cast<Preview>(mode);
		}
		if (PreviewMode_ == Preview::Scrub) {
			ImGui::SliderFloat("Time (s)", &ScrubTime_, 0.0f, total + 0.5f);
		}
		if (ImGui::Button("Replay from start")) {
			PreviewMode_ = Preview::Off;
			Start();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			PreviewMode_ = Preview::Off;
			Reset();
		}

		//----	Overview						----//
		ImGui::SeparatorText("Timeline");
		{
			Game::GameOverGrade::Style const* const grade{ Grade_ ? &Grade_->Look : nullptr };
			Game::GameOverCamera::Style const* const camera{ Camera_ ? &Camera_->Look : nullptr };

			std::vector<Row> rows{};
			if (SlowEndTime() > 0.0f) {
				rows.push_back(Row{ "Slow", 0.0f, SlowEndTime(), 0xFF9060D0U, {} });
			}
			if (auto const* const clip{ DyingClip() }) {
				float const rate{ std::max(style.DyingRate, 1.0e-3f) };
				rows.push_back(Row{ "Dying", 0.0f, std::max((clip->DurationInSeconds - style.DyingStartOffset) / rate, 0.0f), 0xFF3C8CF0U, {} });
			}
			if (camera) {
				std::vector<float> marks{};
				for (auto const& key : camera->Keys) { marks.push_back(key.Time); }
				rows.push_back(Row{ "Camera", 0.0f, camera->Keys.back().Time, 0xFFB08040U, std::move(marks) });
			}
			if (grade) {
				rows.push_back(Row{ "Gradient", grade->GradientDelay, grade->GradientDelay + grade->GradientDuration, 0xFF909090U, {} });
				rows.push_back(Row{ "Vignette", grade->VignetteDelay, grade->VignetteDelay + grade->VignetteDuration, 0xFF606060U, {} });
				rows.push_back(Row{ "UI fade", grade->UiFadeDelay, grade->UiFadeDelay + grade->UiFadeDuration, 0xFF6060C0U, {} });
				rows.push_back(Row{ "Split", grade->SplitDelay, grade->SplitDelay + grade->SplitDuration, 0xFFB0B040U, {} });
				rows.push_back(Row{ "Blur", grade->BlurDelay, grade->BlurDelay + grade->BlurDuration, 0xFF80A0A0U, {} });
			}
			if (Shatter_) {
				rows.push_back(Row{ "Dissolve", Shatter_->Look.DissolveDelay, Shatter_->TotalTime(), 0xFFB060A0U, {} });
			}
			rows.push_back(Row{ "Result", style.ResultDelay, style.ResultDelay + style.ResultFadeDuration, 0xFF50A050U, {} });
			rows.push_back(Row{ "Prompt", style.PromptDelay, style.PromptDelay + style.PromptFadeDuration, 0xFF70B070U, {} });
			rows.push_back(Row{ "Restart", style.RestartDelay, style.RestartDelay, 0xFF50C0E0U, { style.RestartDelay } });

			float const range{ std::max(4.0f, std::ceil(total * 2.0f) * 0.5f + 0.5f) };
			std::optional<float> const picked{ OverviewBar(rows, range, Elapsed(), IsActive()) };
			if (picked.has_value()) {
				PreviewMode_ = Preview::Scrub;
				ScrubTime_ = *picked;
			}
			ImGui::TextDisabled("Click / drag: scrub all tracks  (the white diamonds are camera keys / the restart time)");
		}

		// スクラブ中は、色抜けとカメラの時計をこのウィンドウの時刻にそろえる。スクラブをやめたら元に戻す
		{
			bool const scrubbing{ PreviewMode_ == Preview::Scrub };
			if (scrubbing) {
				if (Grade_) { Grade_->ScrubTo(ScrubTime_); }
				if (Camera_) { Camera_->ScrubTo(ScrubTime_); }
			}
			else if (WasScrubbing_) {
				if (Grade_) { Grade_->ClearScrub(); }
				if (Camera_) { Camera_->ClearScrub(); }
			}
			WasScrubbing_ = scrubbing;
		}

		//----	Dying							----//
		ImGui::SeparatorText("Dying motion");
		ImGui::TextDisabled("%s", ClipStatus_.c_str());
		if (auto const* const clip{ DyingClip() }) {
			// クリップの切り替え（動かす骨の数つき。0 のクリップはポーズが変わらない）
			{
				std::string const current{ clip->Name + "  [drives " + std::to_string(Driven_[ClipIndex_]) + "]" };
				if (ImGui::BeginCombo("Clip", current.c_str())) {
					for (std::size_t idx{ 0U }; idx < Clips_.size(); ++idx) {
						bool const selected{ idx == ClipIndex_ };
						std::string const item{ Clips_[idx].Name + "  [drives " + std::to_string(Driven_[idx]) + "]" };
						if (ImGui::Selectable(item.c_str(), selected)) {
							ClipIndex_ = idx;
							style.ClipName = Clips_[idx].Name;
						}
						if (selected) { ImGui::SetItemDefaultFocus(); }
					}
					ImGui::EndCombo();
				}
			}
			if (Driven_[ClipIndex_] == 0U) {
				ImGui::TextColored(ImVec4{ 1.0f, 0.5f, 0.3f, 1.0f }, "This clip drives 0 bones of the player skeleton: the pose will not change.");
			}
			ImGui::TextDisabled("Clip length %.2f s. (Rough guide: staggers until ~1.7 s, collapses to the floor by ~3.4 s, then lies still.)", clip->DurationInSeconds);
			ImGui::SliderFloat("Playback rate (x)", &style.DyingRate, 0.1f, 4.0f);
			ImGui::SliderFloat("Start offset (clip s)", &style.DyingStartOffset, 0.0f, std::max(clip->DurationInSeconds - 0.1f, 0.1f));
			ImGui::TextDisabled("Clip time now: %.2f s", DyingTime());
		}

		//----	Result / restart				----//
		ImGui::SeparatorText("Slow motion (intro)");
		ImGui::SliderFloat("Speed (x)", &style.SlowScale, 0.05f, 1.0f);
		ImGui::SliderFloat("Hold (real s)", &style.SlowHold, 0.0f, 3.0f);
		ImGui::SliderFloat("Recover (real s)", &style.SlowRecover, 0.0f, 3.0f);
		{
			float const slowExtra{ SlowEndTime() > 0.0f ? std::max(style.SlowHold, 0.0f) + std::max(style.SlowRecover, 0.0f) - SlowEndTime() : 0.0f };
			ImGui::TextDisabled("Slows the death clock only (dying motion, camera, grade, dissolve timing); the boss and bullets are not slowed.");
			ImGui::TextDisabled("Every time in this window is on that clock, so the slow-down adds %.2f s of real time. Speed now: %.2f", slowExtra, TimeScale());
		}

		ImGui::SeparatorText("Labels / restart");
		ImGui::SliderFloat("Game Over delay (s)", &style.ResultDelay, 0.0f, 10.0f);
		ImGui::SliderFloat("Game Over fade (s)", &style.ResultFadeDuration, 0.0f, 5.0f);
		ImGui::SliderFloat("Restart allowed from (s)", &style.RestartDelay, 0.0f, 10.0f);
		ImGui::SliderFloat("Prompt delay (s)", &style.PromptDelay, 0.0f, 10.0f);
		ImGui::SliderFloat("Prompt fade (s)", &style.PromptFadeDuration, 0.0f, 5.0f);
		ImGui::SliderFloat("Game Over y (px)", &style.GameOverY, 0.0f, 720.0f);
		ImGui::SliderFloat("Game Over scale", &style.GameOverScale, 0.1f, 2.0f);
		ImGui::SliderFloat("Prompt y (px)", &style.PromptY, 0.0f, 720.0f);
		ImGui::SliderFloat("Prompt scale", &style.PromptScale, 0.1f, 2.0f);
		ImGui::TextDisabled("Labels: %s (%.0fx%.0f), %s (%.0fx%.0f)", LabelImage, LabelImageSize_.x, LabelImageSize_.y, PromptImage, PromptImageSize_.x, PromptImageSize_.y);
		ImGui::Checkbox("Force labels (debug: draw regardless of phase / time)", &ForceLabels);
		ImGui::TextDisabled("Elapsed %.2f s / Game Over alpha %.2f / Prompt alpha %.2f", Elapsed(), ResultAlpha(), PromptAlpha());

		ImGui::SeparatorText("Rewind (SPACE, before restart)");
		ImGui::SliderFloat("Rewind duration (s, real time)", &style.RewindDuration, 0.0f, 5.0f);
		ImGui::SliderFloat("Rewind power (1 = constant speed)", &style.RewindPower, 1.0f, 4.0f);
		ImGui::SliderFloat("Label fade-out on rewind (s)", &style.RewindLabelFade, 0.0f, 2.0f);
		ImGui::SliderFloat("Spin blur angle (deg, 0 = off)", &style.SpinAngleDeg, -90.0f, 90.0f);
		ImGui::SliderFloat("Spin blur centre (0 = screen, 1 = player)", &style.SpinFocus, 0.0f, 1.0f);
		ImGui::SliderFloat("Spin blur in (fraction)", &style.SpinIn, 0.0f, 1.0f);
		ImGui::SliderFloat("Spin blur out (fraction)", &style.SpinOut, 0.0f, 1.0f);
		ImGui::TextDisabled("Plays the death clock backwards (grade, camera, dying motion, dissolve, labels) while the player and boss go back to their start positions, then restarts. 0 s = restart at once.");
		if (ImGui::Button("Rewind now (then restarts)")) { BeginRewind(); }
		ImGui::SameLine();
		ImGui::TextDisabled("%s  progress %.2f", Rewinding_ ? "rewinding" : "idle", RewindProgress());

		ImGui::End();
#endif
	}
}
