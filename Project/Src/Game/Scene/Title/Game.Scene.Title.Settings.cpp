module Game.Scene.Title;

import <cstdint>;
import <cmath>;
import <string>;
import <vector>;
import <fstream>;
import <concepts>;
import <type_traits>;

import nlohmann.json;

import : Impl;

import Lumina;
import Lumina.FirePillar;
import Lumina.MagicCircle;
import Lumina.ShowcaseCharacter;

//////	//////	//////	//////	//////	//////
//	Title の調整結果の保存・読み込み		//
//////	//////	//////	//////	//////	//////
// 書き出しと読み込みで同じ Visit 関数を通す（キーの書き間違いで片方だけずれるのを防ぐ）。
// 読み込みは「ファイルにあるキーだけ上書き」なので、後からパラメータを足しても古いファイルが読める。
// 配列（火柱・魔法陣レイヤー）はキーがあれば丸ごと置き換える。

namespace Game::Scene::Impl {
	namespace {
		using JSON = nlohmann::json;

		// 0.6f → 0.6000000238… のような表記を避ける（差分を読みやすく）
		auto Round(float v_) -> double {
			return std::round(static_cast<double>(v_) * 1e6) / 1e6;
		}

		//----	------	------	------	------	----//
		// Writer
		//----	------	------	------	------	----//

		class Writer {
		public:
			explicit Writer(JSON& json_) : J_(json_) {}

			void operator()(char const* key_, float value_) { J_[key_] = Round(value_); }
			void operator()(char const* key_, int value_) { J_[key_] = value_; }
			void operator()(char const* key_, uint32_t value_) { J_[key_] = value_; }
			void operator()(char const* key_, bool value_) { J_[key_] = value_; }
			void operator()(char const* key_, Lumina::Float3 const& value_) {
				Floats(key_, &value_.x, 3U);
			}

			void Floats(char const* key_, float const* values_, size_t count_) {
				JSON arr = JSON::array();
				for (size_t idx{ 0U }; idx < count_; ++idx) {
					arr.push_back(Round(values_[idx]));
				}
				J_[key_] = std::move(arr);
			}

			template<size_t N>
			void Chars(char const* key_, char const (&value_)[N]) {
				J_[key_] = std::string{ value_ };
			}

			template<typename T, typename Func>
			void Object(char const* key_, T& value_, Func&& func_) {
				JSON child = JSON::object();
				Writer sub(child);
				func_(sub, value_);
				J_[key_] = std::move(child);
			}

			template<typename T, typename Func>
			void Array(char const* key_, std::vector<T> const& values_, Func&& func_) {
				JSON arr = JSON::array();
				for (auto const& value : values_) {
					JSON child = JSON::object();
					Writer sub(child);
					func_(sub, value);
					arr.push_back(std::move(child));
				}
				J_[key_] = std::move(arr);
			}

		private:
			JSON& J_;
		};

		//----	------	------	------	------	----//
		// Reader（キーが無い・型が違うときは何もしない）
		//----	------	------	------	------	----//

		class Reader {
		public:
			explicit Reader(JSON const& json_) : J_(json_) {}

			void operator()(char const* key_, float& value_) const {
				if (JSON const* node{ Find(key_) }; node && node->is_number()) {
					value_ = node->get<float>();
				}
			}
			void operator()(char const* key_, int& value_) const {
				if (JSON const* node{ Find(key_) }; node && node->is_number_integer()) {
					value_ = node->get<int>();
				}
			}
			void operator()(char const* key_, uint32_t& value_) const {
				if (JSON const* node{ Find(key_) }; node && node->is_number_integer()) {
					value_ = node->get<uint32_t>();
				}
			}
			void operator()(char const* key_, bool& value_) const {
				if (JSON const* node{ Find(key_) }; node && node->is_boolean()) {
					value_ = node->get<bool>();
				}
			}
			void operator()(char const* key_, Lumina::Float3& value_) const {
				Floats(key_, &value_.x, 3U);
			}

