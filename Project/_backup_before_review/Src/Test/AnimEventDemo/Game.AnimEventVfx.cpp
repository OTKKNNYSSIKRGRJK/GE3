module Game.AnimEventVfx;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれも Game.AnimEventVfx.ixx で import 済み
import <algorithm>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <span>;
import <string>;
import <vector>;

import nlohmann.json;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.RadialRays;
import Lumina.AnimEvent;
import Game.AnimEventDemo;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

//////	//////	//////	//////	//////	//////

namespace Game::AnimEventVfx {
	namespace AE = Lumina::AnimEvent;
	using JSON = nlohmann::json;

	namespace {
		constexpr AE::EventTypeId EffectTipRays{ AE::TypeIdOf("TipRays") };
		constexpr AE::EventTypeId SocketWeaponTip{ AE::TypeIdOf("WeaponTip") };

		auto SRGBToLinear(float c_) -> float {
			return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
		}
		auto SRGBToLinear(Lumina::Float3 const& c_) -> Lumina::Float3 {
			return { SRGBToLinear(c_.x), SRGBToLinear(c_.y), SRGBToLinear(c_.z) };
		}

		auto Saturate(float x_) -> float { return std::clamp(x_, 0.0f, 1.0f); }

		// 窓の年齢 → 強さ（0..1）と、消え始めてからの進み具合（0..1）
		struct Envelope {
			float Rise{ 0.0f };		// 立ち上がり（ease-out cubic）
			float Fall{ 1.0f };		// 消え方（(1 - r)^2）
			float Released{ 0.0f };	// 消え始めてからの進み r
		};

		auto Evaluate(TipRaysStyle const& style_, Game::AnimEventDemo::VfxTrack const& track_) -> Envelope {
			// プレビューは窓の開閉をそのまま見せる（開いている間は最大）
			if (track_.Preview) { return { 1.0f, 1.0f, 0.0f }; }

			float const a{ (style_.Attack > 0.0f) ? Saturate(track_.OpenAge / style_.Attack) : 1.0f };
			float const rise{ 1.0f - (1.0f - a) * (1.0f - a) * (1.0f - a) };
			if (track_.ClosedAge < 0.0f) { return { rise, 1.0f, 0.0f }; }

			float const duration{ (track_.Reason == AE::EndReason::Completed) ? style_.Release : style_.ReleaseFast };
			float const r{ (duration > 0.0f) ? Saturate(track_.ClosedAge / duration) : 1.0f };
			return { rise, (1.0f - r) * (1.0f - r), r };
		}

		//----	保存・読み込みの小道具（例外を使わない）	----//

		auto ToJSON(Lumina::Float3 const& v_) -> JSON { return JSON::array({ v_.x, v_.y, v_.z }); }

