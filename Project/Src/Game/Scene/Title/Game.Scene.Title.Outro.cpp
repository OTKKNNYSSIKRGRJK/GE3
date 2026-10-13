module Game.Scene.Title;

import <cmath>;
import <numbers>;
import <algorithm>;
import <vector>;
import <string>;
import <string_view>;

import : Impl;

import Lumina;
import Game.Scene;

import Lumina.MagicCircle;
import Lumina.ShowcaseCharacter;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

//////	//////	//////	//////	//////	//////
//	Title の Outro（Start 決定後の演出）	//
//////	//////	//////	//////	//////	//////
// Outro 開始からの秒 t を 1 本のタイムラインとして持ち、カメラ・魔法陣・火柱・UI・白への抜けが
// それぞれ t から値を決める（どれも t の関数なので、ImGui で t を直接動かして確認できる）。
// 時計は Pause / TimeScale の影響を受けない固定 1/60 秒。Outro 中はシーン全体もこの刻みで進める。
//
// 元の調整値（MagicCircleLayers_ / FirePillars_ / カメラの往復設定）は書き換えない。
// 魔法陣・火柱は毎フレーム作る描画用のコピーにだけ掛け、カメラは開始時のポーズを保存して巻き戻しで戻す。
//
// 区間：1 溜め [0, ChargeEnd) / 2 振り抜き [ChargeEnd, SwingEnd) / 3 命中 [SwingEnd, ImpactEnd)
//       4 割れる [ImpactEnd, SplitEnd) / 5 白 [SplitEnd, WhiteEnd) → FinishOutro()

namespace Game::Scene::Impl {
	namespace {
		constexpr float OutroDeltaTime{ 1.0f / 60.0f };
		constexpr float TwoPi{ std::numbers::pi_v<float> * 2.0f };

		float Saturate(float x_) {
			return std::clamp(x_, 0.0f, 1.0f);
		}
		float SmoothStep(float x_) {
			float const t{ Saturate(x_) };
			return t * t * (3.0f - 2.0f * t);
		}
		float EaseOutCubic(float x_) {
			float const t{ 1.0f - Saturate(x_) };
			return 1.0f - t * t * t;
		}
		float EaseInOutCubic(float x_) {
			float const t{ Saturate(x_) };
			return (t < 0.5f) ? (4.0f * t * t * t) : (1.0f - 4.0f * (1.0f - t) * (1.0f - t) * (1.0f - t));
		}
		// [begin_, end_] での進み具合 0..1（長さ 0 の区間は end_ を過ぎたら 1）
		float Progress(float t_, float begin_, float end_) {
			if (end_ <= begin_) { return (t_ >= end_) ? 1.0f : 0.0f; }
			return Saturate((t_ - begin_) / (end_ - begin_));
		}
		// -π..π に折り返す
		float WrapAngle(float angle_) {
			return std::remainder(angle_, TwoPi);
		}

		// 前の区間より前にならないように揃える（ImGui や JSON で逆転した値が入っても破綻させない）
		OutroTimeline Sanitized(OutroTimeline tl_) {
			tl_.ChargeEnd = std::max(tl_.ChargeEnd, 0.0f);
			tl_.SwingEnd = std::max(tl_.SwingEnd, tl_.ChargeEnd);
			tl_.ImpactEnd = std::max(tl_.ImpactEnd, tl_.SwingEnd);
			tl_.SplitEnd = std::max(tl_.SplitEnd, tl_.ImpactEnd);
			tl_.WhiteEnd = std::max(tl_.WhiteEnd, tl_.SplitEnd);
			return tl_;
		}

		#if defined(_DEBUG)
		char const* SectionName(OutroTimeline const& tl_, float t_) {
			if (t_ < tl_.ChargeEnd) { return "1 Charge"; }
			if (t_ < tl_.SwingEnd) { return "2 Swing"; }
			if (t_ < tl_.ImpactEnd) { return "3 Impact"; }
			if (t_ < tl_.SplitEnd) { return "4 Split"; }
			if (t_ < tl_.WhiteEnd) { return "5 White"; }
			return "End";
		}

		template<size_t N>
		void CopyName(char (&dst_)[N], std::string const& src_) {
			size_t const length{ src_.copy(dst_, N - 1U) };
			dst_[length] = '\0';
		}