			void Floats(char const* key_, float* values_, size_t count_) const {
				JSON const* node{ Find(key_) };
				if (!node || !node->is_array() || node->size() != count_) { return; }
				for (size_t idx{ 0U }; idx < count_; ++idx) {
					if (!(*node)[idx].is_number()) { return; }
				}
				for (size_t idx{ 0U }; idx < count_; ++idx) {
					values_[idx] = (*node)[idx].get<float>();
				}
			}

			template<size_t N>
			void Chars(char const* key_, char (&value_)[N]) const {
				if (JSON const* node{ Find(key_) }; node && node->is_string()) {
					std::string const str{ node->get<std::string>() };
					size_t const length{ str.copy(value_, N - 1U) };
					value_[length] = '\0';
				}
			}

			template<typename T, typename Func>
			void Object(char const* key_, T& value_, Func&& func_) const {
				if (JSON const* node{ Find(key_) }; node && node->is_object()) {
					Reader sub(*node);
					func_(sub, value_);
				}
			}

			template<typename T, typename Func>
			void Array(char const* key_, std::vector<T>& values_, Func&& func_) const {
				JSON const* node{ Find(key_) };
				if (!node || !node->is_array()) { return; }

				values_.clear();
				for (size_t idx{ 0U }; idx < node->size(); ++idx) {
					JSON const& child{ (*node)[idx] };
					if (!child.is_object()) { continue; }
					T& value{ values_.emplace_back() };	// 既定値から始めて、あるキーだけ上書き
					Reader sub(child);
					func_(sub, value);
				}
			}

		private:
			JSON const* Find(char const* key_) const {
				if (!J_.is_object()) { return nullptr; }
				auto const itr{ J_.find(key_) };
				return (itr != J_.end()) ? &(*itr) : nullptr;
			}

			JSON const& J_;
		};

		//----	------	------	------	------	----//
		// 型ごとのキー一覧（T は const でも非 const でもよい）
		//----	------	------	------	------	----//

		template<typename T, typename U>
		concept Is = std::same_as<std::remove_const_t<T>, U>;

		template<typename Ar, typename T> requires Is<T, Lumina::NoiseLayer>
		void Visit(Ar& ar_, T& v_) {
			ar_("ScaleU", v_.ScaleU);
			ar_("ScaleV", v_.ScaleV);
			ar_("ScrollSpeed", v_.ScrollSpeed);
			ar_("WarpAmount", v_.WarpAmount);
		}

		template<typename Ar, typename T> requires Is<T, Lumina::ColorRamp>
		void Visit(Ar& ar_, T& v_) {
			ar_("Low", v_.Low);
			ar_("Mid", v_.Mid);
			ar_("High", v_.High);
			ar_("MidPosition", v_.MidPosition);
		}

		template<typename Ar, typename T> requires Is<T, Lumina::FlamePillar::Description>
		void Visit(Ar& ar_, T& v_) {
			ar_("Radius", v_.Radius);
			ar_("Height", v_.Height);
			ar_("TopScale", v_.TopScale);
			ar_("ShapeCurve", v_.ShapeCurve);

			ar_("WobbleAmp", v_.WobbleAmp);
			ar_("WobbleFreq", v_.WobbleFreq);
			ar_("WobbleWave", v_.WobbleWave);
			ar_("Twist", v_.Twist);

			ar_.Object("Layer", v_.Layer, [](auto& a_, auto& x_) { Visit(a_, x_); });
			ar_.Object("WarpLayer", v_.WarpLayer, [](auto& a_, auto& x_) { Visit(a_, x_); });

			ar_("CutBottom", v_.CutBottom);
			ar_("CutTop", v_.CutTop);
			ar_("CutCurve", v_.CutCurve);
			ar_("Softness", v_.Softness);

			ar_("FadeBottom", v_.FadeBottom);
			ar_("FadeTopStart", v_.FadeTopStart);
			ar_("RimPower", v_.RimPower);
			ar_("BackDarken", v_.BackDarken);

			ar_("Intensity", v_.Intensity);
			ar_("Opacity", v_.Opacity);
			ar_("Additive", v_.Additive);
			ar_("TextureColor", v_.TextureColor);
			ar_("HeatScale", v_.HeatScale);
			ar_("HeatHeight", v_.HeatHeight);
			ar_.Object("Ramp", v_.Ramp, [](auto& a_, auto& x_) { Visit(a_, x_); });
		}

