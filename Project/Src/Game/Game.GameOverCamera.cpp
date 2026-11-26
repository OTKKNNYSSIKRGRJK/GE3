module Game.GameOverCamera;

// ここで import しているヘッダユニットは、いずれも Game.GameOverCamera.ixx で import 済み
import <algorithm>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <string>;
import <vector>;

import Lumina;
import Game.PlayerCamera;
import nlohmann.json;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace Game::GameOverCamera {
	namespace {
		constexpr float Pi{ 3.14159265358979f };

		auto ToRad(float deg_) -> float { return deg_ * (Pi / 180.0f); }
		auto ToDeg(float rad_) -> float { return rad_ * (180.0f / Pi); }
		auto Saturate(float x_) -> float { return std::clamp(x_, 0.0f, 1.0f); }

		constexpr char const* EaseNames[static_cast<int>(Ease::Count)]{
			"Hold",
			"Linear",
			"InQuad",
			"OutQuad",
			"InOutQuad",
			"InCubic",
			"OutCubic",
			"InOutCubic",
			"InOutSine",
			"OutQuart",
			"SmootherStep",
		};

		auto ApplyEase(Ease ease_, float u_) -> float {
			float const u{ Saturate(u_) };
			float const v{ 1.0f - u };
			switch (ease_) {
				case Ease::Hold:
					return (u_ >= 1.0f) ? 1.0f : 0.0f;
				case Ease::Linear:
					return u;
				case Ease::InQuad:
					return u * u;
				case Ease::OutQuad:
					return 1.0f - v * v;
				case Ease::InOutQuad:
					return (u < 0.5f) ? (2.0f * u * u) : (1.0f - 2.0f * v * v);
				case Ease::InCubic:
					return u * u * u;
				case Ease::OutCubic:
					return 1.0f - v * v * v;
				case Ease::InOutCubic:
					return (u < 0.5f) ? (4.0f * u * u * u) : (1.0f - 4.0f * v * v * v);
				case Ease::InOutSine:
					return -(std::cos(Pi * u) - 1.0f) * 0.5f;
				case Ease::OutQuart:
					return 1.0f - v * v * v * v;
				case Ease::SmootherStep:
					return u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
				default:
					return u;
			}
		}

		//----	Pose							----//

		auto PoseOfKey(Key const& key_) -> Pose {
			return Pose{ key_.Distance, key_.TargetHeight, key_.PitchDeg, key_.YawDeg, key_.RollDeg, key_.FovDeg, key_.Focus };
		}

		auto Mix(Pose const& a_, Pose const& b_, float w_) -> Pose {
			auto const mix{ [w_](float x_, float y_) -> float { return x_ + (y_ - x_) * w_; } };
			return Pose{
				mix(a_.Distance, b_.Distance),
				mix(a_.TargetHeight, b_.TargetHeight),
				mix(a_.PitchDeg, b_.PitchDeg),
				mix(a_.YawDeg, b_.YawDeg),
				mix(a_.RollDeg, b_.RollDeg),
				mix(a_.FovDeg, b_.FovDeg),
				mix(a_.Focus, b_.Focus),
			};
		}

		// 時刻 t_ の姿勢。先頭のキーは start_（取り込んだ実カメラ）で置き換える
		auto EvaluateKeys(std::vector<Key> const& keys_, Pose const& start_, float t_) -> Pose {
			if (keys_.empty()) { return start_; }
			auto const poseAt{
				[&keys_, &start_](std::size_t idx_) -> Pose { return (idx_ == 0U) ? start_ : PoseOfKey(keys_[idx_]); }
			};
			for (std::size_t idx{ 0U }; idx + 1U < keys_.size(); ++idx) {
				float const t0{ keys_[idx].Time };
				float const t1{ keys_[idx + 1U].Time };
				if (t_ < t1) {
					float const u{ (t1 - t0 > 1.0e-5f) ? Saturate((t_ - t0) / (t1 - t0)) : 1.0f };
					return Mix(poseAt(idx), poseAt(idx + 1U), ApplyEase(keys_[idx].EaseToNext, u));
				}
			}
			return poseAt(keys_.size() - 1U);
		}

		//----	View							----//

		struct V3 {
			float x;
			float y;
			float z;
		};
		auto Sub(V3 const& a_, V3 const& b_) -> V3 { return V3{ a_.x - b_.x, a_.y - b_.y, a_.z - b_.z }; }
		auto Dot(V3 const& a_, V3 const& b_) -> float { return a_.x * b_.x + a_.y * b_.y + a_.z * b_.z; }
		auto Cross(V3 const& a_, V3 const& b_) -> V3 {
			return V3{ a_.y * b_.z - a_.z * b_.y, a_.z * b_.x - a_.x * b_.z, a_.x * b_.y - a_.y * b_.x };
		}
		auto Normalize(V3 const& v_) -> V3 {
			float const length{ std::sqrt(Dot(v_, v_)) };
			if (length < 1.0e-6f) { return V3{ 0.0f, 0.0f, 1.0f }; }
			return V3{ v_.x / length, v_.y / length, v_.z / length };
		}

		// eye_ から target_ を見る View 行列（InGame.Update.cpp の LookAt と同じ並び）。up は前方向まわりに roll_ だけ回す
		auto LookAtRoll(V3 const& eye_, V3 const& target_, float roll_) -> Lumina::Mat4 {
			V3 const forward{ Normalize(Sub(target_, eye_)) };
			V3 const worldUp{ 0.0f, 1.0f, 0.0f };

			// worldUp を forward まわりに回す（ロドリゲスの回転公式）
			float const cosR{ std::cos(roll_) };
			float const sinR{ std::sin(roll_) };
			V3 const kxv{ Cross(forward, worldUp) };
			float const kv{ Dot(forward, worldUp) };
			V3 const rolledUp{
				worldUp.x * cosR + kxv.x * sinR + forward.x * kv * (1.0f - cosR),
				worldUp.y * cosR + kxv.y * sinR + forward.y * kv * (1.0f - cosR),
				worldUp.z * cosR + kxv.z * sinR + forward.z * kv * (1.0f - cosR),
			};

			V3 const right{ Normalize(Cross(rolledUp, forward)) };
			V3 const up{ Cross(forward, right) };

			return {
				right.x,
				up.x,
				forward.x,
				0.0f,

				right.y,
				up.y,
				forward.y,
				0.0f,

				right.z,
				up.z,
				forward.z,
				0.0f,

				-Dot(eye_, right),
				-Dot(eye_, up),
				-Dot(eye_, forward),
				1.0f,
			};
		}

		//----	JSON							----//

		using JSON = nlohmann::json;

		void Read(JSON const& obj_, char const* key_, float& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = itr->get<float>(); }
		}

		auto ParseEase(JSON const& obj_, char const* key_, Ease fallback_) -> Ease {
			auto const itr{ obj_.find(key_) };
			if (itr == obj_.end() || !itr->is_string()) { return fallback_; }
			std::string const name{ itr->get<std::string>() };
			for (int idx{ 0 }; idx < static_cast<int>(Ease::Count); ++idx) {
				if (name == EaseNames[idx]) { return static_cast<Ease>(idx); }
			}
			return fallback_;
		}

#if defined(_DEBUG)
		//////	//////	//////	//////	//////	//////
		//	Editor helpers							//
		//////	//////	//////	//////	//////	//////

		// t_ の位置に、その時刻の姿勢でキーを足す。足したキーの添字を返す（満杯なら -1）
		auto InsertKeyAt(Style& style_, Pose const& start_, float time_) -> int {
			auto& keys{ style_.Keys };
			if (keys.size() >= MaxKeys) { return -1; }
			std::size_t at{ 1U };
			while (at < keys.size() && keys[at].Time <= time_) { ++at; }
			Pose const pose{ EvaluateKeys(keys, start_, time_) };
			Key const added{
				time_, pose.Distance, pose.TargetHeight, pose.PitchDeg, pose.YawDeg, pose.RollDeg, pose.FovDeg,
				keys[at - 1U].EaseToNext, pose.Focus
			};
			keys.insert(keys.begin() + static_cast<std::vector<Key>::difference_type>(at), added);
			return static_cast<int>(at);
		}

		void RemoveKey(Style& style_, int idx_) {
			auto& keys{ style_.Keys };
			if (idx_ <= 0 || keys.size() <= MinKeys) { return; }
			keys.erase(keys.begin() + static_cast<std::vector<Key>::difference_type>(idx_));
		}

		auto EaseCombo(char const* label_, Ease& ease_) -> bool {
			bool changed{ false };
			if (ImGui::BeginCombo(label_, EaseNames[static_cast<int>(ease_)])) {
				for (int idx{ 0 }; idx < static_cast<int>(Ease::Count); ++idx) {
					bool const isSelected{ idx == static_cast<int>(ease_) };
					if (ImGui::Selectable(EaseNames[idx], isSelected)) {
						ease_ = static_cast<Ease>(idx);
						changed = true;
					}
					if (isSelected) { ImGui::SetItemDefaultFocus(); }
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		// タイムライン：キー（ひし形）と、区間ごとのイージング曲線。現在の時刻は赤線
		void TimelineBar(
			Style& style_,
			Pose const& start_,
			float range_,
			float now_,
			bool showNow_,
			int& selected_,
			int& dragging_
		) {
			constexpr float Height{ 76.0f };
			constexpr float RulerHeight{ 16.0f };
			constexpr float KeyRadius{ 6.0f };
			constexpr float GrabRadius{ 9.0f };

			ImVec2 const origin{ ImGui::GetCursorScreenPos() };
			float const width{ std::max(ImGui::GetContentRegionAvail().x - 4.0f, 120.0f) };
			ImGui::InvisibleButton("##CameraTimeline", ImVec2{ width, Height });
			bool const hovered{ ImGui::IsItemHovered() };
			bool const held{ ImGui::IsItemActive() };
			ImVec2 const mouse{ ImGui::GetIO().MousePos };

			auto const xOf{ [&origin, width, range_](float t_) -> float { return origin.x + t_ / range_ * width; } };
			float const mouseT{ std::clamp((mouse.x - origin.x) / width * range_, 0.0f, range_) };
			auto& keys{ style_.Keys };

			// マウスに一番近い（GrabRadius 以内の）キー
			int nearest{ -1 };
			float nearestDist{ GrabRadius };
			for (std::size_t idx{ 0U }; idx < keys.size(); ++idx) {
				float const dist{ std::abs(xOf(keys[idx].Time) - mouse.x) };
				if (dist <= nearestDist) {
					nearest = static_cast<int>(idx);
					nearestDist = dist;
				}
			}

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				if (nearest >= 0) {
					selected_ = nearest;
					dragging_ = (nearest > 0) ? nearest : -1;		// 先頭は動かせない
				}
				else {
					int const added{ InsertKeyAt(style_, start_, std::max(mouseT, 0.05f)) };
					if (added >= 0) {
						selected_ = added;
						dragging_ = added;
					}
				}
			}
			else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && nearest > 0 && keys.size() > MinKeys) {
				RemoveKey(style_, nearest);
				selected_ = std::clamp(selected_ - ((selected_ >= nearest) ? 1 : 0), 0, static_cast<int>(keys.size()) - 1);
				dragging_ = -1;
			}
			if (!held || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) { dragging_ = -1; }
			if (dragging_ > 0 && static_cast<std::size_t>(dragging_) < keys.size()) {
				std::size_t const dragged{ static_cast<std::size_t>(dragging_) };
				float const lo{ keys[dragged - 1U].Time + 0.02f };
				float const hi{ (dragged + 1U < keys.size()) ? (keys[dragged + 1U].Time - 0.02f) : range_ };
				keys[dragged].Time = std::clamp(mouseT, lo, std::max(lo, hi));
			}

			//----	Draw						----//
			ImDrawList* const drawList{ ImGui::GetWindowDrawList() };
			float const bottom{ origin.y + Height };
			float const curveTop{ origin.y + RulerHeight + 4.0f };
			float const curveBottom{ bottom - 22.0f };
			float const keyY{ bottom - 11.0f };

			drawList->AddRectFilled(origin, ImVec2{ origin.x + width, bottom }, 0xFF262626U);

			// 区間のイージング曲線
			for (std::size_t idx{ 0U }; idx + 1U < keys.size(); ++idx) {
				float const x0{ xOf(keys[idx].Time) };
				float const x1{ xOf(keys[idx + 1U].Time) };
				drawList->AddRectFilled(
					ImVec2{ x0, curveTop }, ImVec2{ x1, curveBottom },
					(idx % 2U == 0U) ? 0xFF303030U : 0xFF383838U
				);
				std::vector<ImVec2> curve{};
				constexpr int Samples{ 24 };
				curve.reserve(Samples + 1);
				for (int s{ 0 }; s <= Samples; ++s) {
					float const u{ static_cast<float>(s) / static_cast<float>(Samples) };
					curve.push_back(ImVec2{
						x0 + (x1 - x0) * u,
						curveBottom - (curveBottom - curveTop) * ApplyEase(keys[idx].EaseToNext, (s == Samples) ? 0.9999f : u)
					});
				}
				drawList->AddPolyline(curve.data(), static_cast<int>(curve.size()), 0xFF9A9A9AU, ImDrawFlags_None, 1.5f);
				if (x1 - x0 > 70.0f) {
					drawList->AddText(ImVec2{ x0 + 3.0f, curveTop + 1.0f }, 0xFF8A8A8AU, EaseNames[static_cast<int>(keys[idx].EaseToNext)]);
				}
			}

			// 目盛り（0.5 秒ごと、1 秒ごとに数字）
			for (int tick{ 0 }; static_cast<float>(tick) * 0.5f <= range_; ++tick) {
				float const t{ static_cast<float>(tick) * 0.5f };
				float const x{ xOf(t) };
				bool const whole{ tick % 2 == 0 };
				drawList->AddLine(ImVec2{ x, origin.y + RulerHeight - (whole ? 8.0f : 4.0f) }, ImVec2{ x, origin.y + RulerHeight }, 0xFF707070U);
				if (whole) {
					std::string const label{ std::to_string(tick / 2) };
					drawList->AddText(ImVec2{ x + 2.0f, origin.y }, 0xFFC8C8C8U, label.c_str());
				}
			}

			// キー
			for (std::size_t idx{ 0U }; idx < keys.size(); ++idx) {
				float const x{ xOf(keys[idx].Time) };
				bool const isSelected{ static_cast<int>(idx) == selected_ };
				ImU32 const fill{ (idx == 0U) ? 0xFF50B0F0U : 0xFFE0C060U };
				drawList->AddQuadFilled(
					ImVec2{ x, keyY - KeyRadius }, ImVec2{ x + KeyRadius, keyY },
					ImVec2{ x, keyY + KeyRadius }, ImVec2{ x - KeyRadius, keyY }, fill
				);
				drawList->AddQuad(
					ImVec2{ x, keyY - KeyRadius }, ImVec2{ x + KeyRadius, keyY },
					ImVec2{ x, keyY + KeyRadius }, ImVec2{ x - KeyRadius, keyY },
					isSelected ? 0xFFFFFFFFU : 0xFF606060U, isSelected ? 2.0f : 1.0f
				);
			}

			if (showNow_) {
				float const x{ xOf(std::clamp(now_, 0.0f, range_)) };
				drawList->AddLine(ImVec2{ x, origin.y }, ImVec2{ x, bottom }, 0xFF4040FFU, 2.0f);
			}
		}

		// 真上（左）と真横（右）から見たカメラの軌道。プレイヤーはオレンジ。開始時のカメラは真上図で下側
		void DrawPlots(Style const& style_, Pose const& start_, float now_, bool showNow_, int selected_) {
			constexpr float Size{ 170.0f };
			constexpr float Gap{ 12.0f };
			constexpr float PlayerHeight{ 5.7f };		// メッシュのおおよその背丈
			constexpr int Samples{ 64 };

			auto const& keys{ style_.Keys };
			float const total{ std::max(keys.back().Time, 0.1f) };

			std::vector<Pose> poses{};
			poses.reserve(Samples + 1);
			float maxRadius{ 1.0f };
			float maxHeight{ PlayerHeight };
			for (int idx{ 0 }; idx <= Samples; ++idx) {
				Pose const pose{ EvaluateKeys(keys, start_, total * static_cast<float>(idx) / static_cast<float>(Samples)) };
				poses.push_back(pose);
				float const pitch{ ToRad(pose.PitchDeg) };
				maxRadius = std::max(maxRadius, pose.Distance * std::cos(pitch));
				maxHeight = std::max(maxHeight, pose.TargetHeight + pose.Distance * std::sin(pitch));
			}

			ImVec2 const origin{ ImGui::GetCursorScreenPos() };
			ImGui::Dummy(ImVec2{ Size * 2.0f + Gap, Size });
			ImDrawList* const drawList{ ImGui::GetWindowDrawList() };

			// 真上：x = 横、y = 前後（開始時のカメラはプレイヤーの下）
			ImVec2 const topOrigin{ origin };
			ImVec2 const topCenter{ topOrigin.x + Size * 0.5f, topOrigin.y + Size * 0.5f };
			float const topScale{ Size * 0.45f / maxRadius };
			auto const topPoint{
				[&topCenter, topScale](Pose const& pose_) -> ImVec2 {
					float const radius{ pose_.Distance * std::cos(ToRad(pose_.PitchDeg)) };
					float const yaw{ ToRad(pose_.YawDeg) };
					return ImVec2{ topCenter.x + radius * std::sin(yaw) * topScale, topCenter.y + radius * std::cos(yaw) * topScale };
				}
			};
			drawList->AddRectFilled(topOrigin, ImVec2{ topOrigin.x + Size, topOrigin.y + Size }, 0xFF262626U);
			drawList->AddCircleFilled(topCenter, 4.0f, 0xFF3C8CF0U);

			// 真横：x = プレイヤーからの水平距離、y = 高さ
			ImVec2 const sideOrigin{ origin.x + Size + Gap, origin.y };
			float const ground{ sideOrigin.y + Size - 14.0f };
			float const playerX{ sideOrigin.x + 24.0f };
			float const sideScale{ (Size - 30.0f) / std::max(maxRadius, maxHeight) };
			auto const sidePoint{
				[playerX, ground, sideScale](Pose const& pose_) -> ImVec2 {
					float const pitch{ ToRad(pose_.PitchDeg) };
					return ImVec2{
						playerX + pose_.Distance * std::cos(pitch) * sideScale,
						ground - (pose_.TargetHeight + pose_.Distance * std::sin(pitch)) * sideScale
					};
				}
			};
			drawList->AddRectFilled(sideOrigin, ImVec2{ sideOrigin.x + Size, sideOrigin.y + Size }, 0xFF262626U);
			drawList->AddLine(ImVec2{ sideOrigin.x, ground }, ImVec2{ sideOrigin.x + Size, ground }, 0xFF606060U);
			drawList->AddLine(ImVec2{ playerX, ground }, ImVec2{ playerX, ground - PlayerHeight * sideScale }, 0xFF3C8CF0U, 3.0f);

			// 軌道
			std::vector<ImVec2> topPath{};
			std::vector<ImVec2> sidePath{};
			for (Pose const& pose : poses) {
				topPath.push_back(topPoint(pose));
				sidePath.push_back(sidePoint(pose));
			}
			drawList->AddPolyline(topPath.data(), static_cast<int>(topPath.size()), 0xFFE0C060U, ImDrawFlags_None, 2.0f);
			drawList->AddPolyline(sidePath.data(), static_cast<int>(sidePath.size()), 0xFFE0C060U, ImDrawFlags_None, 2.0f);

			// キーの位置
			for (std::size_t idx{ 0U }; idx < keys.size(); ++idx) {
				Pose const pose{ EvaluateKeys(keys, start_, keys[idx].Time) };
				bool const isSelected{ static_cast<int>(idx) == selected_ };
				ImU32 const colour{ isSelected ? 0xFFFFFFFFU : ((idx == 0U) ? 0xFF50B0F0U : 0xFFE0C060U) };
				float const radius{ isSelected ? 6.0f : 4.0f };
				drawList->AddCircleFilled(topPoint(pose), radius, colour);
				drawList->AddCircleFilled(sidePoint(pose), radius, colour);
			}

			// 現在の時刻
			if (showNow_) {
				Pose const pose{ EvaluateKeys(keys, start_, now_) };
				drawList->AddCircleFilled(topPoint(pose), 5.0f, 0xFF4040FFU);
				drawList->AddCircleFilled(sidePoint(pose), 5.0f, 0xFF4040FFU);
				drawList->AddLine(sidePoint(pose), ImVec2{ playerX, ground - pose.TargetHeight * sideScale }, 0xFF4040FFU, 1.0f);
			}

			drawList->AddText(ImVec2{ topOrigin.x + 4.0f, topOrigin.y + 2.0f }, 0xFFC8C8C8U, "Top (start = bottom)");
			drawList->AddText(ImVec2{ sideOrigin.x + 4.0f, sideOrigin.y + 2.0f }, 0xFFC8C8C8U, "Side");
		}
#endif
	}

	//////	//////	//////	//////	//////	//////
	//	Time									//
	//////	//////	//////	//////	//////	//////

	void Rig::Initialize(char const* configPath_, char const* editorTitle_) {
		if (configPath_) { ConfigPath = configPath_; }
		if (editorTitle_) { EditorTitle = editorTitle_; }
		Load();
	}

	void Rig::Start() {
		Started_ = true;
		Time_ = 0.0f;
		Captured_ = false;		// 次の Apply で、いまのカメラを取り込む
	}

	void Rig::Reset() {
		Started_ = false;
		Time_ = 0.0f;
		Captured_ = false;
	}

	void Rig::Update(float deltaTime_) {
		if (Started_) { Time_ += std::max(deltaTime_, 0.0f); }
	}

	auto Rig::TotalTime() const -> float {
		return Look.Keys.empty() ? 0.0f : Look.Keys.back().Time;
	}

	auto Rig::Elapsed() const -> float {
		if (PreviewMode_ == Preview::Scrub) { return ScrubTime_; }
		return Started_ ? Time_ : 0.0f;
	}

	auto Rig::StartPose() const -> Pose {
		return Pose{ StartDistance_, 0.0f, StartPitchDeg_, StartYawOffsetDeg_, 0.0f, StartFovDeg_, 0.0f };
	}

	//////	//////	//////	//////	//////	//////
	//	Camera									//
	//////	//////	//////	//////	//////	//////

	// UpdatePlayerCamera が作った今フレームの View から、カメラの位置を逆算して球座標に直す
	void Rig::Capture(Game::TrackingCamera& camera_) {
		Lumina::Mat4 inverseView{ camera_.View.Inv() };
		V3 const eye{ inverseView[3][0], inverseView[3][1], inverseView[3][2] };
		V3 const offset{ eye.x - camera_.Target.x, eye.y - camera_.Target.y, eye.z - camera_.Target.z };

		StartDistance_ = std::max(std::sqrt(Dot(offset, offset)), 0.1f);
		StartPitchDeg_ = ToDeg(std::asin(std::clamp(offset.y / StartDistance_, -1.0f, 1.0f)));
		StartYawRad_ = std::atan2(offset.x, offset.z);
		StartFovDeg_ = ToDeg(BaseFovRad_);
		// Yaw の基準。YawFromOther なら「もう一人 → プレイヤー」の水平の向き（重なっていたら取り込んだ向きのまま）
		BaseYawRad_ = StartYawRad_;
		StartYawOffsetDeg_ = 0.0f;
		if (Look.YawFromOther && HasOther_) {
			float const dx{ camera_.Target.x - Other_.x };
			float const dz{ camera_.Target.z - Other_.z };
			if (dx * dx + dz * dz > 1.0e-6f) {
				BaseYawRad_ = std::atan2(dx, dz);
				float yawOffset{ StartYawRad_ - BaseYawRad_ };
				yawOffset -= 2.0f * Pi * std::floor((yawOffset + Pi) / (2.0f * Pi));		// [-π, π) に畳む
				StartYawOffsetDeg_ = ToDeg(yawOffset);
			}
		}
		Captured_ = true;
	}

	void Rig::Apply(Game::TrackingCamera& camera_) {
		if (!IsActive()) {
			if (IsFovModified_) {
				// 演出が終わった：FOV を元に戻す（View は今フレームの UpdatePlayerCamera がすでに元の計算で作っている）
				camera_.SetFov(BaseFovRad_);
				camera_.VP = camera_.View * camera_.Projection;
				camera_.VPUploadBuffer.Store(&camera_.VP, sizeof(Lumina::Mat4), 0LLU);
				IsFovModified_ = false;
			}
			return;
		}

		if (!IsFovModified_) { BaseFovRad_ = camera_.FovY; }
		if (!Captured_ || PreviewMode_ != Preview::Off) { Capture(camera_); }

		Pose const pose{ EvaluateKeys(Look.Keys, StartPose(), Elapsed()) };
		float const yaw{ BaseYawRad_ + ToRad(pose.YawDeg) };
		float const pitch{ ToRad(std::clamp(pose.PitchDeg, -85.0f, 85.0f)) };
		float const cosPitch{ std::cos(pitch) };
		float const distance{ std::max(pose.Distance, 0.5f) };

		// 注視点：プレイヤー（camera_.Target）ともう一人（SetOther）の間を Focus で。高さはその点の足元から TargetHeight 上
		float const focus{ HasOther_ ? std::clamp(pose.Focus, 0.0f, 1.0f) : 0.0f };
		V3 const target{
			camera_.Target.x + (Other_.x - camera_.Target.x) * focus,
			camera_.Target.y + (Other_.y - camera_.Target.y) * focus + pose.TargetHeight,
			camera_.Target.z + (Other_.z - camera_.Target.z) * focus
		};
		V3 const eye{
			target.x + distance * std::sin(yaw) * cosPitch,
			target.y + distance * std::sin(pitch),
			target.z + distance * std::cos(yaw) * cosPitch
		};

		camera_.SetFov(ToRad(std::clamp(pose.FovDeg, 1.0f, 150.0f)));
		IsFovModified_ = true;
		// カメラシェーク：注視点と視点を同じだけ画面の縦横へずらす（平行移動）＋画面の傾き。向きは視線に垂直な 2 方向（横 = 水平、縦 = 俯角の接線）
		V3 shift{ 0.0f, 0.0f, 0.0f };
		float rollShakeDeg{ 0.0f };
		float const shakeLevel{ std::clamp(Shake_, 0.0f, 1.0f) };
		if (shakeLevel > 1.0e-4f) {
			float const w{ 6.2831853f * std::max(Look.ShakeFrequency, 0.0f) * Elapsed() };
			float const nx{ (std::sin(w + 0.3f) + 0.6f * std::sin(w * 2.3f + 1.7f)) * (1.0f / 1.6f) };
			float const ny{ (std::sin(w * 1.3f + 2.1f) + 0.6f * std::sin(w * 2.9f + 0.4f)) * (1.0f / 1.6f) };
			float const nr{ std::sin(w * 0.9f + 4.0f) };
			float const amount{ Look.ShakeAmount * shakeLevel };
			float const rightX{ std::cos(yaw) * nx * amount };
			float const rightZ{ -std::sin(yaw) * nx * amount };
			float const upX{ -std::sin(yaw) * std::sin(pitch) * ny * amount };
			float const upY{ cosPitch * ny * amount };
			float const upZ{ -std::cos(yaw) * std::sin(pitch) * ny * amount };
			shift = V3{ rightX + upX, upY, rightZ + upZ };
			rollShakeDeg = Look.ShakeRollDeg * shakeLevel * nr;
		}
		V3 const shakenEye{ eye.x + shift.x, eye.y + shift.y, eye.z + shift.z };
		V3 const shakenTarget{ target.x + shift.x, target.y + shift.y, target.z + shift.z };
		camera_.View = LookAtRoll(shakenEye, shakenTarget, ToRad(pose.RollDeg + rollShakeDeg));
		camera_.VP = camera_.View * camera_.Projection;
		camera_.VPUploadBuffer.Store(&camera_.VP, sizeof(Lumina::Mat4), 0LLU);
	}

	//////	//////	//////	//////	//////	//////
	//	Save / Load								//
	//////	//////	//////	//////	//////	//////

	auto Rig::Load() -> bool {
		std::ifstream ifs{ ConfigPath };
		if (!ifs.is_open()) { Status_ = "No config file (using defaults)."; return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { Status_ = "Config is not valid JSON."; return false; }

		// キーは 2 個以上あるときだけ丸ごと置き換える
		auto const itr{ root.find("Keys") };
		if (itr != root.end() && itr->is_array()) {
			std::vector<Key> keys{};
			for (auto const& element : *itr) {
				if (keys.size() >= MaxKeys) { break; }
				if (!element.is_object()) { continue; }
				Key key{};
				Read(element, "Time", key.Time);
				Read(element, "Distance", key.Distance);
				Read(element, "TargetHeight", key.TargetHeight);
				Read(element, "Pitch", key.PitchDeg);
				Read(element, "Yaw", key.YawDeg);
				Read(element, "Roll", key.RollDeg);
				Read(element, "Fov", key.FovDeg);
				Read(element, "Focus", key.Focus);
				key.EaseToNext = ParseEase(element, "Ease", Ease::Linear);
				key.Time = std::max(key.Time, 0.0f);
				keys.push_back(key);
			}
			if (keys.size() >= MinKeys) {
				std::stable_sort(keys.begin(), keys.end(), [](Key const& a_, Key const& b_) { return a_.Time < b_.Time; });
				keys.front().Time = 0.0f;
				Look.Keys = keys;
			}
		}

		Read(root, "ShakeAmount", Look.ShakeAmount);
		Read(root, "ShakeRollDeg", Look.ShakeRollDeg);
		Read(root, "ShakeFrequency", Look.ShakeFrequency);
		if (auto const yawItr{ root.find("YawFromOther") }; yawItr != root.end() && yawItr->is_boolean()) { Look.YawFromOther = yawItr->get<bool>(); }

		Selected_ = std::clamp(Selected_, 0, static_cast<int>(Look.Keys.size()) - 1);
		Status_ = "Loaded.";
		return true;
	}

	auto Rig::Save() -> bool {
		JSON keys = JSON::array();
		for (Key const& key : Look.Keys) {
			keys.push_back(JSON{
				{ "Time", key.Time },
				{ "Distance", key.Distance },
				{ "TargetHeight", key.TargetHeight },
				{ "Pitch", key.PitchDeg },
				{ "Yaw", key.YawDeg },
				{ "Roll", key.RollDeg },
				{ "Fov", key.FovDeg },
				{ "Focus", key.Focus },
				{ "Ease", EaseNames[static_cast<int>(key.EaseToNext)] },
			});
		}
		JSON root = JSON::object();
		root["Keys"] = keys;
		root["ShakeAmount"] = Look.ShakeAmount;
		root["ShakeRollDeg"] = Look.ShakeRollDeg;
		root["ShakeFrequency"] = Look.ShakeFrequency;
		root["YawFromOther"] = Look.YawFromOther;

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

	void Rig::DrawEditor() {
#if defined(_DEBUG)
		if (!ImGui::Begin(EditorTitle)) {
			ImGui::End();
			return;
		}
		Style& style{ Look };
		auto& keys{ style.Keys };

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { Look = Style{}; Selected_ = 0; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		ImGui::SeparatorText("Shake (follows the channel split pulse)");
		ImGui::SliderFloat("Shake amount (world units)", &style.ShakeAmount, 0.0f, 1.0f);
		ImGui::SliderFloat("Shake roll (deg)", &style.ShakeRollDeg, 0.0f, 3.0f);
		ImGui::SliderFloat("Shake frequency (Hz)", &style.ShakeFrequency, 1.0f, 60.0f);
		ImGui::TextDisabled("Shake now: %.2f (0..1)", Shake_);

		ImGui::SeparatorText("Subject");
		ImGui::Checkbox("Yaw 0 = behind the player, facing the other (needs SetOther)", &style.YawFromOther);
		ImGui::TextDisabled("Other: %s  (Focus per key: 0 = player, 1 = other)", HasOther_ ? "set" : "not set");

		//----	Preview							----//
		ImGui::SeparatorText("Preview");
		{
			int mode{ static_cast<int>(PreviewMode_) };
			ImGui::RadioButton("Game", &mode, static_cast<int>(Preview::Off));
			ImGui::SameLine();
			ImGui::RadioButton("Scrub", &mode, static_cast<int>(Preview::Scrub));
			PreviewMode_ = static_cast<Preview>(mode);
		}
		if (PreviewMode_ == Preview::Scrub) {
			ImGui::SliderFloat("Time (s)", &ScrubTime_, 0.0f, std::max(TotalTime(), 0.1f) + 0.5f);
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
		ImGui::TextDisabled(
			"%s  elapsed %.2f / %.2f s",
			IsActive() ? "Active" : "Inactive (normal camera)",
			Elapsed(), TotalTime()
		);

		//----	Timeline						----//
		ImGui::SeparatorText("Keys");
		if (DraggingKey_ < 0) {
			TimelineRange_ = std::max(4.0f, std::ceil(keys.back().Time * 2.0f) * 0.5f + 0.5f);
		}
		Selected_ = std::clamp(Selected_, 0, static_cast<int>(keys.size()) - 1);
		TimelineBar(style, StartPose(), TimelineRange_, Elapsed(), IsActive(), Selected_, DraggingKey_);
		Selected_ = std::clamp(Selected_, 0, static_cast<int>(keys.size()) - 1);
		ImGui::TextDisabled("Click: add key / Drag: move in time / Right-click: delete  (max %u keys)", MaxKeys);

		//----	Selected key					----//
		{
			std::size_t const idx{ static_cast<std::size_t>(Selected_) };
			Key& key{ keys[idx] };
			ImGui::Text("Key %d / %u%s", Selected_ + 1, static_cast<uint32_t>(keys.size()), (idx == 0U) ? "  (start = live camera)" : "");

			if (idx == 0U) {
				ImGui::TextDisabled(
					"Captured at Start: distance %.1f, pitch %.1f deg, yaw %.1f deg, FOV %.1f deg (roll 0, target height 0, focus 0)",
					StartDistance_, StartPitchDeg_, StartYawOffsetDeg_, StartFovDeg_
				);
			}
			else {
				float const lo{ keys[idx - 1U].Time + 0.02f };
				float const hi{ (idx + 1U < keys.size()) ? (keys[idx + 1U].Time - 0.02f) : 30.0f };
				ImGui::SliderFloat("Time (s)##Key", &key.Time, lo, std::max(lo, hi));
				ImGui::SliderFloat("Distance", &key.Distance, 0.5f, 80.0f);
				ImGui::SliderFloat("Target height", &key.TargetHeight, -2.0f, 10.0f);
				ImGui::SliderFloat("Pitch (deg)", &key.PitchDeg, -10.0f, 85.0f);
				ImGui::SliderFloat("Yaw (deg)", &key.YawDeg, -180.0f, 180.0f);
				ImGui::SliderFloat("Roll (deg)", &key.RollDeg, -30.0f, 30.0f);
				ImGui::SliderFloat("FOV (deg)", &key.FovDeg, 5.0f, 90.0f);
				ImGui::SliderFloat("Focus (0 = player, 1 = other)", &key.Focus, 0.0f, 1.0f);
			}

			ImGui::BeginDisabled(idx + 1U >= keys.size());
			EaseCombo("Ease to next key", key.EaseToNext);
			ImGui::EndDisabled();
		}

		ImGui::BeginDisabled(keys.size() >= MaxKeys);
		if (ImGui::Button("Add key")) {
			std::size_t const idx{ static_cast<std::size_t>(Selected_) };
			float const time{ (idx + 1U < keys.size()) ? (0.5f * (keys[idx].Time + keys[idx + 1U].Time)) : (keys[idx].Time + 1.0f) };
			int const added{ InsertKeyAt(style, StartPose(), time) };
			if (added >= 0) { Selected_ = added; }
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(Selected_ == 0 || keys.size() <= MinKeys);
		if (ImGui::Button("Delete key")) {
			RemoveKey(style, Selected_);
			Selected_ = std::clamp(Selected_, 0, static_cast<int>(keys.size()) - 1);
		}
		ImGui::EndDisabled();

		//----	Orbit plots						----//
		ImGui::SeparatorText("Camera path");
		DrawPlots(style, StartPose(), Elapsed(), IsActive(), Selected_);

		ImGui::End();
#endif
	}
}
