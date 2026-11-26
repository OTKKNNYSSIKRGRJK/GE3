//////	//////	//////	//////	//////	//////
//	FxMesh (prototype)						//
//////	//////	//////	//////	//////	//////
// ワールド空間の加算エフェクトを「三角形の列」で描く小さなレンダラ。剣の軌跡（帯）、予兆の帯・足元の輪（地面に貼る）、
// ガードの弧（縦の帯）、命中の輪・煌めき・スタンの星（ビルボードの四角）などを、毎フレーム CPU で組み立てて 1 回の Draw で描く。
//
// - 頂点は StructuredBuffer（ルート SRV、アップロードヒープ）。VS が SV_VertexID で読む。頂点バッファ・入力レイアウトなし。
//   毎フレーム Clear → Triangle / Quad で積む → Render で写して描く（CPU が毎フレーム GPU を待つので、写し先は 1 本でよい）。
// - 形は頂点ごとの ShapeID（Assets/Effects/FxMesh.hlsl の説明を参照）。テクスチャは使わない。
// - 深度は G-buffer の深度（SRV）をピクセルシェーダで読んで比べる（MergePass は深度を持たないため）。
// - 加算（ONE / ONE）・深度テストなし・両面。描く順序は問わない。
//
// 色はリニアで渡す（a は強さ。1 を超えてよい）。

export module Lumina.FxMesh;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cstdint>;
import <vector>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

export namespace Lumina::FxMesh {
	enum class Shape : std::uint32_t {
		Glow = 0U,
		Ring = 1U,
		Star = 2U,
		Trail = 3U,
		Ward = 4U,
		Star5 = 5U,		// 五芒星（スタンの星）
		Lane = 6U,		// 予兆の帯（地面に貼る）
	};

	// FxMesh.hlsl の FxVertex と同じ並び（StructuredBuffer は 4 バイト詰めなので、そのまま 64 バイト）
	struct Vertex {
		Float3 Position{};
		std::uint32_t ShapeID{ 0U };
		Float2 UV{};
		float DepthBias{ 0.0f };		// シーンの深さにこれを足した位置まで見える（ワールドの長さ）
		float SoftRange{ 0.3f };		// 物に刺さったところを消す幅
		Float4 Param{};
		Float4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };		// rgb = リニアの色、a = 強さ
	};
	static_assert(sizeof(Vertex) == 64U);

	class Renderer {
	public:
		static constexpr std::uint32_t DefaultMaxVertices{ 24576U };	// 三角形 8192 枚

		// rtvFormat_ / dsvFormat_ は描き込むパスに合わせる（InGame の MergePass は SRGB 1 枚。DSV はパーティクルの PSO と同じく既定値）
		void Initialize(
			DX12::Context const& dxContext_,
			DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT dsvFormat_ = DX12::GraphicsPSO::DefaultDSVFormat,
			std::uint32_t maxVertices_ = DefaultMaxVertices
		);

		// 積んだ三角形を捨てる（毎フレームの頭）
		void Clear() noexcept;

		void Triangle(Vertex const& v0_, Vertex const& v1_, Vertex const& v2_);
		// 四角形（v00 = uv(0,0) 側、v10 = (1,0)、v01 = (0,1)、v11 = (1,1) の並びを想定。三角形 2 枚）
		void Quad(Vertex const& v00_, Vertex const& v10_, Vertex const& v01_, Vertex const& v11_);

		auto VertexCount() const noexcept -> std::uint32_t { return static_cast<std::uint32_t>(Vertices_.size()); }
		auto MaxVertices() const noexcept -> std::uint32_t { return MaxVertices_; }
		auto DroppedTriangles() const noexcept -> std::uint32_t { return Dropped_; }		// 上限を超えて捨てた数（Clear で 0）

		// RT / Viewport / DescriptorHeap は呼び出し側で設定済みであること。
		// depthSRV_ = G-buffer の深度の SRV（R24_UNORM_X8）。nearZ_ / farZ_ はカメラの射影と同じ値
		void Render(
			DX12::CommandList const& cmdList_,
			Mat4 const& viewProjection_,
			float nearZ_,
			float farZ_,
			float time_,
			D3D12_GPU_DESCRIPTOR_HANDLE depthSRV_
		);

	private:
		DX12::RootSignature RS_{};
		DX12::Shader VS_{};
		DX12::Shader PS_{};
		DX12::GraphicsPSO PSO_{};

		DX12::UploadBuffer UB_Vertices_{};
		DX12::UploadBuffer UB_Constants_{};

		std::vector<Vertex> Vertices_{};
		std::uint32_t MaxVertices_{ 0U };
		std::uint32_t Dropped_{ 0U };
	};
}