		template<typename Ar, typename T> requires Is<T, FirePillarEntry>
		void Visit(Ar& ar_, T& v_) {
			ar_.Chars("Name", v_.Name);
			ar_("Enabled", v_.Enabled);
			ar_("Offset", v_.Offset);
			ar_("Seed", v_.Seed);
			ar_("IntensityScale", v_.IntensityScale);
			ar_.Object("Desc", v_.Desc, [](auto& a_, auto& x_) { Visit(a_, x_); });
		}

		template<typename Ar, typename T> requires Is<T, Lumina::MagicCircle::Layer>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Elements", v_.Elements);

			ar_("Radius", v_.Radius);
			ar_("Height", v_.Height);
			ar_("RotateSpeed", v_.RotateSpeed);
			ar_("Phase", v_.Phase);
			ar_("Reveal", v_.Reveal);

			ar_("Color", v_.Color);
			ar_("Intensity", v_.Intensity);
			ar_("LineWidth", v_.LineWidth);
			ar_("GlowAmount", v_.GlowAmount);
			ar_("GlowWidth", v_.GlowWidth);
			ar_("FillAmount", v_.FillAmount);

			ar_("Ring0", v_.Ring0);
			ar_("Ring1", v_.Ring1);
			ar_("PolygonVertices", v_.PolygonVertices);
			ar_("PolygonStep", v_.PolygonStep);
			ar_("PolygonRadius", v_.PolygonRadius);
			ar_("NodeRadius", v_.NodeRadius);
			ar_("TickCount", v_.TickCount);
			ar_("TickInner", v_.TickInner);
			ar_("TickOuter", v_.TickOuter);
			ar_("SpokeCount", v_.SpokeCount);
			ar_("SpokeInner", v_.SpokeInner);
			ar_("SpokeOuter", v_.SpokeOuter);
			ar_("RuneCount", v_.RuneCount);
			ar_("RuneInner", v_.RuneInner);
			ar_("RuneOuter", v_.RuneOuter);
			ar_("RuneSeed", v_.RuneSeed);
			ar_("RuneDensity", v_.RuneDensity);
			ar_("RuneShimmer", v_.RuneShimmer);
		}

		template<typename Ar, typename T> requires Is<T, Lumina::ShowcaseMaterial>
		void Visit(Ar& ar_, T& v_) {
			ar_("BaseColor", v_.BaseColor);
			ar_("EmissiveColor", v_.EmissiveColor);
			ar_("EmissiveIntensity", v_.EmissiveIntensity);
		}

		template<typename Ar, typename T> requires Is<T, Lumina::ShowcaseLighting>
		void Visit(Ar& ar_, T& v_) {
			// FireBase は毎フレームキャラ位置から決めるので保存しない
			ar_("FireColor", v_.FireColor);
			ar_("FireIntensity", v_.FireIntensity);
			ar_("FireHeight", v_.FireHeight);
			ar_("UnderWeight", v_.UnderWeight);

			ar_("SkyColor", v_.SkyColor);
			ar_("GroundColor", v_.GroundColor);
			ar_("AmbientIntensity", v_.AmbientIntensity);

			ar_("RimColor", v_.RimColor);
			ar_("RimIntensity", v_.RimIntensity);
			ar_("RimThreshold", v_.RimThreshold);
			ar_("RimSoftness", v_.RimSoftness);

			ar_("CelThreshold", v_.CelThreshold);
			ar_("CelSoftness", v_.CelSoftness);
			ar_("ShadowLevel", v_.ShadowLevel);
		}