		// 読み込み済みのクリップ名から選ぶ
		template<size_t N>
		void ClipCombo(char const* label_, char (&clip_)[N], std::vector<std::string> const& names_) {
			if (ImGui::BeginCombo(label_, clip_)) {
				for (auto const& name : names_) {
					bool const isSelected{ name == clip_ };
					if (ImGui::Selectable(name.c_str(), isSelected)) { CopyName(clip_, name); }
					if (isSelected) { ImGui::SetItemDefaultFocus(); }
				}
				ImGui::EndCombo();
			}
		}
		#endif
	}

	//////	//////	//////	//////	//////	//////
	//	Start / Rewind / Finish					//
	//////	//////	//////	//////	//////	//////

	void Title::StartOutro(bool isPreview_) {
		// 再生中にもう一度呼ばれたら、いったん戻してから頭から
		if (IsOutroActive()) { RewindOutro(); }

		// ここから補間する（往復・自動回転はこの後 Outro 中は止まる）
		OutroStartPose_ = CameraPose{ CameraPitch_, CameraDistance_, CameraTargetY_, CameraFovY_ };
		OutroStartYaw_ = CameraYaw_;
		OutroStartAnimation_ = Character_.CurrentAnimationName();

		OutroState_ = OutroState::Playing;
		OutroTime_ = 0.0f;
		OutroIsPreview_ = isPreview_;

		// メニューは Start を選んだ状態で Outro へ（ImGui から再生したときも同じ見た目にする）
		MenuSelected_ = 0;
		MenuPhase_ = MenuPhase::Outro;
		MenuTimer_ = 0.0f;
	}

	void Title::RewindOutro() {
		if (!IsOutroActive()) { return; }

		CameraPitch_ = OutroStartPose_.Pitch;
		CameraDistance_ = OutroStartPose_.Distance;
		CameraTargetY_ = OutroStartPose_.TargetY;
		CameraFovY_ = OutroStartPose_.FovY;
		CameraYaw_ = OutroStartYaw_;
		CameraShake_ = Lumina::Float3{ 0.0f, 0.0f, 0.0f };

		// アニメは Outro 前のクリップをループ再生に戻す
		if (!OutroStartAnimation_.empty()) {
			Character_.Play(OutroStartAnimation_, true);
		}

		OutroState_ = OutroState::None;
		OutroTime_ = 0.0f;
		OutroIsPreview_ = false;

		// メニューは「表示し終わった」状態に戻す（黒からのフェードインはやり直さない）
		MenuPhase_ = MenuPhase::Idle;
		MenuTimer_ = 0.0f;
		FadeAlpha_ = 0.0f;
		LogoTimer_ = std::max(LogoTimer_, LogoStyle_.Delay + LogoStyle_.FadeDuration);
		PrevKeyConfirm_ = true;		// 押しっぱなしのキーで即再生しない
	}

	void Title::FinishOutro() {
		// 次のシーンへ渡す。遷移シーンを作ったら、渡す先はここだけ変える
		Game::Scene::Flag = 1;
	}

	auto Title::AdvanceOutro() -> float {
		float rate{ 1.0f };
		#if defined(_DEBUG)
		rate = std::max(OutroDebugRate_, 0.0f);
		#endif
		float const deltaTime{ OutroDeltaTime * rate };

		if (OutroState_ == OutroState::Playing) {
			float const end{ Sanitized(Outro_.Timeline).WhiteEnd };
			OutroTime_ += deltaTime;
			if (OutroTime_ >= end) {
				OutroTime_ = end;
				OutroState_ = OutroState::Finished;
				if (!OutroIsPreview_ || OutroSwitchSceneInPreview_) {
					FinishOutro();
				}
			}
		}

		// Finished でもシーン（パーティクル等）は動かし続ける
		return deltaTime;
	}

	//////	//////	//////	//////	//////	//////
	//	各要素の駆動（どれも OutroTime_ の関数）	//
	//////	//////	//////	//////	//////	//////