		void Read(JSON const& obj_, char const* key_, float& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = itr->get<float>(); }
		}
		void Read(JSON const& obj_, char const* key_, int& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number_integer()) { out_ = itr->get<int>(); }
		}
		void Read(JSON const& obj_, char const* key_, bool& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_boolean()) { out_ = itr->get<bool>(); }
		}
		void Read(JSON const& obj_, char const* key_, Lumina::Float3& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr == obj_.end() || !itr->is_array() || itr->size() != 3U) { return; }
			for (auto const& e : *itr) { if (!e.is_number()) { return; } }
			out_ = { (*itr)[0].get<float>(), (*itr)[1].get<float>(), (*itr)[2].get<float>() };
		}
	}

	//====	======	======	======	======	====//
	//	初期化									//
	//====	======	======	======	======	====//

	void VfxPresenter::Initialize(
		Lumina::DX12::Context const& dxContext_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_
	) {
		Rays_.Initialize(dxContext_, rtvFormat_, dsvFormat_, 1280.0f / 720.0f);
		RaysInstances_.reserve(Lumina::RadialRays::Renderer::MaxInstances);
		Load();		// ファイルが無ければ初期値のまま
	}

	//====	======	======	======	======	====//
	//	毎フレーム								//
	//====	======	======	======	======	====//

	void VfxPresenter::Prepare(
		std::span<Game::AnimEventDemo::VfxTrack const> tracks_,
		Lumina::Mat4 const& weaponWorld_,
		Lumina::Mat4 const& viewProj_
	) {
		RaysInstances_.clear();
		NumTracks_ = static_cast<std::uint32_t>(tracks_.size());
		ShownIntensity_ = 0.0f;
		ShownLength_ = 0.0f;

		// * ソケット：武器メッシュのローカル → ワールド → 画面 UV（カメラの後ろなら出さない）
		{
			TipRaysStyle const& s{ TipRays };
			Lumina::Vec4 const world{ Lumina::Vec4{ s.SocketOffset.x, s.SocketOffset.y, s.SocketOffset.z, 1.0f } * weaponWorld_ };
			Lumina::Vec4 const clip{ Lumina::Vec4{ world.x, world.y, world.z, 1.0f } * viewProj_ };
			IsTipOnScreen_ = (clip.w > 1e-4f);
			if (IsTipOnScreen_) {
				TipUV_ = { (clip.x / clip.w) * 0.5f + 0.5f, 0.5f - (clip.y / clip.w) * 0.5f };
			}
		}

		if (!TipRays.Enabled || !IsTipOnScreen_) { return; }

		TipRaysStyle const& s{ TipRays };
		Lumina::RadialRays::Look const look{
			.RayCount{ s.RayCount },
			.Sharpness{ s.Sharpness },
			.RotateSpeed{ s.RotateSpeed },
			.FlickerSpeed{ s.FlickerSpeed },
			.Color{ SRGBToLinear(s.Color) },
			.CoreColor{ SRGBToLinear(s.CoreColor) },
			.CoreRadius{ s.CoreRadius },
			.CoreIntensity{ s.CoreIntensity },
		};

		for (Game::AnimEventDemo::VfxTrack const& t : tracks_) {
			if (t.Payload.Effect != EffectTipRays || t.Payload.Socket != SocketWeaponTip) { continue; }
			if (RaysInstances_.size() >= Lumina::RadialRays::Renderer::MaxInstances) { break; }

			Envelope const env{ Evaluate(s, t) };
			float const intensity{ s.Intensity * t.Payload.Scale * env.Rise * env.Fall };
			if (!(intensity > 0.0f)) { continue; }

			float const length{
				s.LengthStart + (s.LengthPeak - s.LengthStart) * env.Rise +
				(s.LengthAfter - s.LengthPeak) * env.Released
			};

			// 模様の時間は窓の年齢から（閉じた後も流れ続ける）。トラックごとにずらして、続けて出たときに模様が揃わないようにする
			float const age{ t.OpenAge + std::max(t.ClosedAge, 0.0f) };
			RaysInstances_.push_back({
				.Center{ TipUV_ },
				.Intensity{ intensity },
				.Length{ length },
				.Time{ age + static_cast<float>(t.Serial % 64U) * 13.7f },
				.Appearance{ look },
			});

			ShownIntensity_ = std::max(ShownIntensity_, intensity);
			ShownLength_ = std::max(ShownLength_, length);
		}
	}

	void VfxPresenter::Render(Lumina::DX12::CommandList const& cmdList_) {
		if (RaysInstances_.empty()) { return; }
		Rays_.Render(cmdList_, RaysInstances_);
	}

	//====	======	======	======	======	====//
	//	保存・読み込み							//
	//====	======	======	======	======	====//

	auto VfxPresenter::Load() -> bool {
		std::ifstream ifs{ ConfigPath };
		if (!ifs.is_open()) { Status_ = "No config file (using defaults)."; return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { Status_ = "Config is not valid JSON."; return false; }

		// あるキーだけ上書きする
		auto const itr{ root.find("TipRays") };
		if (itr != root.end() && itr->is_object()) {
			JSON const& j{ *itr };
			TipRaysStyle& s{ TipRays };
			Read(j, "Enabled", s.Enabled);
			Read(j, "SocketOffset", s.SocketOffset);
			Read(j, "Attack", s.Attack);
			Read(j, "Release", s.Release);
			Read(j, "ReleaseFast", s.ReleaseFast);
			Read(j, "Intensity", s.Intensity);
			Read(j, "LengthStart", s.LengthStart);
			Read(j, "LengthPeak", s.LengthPeak);
			Read(j, "LengthAfter", s.LengthAfter);
			Read(j, "RayCount", s.RayCount);
			Read(j, "Sharpness", s.Sharpness);
			Read(j, "RotateSpeed", s.RotateSpeed);
			Read(j, "FlickerSpeed", s.FlickerSpeed);
			Read(j, "Color", s.Color);
			Read(j, "CoreColor", s.CoreColor);
			Read(j, "CoreRadius", s.CoreRadius);
			Read(j, "CoreIntensity", s.CoreIntensity);
		}
		Status_ = "Loaded.";
		return true;
	}

	auto VfxPresenter::Save() -> bool {
		TipRaysStyle const& s{ TipRays };
		JSON root = JSON::object();
		root["TipRays"] = JSON{
			{ "Enabled", s.Enabled },
			{ "SocketOffset", ToJSON(s.SocketOffset) },
			{ "Attack", s.Attack },
			{ "Release", s.Release },
			{ "ReleaseFast", s.ReleaseFast },
			{ "Intensity", s.Intensity },
			{ "LengthStart", s.LengthStart },
			{ "LengthPeak", s.LengthPeak },
			{ "LengthAfter", s.LengthAfter },
			{ "RayCount", s.RayCount },
			{ "Sharpness", s.Sharpness },
			{ "RotateSpeed", s.RotateSpeed },
			{ "FlickerSpeed", s.FlickerSpeed },
			{ "Color", ToJSON(s.Color) },
			{ "CoreColor", ToJSON(s.CoreColor) },
			{ "CoreRadius", s.CoreRadius },
			{ "CoreIntensity", s.CoreIntensity },
		};

		std::ofstream ofs{ ConfigPath, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { Status_ = "Save failed (cannot open file)."; return false; }
		ofs << root.dump(2) << '\n';
		if (!ofs.good()) { Status_ = "Save failed (write error)."; return false; }
		Status_ = "Saved.";
		return true;
	}

	//====	======	======	======	======	====//
	//	エディタ（Debug のみ）					//
	//====	======	======	======	======	====//

	void VfxPresenter::DrawEditor() {
#if defined(_DEBUG)
		if (!ImGui::Begin("AnimEvent VFX")) {
			ImGui::End();
			return;
		}

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { TipRays = TipRaysStyle{}; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		ImGui::SeparatorText("Status");
		ImGui::Text("VfxLoop tracks %u / rays drawn %u", NumTracks_, static_cast<std::uint32_t>(RaysInstances_.size()));
		if (IsTipOnScreen_) {
			ImGui::Text("WeaponTip uv (%.3f, %.3f)   intensity %.2f   length %.2f", TipUV_.x, TipUV_.y, ShownIntensity_, ShownLength_);
		}
		else {
			ImGui::TextDisabled("WeaponTip is behind the camera.");
		}

		TipRaysStyle& s{ TipRays };
		ImGui::PushID("TipRays");
		ImGui::SeparatorText("TipRays");
		ImGui::Checkbox("Enabled", &s.Enabled);

		ImGui::SeparatorText("Socket: WeaponTip (weapon mesh local)");
		ImGui::DragFloat3("Offset", &s.SocketOffset.x, 0.01f);

		ImGui::SeparatorText("Envelope (seconds, real time)");
		float const maxRelease{ Game::AnimEventDemo::PlayerAnimEvents::VfxLinger - 0.05f };
		ImGui::DragFloat("Attack", &s.Attack, 0.005f, 0.0f, 1.0f, "%.3f");
		ImGui::DragFloat("Release (Completed)", &s.Release, 0.005f, 0.0f, maxRelease, "%.3f");
		ImGui::DragFloat("Release (Interrupted)", &s.ReleaseFast, 0.005f, 0.0f, maxRelease, "%.3f");
		ImGui::DragFloat("Intensity", &s.Intensity, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Length start", &s.LengthStart, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::DragFloat("Length peak", &s.LengthPeak, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::DragFloat("Length after", &s.LengthAfter, 0.01f, 0.0f, 4.0f, "%.2f");

		ImGui::SeparatorText("Look");
		ImGui::SliderInt("Ray count", &s.RayCount, 1, 128);
		ImGui::DragFloat("Sharpness", &s.Sharpness, 0.05f, 0.01f, 32.0f, "%.2f");
		ImGui::DragFloat("Rotate speed", &s.RotateSpeed, 0.01f, -10.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Flicker speed", &s.FlickerSpeed, 0.05f, 0.0f, 60.0f, "%.2f");
		ImGui::ColorEdit3("Color (sRGB)", &s.Color.x);
		ImGui::ColorEdit3("Core color (sRGB)", &s.CoreColor.x);
		ImGui::DragFloat("Core radius", &s.CoreRadius, 0.001f, 0.0001f, 1.0f, "%.3f");
		ImGui::DragFloat("Core intensity", &s.CoreIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::PopID();

		ImGui::End();
#endif
	}
}