		template<typename Ar, typename T> requires Is<T, Lumina::ShowcaseCharacter>
		void Visit(Ar& ar_, T& v_) {
			// 再生中のアニメ名は Save/Load 側で別に扱う（Play を通す必要があるため）
			ar_("AnimationSpeed", v_.AnimationSpeed);
			ar_("Position", v_.Position);
			ar_("Yaw", v_.Yaw);
			ar_("Scale", v_.Scale);

			ar_("ShowWeapon", v_.ShowWeapon);
			ar_("WeaponScale", v_.WeaponScale);
			ar_("WeaponRotate", v_.WeaponRotate);
			ar_("WeaponOffset", v_.WeaponOffset);

			ar_.Object("BodyMaterial", v_.BodyMaterial, [](auto& a_, auto& x_) { Visit(a_, x_); });
			ar_.Object("WeaponMaterial", v_.WeaponMaterial, [](auto& a_, auto& x_) { Visit(a_, x_); });
		}

		template<typename Ar, typename T> requires Is<T, SparkleEmitter>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Rate", v_.Rate);
			ar_("AreaRadius", v_.AreaRadius);
			ar_("HeightMax", v_.HeightMax);
			ar_("Size", v_.Size);
			ar_("Alpha", v_.Alpha);
		}

		template<typename Ar, typename T> requires Is<T, AuraEmitter>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Rate", v_.Rate);
			ar_("OrbitRadius", v_.OrbitRadius);
			ar_("OrbitHeight", v_.OrbitHeight);
			ar_("HeightOffset", v_.HeightOffset);
			ar_("Size", v_.Size);
			ar_("Alpha", v_.Alpha);
			ar_("HueCycle", v_.HueCycle);
			ar_("Color", v_.Color);
		}

		template<typename Ar, typename T> requires Is<T, EmberEmitter>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Rate", v_.Rate);
			ar_("LifeMin", v_.LifeMin);
			ar_("LifeMax", v_.LifeMax);
			ar_("SpawnRadius", v_.SpawnRadius);
			ar_("SpawnRadiusJitter", v_.SpawnRadiusJitter);
			ar_("SpawnHeight", v_.SpawnHeight);
			ar_("RiseSpeed", v_.RiseSpeed);
			ar_("RiseJitter", v_.RiseJitter);
			ar_("Swirl", v_.Swirl);
			ar_("Turbulence", v_.Turbulence);
			ar_("Drag", v_.Drag);
			ar_("Size", v_.Size);
			ar_("Stretch", v_.Stretch);
			ar_("Alpha", v_.Alpha);
			ar_("ColorHot", v_.ColorHot);
			ar_("ColorCool", v_.ColorCool);
			ar_("Atlas", v_.Atlas);
		}

		template<typename Ar, typename T> requires Is<T, CircleLightEmitter>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Rate", v_.Rate);
			ar_("LifeMin", v_.LifeMin);
			ar_("LifeMax", v_.LifeMax);
			ar_("RiseSpeed", v_.RiseSpeed);
			ar_("RiseJitter", v_.RiseJitter);
			ar_("Sway", v_.Sway);
			ar_("Size", v_.Size);
			ar_("Stretch", v_.Stretch);
			ar_("Alpha", v_.Alpha);
			ar_("Whiten", v_.Whiten);
			ar_("RespectReveal", v_.RespectReveal);
			ar_("Atlas", v_.Atlas);
		}

		template<typename Ar, typename T> requires Is<T, CameraPose>
		void Visit(Ar& ar_, T& v_) {
			ar_("Pitch", v_.Pitch);
			ar_("Distance", v_.Distance);
			ar_("TargetY", v_.TargetY);
			ar_("FovY", v_.FovY);
		}

		template<typename Ar, typename T> requires Is<T, SwayChannel>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Period", v_.Period);
			ar_("Phase", v_.Phase);
			ar_("Dwell", v_.Dwell);
		}

		template<typename Ar, typename T> requires Is<T, CameraSway>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_.Object("Hero", v_.Hero, [](auto& a_, auto& x_) { Visit(a_, x_); });
			ar_.Object("Overview", v_.Overview, [](auto& a_, auto& x_) { Visit(a_, x_); });
			ar_.Object("Pitch", v_.Pitch, [](auto& a_, auto& x_) { Visit(a_, x_); });
			ar_.Object("Distance", v_.Distance, [](auto& a_, auto& x_) { Visit(a_, x_); });
			ar_.Object("TargetY", v_.TargetY, [](auto& a_, auto& x_) { Visit(a_, x_); });
			ar_.Object("FovY", v_.FovY, [](auto& a_, auto& x_) { Visit(a_, x_); });
		}

		template<typename Ar, typename T> requires Is<T, TimeScaleSway>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Min", v_.Min);
			ar_("Max", v_.Max);
			ar_.Object("Channel", v_.Channel, [](auto& a_, auto& x_) { Visit(a_, x_); });
		}

		template<typename Ar, typename T> requires Is<T, TitleMenuStyle>
		void Visit(Ar& ar_, T& v_) {
			ar_("RightX", v_.RightX);
			ar_("FirstY", v_.FirstY);
			ar_("Spacing", v_.Spacing);
			ar_("Width", v_.Width);
			ar_("Height", v_.Height);
			ar_("UnselectedAlpha", v_.UnselectedAlpha);
			ar_("UnselectedScale", v_.UnselectedScale);
			ar_("SelectedOffsetX", v_.SelectedOffsetX);
			ar_("Smoothing", v_.Smoothing);
			ar_("FadeInDuration", v_.FadeInDuration);
			ar_("BlinkDuration", v_.BlinkDuration);
			ar_("BlinkFrequency", v_.BlinkFrequency);
			ar_("FadeOutDuration", v_.FadeOutDuration);
		}

		template<typename Ar, typename T> requires Is<T, TitleLogoStyle>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("X", v_.X);
			ar_("Y", v_.Y);
			ar_("AnchorX", v_.AnchorX);
			ar_("AnchorY", v_.AnchorY);
			ar_("Width", v_.Width);
			ar_("Height", v_.Height);
			ar_("Alpha", v_.Alpha);
			ar_("Delay", v_.Delay);
			ar_("FadeDuration", v_.FadeDuration);
			ar_("RiseDistance", v_.RiseDistance);
		}

		template<typename Ar, typename T> requires Is<T, WeaponGlowEmitter>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Rate", v_.Rate);
			ar_("LifeMin", v_.LifeMin);
			ar_("LifeMax", v_.LifeMax);
			ar_("SpanBegin", v_.SpanBegin);
			ar_("SpanEnd", v_.SpanEnd);
			ar_("RadialJitter", v_.RadialJitter);
			ar_("SizeMin", v_.SizeMin);
			ar_("SizeMax", v_.SizeMax);
			ar_("Grow", v_.Grow);
			ar_("Alpha", v_.Alpha);
			ar_("HueFlow", v_.HueFlow);
			ar_("Color", v_.Color);
			ar_("HueBase", v_.HueBase);
			ar_("HueRange", v_.HueRange);
			ar_("HueSpeed", v_.HueSpeed);
			ar_("Saturation", v_.Saturation);
			ar_("Value", v_.Value);
			ar_("Atlas", v_.Atlas);
		}

		template<typename Ar, typename T> requires Is<T, TitleGlowStyle>
		void Visit(Ar& ar_, T& v_) {
			ar_("Enabled", v_.Enabled);
			ar_("Color", v_.Color);
			ar_("Intensity", v_.Intensity);
			ar_("RadiusPx", v_.RadiusPx);
			ar_("PulseDepth", v_.PulseDepth);
			ar_("PulseFrequency", v_.PulseFrequency);
			ar_("PhaseStep", v_.PhaseStep);
		}
	}

	//////	//////	//////	//////	//////	//////

	template<typename Archive, typename Self>
	void Title::VisitSettings(Archive& ar_, Self& self_) {
		ar_("TimeScale", self_.TimeScale_);
		ar_.Object("TimeScaleSway", self_.TimeScaleSway_, [](auto& a_, auto& x_) { Visit(a_, x_); });
		ar_.Object("CameraSway", self_.CameraSway_, [](auto& a_, auto& x_) { Visit(a_, x_); });
		ar_.Object("Menu", self_.MenuStyle_, [](auto& a_, auto& x_) { Visit(a_, x_); });
		ar_.Object("Logo", self_.LogoStyle_, [](auto& a_, auto& x_) { Visit(a_, x_); });
		ar_.Object("LabelGlow", self_.GlowStyle_, [](auto& a_, auto& x_) { Visit(a_, x_); });

		ar_.Object("Camera", self_, [](auto& a_, auto& s_) {
			a_("AutoOrbit", s_.AutoOrbit_);
			a_("OrbitSpeed", s_.OrbitSpeed_);
			a_("Yaw", s_.CameraYaw_);
			a_("Pitch", s_.CameraPitch_);
			a_("Distance", s_.CameraDistance_);
			a_("TargetY", s_.CameraTargetY_);
			a_("FovY", s_.CameraFovY_);
			a_.Floats("ClearColor", s_.ClearColor_, 4U);
		});

		ar_.Object("Character", self_.Character_, [](auto& a_, auto& x_) { Visit(a_, x_); });

		ar_.Object("Lighting", self_, [](auto& a_, auto& s_) {
			a_("LinkFireToPillar", s_.LinkFireToPillar_);
			a_("FireLightIntensity", s_.FireLightIntensity_);
			a_.Object("Params", s_.Lighting_, [](auto& b_, auto& x_) { Visit(b_, x_); });
		});

		ar_.Array("FirePillars", self_.FirePillars_, [](auto& a_, auto& x_) { Visit(a_, x_); });

		ar_.Object("MagicCircle", self_, [](auto& a_, auto& s_) {
			a_("Intensity", s_.MagicCircleIntensity_);
			a_("RevealDuration", s_.RevealDuration_);
			a_("RevealStagger", s_.RevealStagger_);
			a_.Array("Layers", s_.MagicCircleLayers_, [](auto& b_, auto& x_) { Visit(b_, x_); });
		});

		ar_.Object("Particles", self_, [](auto& a_, auto& s_) {
			a_.Object("Sparkles", s_.SparkleParams_, [](auto& b_, auto& x_) { Visit(b_, x_); });
			a_.Object("Aura", s_.AuraParams_, [](auto& b_, auto& x_) { Visit(b_, x_); });
			a_.Object("Embers", s_.EmberParams_, [](auto& b_, auto& x_) { Visit(b_, x_); });
			a_.Object("CircleLights", s_.CircleLightParams_, [](auto& b_, auto& x_) { Visit(b_, x_); });
			a_.Object("WeaponGlow", s_.WeaponGlowParams_, [](auto& b_, auto& x_) { Visit(b_, x_); });
		});
	}

	bool Title::SaveSettings() const {
		JSON root = JSON::object();
		Writer ar(root);
		VisitSettings(ar, *this);

		root["Character"]["Animation"] = Character_.CurrentAnimationName();

		std::ofstream ofs{ SettingsFilePath, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { return false; }
		ofs << root.dump(2) << '\n';
		return ofs.good();
	}

	bool Title::LoadSettings() {
		std::ifstream ifs{ SettingsFilePath };
		if (!ifs.is_open()) { return false; }

		// 例外なしでパース（壊れていたら何もしない）
		JSON const root = JSON::parse(ifs, nullptr, false);
		if (root.is_discarded() || !root.is_object()) { return false; }

		Reader ar(root);
		VisitSettings(ar, *this);

		auto const itr_Character{ root.find("Character") };
		if (itr_Character != root.end() && itr_Character->is_object()) {
			auto const itr_Animation{ itr_Character->find("Animation") };
			if (itr_Animation != itr_Character->end() && itr_Animation->is_string()) {
				Character_.Play(itr_Animation->get<std::string>(), true);
			}
		}

		// 描画用 Instance は Update で作り直されるので、ここでは古いポインタを捨てるだけ
		FirePillarInstances_.clear();
		return true;
	}
}
