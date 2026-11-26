//////	//////	//////	//////	//////	//////
//	SceneLight - 平行光 1 本（暗さを和らげる）	//
//////	//////	//////	//////	//////	//////
// InGame のディファードは点光源（粒子から作る）しか足さないので、粒子の光が届かない所は真っ黒になる。
// その暗さを和らげるために、平行光 1 本＋半球の環境光を全画面の加算パスで足す（2026-11-11）。
// 光を遮るオブジェ・光の筋・影は無い（後で作る予定）。
//
// 描画: InGame の MergePass で、ライティングの結果を写した直後（粒子より前）。G-buffer（Canvas の SRV 0〜2）を読む。
// 調整: ImGui「Scene Light」（Debug のみ）、Assets/Configs/SceneLight.json。色は sRGB で指定（描画時にリニアへ）。

export module Game.SceneLight;

//****	******	******	******	******	****//

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <string>;
import <vector>;
import <d3d12.h>;

import nlohmann.json;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

//////	//////	//////	//////	//////	//////

export namespace Game {
	struct SceneLightStyle {
		bool Enabled{ true };

		// 光の来る向き（度）。Yaw は XZ の向き（0 = +X、90 = +Z）、Pitch は地平線からの高さ
		float Yaw{ 120.0f };
		float Pitch{ 55.0f };
		Lumina::Float3 Color{ 0.78f, 0.86f, 1.0f };		// 月明かりっぽい青白
		float Intensity{ 0.6f };
		float Wrap{ 0.35f };				// 回り込み（0 = ランバート。大きいほど影側も明るい）

		// 半球の環境光（上を向いた面は Sky、下を向いた面は Ground）
		Lumina::Float3 SkyAmbient{ 0.35f, 0.45f, 0.6f };
		Lumina::Float3 GroundAmbient{ 0.22f, 0.26f, 0.18f };
		float Ambient{ 0.2f };

		float SkyBrightness{ 0.35f };		// 空（スカイボックス）を何割見せるか。0 なら今までどおり黒
		float AlbedoScale{ 0.75f };			// 点光源（Lighting.PS）と同じ
	};

	class SceneLight {
	public:
		SceneLightStyle Look{};

		// rtvFormat_ / dsvFormat_ は MergePass に合わせる。設定ファイルがあれば読む
		void Initialize(Lumina::DX12::Context const& dxContext_, DXGI_FORMAT rtvFormat_, DXGI_FORMAT dsvFormat_);

		// MergePass の中。gbuffer_ = Canvas の SRV テーブルの 0 番（0 = アルベド、1 = 法線、2 = 深度、が続いている）
		void Render(Lumina::DX12::CommandList const& cmdList_, D3D12_GPU_DESCRIPTOR_HANDLE gbuffer_);

		// 光の来る向き（長さ 1）
		auto ToLight() const noexcept -> Lumina::Float3;

		// ImGui「Scene Light」（Debug のみ）
		void DrawEditor();

		static constexpr char const* ConfigPath{ "Assets/Configs/SceneLight.json" };
		auto Save() -> bool;
		auto Load() -> bool;

	private:
		Lumina::DX12::RootSignature RS_{};
		Lumina::DX12::Shader VS_{};
		Lumina::DX12::Shader PS_{};
		Lumina::DX12::GraphicsPSO PSO_{};
		Lumina::DX12::UploadBuffer UB_Light_{};
		std::string Status_{};
	};
}