	void Title::ApplyOutroCamera() {
		auto const tl{ Sanitized(Outro_.Timeline) };
		auto const& style{ Outro_.Camera };
		float const t{ OutroTime_ };

		// * 区間 1：開始時のポーズ → Approach（ドリーイン、FovY は広げる）
		float const approach{ EaseInOutCubic(Progress(t, 0.0f, tl.ChargeEnd)) };
		CameraPitch_ = std::lerp(OutroStartPose_.Pitch, style.Approach.Pitch, approach);
		CameraDistance_ = std::lerp(OutroStartPose_.Distance, style.Approach.Distance, approach);
		CameraTargetY_ = std::lerp(OutroStartPose_.TargetY, style.Approach.TargetY, approach);
		CameraFovY_ = std::lerp(OutroStartPose_.FovY, style.Approach.FovY, approach);

		// Yaw は近い向きから回り込む（キャラの向き基準）
		float const yawTarget{ Character_.Yaw + style.YawOffset };
		CameraYaw_ = WrapAngle(OutroStartYaw_ + WrapAngle(yawTarget - OutroStartYaw_) * approach);

		// * 区間 2：一瞬さらに寄る（以降は維持）
		float const push{ EaseOutCubic(Progress(t, tl.ChargeEnd, tl.SwingEnd)) };
		CameraDistance_ *= std::lerp(1.0f, style.PushDistanceScale, push);
		CameraFovY_ += style.PushFovYAdd * push;

		// * 区間 3 の頭から：微小シェイク（減衰）
		float const s{ t - tl.SwingEnd };
		if (s >= 0.0f && s < style.ShakeDuration) {
			float const decay{ 1.0f - s / std::max(style.ShakeDuration, 0.001f) };
			float const amplitude{ style.ShakeAmplitude * decay * decay };
			float const w{ TwoPi * style.ShakeFrequency * s };
			CameraShake_ = Lumina::Float3{
				amplitude * std::sin(w),
				amplitude * std::sin(w * 1.37f + 1.1f),
				amplitude * std::sin(w * 0.71f + 2.3f),
			};
		}
		else {
			CameraShake_ = Lumina::Float3{ 0.0f, 0.0f, 0.0f };
		}
	}

	void Title::ApplyOutroToMagicCircle(std::vector<Lumina::MagicCircle::Layer>& layers_) const {
		auto const tl{ Sanitized(Outro_.Timeline) };
		auto const& style{ Outro_.MagicCircle };
		float const t{ OutroTime_ };

		// * 回転：速度の倍率 m(τ) = 1 + (M - 1)(τ / C)^2（τ ≥ C では M）を 0..t で積分した「余分に進んだ時間」
		//   を位相に足す（RotateSpeed × Time_ の通常回転はそのまま）
		float const c{ std::max(tl.ChargeEnd, 0.001f) };
		float const s{ std::min(t, c) };
		float const extraTime{
			(style.SpinMultiplier - 1.0f) * (s * s * s / (3.0f * c * c) + std::max(t - c, 0.0f))
		};

		float const intensity{
			std::lerp(1.0f, style.IntensityBoost, SmoothStep(Progress(t, 0.0f, tl.ChargeEnd))) *
			(1.0f - Progress(t, tl.ImpactEnd, tl.ImpactEnd + std::max(style.FadeOutDuration, 0.0f)))
		};

		// * 外周から順に収束：有効なレイヤーに半径の大きい順で番号を振る
		std::vector<size_t> order{};
		order.reserve(layers_.size());
		for (size_t idx{ 0U }; idx < layers_.size(); ++idx) {
			if (layers_[idx].Enabled) { order.emplace_back(idx); }
		}
		std::stable_sort(
			order.begin(), order.end(),
			[&layers_](size_t lhs_, size_t rhs_) { return layers_[lhs_].Radius > layers_[rhs_].Radius; }
		);

		for (size_t rank{ 0U }; rank < order.size(); ++rank) {
			auto& layer{ layers_[order[rank]] };
			float const begin{ style.CollapseStart + style.CollapseStagger * static_cast<float>(rank) };
			float const collapse{ SmoothStep(Progress(t, begin, begin + std::max(style.CollapseDuration, 0.0f))) };

			layer.Phase += layer.RotateSpeed * extraTime;
			layer.Reveal *= 1.0f - collapse;
			layer.Radius *= 1.0f - Saturate(style.ShrinkAmount) * collapse;
			layer.Intensity *= intensity;
		}
	}

	void Title::ApplyOutroAnimation() {
		auto const tl{ Sanitized(Outro_.Timeline) };
		auto const& style{ Outro_.Animation };
		float const t{ OutroTime_ };

		// * 区間 1：ChargeClip を ChargeStart から
		if (t < tl.ChargeEnd) {
			Character_.Pose(style.ChargeClip, style.ChargeStart + t * style.ChargeSpeed);
			return;
		}

		// * 区間 2：区間 2 の終わりでちょうど SlashTime になるように逆算して再生
		// * 区間 3 の頭から：HitStopDuration だけアニメの時間を止め、その後はゆっくり続ける（ループしない）
		float clipTime{ 0.0f };
		if (t < tl.SwingEnd) {
			clipTime = style.SlashTime - (tl.SwingEnd - t) * style.SwingSpeed;
		}
		else {
			float const afterHitStop{ std::max(t - tl.SwingEnd - std::max(style.HitStopDuration, 0.0f), 0.0f) };
			clipTime = style.SlashTime + afterHitStop * style.SwingSpeed * style.FollowSpeedScale;
		}
		Character_.Pose(style.SwingClip, clipTime);
	}

