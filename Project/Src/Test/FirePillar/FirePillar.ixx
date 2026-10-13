export module Lumina.FirePillar;

import <cstdint>;
import <vector>;
import <span>;
import <string_view>;
import <cmath>;
import <algorithm>;
import <d3d12.h>;

import nlohmann.json;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;
import Lumina.AssetManager;
import Lumina.Utils.Data;

export namespace Lumina {
	// テクスチャのスクロール層
	class NoiseLayer {
	public:
		float ScaleU{ 2.0f };		// 円周方向のタイル数。継ぎ目が出ないよう整数にすること
		float ScaleV{ 0.8f };		// 高さ方向のタイル数
		float ScrollSpeed{ 0.6f };	// 上方向へのスクロール速度（タイル/秒）
		float WarpAmount{ 0.15f };	// 歪み層によって UV をずらす量（本体層でのみ使用）
	};

	// 3色ランプ。色は sRGB で指定（GPU へ送る前にリニア化する）
	class ColorRamp {
	public:
		Float3 Low{ 0.55f, 0.06f, 0.02f };
		Float3 Mid{ 1.00f, 0.42f, 0.05f };
		Float3 High{ 1.00f, 0.90f, 0.55f };
		float MidPosition{ 0.45f };
	};

	class FlamePillar {
	public:
		class Description {
		public:
			// 形状
			float Radius{ 1.0f };
			float Height{ 6.0f };
			float TopScale{ 0.55f };		// 上端の半径倍率
			float ShapeCurve{ 1.5f };		// 半径変化のカーブ（pow の指数）

			// 揺らぎ
			float WobbleAmp{ 0.15f };		// 軸の横揺れ量（上ほど大きい）
			float WobbleFreq{ 2.0f };
			float WobbleWave{ 0.75f };		// 高さ方向の波の数
			float Twist{ 0.25f };			// 高さ1あたりの U オフセット（らせん状に昇る）

			// テクスチャ
			NoiseLayer Layer{};										// 本体
			NoiseLayer WarpLayer{ 3.0f, 1.2f, 1.1f, 0.0f };			// 歪み（WarpAmount は未使用）

			// 侵食（しきい値で舌状に削る）
			float CutBottom{ 0.30f };		// 根元のしきい値
			float CutTop{ 0.72f };			// 上端のしきい値
			float CutCurve{ 1.2f };
			float Softness{ 0.06f };		// 縁のぼかし幅。小さいほどセル調

			// 円柱っぽさを消すフェード
			float FadeBottom{ 0.06f };		// 根元のフェード幅（v）
			float FadeTopStart{ 0.75f };	// 上端フェード開始位置（v）
			float RimPower{ 1.2f };			// 輪郭フェードの強さ
			float BackDarken{ 0.55f };		// 裏面（奥の壁）の明るさ倍率

			// 見た目
			float Intensity{ 1.0f };
			float Opacity{ 1.0f };
			float Additive{ 0.6f };			// 0: 通常の半透明 / 1: 加算
			float TextureColor{ 0.0f };		// テクスチャ RGB をどれだけ色に乗算するか
			float HeatScale{ 2.5f };		// しきい値をどれだけ上回ったか → ランプ位置
			float HeatHeight{ 0.35f };		// 根元ほど熱く（ランプ上側に）する量
			ColorRamp Ramp{};

			float GrowTime{ 0.0f };			// 未使用（今は持続のみ）
		};

		class Instance {
		public:
			Description const* Desc{ nullptr };
			Float3 Position{ 0.0f, 0.0f, 0.0f };	// 根元の位置（Vec3 だとアライメントのパディングで C4324 になるので Float3）
			float Age{ 0.0f };				// スクロール・揺らぎの時間
			int Seed{ 0 };					// UV オフセット（同じ見た目の柱が並ばないように）
			float IntensityScale{ 1.0f };   // 個別の上書きは最小限だけ持つ
		};

		class Renderer;
	};

	//////	//////	//////	//////	//////	//////

	class FlamePillar::Renderer {
	public:
		// rtvFormat_ / dsvFormat_ は描き込む先に合わせる。深度なしなら dsvFormat_ = DXGI_FORMAT_UNKNOWN
		void Initialize(
			DX12::Context const& dxContext_,
			AssetManager& assetMngr_,
			DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT dsvFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT
		);

		// RT / DSV / Viewport / DescriptorHeap は呼び出し側で設定済みであること
		// instances_ は外側の層 → 内側の層の順。各 Instance の Desc は必須（nullptr 不可）
		void Render(
			DX12::CommandList const& cmdList_,
			std::span<Instance const> instances_,
			Mat4 const& viewProj_,
			Vec3 const& cameraPos_
		);

	public:
		static constexpr uint32_t MaxInstances{ 16U };
		static constexpr uint32_t SegmentsU{ 48U };
		static constexpr uint32_t SegmentsV{ 24U };

	private:
		// HLSL 側の cbuffer Constants と同じ並び。16 バイト境界をまたぐベクトルは置かないこと
		struct ShaderConstants {
			Mat4 World;
			Mat4 ViewProj;

			Float3 CameraPos;	float Time;
			float Radius;		float Height;		float TopScale;		float ShapeCurve;
			float WobbleAmp;	float WobbleFreq;	float WobbleWave;	float Twist;
			float NoiseScaleU;	float NoiseScaleV;	float NoiseSpeed;	float WarpAmount;
			float WarpScaleU;	float WarpScaleV;	float WarpSpeed;	float Seed;
			float CutBottom;	float CutTop;		float CutCurve;		float Softness;
			float FadeBottom;	float FadeTopStart;	float RimPower;		float BackDarken;
			float Intensity;	float Opacity;		float Additive;		float TextureColor;
			Float3 RampLow;		float HeatScale;
			Float3 RampMid;		float HeatHeight;
			Float3 RampHigh;	float RampMidPos;
			uint32_t NumSegmentsU;	uint32_t NumSegmentsV;	uint32_t Padding0;	uint32_t Padding1;
		};
		static_assert(sizeof(ShaderConstants) % 16U == 0U);

		static constexpr uint64_t ConstantsStride{ (sizeof(ShaderConstants) + 0xFFLLU) & ~0xFFLLU };

	private:
		void StoreConstants(
			uint32_t slot_,
			Instance const& instance_,
			Mat4 const& viewProj_,
			Vec3 const& cameraPos_
		);

	private:
		DX12::RootSignature RS_{};
		DX12::Shader VS_{};
		DX12::Shader PS_{};
		DX12::GraphicsPSO PSO_BackFaces_{};		// CULL_FRONT（奥の壁）
		DX12::GraphicsPSO PSO_FrontFaces_{};	// CULL_BACK（手前の壁）

		DX12::UploadBuffer UB_Constants_{};
		DX12::DescriptorTable SRV_Texture_{};
	};
}