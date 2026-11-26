module Game.SceneLight;

// ここで import しているヘッダユニットは、いずれも SceneLight.ixx で import 済み
import <algorithm>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <string>;
import <vector>;

import nlohmann.json;

import Lumina.Math.Numerics;
import Lumina.Math;
import Lumina.Utils.Data;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace {
	using Lumina::Float3;
	using JSON = nlohmann::json;

	constexpr float DegToRad{ 3.14159265358979f / 180.0f };

	auto SRGBToLinear(float c_) noexcept -> float {
		return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
	}

	// SceneLight.hlsl の LIGHT と同じ並び（float4 × 5）
	struct LightConstants {
		float ToLightX, ToLightY, ToLightZ, Intensity;
		float ColorR, ColorG, ColorB, Wrap;
		float SkyR, SkyG, SkyB, Ambient;
		float GroundR, GroundG, GroundB, SkyBrightness;
		float AlbedoScale, Padding0, Padding1, Padding2;
	};
	static_assert(sizeof(LightConstants) == 80U);

	void Read(JSON const& j_, char const* key_, float& v_) {
		auto const it{ j_.find(key_) };
		if (it != j_.end() && it->is_number()) { v_ = it->get<float>(); }
	}
	void Read(JSON const& j_, char const* key_, bool& v_) {
		auto const it{ j_.find(key_) };
		if (it != j_.end() && it->is_boolean()) { v_ = it->get<bool>(); }
	}
	void Read(JSON const& j_, char const* key_, Float3& v_) {
		auto const it{ j_.find(key_) };
		if (it == j_.end() || !it->is_array() || it->size() != 3U) { return; }
		for (auto const& e : *it) { if (!e.is_number()) { return; } }
		v_ = { (*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>() };
	}
}

namespace Game {
	void SceneLight::Initialize(Lumina::DX12::Context const& dxContext_, DXGI_FORMAT rtvFormat_, DXGI_FORMAT dsvFormat_) {
		auto const& device{ dxContext_.Device() };

		{
			auto const settings = Lumina::Utils::LoadFromFile<nlohmann::json>("SceneLight.json", "Assets/Effects");	// = で受ける（波かっこだと配列になるコンパイラがある）
			RS_.Initialize(device, Lumina::DX12::LoadRootSignatureSetup(settings.at("RS")), "SceneLight RS");
		}
		dxContext_.Compile(VS_, L"Assets/Effects/SceneLight.hlsl", L"vs_6_6", L"VSMain", "SceneLight.VS");
		dxContext_.Compile(PS_, L"Assets/Effects/SceneLight.hlsl", L"ps_6_6", L"PSMain", "SceneLight.PS");

		// 加算（ONE / ONE）。α は書かない
		Lumina::DX12::BlendState blendState{};
		blendState.RenderTarget[0] = {
			.BlendEnable{ true },
			.LogicOpEnable{ false },
			.SrcBlend{ D3D12_BLEND_ONE },
			.DestBlend{ D3D12_BLEND_ONE },
			.BlendOp{ D3D12_BLEND_OP_ADD },
			.SrcBlendAlpha{ D3D12_BLEND_ZERO },
			.DestBlendAlpha{ D3D12_BLEND_ONE },
			.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
			.LogicOp{ D3D12_LOGIC_OP_NOOP },
			.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
		};
		Lumina::DX12::GraphicsPipelineState::InputLayout inputLayout{};		// 空（VS が SV_VertexID で三角形を作る）
		std::vector<DXGI_FORMAT> const rtvFormats{ rtvFormat_ };
		PSO_.Initialize(
			device,
			RS_,
			VS_,
			PS_,
			blendState,
			Lumina::DX12::RasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_NONE },
			},
			Lumina::DX12::DepthStencilState{
				.DepthEnable{ false },
				.StencilEnable{ false },
			},
			inputLayout,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			rtvFormats,
			dsvFormat_,
			"SceneLight PSO"
		);

		UB_Light_.Initialize(device, 256LLU, "SceneLight Constants");
		Load();
	}

	auto SceneLight::ToLight() const noexcept -> Float3 {
		float const yaw{ Look.Yaw * DegToRad };
		float const pitch{ std::clamp(Look.Pitch, -89.0f, 89.0f) * DegToRad };
		return { std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw) };
	}

	void SceneLight::Render(Lumina::DX12::CommandList const& cmdList_, D3D12_GPU_DESCRIPTOR_HANDLE gbuffer_) {
		if (!Look.Enabled) { return; }

		SceneLightStyle const& s{ Look };
		Float3 const l{ ToLight() };
		LightConstants const c{
			l.x, l.y, l.z, std::max(s.Intensity, 0.0f),
			SRGBToLinear(s.Color.x), SRGBToLinear(s.Color.y), SRGBToLinear(s.Color.z), std::clamp(s.Wrap, 0.0f, 1.0f),
			SRGBToLinear(s.SkyAmbient.x), SRGBToLinear(s.SkyAmbient.y), SRGBToLinear(s.SkyAmbient.z), std::max(s.Ambient, 0.0f),
			SRGBToLinear(s.GroundAmbient.x), SRGBToLinear(s.GroundAmbient.y), SRGBToLinear(s.GroundAmbient.z), std::max(s.SkyBrightness, 0.0f),
			std::max(s.AlbedoScale, 0.0f), 0.0f, 0.0f, 0.0f,
		};
		UB_Light_.Store(&c, sizeof(LightConstants), 0LLU);

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->SetGraphicsRootConstantBufferView(0U, UB_Light_->GetGPUVirtualAddress());
		cmdList_->SetGraphicsRootDescriptorTable(1U, gbuffer_);
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList_->DrawInstanced(3U, 1U, 0U, 0U);
	}

	//====	======	======	======	======	====//
	//	保存・読み込み							//
	//====	======	======	======	======	====//

	auto SceneLight::Load() -> bool {
		std::ifstream ifs{ ConfigPath };
		if (!ifs.is_open()) { Status_ = "No config file (using defaults)."; return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { Status_ = "Config is not valid JSON."; return false; }
		SceneLightStyle& s{ Look };
		Read(root, "Enabled", s.Enabled);
		Read(root, "Yaw", s.Yaw);
		Read(root, "Pitch", s.Pitch);
		Read(root, "Color", s.Color);
		Read(root, "Intensity", s.Intensity);
		Read(root, "Wrap", s.Wrap);
		Read(root, "SkyAmbient", s.SkyAmbient);
		Read(root, "GroundAmbient", s.GroundAmbient);
		Read(root, "Ambient", s.Ambient);
		Read(root, "SkyBrightness", s.SkyBrightness);
		Read(root, "AlbedoScale", s.AlbedoScale);
		Status_ = "Loaded.";
		return true;
	}

	auto SceneLight::Save() -> bool {
		SceneLightStyle const& s{ Look };
		auto const f3{ [](Float3 const& v_) { return JSON::array({ v_.x, v_.y, v_.z }); } };
		JSON const root{
			{ "Enabled", s.Enabled },
			{ "Yaw", s.Yaw },
			{ "Pitch", s.Pitch },
			{ "Color", f3(s.Color) },
			{ "Intensity", s.Intensity },
			{ "Wrap", s.Wrap },
			{ "SkyAmbient", f3(s.SkyAmbient) },
			{ "GroundAmbient", f3(s.GroundAmbient) },
			{ "Ambient", s.Ambient },
			{ "SkyBrightness", s.SkyBrightness },
			{ "AlbedoScale", s.AlbedoScale },
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

	void SceneLight::DrawEditor() {
#if defined(_DEBUG)
		if (!ImGui::Begin("Scene Light")) {
			ImGui::End();
			return;
		}
		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { Look = SceneLightStyle{}; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		SceneLightStyle& s{ Look };
		ImGui::Checkbox("Enabled", &s.Enabled);
		ImGui::SeparatorText("Directional light");
		ImGui::SliderFloat("Yaw (deg)", &s.Yaw, -180.0f, 180.0f);
		ImGui::SliderFloat("Pitch (deg)", &s.Pitch, -10.0f, 89.0f);
		Float3 const l{ ToLight() };
		ImGui::TextDisabled("to light = (%.2f, %.2f, %.2f)", l.x, l.y, l.z);
		ImGui::ColorEdit3("Color (sRGB)", &s.Color.x);
		ImGui::SliderFloat("Intensity", &s.Intensity, 0.0f, 4.0f);
		ImGui::SliderFloat("Wrap", &s.Wrap, 0.0f, 1.0f);
		ImGui::SeparatorText("Ambient (hemisphere)");
		ImGui::ColorEdit3("Sky (sRGB)", &s.SkyAmbient.x);
		ImGui::ColorEdit3("Ground (sRGB)", &s.GroundAmbient.x);
		ImGui::SliderFloat("Ambient", &s.Ambient, 0.0f, 2.0f);
		ImGui::SeparatorText("Other");
		ImGui::SliderFloat("Sky brightness", &s.SkyBrightness, 0.0f, 1.5f);
		ImGui::SliderFloat("Albedo scale", &s.AlbedoScale, 0.0f, 2.0f);
		ImGui::End();
#endif
	}
}