	auto Title::OutroWeaponGlowLevel() const -> float {
		if (!IsOutroActive()) { return 0.0f; }
		auto const tl{ Sanitized(Outro_.Timeline) };
		auto const& style{ Outro_.WeaponGlow };

		// 区間 1 で段階的に（各段の終わり 2 割で次の段へ上がる）。区間 2 以降は最大のまま
		float const p{ Progress(OutroTime_, 0.0f, tl.ChargeEnd) };
		float level{ p };
		if (style.Steps > 0) {
			float const steps{ static_cast<float>(style.Steps) };
			float const x{ p * steps };
			float const k{ std::floor(x) };
			level = std::min((k + SmoothStep((x - k - 0.8f) / 0.2f)) / steps, 1.0f);
		}

		// Outro の頭の一瞬の発光（Idle → 溜めのクリップの切り替わりを、メニューのグローと一緒に隠す）
		float const flash{ style.StartFlash * (1.0f - Progress(OutroTime_, 0.0f, style.StartFlashDuration)) };
		return Saturate(std::max(level, flash));
	}

	auto Title::OutroWeaponFlareStrength() const -> float {
		if (!IsOutroActive()) { return 0.0f; }
		auto const tl{ Sanitized(Outro_.Timeline) };
		auto const& style{ Outro_.WeaponGlow };
		float const t{ OutroTime_ };

		if (t < tl.ChargeEnd) {
			return Saturate(style.FlareChargeLevel) * OutroWeaponGlowLevel();
		}
		// 区間 2・3 で最大、区間 4 の頭から消す
		return 1.0f - Progress(t, tl.ImpactEnd, tl.ImpactEnd + std::max(style.FlareFadeOutDuration, 0.0f));
	}

	auto Title::OutroPostValue(OutroPostCurve const& curve_) const -> float {
		if (!IsOutroActive()) { return curve_.Base; }
		auto const tl{ Sanitized(Outro_.Timeline) };
		float const t{ OutroTime_ };

		if (t < tl.ChargeEnd) {
			return std::lerp(curve_.Base, curve_.Charge, SmoothStep(Progress(t, 0.0f, tl.ChargeEnd)));
		}
		if (t < tl.SwingEnd) {
			// 命中に向けて加速
			float const p{ Progress(t, tl.ChargeEnd, tl.SwingEnd) };
			return std::lerp(curve_.Charge, curve_.Hit, p * p);
		}
		return std::lerp(curve_.Hit, curve_.Split, SmoothStep(Progress(t, tl.SwingEnd, tl.SplitEnd)));
	}

	auto Title::OutroFirePillarBoost() const -> float {
		if (!IsOutroActive()) { return 0.0f; }
		auto const tl{ Sanitized(Outro_.Timeline) };
		float const p{ Progress(OutroTime_, 0.0f, tl.ChargeEnd) };
		return std::pow(p, std::max(Outro_.FirePillar.RiseCurve, 0.01f));
	}

	auto Title::OutroUIVisibility() const -> float {
		if (!IsOutroActive()) { return 1.0f; }
		return 1.0f - Progress(OutroTime_, 0.0f, Outro_.UI.FadeDuration);
	}

	auto Title::OutroWhiteAmount() const -> float {
		if (!IsOutroActive()) { return 0.0f; }
		auto const tl{ Sanitized(Outro_.Timeline) };
		auto const& style{ Outro_.Screen };

		// * 区間 5 への白への抜け
		float const begin{ tl.SplitEnd - std::max(style.WhiteRampDuration, 0.0f) };
		float const ramp{ SmoothStep(Progress(OutroTime_, begin, tl.SplitEnd)) };

		// * 命中の白フラッシュ（素早く立ち上がり、2 乗で減衰）
		float flash{ 0.0f };
		float const s{ OutroTime_ - tl.SwingEnd };
		if (s >= 0.0f) {
			float const attack{ std::max(style.FlashAttack, 0.0f) };
			if (s < attack) {
				flash = style.FlashPeak * (s / attack);
			}
			else {
				float const decay{ 1.0f - Progress(s, attack, attack + std::max(style.FlashDecay, 0.0f)) };
				flash = style.FlashPeak * decay * decay;
			}
		}

		return Saturate(std::max(ramp, flash));
	}

