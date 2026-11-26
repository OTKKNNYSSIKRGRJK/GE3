//////	//////	//////	//////	//////	//////
//	Phys.DebugDraw							//
//////	//////	//////	//////	//////	//////
// コライダのワイヤーフレーム表示。ライン 1 本 = 頂点 2 つを StructuredBuffer に積み、
// VS が SV_VertexID で読む（頂点バッファ・入力レイアウトなし）。
// 深度なしで上書き描画する前提（UI パスなど）。深度ありの先に描くなら dsvFormat_ を渡す。
//
//   debug.Draw(world);                    // 登録済みコライダ全部（重なっているものは赤、無効は灰）
//   debug.Draw(shape, color);             // 任意の形状
//   debug.Render(cmdList, viewProj);      // 積んだ線を描いて空にする

export module Lumina.Phys.DebugDraw;

import <cstdint>;
import <vector>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

export import Lumina.Phys.Collider;

export namespace Lumina::Phys {
	class DebugRenderer {
	public:
		void Initialize(
			DX12::Context const& dxContext_,
			DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT dsvFormat_ = DXGI_FORMAT_UNKNOWN
		);

		//----	線を積む						----//

		void Line(Float3 const& p0_, Float3 const& p1_, Float4 const& color_);
		void Cross(Float3 const& p_, float size_, Float4 const& color_);

		void Draw(Sphere const& sphere_, Float4 const& color_);
		void Draw(AABB const& aabb_, Float4 const& color_);
		void Draw(OBB const& obb_, Float4 const& color_);
		void Draw(Shape const& shape_, Float4 const& color_);
		void Draw(Ray const& ray_, Float4 const& color_);

		// 登録済みのコライダと、接触点・法線
		void Draw(CollisionWorld const& world_);

		//----	描画							----//

		// RT / Viewport / DescriptorHeap は呼び出し側で設定済みであること
		void Render(DX12::CommandList const& cmdList_, Mat4 const& viewProj_);

	public:
		bool ShowBounds{ false };		// 外接 AABB も描く
		bool ShowContacts{ true };		// 接触点と法線を描く

		Float4 ColorIdle{ 0.1f, 1.0f, 0.3f, 1.0f };
		Float4 ColorHit{ 1.0f, 0.15f, 0.1f, 1.0f };
		Float4 ColorDisabled{ 0.35f, 0.35f, 0.35f, 0.6f };
		Float4 ColorBounds{ 0.2f, 0.4f, 1.0f, 0.5f };
		Float4 ColorContact{ 1.0f, 0.9f, 0.1f, 1.0f };

		static constexpr uint32_t MaxLines{ 16384U };
		static constexpr uint32_t CircleSegments{ 32U };

	private:
		// HLSL の struct LineVertex と同じ並び（32 バイト）
		struct LineVertex {
			Float3 Position;
			float Padding;
			Float4 Color;
		};
		static_assert(sizeof(LineVertex) == 32U);

		void Circle(Float3 const& center_, Float3 const& axisU_, Float3 const& axisV_, float radius_, Float4 const& color_);

	private:
		DX12::RootSignature RS_{};
		DX12::Shader VS_{};
		DX12::Shader PS_{};
		DX12::GraphicsPSO PSO_{};

		DX12::UploadBuffer UB_Vertices_{};
		DX12::UploadBuffer UB_Constants_{};

		std::vector<LineVertex> Vertices_{};
		std::vector<uint32_t> HitIDs_{};
	};
}
