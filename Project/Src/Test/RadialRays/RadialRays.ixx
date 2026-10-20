//////	//////	//////	//////	//////	//////
//	RadialRays (prototype)					//
//////	//////	//////	//////	//////	//////
// 画面上の 1 点から放射状に伸びる光線＋中心の光（スクリーン空間・加算）。
// Title の Outro の光線（Game.Scene.Title.Rays.cpp / Assets/Title/TitleRays.hlsl）の写し。
// Title 側は Outro の時刻 t から値を決めていたが、こちらは「中心・強さ・長さ・模様の時間」を呼び出し側から受け取るだけにしてある。
//
// 1 インスタンス = フルスクリーン三角形 1 枚（VS が SV_VertexID から生成）。シーンのテクスチャは読まない。
// ルート CBV 1 つ（インスタンスごとに 256 バイト境界のスロット、最大 MaxInstances）。加算・深度テストなし。

export module Lumina.RadialRays;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cstdint>;
import <span>;
import <vector>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

export namespace Lumina::RadialRays {
	// 見た目（色はリニア。sRGB で調整したい場合は呼び出し側で変換する）
	struct Look {
		int RayCount{ 40 };				// 一周あたりの光線の数
		float Sharpness{ 6.0f };		// 大きいほど細く・まばら
		float RotateSpeed{ 0.3f };		// 模様の回転（rad/s）
		float FlickerSpeed{ 8.0f };		// 光線の入れ替わりの速さ（/秒）
		Float3 Color{ 1.0f, 1.0f, 1.0f };
		Float3 CoreColor{ 1.0f, 1.0f, 1.0f };
		float CoreRadius{ 0.05f };		// 中心の光の半径（画面の高さに対する割合）
		float CoreIntensity{ 1.0f };
	};

	struct Instance {
		Float2 Center{ 0.5f, 0.5f };	// 画面 UV（左上が 0,0）
		float Intensity{ 1.0f };
		float Length{ 1.0f };			// 光線の長さ（画面の高さに対する割合）
		float Time{ 0.0f };				// 模様の時間（秒）。インスタンスごとにずらすと模様が揃わない
		Look Appearance{};
	};

	class Renderer {
	public:
		// rtvFormat_ / dsvFormat_ は描き込む先に合わせる（深度テストはしないので、DSV はフォーマットだけ合わせればよい）
		// aspect_ は描き込む先の幅 / 高さ
		void Initialize(
			DX12::Context const& dxContext_,
			DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT dsvFormat_ = DX12::GraphicsPSO::DefaultDSVFormat,
			float aspect_ = 1280.0f / 720.0f
		);

		// RT / Viewport / DescriptorHeap は呼び出し側で設定済みであること。MaxInstances を超えた分は描かない
		void Render(
			DX12::CommandList const& cmdList_,
			std::span<Instance const> instances_
		);

	public:
		static constexpr uint32_t MaxInstances{ 8U };

	private:
		// RadialRays.hlsl の cbuffer Constants と同じ並び。16 バイト境界をまたぐベクトルは置かない。32 の倍数に揃える（C4324 対策の方針）
		struct ShaderConstants {
			Float2 Center;		float Intensity;	float Length;
			Float3 Color;		float Time;
			float RayCount;		float Sharpness;	float RotateSpeed;	float FlickerSpeed;
			Float3 CoreColor;	float CoreRadius;
			float CoreIntensity; float Aspect;		float Padding0;		float Padding1;
			float Padding2[4];		// 80 バイト → 96 バイト
		};
		static_assert(sizeof(ShaderConstants) % 32U == 0U);

		static constexpr uint64_t ConstantsStride{ (sizeof(ShaderConstants) + 0xFFLLU) & ~0xFFLLU };

	private:
		DX12::RootSignature RS_{};
		DX12::Shader VS_{};
		DX12::Shader PS_{};
		DX12::GraphicsPSO PSO_{};

		DX12::UploadBuffer UB_Constants_{};
		float Aspect_{ 1280.0f / 720.0f };
	};
}