	auto Title::OutroRaysIntensity() const -> float {
		if (!IsOutroActive()) { return 0.0f; }
		auto const tl{ Sanitized(Outro_.Timeline) };
		auto const& style{ Outro_.Rays };
		float const t{ OutroTime_ };

		// * 区間 1：武器の光の段階に合わせて弱く
		float const charge{ style.ChargeLevel * OutroWeaponGlowLevel() };
		if (t < tl.ChargeEnd) { return charge; }

		// * 区間 2：命中に向けて加速しながら強める
		if (t < tl.SwingEnd) {
			float const p{ Progress(t, tl.ChargeEnd, tl.SwingEnd) };
			return std::lerp(charge, style.Peak, p * p);
		}

		// * 命中から：減衰
		float const decay{ 1.0f - Progress(t, tl.SwingEnd, tl.SwingEnd + std::max(style.DecayDuration, 0.0f)) };
		return style.Peak * decay * decay;
	}

	auto Title::OutroRaysLength() const -> float {
		auto const tl{ Sanitized(Outro_.Timeline) };
		auto const& style{ Outro_.Rays };
		float const t{ OutroTime_ };

		if (t < tl.ChargeEnd) { return style.LengthCharge; }
		if (t < tl.SwingEnd) {
			float const p{ Progress(t, tl.ChargeEnd, tl.SwingEnd) };
			return std::lerp(style.LengthCharge, style.LengthPeak, p * p);
		}
		// 消えていく間も外へ伸び続ける
		float const p{ EaseOutCubic(Progress(t, tl.SwingEnd, tl.SwingEnd + std::max(style.DecayDuration, 0.0f))) };
		return std::lerp(style.LengthPeak, style.LengthAfter, p);
	}

	//////	//////	//////	//////	//////	//////
	//	ImGui									//
	//////	//////	//////	//////	//////	//////

