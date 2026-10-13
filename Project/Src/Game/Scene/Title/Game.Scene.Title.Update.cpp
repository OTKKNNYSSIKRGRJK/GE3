module Game.Scene.Title;

import <string>;
import <cmath>;
import <numbers>;
import <algorithm>;

import : Impl;

import Lumina;
import Game.Scene;

import Lumina.FirePillar;
import Lumina.MagicCircle;
import Lumina.ShowcaseCharacter;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace Game::Scene::Impl {
	namespace {
		// 往復の重み 0..1（0 = 端点 A, 1 = 端点 B）。clock_ は秒
		float SwayWeight(SwayChannel const& channel_, float clock_) {
			constexpr float twoPi{ std::numbers::pi_v<float> * 2.0f };
			float const period{ std::max(channel_.Period, 0.1f) };
			float const s{ 0.5f - 0.5f * std::cos(twoPi * (clock_ / period + channel_.Phase)) };
			float const eased{ s * s * (3.0f - 2.0f * s) };
			return std::lerp(s, eased, std::clamp(channel_.Dwell, 0.0f, 1.0f));
		}
	}

	#if defined(_DEBUG)
	namespace {
		void EditCameraPose(char const* label_, CameraPose& pose_) {
			if (ImGui::TreeNode(label_)) {
				ImGui::SliderFloat("Pitch", &pose_.Pitch, -1.4f, 1.4f);
				ImGui::SliderFloat("Distance", &pose_.Distance, 2.0f, 60.0f);
				ImGui::SliderFloat("TargetY", &pose_.TargetY, -5.0f, 15.0f);
				ImGui::SliderFloat("FovY", &pose_.FovY, 0.2f, 1.6f);
				ImGui::TreePop();
			}
		}

		void EditSwayChannel(char const* label_, SwayChannel& channel_, float weight_) {
			ImGui::PushID(label_);
			ImGui::Checkbox(label_, &channel_.Enabled);
			ImGui::SameLine();
			ImGui::ProgressBar(weight_, ImVec2{ -1.0f, 0.0f }, "");
			ImGui::SliderFloat("Period (s)", &channel_.Period, 0.5f, 60.0f);
			ImGui::SliderFloat("Phase", &channel_.Phase, 0.0f, 1.0f);
			ImGui::SliderFloat("Dwell", &channel_.Dwell, 0.0f, 1.0f);
			ImGui::PopID();
		}

		// 整数で扱いたい float パラメータ用（NoiseLayer::ScaleU は継ぎ目対策で整数）
		bool SliderIntAsFloat(char const* label_, float& value_, int min_, int max_) {
			int i{ static_cast<int>(value_ + 0.5f) };
			bool const changed{ ImGui::SliderInt(label_, &i, min_, max_) };
			if (changed) { value_ = static_cast<float>(i); }
			return changed;
		}

		void EditNoiseLayer(char const* id_, Lumina::NoiseLayer& layer_, bool showWarpAmount_) {
			ImGui::PushID(id_);
			SliderIntAsFloat("ScaleU (int)", layer_.ScaleU, 1, 8);
			ImGui::SliderFloat("ScaleV", &layer_.ScaleV, 0.1f, 4.0f);
			ImGui::SliderFloat("ScrollSpeed", &layer_.ScrollSpeed, -3.0f, 3.0f);
			if (showWarpAmount_) {
				ImGui::SliderFloat("WarpAmount", &layer_.WarpAmount, 0.0f, 1.0f);
			}
			ImGui::PopID();
		}

		void EditFirePillarEntry(FirePillarEntry& entry_) {
			auto& desc{ entry_.Desc };

			ImGui::InputText("Name", entry_.Name, sizeof(entry_.Name));
			ImGui::Checkbox("Enabled", &entry_.Enabled);
			ImGui::DragFloat3("Offset", &entry_.Offset.x, 0.01f);
			ImGui::InputInt("Seed", &entry_.Seed);
			ImGui::SliderFloat("IntensityScale", &entry_.IntensityScale, 0.0f, 4.0f);

			if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::SliderFloat("Radius", &desc.Radius, 0.05f, 5.0f);
				ImGui::SliderFloat("Height", &desc.Height, 0.5f, 20.0f);
				ImGui::SliderFloat("TopScale", &desc.TopScale, 0.0f, 2.0f);
				ImGui::SliderFloat("ShapeCurve", &desc.ShapeCurve, 0.1f, 4.0f);
				ImGui::SliderFloat("WobbleAmp", &desc.WobbleAmp, 0.0f, 1.0f);
				ImGui::SliderFloat("WobbleFreq", &desc.WobbleFreq, 0.0f, 10.0f);
				ImGui::SliderFloat("WobbleWave", &desc.WobbleWave, 0.0f, 4.0f);
				ImGui::SliderFloat("Twist", &desc.Twist, -2.0f, 2.0f);
			}

			if (ImGui::CollapsingHeader("Texture")) {
				ImGui::SeparatorText("Main Layer");
				EditNoiseLayer("Main", desc.Layer, true);
				ImGui::SeparatorText("Warp Layer");
				EditNoiseLayer("Warp", desc.WarpLayer, false);
			}

			if (ImGui::CollapsingHeader("Erosion / Fade")) {
				ImGui::SliderFloat("CutBottom", &desc.CutBottom, 0.0f, 1.0f);
				ImGui::SliderFloat("CutTop", &desc.CutTop, 0.0f, 1.0f);
				ImGui::SliderFloat("CutCurve", &desc.CutCurve, 0.1f, 4.0f);
				ImGui::SliderFloat("Softness", &desc.Softness, 0.001f, 0.5f);
				ImGui::SliderFloat("FadeBottom", &desc.FadeBottom, 0.001f, 0.5f);
				ImGui::SliderFloat("FadeTopStart", &desc.FadeTopStart, 0.0f, 0.999f);
				ImGui::SliderFloat("RimPower", &desc.RimPower, 0.0f, 8.0f);
				ImGui::SliderFloat("BackDarken", &desc.BackDarken, 0.0f, 1.0f);
			}

			if (ImGui::CollapsingHeader("Color")) {
				ImGui::SliderFloat("Intensity", &desc.Intensity, 0.0f, 4.0f);
				ImGui::SliderFloat("Opacity", &desc.Opacity, 0.0f, 1.0f);
				ImGui::SliderFloat("Additive", &desc.Additive, 0.0f, 1.0f);
				ImGui::SliderFloat("TextureColor", &desc.TextureColor, 0.0f, 1.0f);
				ImGui::SliderFloat("HeatScale", &desc.HeatScale, 0.0f, 10.0f);
				ImGui::SliderFloat("HeatHeight", &desc.HeatHeight, 0.0f, 1.0f);
				ImGui::ColorEdit3("Ramp Low", &desc.Ramp.Low.x);
				ImGui::ColorEdit3("Ramp Mid", &desc.Ramp.Mid.x);
				ImGui::ColorEdit3("Ramp High", &desc.Ramp.High.x);
				ImGui::SliderFloat("Ramp MidPosition", &desc.Ramp.MidPosition, 0.01f, 0.99f);
			}
		}

		void EditMaterial(char const* id_, Lumina::ShowcaseMaterial& material_) {
			ImGui::PushID(id_);
			ImGui::ColorEdit3("BaseColor", &material_.BaseColor.x);
			ImGui::ColorEdit3("EmissiveColor", &material_.EmissiveColor.x);
			ImGui::SliderFloat("EmissiveIntensity", &material_.EmissiveIntensity, 0.0f, 3.0f);
			ImGui::PopID();
		}

		void EditMagicCircleLayer(Lumina::MagicCircle::Layer& layer_) {
			using MC = Lumina::MagicCircle;

			ImGui::Checkbox("Enabled", &layer_.Enabled);

			ImGui::SeparatorText("Elements");
			ImGui::CheckboxFlags("Ring0", &layer_.Elements, MC::Element_Ring0);
			ImGui::SameLine();
			ImGui::CheckboxFlags("Ring1", &layer_.Elements, MC::Element_Ring1);
			ImGui::SameLine();
			ImGui::CheckboxFlags("Polygon", &layer_.Elements, MC::Element_Polygon);
			ImGui::SameLine();
			ImGui::CheckboxFlags("Nodes", &layer_.Elements, MC::Element_Nodes);
			ImGui::CheckboxFlags("Ticks", &layer_.Elements, MC::Element_Ticks);
			ImGui::SameLine();
			ImGui::CheckboxFlags("Spokes", &layer_.Elements, MC::Element_Spokes);
			ImGui::SameLine();
			ImGui::CheckboxFlags("Runes", &layer_.Elements, MC::Element_Runes);
			ImGui::SameLine();
			ImGui::CheckboxFlags("Fill", &layer_.Elements, MC::Element_Fill);

			ImGui::SeparatorText("Placement");
			ImGui::SliderFloat("Radius", &layer_.Radius, 0.2f, 15.0f);
			ImGui::SliderFloat("Height", &layer_.Height, 0.0f, 10.0f);
			ImGui::SliderFloat("RotateSpeed", &layer_.RotateSpeed, -2.0f, 2.0f);
			ImGui::SliderFloat("Phase", &layer_.Phase, -3.1416f, 3.1416f);
			ImGui::SliderFloat("Reveal", &layer_.Reveal, 0.0f, 1.0f);

			ImGui::SeparatorText("Look");
			ImGui::ColorEdit3("Color", &layer_.Color.x);
			ImGui::SliderFloat("Intensity", &layer_.Intensity, 0.0f, 4.0f);
			ImGui::SliderFloat("LineWidth", &layer_.LineWidth, 0.0f, 0.03f, "%.4f");
			ImGui::SliderFloat("GlowAmount", &layer_.GlowAmount, 0.0f, 1.5f);
			ImGui::SliderFloat("GlowWidth", &layer_.GlowWidth, 0.001f, 0.15f, "%.4f");
			ImGui::SliderFloat("FillAmount", &layer_.FillAmount, 0.0f, 0.5f);

			if ((layer_.Elements & (MC::Element_Ring0 | MC::Element_Ring1)) != 0U) {
				ImGui::SeparatorText("Rings");
				ImGui::SliderFloat("Ring0", &layer_.Ring0, 0.0f, 1.0f);
				ImGui::SliderFloat("Ring1", &layer_.Ring1, 0.0f, 1.0f);
			}
			if ((layer_.Elements & (MC::Element_Polygon | MC::Element_Nodes)) != 0U) {
				ImGui::SeparatorText("Polygon {N/K}");
				ImGui::SliderInt("N (Vertices)", &layer_.PolygonVertices, 3, 16);
				ImGui::SliderInt("K (Step)", &layer_.PolygonStep, 1, std::max(1, layer_.PolygonVertices - 1));
				ImGui::SliderFloat("PolygonRadius", &layer_.PolygonRadius, 0.0f, 1.0f);
				ImGui::SliderFloat("NodeRadius", &layer_.NodeRadius, 0.0f, 0.3f);
			}
			if ((layer_.Elements & MC::Element_Ticks) != 0U) {
				ImGui::SeparatorText("Ticks");
				ImGui::SliderInt("TickCount", &layer_.TickCount, 1, 360);
				ImGui::SliderFloat("TickInner", &layer_.TickInner, 0.0f, 1.0f);
				ImGui::SliderFloat("TickOuter", &layer_.TickOuter, 0.0f, 1.0f);
			}
			if ((layer_.Elements & MC::Element_Spokes) != 0U) {
				ImGui::SeparatorText("Spokes");
				ImGui::SliderInt("SpokeCount", &layer_.SpokeCount, 1, 128);
				ImGui::SliderFloat("SpokeInner", &layer_.SpokeInner, 0.0f, 1.0f);
				ImGui::SliderFloat("SpokeOuter", &layer_.SpokeOuter, 0.0f, 1.0f);
			}
			if ((layer_.Elements & MC::Element_Runes) != 0U) {
				ImGui::SeparatorText("Runes");
				ImGui::SliderInt("RuneCount", &layer_.RuneCount, 1, 128);
				ImGui::SliderFloat("RuneInner", &layer_.RuneInner, 0.0f, 1.0f);
				ImGui::SliderFloat("RuneOuter", &layer_.RuneOuter, 0.0f, 1.0f);
				ImGui::InputInt("RuneSeed", &layer_.RuneSeed);
				ImGui::SliderFloat("RuneDensity", &layer_.RuneDensity, 0.0f, 1.0f);
				ImGui::SliderFloat("RuneShimmer", &layer_.RuneShimmer, 0.0f, 1.0f);
			}
		}
	}
	#endif

	template<>
	void Title::Update(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		// シーンの切り替え（Start / Exit）はメニュー（UpdateUI）が行う

		constexpr float twoPi{ std::numbers::pi_v<float> * 2.0f };

		// 往復用の時計は実時間で進める（TimeScale 自体も往復させるので、その影響を受けないように）
		float const realDeltaTime{ IsPaused_ ? 0.0f : (1.0f / 60.0f) };
		SwayClock_ += realDeltaTime;

		if (TimeScaleSway_.Enabled && TimeScaleSway_.Channel.Enabled) {
			TimeScale_ = std::lerp(
				TimeScaleSway_.Max,
				TimeScaleSway_.Min,
				SwayWeight(TimeScaleSway_.Channel, SwayClock_)
			);
		}
		float const deltaTime{ realDeltaTime * TimeScale_ };

		Time_ += deltaTime;

		#if defined(_DEBUG)
		{
			ImGui::Begin("TitleStage");

			// * 調整結果の保存・読み込み（火柱・魔法陣・キャラ・照明・カメラをまとめて）
			if (ImGui::Button("Save")) {
				SettingsStatus_ = SaveSettings() ? "Saved" : "Save FAILED";
			}
			ImGui::SameLine();
			if (ImGui::Button("Load")) {
				SettingsStatus_ = LoadSettings() ? "Loaded" : "Load FAILED (file missing or broken)";
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%s  %s", SettingsFilePath, SettingsStatus_.c_str());

			ImGui::Separator();

			ImGui::Checkbox("Pause", &IsPaused_);
			ImGui::SameLine();
			if (ImGui::Button("Reset Time")) {
				Time_ = 0.0f;
				FirePillarAge_ = 0.0f;
			}
			{
				auto& sway{ TimeScaleSway_ };
				ImGui::BeginDisabled(sway.Enabled && sway.Channel.Enabled);
				ImGui::SliderFloat("TimeScale", &TimeScale_, 0.0f, 3.0f);
				ImGui::EndDisabled();
				if (ImGui::TreeNode("TimeScale Sway (Max <-> Min)")) {
					ImGui::Checkbox("Enabled", &sway.Enabled);
					ImGui::SliderFloat("Max (weight 0)", &sway.Max, 0.0f, 3.0f);
					ImGui::SliderFloat("Min (weight 1)", &sway.Min, 0.0f, 3.0f);
					EditSwayChannel("Channel", sway.Channel, SwayWeight(sway.Channel, SwayClock_));
					ImGui::TreePop();
				}
			}

			if (ImGui::CollapsingHeader("Camera")) {
				auto& sway{ CameraSway_ };
				auto const applyPose{
					[this](CameraPose const& pose_) {
						CameraPitch_ = pose_.Pitch;
						CameraDistance_ = pose_.Distance;
						CameraTargetY_ = pose_.TargetY;
						CameraFovY_ = pose_.FovY;
					}
				};
				auto const currentPose{
					[this]() { return CameraPose{ CameraPitch_, CameraDistance_, CameraTargetY_, CameraFovY_ }; }
				};

				ImGui::Checkbox("AutoOrbit", &AutoOrbit_);
				ImGui::SameLine();
				ImGui::SliderFloat("OrbitSpeed", &OrbitSpeed_, -1.0f, 1.0f);

				if (ImGui::Button("Go Hero")) { applyPose(sway.Hero); }
				ImGui::SameLine();
				if (ImGui::Button("Go Overview")) { applyPose(sway.Overview); }
				ImGui::SameLine();
				if (ImGui::Button("Store as Hero")) { sway.Hero = currentPose(); }
				ImGui::SameLine();
				if (ImGui::Button("Store as Overview")) { sway.Overview = currentPose(); }

				// 往復中のパラメータはスライダーを止める（値は毎フレーム上書きされる）
				bool const swayOn{ sway.Enabled };
				ImGui::SliderFloat("Yaw", &CameraYaw_, -3.1416f, 3.1416f);
				ImGui::BeginDisabled(swayOn && sway.Pitch.Enabled);
				ImGui::SliderFloat("Pitch", &CameraPitch_, -1.4f, 1.4f);
				ImGui::EndDisabled();
				ImGui::BeginDisabled(swayOn && sway.Distance.Enabled);
				ImGui::SliderFloat("Distance", &CameraDistance_, 2.0f, 60.0f);
				ImGui::EndDisabled();
				ImGui::BeginDisabled(swayOn && sway.TargetY.Enabled);
				ImGui::SliderFloat("TargetY", &CameraTargetY_, -5.0f, 15.0f);
				ImGui::EndDisabled();
				ImGui::BeginDisabled(swayOn && sway.FovY.Enabled);
				ImGui::SliderFloat("FovY", &CameraFovY_, 0.2f, 1.6f);
				ImGui::EndDisabled();
				ImGui::ColorEdit3("Background", ClearColor_);

				ImGui::SeparatorText("Sway (Hero <-> Overview)");
				ImGui::Checkbox("Enabled##CameraSway", &sway.Enabled);
				ImGui::SameLine();
				if (ImGui::Button("Reset Clock")) { SwayClock_ = 0.0f; }
				ImGui::SameLine();
				ImGui::TextDisabled("Clock %.1fs  (bar = 0 Hero ... 1 Overview)", SwayClock_);
				EditCameraPose("Hero pose", sway.Hero);
				EditCameraPose("Overview pose", sway.Overview);
				EditSwayChannel("Pitch##Sway", sway.Pitch, SwayWeight(sway.Pitch, SwayClock_));
				EditSwayChannel("Distance##Sway", sway.Distance, SwayWeight(sway.Distance, SwayClock_));
				EditSwayChannel("TargetY##Sway", sway.TargetY, SwayWeight(sway.TargetY, SwayClock_));
				EditSwayChannel("FovY##Sway", sway.FovY, SwayWeight(sway.FovY, SwayClock_));
			}

			if (ImGui::CollapsingHeader("Character")) {
				auto const& names{ Character_.AnimationNames() };
				if (ImGui::BeginCombo("Animation", Character_.CurrentAnimationName().c_str())) {
					for (auto const& name : names) {
						bool const isSelected{ name == Character_.CurrentAnimationName() };
						if (ImGui::Selectable(name.c_str(), isSelected)) {
							Character_.Play(name, true);
						}
						if (isSelected) { ImGui::SetItemDefaultFocus(); }
					}
					ImGui::EndCombo();
				}
				ImGui::SliderFloat("AnimationSpeed", &Character_.AnimationSpeed, 0.0f, 8.0f);
				ImGui::DragFloat3("Position", &Character_.Position.x, 0.01f);
				ImGui::SliderFloat("Yaw##Character", &Character_.Yaw, -3.1416f, 3.1416f);
				ImGui::SliderFloat("Scale", &Character_.Scale, 0.1f, 5.0f);

				ImGui::SeparatorText("Weapon");
				ImGui::Checkbox("ShowWeapon", &Character_.ShowWeapon);
				ImGui::DragFloat3("WeaponScale", &Character_.WeaponScale.x, 0.005f);
				ImGui::DragFloat3("WeaponRotate", &Character_.WeaponRotate.x, 0.01f);
				ImGui::DragFloat3("WeaponOffset", &Character_.WeaponOffset.x, 0.01f);

				ImGui::SeparatorText("Body Material");
				EditMaterial("Body", Character_.BodyMaterial);
				ImGui::SeparatorText("Weapon Material");
				EditMaterial("Weapon", Character_.WeaponMaterial);
			}

			if (ImGui::CollapsingHeader("Lighting")) {
				auto& l{ Lighting_ };

				ImGui::SeparatorText("Fire (under-light)");
				ImGui::Checkbox("Link color/height to pillar", &LinkFireToPillar_);
				ImGui::SliderFloat("FireIntensity", &FireLightIntensity_, 0.0f, 6.0f);
				ImGui::BeginDisabled(LinkFireToPillar_);
				ImGui::ColorEdit3("FireColor", &l.FireColor.x);
				ImGui::SliderFloat("FireHeight", &l.FireHeight, 0.1f, 20.0f);
				ImGui::EndDisabled();
				ImGui::SliderFloat("UnderWeight", &l.UnderWeight, 0.0f, 3.0f);

				ImGui::SeparatorText("Cel");
				ImGui::SliderFloat("CelThreshold", &l.CelThreshold, -1.0f, 1.0f);
				ImGui::SliderFloat("CelSoftness", &l.CelSoftness, 0.001f, 0.5f);
				ImGui::SliderFloat("ShadowLevel", &l.ShadowLevel, 0.0f, 1.0f);

				ImGui::SeparatorText("Ambient (hemisphere)");
				ImGui::ColorEdit3("SkyColor", &l.SkyColor.x);
				ImGui::ColorEdit3("GroundColor", &l.GroundColor.x);
				ImGui::SliderFloat("AmbientIntensity", &l.AmbientIntensity, 0.0f, 2.0f);

				ImGui::SeparatorText("Rim");
				ImGui::ColorEdit3("RimColor", &l.RimColor.x);
				ImGui::SliderFloat("RimIntensity", &l.RimIntensity, 0.0f, 4.0f);
				ImGui::SliderFloat("RimThreshold", &l.RimThreshold, 0.0f, 1.0f);
				ImGui::SliderFloat("RimSoftness", &l.RimSoftness, 0.001f, 0.3f);
			}

			if (ImGui::CollapsingHeader("MagicCircle")) {
				ImGui::SliderFloat("GlobalIntensity", &MagicCircleIntensity_, 0.0f, 4.0f);

				ImGui::SeparatorText("Reveal animation");
				if (ImGui::Button("Replay Reveal")) {
					IsRevealPlaying_ = true;
					RevealTimer_ = 0.0f;
				}
				ImGui::SliderFloat("Duration", &RevealDuration_, 0.05f, 5.0f);
				ImGui::SliderFloat("Stagger", &RevealStagger_, 0.0f, 2.0f);

				ImGui::SeparatorText("Layers");
				if (ImGui::Button("Add Layer")) {
					MagicCircleLayers_.emplace_back();
				}

				int idx_Remove{ -1 };
				for (int idx{ 0 }; idx < static_cast<int>(MagicCircleLayers_.size()); ++idx) {
					auto& layer{ MagicCircleLayers_[static_cast<size_t>(idx)] };
					ImGui::PushID(idx);
					if (ImGui::TreeNode("Layer", "Layer %d  (R %.1f)%s", idx, layer.Radius, layer.Enabled ? "" : "  [off]")) {
						EditMagicCircleLayer(layer);
						if (ImGui::Button("Remove This Layer")) { idx_Remove = idx; }
						ImGui::TreePop();
					}
					ImGui::PopID();
				}
				if (idx_Remove >= 0) {
					MagicCircleLayers_.erase(MagicCircleLayers_.begin() + idx_Remove);
				}
			}

			ImGui::End();
		}

		{
			ImGui::Begin("FirePillar");

			ImGui::TextDisabled("Draw order = list order (outer -> inner). Max %u.", Lumina::FlamePillar::Renderer::MaxInstances);
			if (ImGui::Button("Add Pillar")) {
				FirePillars_.emplace_back();
			}
			ImGui::SameLine();
			if (ImGui::Button("Sort by Radius (outer -> inner)")) {
				std::stable_sort(
					FirePillars_.begin(), FirePillars_.end(),
					[](FirePillarEntry const& lhs_, FirePillarEntry const& rhs_) { return lhs_.Desc.Radius > rhs_.Desc.Radius; }
				);
			}
			ImGui::Text("Age: %.2f", FirePillarAge_);

			// * ループ中に並びを変えないよう、操作は記録だけして最後に適用する
			enum class PillarOp { None, Up, Down, Duplicate, Remove };
			PillarOp op{ PillarOp::None };
			int idx_Op{ -1 };

			for (int idx{ 0 }; idx < static_cast<int>(FirePillars_.size()); ++idx) {
				auto& entry{ FirePillars_[static_cast<size_t>(idx)] };
				ImGui::PushID(idx);

				bool const isOpen{
					ImGui::TreeNode(
						"Pillar", "%d: %s  (R %.2f, H %.1f)%s",
						idx, entry.Name, entry.Desc.Radius, entry.Desc.Height, entry.Enabled ? "" : "  [off]"
					)
				};
				if (isOpen) {
					if (ImGui::ArrowButton("Up", ImGuiDir_Up)) { op = PillarOp::Up; idx_Op = idx; }
					ImGui::SameLine();
					if (ImGui::ArrowButton("Down", ImGuiDir_Down)) { op = PillarOp::Down; idx_Op = idx; }
					ImGui::SameLine();
					if (ImGui::Button("Duplicate")) { op = PillarOp::Duplicate; idx_Op = idx; }
					ImGui::SameLine();
					if (ImGui::Button("Remove")) { op = PillarOp::Remove; idx_Op = idx; }

					EditFirePillarEntry(entry);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			if (idx_Op >= 0) {
				auto const itr{ FirePillars_.begin() + idx_Op };
				switch (op) {
				case PillarOp::Up:
					if (idx_Op > 0) { std::iter_swap(itr, itr - 1); }
					break;
				case PillarOp::Down:
					if (idx_Op + 1 < static_cast<int>(FirePillars_.size())) { std::iter_swap(itr, itr + 1); }
					break;
				case PillarOp::Duplicate:
					{
						FirePillarEntry copied = *itr;
						copied.Seed += 1;
						FirePillars_.insert(itr + 1, copied);
					}
					break;
				case PillarOp::Remove:
					FirePillars_.erase(itr);
					break;
				default:
					break;
				}
			}

			ImGui::End();
		}

		EditParticles();
		EditUI();
		#endif

		//----	------	------	------	------	----//
		// Camera

		// Hero ⇔ Overview の往復（チャンネルごとに別周期。実時間で動く）
		if (CameraSway_.Enabled) {
			auto const& sway{ CameraSway_ };
			auto const apply{
				[this](SwayChannel const& channel_, float hero_, float overview_, float& out_) {
					if (channel_.Enabled) {
						out_ = std::lerp(hero_, overview_, SwayWeight(channel_, SwayClock_));
					}
				}
			};
			apply(sway.Pitch, sway.Hero.Pitch, sway.Overview.Pitch, CameraPitch_);
			apply(sway.Distance, sway.Hero.Distance, sway.Overview.Distance, CameraDistance_);
			apply(sway.TargetY, sway.Hero.TargetY, sway.Overview.TargetY, CameraTargetY_);
			apply(sway.FovY, sway.Hero.FovY, sway.Overview.FovY, CameraFovY_);
		}

		if (AutoOrbit_) {
			CameraYaw_ += OrbitSpeed_ * deltaTime;
			if (CameraYaw_ > std::numbers::pi_v<float>) { CameraYaw_ -= twoPi; }
			if (CameraYaw_ < -std::numbers::pi_v<float>) { CameraYaw_ += twoPi; }
		}

		//----	------	------	------	------	----//
		// FirePillar：エントリから描画用 Instance を作り直す（位置はキャラの足元＋オフセット）
		// Instance::Desc は FirePillars_ の要素を指すので、ここより後で FirePillars_ を増減しないこと

		FirePillarAge_ += deltaTime;

		FirePillarEntry const* lightSource{ nullptr };
		FirePillarInstances_.clear();
		for (auto const& entry : FirePillars_) {
			if (!entry.Enabled) { continue; }
			if (FirePillarInstances_.size() >= Lumina::FlamePillar::Renderer::MaxInstances) { break; }
			if (lightSource == nullptr) { lightSource = &entry; }

			auto& instance{ FirePillarInstances_.emplace_back() };
			instance.Desc = &entry.Desc;
			instance.Position = {
				Character_.Position.x + entry.Offset.x,
				Character_.Position.y + entry.Offset.y,
				Character_.Position.z + entry.Offset.z,
			};
			instance.Age = FirePillarAge_;
			instance.Seed = entry.Seed;
			instance.IntensityScale = entry.IntensityScale;
		}

		//----	------	------	------	------	----//
		// Character / Lighting

		Character_.Advance(deltaTime);

		Lighting_.FireBase = Character_.Position;
		Lighting_.FireIntensity = FireLightIntensity_;
		if (LinkFireToPillar_ && lightSource != nullptr) {
			Lighting_.FireColor = lightSource->Desc.Ramp.Mid;
			Lighting_.FireHeight = lightSource->Desc.Height;
		}

		//----	------	------	------	------	----//
		// MagicCircle

		if (IsRevealPlaying_) {
			RevealTimer_ += deltaTime;

			bool isFinished{ true };
			float const duration{ std::max(RevealDuration_, 0.001f) };
			for (size_t idx{ 0U }; idx < MagicCircleLayers_.size(); ++idx) {
				float const t{ (RevealTimer_ - RevealStagger_ * static_cast<float>(idx)) / duration };
				float const reveal{ std::clamp(t, 0.0f, 1.0f) };
				// 終盤で減速する（描き終わりの「止め」）
				MagicCircleLayers_[idx].Reveal = 1.0f - (1.0f - reveal) * (1.0f - reveal);
				if (reveal < 1.0f) { isFinished = false; }
			}
			if (isFinished) { IsRevealPlaying_ = false; }
		}

		//----	------	------	------	------	----//
		// Particles（ビルボード用にここでもカメラを作る。Render でも同じものを作り直す）

		{
			Lumina::Mat4 view{};
			Lumina::Mat4 viewProj{};
			Lumina::Vec3 eye{};
			BuildCamera(view, viewProj, eye);
			UpdateParticles(cmdList_, deltaTime, view, viewProj);
		}

		//----	------	------	------	------	----//
		// Menu UI（Pause / TimeScale の影響を受けない）

		UpdateUI(input_);
	}
}

namespace Game::Scene {
	template<>
	void Title::Update(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::WinApp::RawInput const& input_
	) {
		Impl_->Update(dxContext_, cmdList_, input_);
	}
}
