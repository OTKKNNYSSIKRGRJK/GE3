//////	//////	//////	//////	//////	//////
//	MagicCircle (prototype)					//
//////	//////	//////	//////	//////	//////
// テクスチャを使わない手続き生成（SDF）の魔法陣。
// 1 レイヤー = 地面に水平なクアッド 1 枚。部品（リング / 星形多角形 / 頂点ノード / 目盛り / 放射線 / ルーン帯 / 床の照り返し）を
// ビットフラグで組み合わせ、レイヤーごとに半径・高さ・回転を変えて重ねる。
// 加算合成・深度テストあり・深度書き込みなし。

export module Lumina.MagicCircle;

import <cstdint>;
import <vector>;
import <span>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

export namespace Lumina {
	class MagicCircle {
	public:
		// レイヤーを構成する部品（ビットフラグ）
		enum Element : uint32_t {
			Element_Ring0 = 1U << 0U,
			Element_Ring1 = 1U << 1U,
			Element_Polygon = 1U << 2U,		// 星形多角形 {N/K}（K = 1 で普通の多角形）
			Element_Nodes = 1U << 3U,		// 多角形の頂点に小円
			Element_Ticks = 1U << 4U,		// 目盛り（4 本ごとに長い）
			Element_Spokes = 1U << 5U,		// 放射線
			Element_Runes = 1U << 6U,		// ルーン帯（3x3 格子の 16 ストロークからハッシュで選ぶ）
			Element_Fill = 1U << 7U,		// 床の照り返し（中心ほど明るい円盤）
		};

		class Layer {
		public:
			bool Enabled{ true };
			uint32_t Elements{ Element_Ring0 | Element_Ring1 };

			// 配置
			float Radius{ 4.0f };			// ワールド単位
			float Height{ 0.02f };			// 中心からの高さ
			float RotateSpeed{ 0.2f };		// rad/s（負で逆回転）
			float Phase{ 0.0f };			// rad
			float Reveal{ 1.0f };			// 描き込み量 0..1（真上から時計回りに現れる）

			// 見た目（色は sRGB）
			Float3 Color{ 1.00f, 0.55f, 0.18f };
			float Intensity{ 1.0f };
			float LineWidth{ 0.006f };		// 以下、長さは半径を 1 とした正規化単位
			float GlowAmount{ 0.35f };
			float GlowWidth{ 0.03f };
			float FillAmount{ 0.08f };

			// 部品
			float Ring0{ 0.98f };
			float Ring1{ 0.90f };
			int PolygonVertices{ 6 };
			int PolygonStep{ 2 };			// {6/2} = 六芒星, {5/2} = 五芒星, {8/3} = 八芒星
			float PolygonRadius{ 0.88f };
			float NodeRadius{ 0.05f };
			int TickCount{ 72 };
			float TickInner{ 0.92f };
			float TickOuter{ 0.96f };
			int SpokeCount{ 12 };
			float SpokeInner{ 0.30f };
			float SpokeOuter{ 0.88f };
			int RuneCount{ 24 };
			float RuneInner{ 0.72f };
			float RuneOuter{ 0.84f };
			int RuneSeed{ 0 };
			float RuneDensity{ 0.45f };		// 各ストロークが現れる確率
			float RuneShimmer{ 0.3f };		// 文字ごとの明滅の深さ
		};

		class Renderer;
	};

	//////	//////	//////	//////	//////	//////

	class MagicCircle::Renderer {
	public:
		// rtvFormat_ / dsvFormat_ は描き込む先に合わせる。深度なしなら dsvFormat_ = DXGI_FORMAT_UNKNOWN
		void Initialize(
			DX12::Context const& dxContext_,
			DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT dsvFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT
		);

		// RT / DSV / Viewport / DescriptorHeap は呼び出し側で設定済みであること
		// center_ : 魔法陣の中心 / time_ : 回転・明滅の時間 / intensityScale_ : 全レイヤー共通の明るさ倍率（脈動用）
		void Render(
			DX12::CommandList const& cmdList_,
			std::span<Layer const> layers_,
			Float3 const& center_,
			float time_,
			float intensityScale_,
			Mat4 const& viewProj_
		);

	public:
		static constexpr uint32_t MaxLayers{ 16U };
		static constexpr float QuadExtent{ 1.15f };		// グローが切れないよう、クアッドを半径より少し大きく取る

	private:
		// HLSL 側の cbuffer Constants と同じ並び。16 バイト境界をまたぐベクトルは置かないこと
		// Mat4 が align(32) なので全体を 32 の倍数に揃える（C4324 対策）
		struct ShaderConstants {
			Mat4 ViewProj;

			Float3 Center;		float Radius;
			float Height;		float Angle;		float Reveal;			float Intensity;
			Float3 Color;		float LineWidth;
			float GlowAmount;	float GlowWidth;	float FillAmount;		float Extent;
			float Ring0;		float Ring1;		float PolygonRadius;	float NodeRadius;
			float TickInner;	float TickOuter;	float SpokeInner;		float SpokeOuter;
			float RuneInner;	float RuneOuter;	float RuneDensity;		float RuneShimmer;
			uint32_t Elements;	uint32_t PolygonVertices;	uint32_t PolygonStep;	uint32_t TickCount;
			uint32_t SpokeCount;	uint32_t RuneCount;		uint32_t RuneSeed;		uint32_t Padding0;
			float Time;			float Padding1;		float Padding2;			float Padding3;
		};
		static_assert(sizeof(ShaderConstants) % 32U == 0U);

		static constexpr uint64_t ConstantsStride{ (sizeof(ShaderConstants) + 0xFFLLU) & ~0xFFLLU };

	private:
		void StoreConstants(
			uint32_t slot_,
			Layer const& layer_,
			Float3 const& center_,
			float time_,
			float intensityScale_,
			Mat4 const& viewProj_
		);

	private:
		DX12::RootSignature RS_{};
		DX12::Shader VS_{};
		DX12::Shader PS_{};
		DX12::GraphicsPSO PSO_{};

		DX12::UploadBuffer UB_Constants_{};
	};
}