	void Title::EditOutro() {
		#if defined(_DEBUG)
		ImGui::Begin("Outro");

		auto const tl{ Sanitized(Outro_.Timeline) };

		// * 再生の操作（調整のたびにシーンを抜けずに繰り返し確認できるように）
		if (ImGui::Button("Play Outro")) { StartOutro(true); }
		ImGui::SameLine();
		ImGui::BeginDisabled(!IsOutroActive());
		if (ImGui::Button("Rewind")) { RewindOutro(); }
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Checkbox("Switch scene at end (preview)", &OutroSwitchSceneInPreview_);
		ImGui::SliderFloat("Playback rate (debug)", &OutroDebugRate_, 0.0f, 2.0f);

		constexpr char const* stateNames[]{ "None", "Playing", "Finished" };
		ImGui::Text(
			"%s%s  t = %.3f / %.3f s  [%s]",
			stateNames[static_cast<int>(OutroState_)],
			OutroIsPreview_ ? " (preview)" : "",
			OutroTime_, tl.WhiteEnd,
			IsOutroActive() ? SectionName(tl, OutroTime_) : "-"
		);
		ImGui::BeginDisabled(!IsOutroActive());
		if (ImGui::SliderFloat("Scrub", &OutroTime_, 0.0f, tl.WhiteEnd, "%.3f s")) {
			// 最後まで動かしたら、次の更新で通常どおり終わらせる
			OutroState_ = OutroState::Playing;
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled("Pause / TimeScale are ignored during the Outro (fixed 1/60 s).");

		// * 保存・読み込み（TitleStage の Save / Load と同じ。再生中はカメラが書き換わっているので無効）
		ImGui::BeginDisabled(IsOutroActive());
		if (ImGui::Button("Save")) {
			SettingsStatus_ = SaveSettings() ? "Saved" : "Save FAILED";
		}
		ImGui::SameLine();
		if (ImGui::Button("Load")) {
			SettingsStatus_ = LoadSettings() ? "Loaded" : "Load FAILED (file missing or broken)";
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("%s  (\"Outro\")  %s", SettingsFilePath, SettingsStatus_.c_str());

		if (ImGui::CollapsingHeader("Timeline (end of each section, s)", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto& timeline{ Outro_.Timeline };
			ImGui::SliderFloat("1 Charge end", &timeline.ChargeEnd, 0.0f, 6.0f, "%.3f");
			ImGui::SliderFloat("2 Swing end", &timeline.SwingEnd, 0.0f, 6.0f, "%.3f");
			ImGui::SliderFloat("3 Impact end", &timeline.ImpactEnd, 0.0f, 6.0f, "%.3f");
			ImGui::SliderFloat("4 Split end", &timeline.SplitEnd, 0.0f, 6.0f, "%.3f");
			ImGui::SliderFloat("5 White end", &timeline.WhiteEnd, 0.0f, 6.0f, "%.3f");
			ImGui::TextDisabled("Values earlier than the previous section are treated as equal to it.");
		}

		if (ImGui::CollapsingHeader("Animation")) {
			ImGui::PushID("OutroAnimation");
			auto& anim{ Outro_.Animation };
			auto const& names{ Character_.AnimationNames() };

			ImGui::SeparatorText("Section 1 (charge)");
			ClipCombo("ChargeClip", anim.ChargeClip, names);
			ImGui::SameLine();
			ImGui::TextDisabled("length %.3f s", Character_.AnimationDuration(anim.ChargeClip));
			ImGui::SliderFloat("ChargeStart (clip s)", &anim.ChargeStart, 0.0f, 5.0f, "%.3f");
			ImGui::SliderFloat("ChargeSpeed", &anim.ChargeSpeed, 0.0f, 4.0f, "%.3f");
			ImGui::TextDisabled("Clip time at the end of section 1: %.3f s", anim.ChargeStart + tl.ChargeEnd * anim.ChargeSpeed);

			ImGui::SeparatorText("Section 2 (swing) and after");
			ClipCombo("SwingClip", anim.SwingClip, names);
			ImGui::SameLine();
			ImGui::TextDisabled("length %.3f s", Character_.AnimationDuration(anim.SwingClip));
			ImGui::SliderFloat("SlashTime (clip s, at hit)", &anim.SlashTime, 0.0f, 5.0f, "%.3f");
			ImGui::SliderFloat("SwingSpeed", &anim.SwingSpeed, 0.0f, 6.0f, "%.3f");
			{
				float const swingStart{ anim.SlashTime - (tl.SwingEnd - tl.ChargeEnd) * anim.SwingSpeed };
				ImGui::TextDisabled(
					"Clip time at the start of section 2: %.3f s%s",
					swingStart, (swingStart < 0.0f) ? "  (clamped to 0)" : ""
				);
			}
			ImGui::SliderFloat("HitStopDuration", &anim.HitStopDuration, 0.0f, 0.5f, "%.3f");
			ImGui::SliderFloat("FollowSpeedScale", &anim.FollowSpeedScale, 0.0f, 2.0f, "%.3f");

			ImGui::SeparatorText("Now");
			ImGui::Text(
				"%s  %.3f / %.3f s",
				Character_.CurrentAnimationName().c_str(),
				Character_.CurrentAnimationTime(),
				Character_.CurrentAnimationDuration()
			);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("WeaponGlow")) {
			ImGui::PushID("OutroWeaponGlow");
			auto& glow{ Outro_.WeaponGlow };
			ImGui::ProgressBar(OutroWeaponGlowLevel(), ImVec2{ -1.0f, 0.0f }, "glow level");
			ImGui::ProgressBar(OutroWeaponFlareStrength(), ImVec2{ -1.0f, 0.0f }, "flare");

			ImGui::SeparatorText("Trail (multiplies the Particles > WeaponGlow settings)");
			ImGui::SliderInt("Steps (0 = smooth)", &glow.Steps, 0, 8);
			ImGui::SliderFloat("StartFlash", &glow.StartFlash, 0.0f, 1.0f);
			ImGui::SliderFloat("StartFlashDuration", &glow.StartFlashDuration, 0.0f, 1.0f);
			ImGui::SliderFloat("RateScale", &glow.RateScale, 0.0f, 10.0f);
			ImGui::SliderFloat("SpanBegin", &glow.SpanBegin, 0.0f, 1.0f);
			ImGui::SliderFloat("SizeScale", &glow.SizeScale, 0.0f, 4.0f);
			ImGui::SliderFloat("AlphaScale", &glow.AlphaScale, 0.0f, 6.0f);
			ImGui::SliderFloat("WhiteHot", &glow.WhiteHot, 0.0f, 1.0f);

			ImGui::SeparatorText("Tip flare (Particles.png cell)");
			ImGui::SliderFloat("FlareChargeLevel", &glow.FlareChargeLevel, 0.0f, 1.0f);
			ImGui::SliderFloat("FlareFadeOutDuration", &glow.FlareFadeOutDuration, 0.0f, 1.0f);
			ImGui::SliderFloat("FlareSize", &glow.FlareSize, 0.0f, 10.0f);
			ImGui::SliderFloat("FlareAlpha", &glow.FlareAlpha, 0.0f, 4.0f);
			ImGui::ColorEdit3("FlareColor", &glow.FlareColor.x);
			ImGui::SliderFloat("FlareFlickerDepth", &glow.FlareFlickerDepth, 0.0f, 1.0f);
			ImGui::SliderFloat("FlareFlickerFrequency (Hz)", &glow.FlareFlickerFrequency, 0.0f, 40.0f);
			ImGui::SliderFloat("FlareSpin (rad/s)", &glow.FlareSpin, -10.0f, 10.0f);
			ImGui::SliderFloat("FlareCameraOffset", &glow.FlareCameraOffset, 0.0f, 2.0f);
			ImGui::SliderInt("FlareAtlas", &glow.FlareAtlas, 0, 7);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Post (Title canvas -> back buffer)")) {
			ImGui::PushID("OutroPost");
			auto& post{ Outro_.Post };
			ImGui::TextDisabled("Each curve: Base (outside Outro / t = 0) -> end of 1 -> hit -> end of 4");
			auto const editCurve{
				[this](char const* label_, OutroPostCurve& curve_, float max_, char const* format_) {
					ImGui::PushID(label_);
					ImGui::SeparatorText(label_);
					ImGui::Text("now %.4f", OutroPostValue(curve_));
					ImGui::SliderFloat("Base", &curve_.Base, 0.0f, max_, format_);
					ImGui::SliderFloat("Charge", &curve_.Charge, 0.0f, max_, format_);
					ImGui::SliderFloat("Hit", &curve_.Hit, 0.0f, max_, format_);
					ImGui::SliderFloat("Split", &curve_.Split, 0.0f, max_, format_);
					ImGui::PopID();
				}
			};
			editCurve("ZoomBlur", post.ZoomBlur, 1.0f, "%.3f");
			ImGui::SliderInt("ZoomTaps", &post.ZoomTaps, 1, 32);
			editCurve("Chromatic", post.Chromatic, 0.05f, "%.4f");
			editCurve("Bloom", post.Bloom, 6.0f, "%.3f");
			ImGui::SliderInt("BloomTaps", &post.BloomTaps, 1, 32);
			ImGui::SliderFloat("BloomRadiusPx", &post.BloomRadiusPx, 0.0f, 128.0f);
			ImGui::SliderFloat("BloomThreshold", &post.BloomThreshold, 0.0f, 2.0f);
			editCurve("Vignette (white)", post.Vignette, 1.0f, "%.3f");
			ImGui::ColorEdit3("VignetteColor", &post.VignetteColor.x);
			ImGui::SliderFloat("VignetteRadius", &post.VignetteRadius, 0.0f, 1.5f);
			ImGui::SliderFloat("VignetteSoftness", &post.VignetteSoftness, 0.001f, 1.5f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Rays (from weapon tip)")) {
			ImGui::PushID("OutroRays");
			auto& rays{ Outro_.Rays };
			ImGui::Checkbox("Enabled", &rays.Enabled);
			ImGui::SameLine();
			ImGui::Text("intensity %.2f  length %.2f", OutroRaysIntensity(), OutroRaysLength());

			ImGui::SeparatorText("Timing");
			ImGui::SliderFloat("ChargeLevel", &rays.ChargeLevel, 0.0f, 1.0f);
			ImGui::SliderFloat("Peak", &rays.Peak, 0.0f, 4.0f);
			ImGui::SliderFloat("DecayDuration", &rays.DecayDuration, 0.0f, 2.0f);
			ImGui::SliderFloat("LengthCharge", &rays.LengthCharge, 0.0f, 2.0f);
			ImGui::SliderFloat("LengthPeak", &rays.LengthPeak, 0.0f, 3.0f);
			ImGui::SliderFloat("LengthAfter", &rays.LengthAfter, 0.0f, 4.0f);

			ImGui::SeparatorText("Look");
			ImGui::SliderInt("RayCount", &rays.RayCount, 1, 128);
			ImGui::SliderFloat("Sharpness", &rays.Sharpness, 0.5f, 20.0f);
			ImGui::SliderFloat("RotateSpeed (rad/s)", &rays.RotateSpeed, -3.0f, 3.0f);
			ImGui::SliderFloat("FlickerSpeed (/s)", &rays.FlickerSpeed, 0.0f, 30.0f);
			ImGui::ColorEdit3("Color", &rays.Color.x);
			ImGui::ColorEdit3("CoreColor", &rays.CoreColor.x);
			ImGui::SliderFloat("CoreRadius", &rays.CoreRadius, 0.0f, 0.3f, "%.3f");
			ImGui::SliderFloat("CoreIntensity", &rays.CoreIntensity, 0.0f, 5.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Camera")) {
			ImGui::PushID("OutroCamera");
			auto& camera{ Outro_.Camera };
			ImGui::SeparatorText("Approach pose (end of section 1)");
			ImGui::SliderFloat("Pitch", &camera.Approach.Pitch, -1.4f, 1.4f);
			ImGui::SliderFloat("Distance", &camera.Approach.Distance, 1.0f, 60.0f);
			ImGui::SliderFloat("TargetY", &camera.Approach.TargetY, -5.0f, 15.0f);
			ImGui::SliderFloat("FovY", &camera.Approach.FovY, 0.2f, 1.6f);
			ImGui::SliderFloat("YawOffset (from character)", &camera.YawOffset, -3.1416f, 3.1416f);
			ImGui::BeginDisabled(IsOutroActive());
			if (ImGui::Button("Store current camera as Approach")) {
				camera.Approach = CameraPose{ CameraPitch_, CameraDistance_, CameraTargetY_, CameraFovY_ };
				camera.YawOffset = WrapAngle(CameraYaw_ - Character_.Yaw);
			}
			ImGui::EndDisabled();
			ImGui::SeparatorText("Push (section 2)");
			ImGui::SliderFloat("PushDistanceScale", &camera.PushDistanceScale, 0.2f, 1.5f);
			ImGui::SliderFloat("PushFovYAdd", &camera.PushFovYAdd, -0.5f, 0.5f);
			ImGui::SeparatorText("Shake (from section 3)");
			ImGui::SliderFloat("ShakeAmplitude", &camera.ShakeAmplitude, 0.0f, 0.5f);
			ImGui::SliderFloat("ShakeDuration", &camera.ShakeDuration, 0.0f, 1.0f);
			ImGui::SliderFloat("ShakeFrequency (Hz)", &camera.ShakeFrequency, 1.0f, 60.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("MagicCircle")) {
			ImGui::PushID("OutroMagicCircle");
			auto& circle{ Outro_.MagicCircle };
			ImGui::SliderFloat("SpinMultiplier", &circle.SpinMultiplier, 1.0f, 30.0f);
			ImGui::SliderFloat("IntensityBoost", &circle.IntensityBoost, 0.0f, 6.0f);
			ImGui::SliderFloat("CollapseStart", &circle.CollapseStart, 0.0f, 3.0f);
			ImGui::SliderFloat("CollapseDuration", &circle.CollapseDuration, 0.0f, 3.0f);
			ImGui::SliderFloat("CollapseStagger (outer -> inner)", &circle.CollapseStagger, 0.0f, 1.0f);
			ImGui::SliderFloat("ShrinkAmount", &circle.ShrinkAmount, 0.0f, 1.0f);
			ImGui::SliderFloat("FadeOutDuration (from section 4)", &circle.FadeOutDuration, 0.0f, 1.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("FirePillar")) {
			ImGui::PushID("OutroFirePillar");
			auto& pillar{ Outro_.FirePillar };
			ImGui::SliderFloat("IntensityPeak", &pillar.IntensityPeak, 0.0f, 8.0f);
			ImGui::SliderFloat("HeightPeak", &pillar.HeightPeak, 0.1f, 4.0f);
			ImGui::SliderFloat("LightPeak", &pillar.LightPeak, 0.0f, 8.0f);
			ImGui::SliderFloat("RiseCurve", &pillar.RiseCurve, 0.1f, 6.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("UI (logo / menu)")) {
			ImGui::PushID("OutroUI");
			auto& ui{ Outro_.UI };
			ImGui::SliderFloat("FadeDuration", &ui.FadeDuration, 0.0f, 2.0f);
			ImGui::SliderFloat("SelectedGlowBoost", &ui.SelectedGlowBoost, 0.0f, 10.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Screen")) {
			ImGui::PushID("OutroScreen");
			auto& screen{ Outro_.Screen };
			ImGui::ProgressBar(OutroWhiteAmount(), ImVec2{ -1.0f, 0.0f }, "white");
			ImGui::SliderFloat("WhiteRampDuration (before section 5)", &screen.WhiteRampDuration, 0.0f, 2.0f);
			ImGui::SeparatorText("Flash (at hit = start of section 3)");
			ImGui::SliderFloat("FlashPeak", &screen.FlashPeak, 0.0f, 1.0f);
			ImGui::SliderFloat("FlashAttack", &screen.FlashAttack, 0.0f, 0.3f, "%.3f");
			ImGui::SliderFloat("FlashDecay", &screen.FlashDecay, 0.0f, 1.0f, "%.3f");
			ImGui::PopID();
		}

		ImGui::End();
		#endif
	}
}
