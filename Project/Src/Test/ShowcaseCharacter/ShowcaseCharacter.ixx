//////	//////	//////	//////	//////	//////
//	ShowcaseCharacter (prototype)			//
//////	//////	//////	//////	//////	//////
// タイトル等の「見せる」シーン用に、プレイヤー（Neki）＋武器をフォワードで描く。
// InGame のディファード経路は通さず、バックバッファ（SRGB）＋深度に直接描く前提。
// スキニング VS は Assets/Shaders/MeshSkinning.VS.hlsl をそのまま流用し、PS だけ専用。

export module Lumina.ShowcaseCharacter;

import <cstdint>;
import <vector>;
import <map>;
import <string>;
import <string_view>;
import <memory>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;

import Lumina.CG3D;
import Lumina.CG3D.Struct;
import Lumina.CG3D.Animation;

namespace Lumina::ShowcaseDetail {
	// InGame の SkinnedModel + SkinnedInstance をひとまとめにしたもの
	// （CG3D の型は align(32) の Mat4 を含むので、unique_ptr 越しに持つ）
	struct SkinnedResources {
		CG3D::Collection Collection;

		DX12::UploadBuffer VertexBuffer;
		DX12::UploadBuffer IndexBuffer;
		DX12::VBV VBV;
		DX12::IBV IBV;
		uint32_t NumIndices{ 0U };

		CG3D::Skeleton Skeleton;
		CG3D::SkinCluster SkinCluster;
		uint32_t IDX_RightHand{ 0U };
		bool HasRightHand{ false };

		std::map<std::string, CG3D::MyAnimation> Animations;
	};

	// 武器（静的メッシュ）。頂点レイアウトはスキンメッシュの 0 番ストリームと同じ
	struct StaticResources {
		DX12::UploadBuffer VertexBuffer;
		DX12::VBV VBV;
		uint32_t NumVertices{ 0U };

		// メッシュ空間での軸（Y 方向）の範囲と、軸からの最大距離（エフェクトの発生位置用）
		float AxisMinY{ 0.0f };
		float AxisMaxY{ 0.0f };
		float AxisRadius{ 0.0f };
	};
}

export namespace Lumina {
	// 色はすべて sRGB で指定（GPU へ送る前にリニア化する）
	class ShowcaseLighting {
	public:
		// 炎の照り返し。キャラを囲む火柱から「軸の外側＋下」方向に光源がある扱い
		Float3 FireBase{ 0.0f, 0.0f, 0.0f };	// 火柱の根元（ワールド）
		Float3 FireColor{ 1.00f, 0.42f, 0.05f };
		float FireIntensity{ 1.6f };
		float FireHeight{ 6.0f };				// この高さで照り返しが 0 になる
		float UnderWeight{ 0.6f };				// 光の向きの「下から」成分。大きいほどあおり光になる

		// 半球環境光（上を向く面 = 空色 / 下を向く面 = 地面色）
		Float3 SkyColor{ 0.10f, 0.12f, 0.22f };
		Float3 GroundColor{ 0.45f, 0.16f, 0.05f };
		float AmbientIntensity{ 0.5f };

		// リム（輪郭の縁取り）
		Float3 RimColor{ 1.00f, 0.70f, 0.35f };
		float RimIntensity{ 1.0f };
		float RimThreshold{ 0.62f };			// 1 - N・V がこれを超えた所から光る
		float RimSoftness{ 0.04f };

		// セル調
		float CelThreshold{ 0.10f };
		float CelSoftness{ 0.05f };
		float ShadowLevel{ 0.20f };				// 影側に残す炎光の割合
	};

	class ShowcaseMaterial {
	public:
		Float3 BaseColor{ 0.75f, 0.75f, 0.78f };
		Float3 EmissiveColor{ 1.00f, 0.50f, 0.15f };
		float EmissiveIntensity{ 0.0f };
	};

	class ShowcaseCharacter {
	public:
		void Initialize(
			DX12::Context const& dxContext_,
			DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT dsvFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT
		);

		// * Animation
		// 同じ名前を再生し直しても時間はリセットしない
		void Play(std::string_view name_, bool isLoop_ = true);
		// deltaTime_ × AnimationSpeed だけ進めてスキンクラスタを更新する
		void Advance(float deltaTime_);

		auto AnimationNames() const noexcept -> std::vector<std::string> const& { return AnimationNames_; }
		auto CurrentAnimationName() const noexcept -> std::string const& { return CurrentAnimationName_; }

		// 武器の軸（メッシュの Y 方向）の根元・先端のワールド座標と、軸からの太さ（ワールド単位）。
		// 武器が無い / 非表示 / 右手ボーンが無いときは false。Advance の後に呼ぶとそのフレームの姿勢になる
		bool WeaponAxis(Float3& base_, Float3& tip_, float& radius_) const;

		// RT / DSV / Viewport / DescriptorHeap は呼び出し側で設定済みであること
		void Render(
			DX12::CommandList const& cmdList_,
			Mat4 const& viewProj_,
			Vec3 const& cameraPos_,
			ShowcaseLighting const& lighting_
		);

	public:
		// * Transform（InGame の「ModelScale × 2」に合わせて Scale = 2 が等身大）
		Float3 Position{ 0.0f, 0.0f, 0.0f };
		float Yaw{ 0.0f };
		float Scale{ 2.0f };
		float AnimationSpeed{ 4.0f };			// InGame と同じ 4 倍速が既定

		// * Weapon（InGame と同じ：右手ボーン Bone.024 に SRT(0.125, 0.5, 0.125 / -90°X) を掛ける）
		bool ShowWeapon{ true };
		Float3 WeaponScale{ 0.125f, 0.5f, 0.125f };
		Float3 WeaponRotate{ -1.57079633f, 0.0f, 0.0f };
		Float3 WeaponOffset{ 0.0f, 0.0f, 0.0f };

		ShowcaseMaterial BodyMaterial{};
		ShowcaseMaterial WeaponMaterial{
			.BaseColor{ 0.55f, 0.50f, 0.48f },
			.EmissiveColor{ 1.00f, 0.55f, 0.20f },
			.EmissiveIntensity{ 0.15f },
		};

	public:
		ShowcaseCharacter() = default;
		~ShowcaseCharacter() = default;

	private:
		// MeshSkinning.VS.hlsl の cbuffer Global（b0, space16）と同じ並び
		struct TransformConstants {
			Mat4 WVP;
			Mat4 World;
			Mat4 TR_INV_World;
		};
		static_assert(sizeof(TransformConstants) % 32U == 0U);

		// ShowcaseCharacter.hlsl の cbuffer Shading（b0, space0）と同じ並び
		struct ShadingConstants {
			Float3 BaseColor;		float CelThreshold;
			Float3 EmissiveColor;	float CelSoftness;
			Float3 FireBase;		float FireHeight;
			Float3 FireColor;		float UnderWeight;
			Float3 SkyColor;		float ShadowLevel;
			Float3 GroundColor;		float RimThreshold;
			Float3 RimColor;		float RimSoftness;
			Float3 CameraPos;		float Padding0;
		};
		static_assert(sizeof(ShadingConstants) % 16U == 0U);

		static constexpr uint64_t ConstantsStride{ 256LLU };
		static constexpr uint32_t NumSlots{ 2U };	// 0: 本体 / 1: 武器

	private:
		Mat4 MeshWorld() const;
		bool WeaponWorld(Mat4 const& meshWorld_, Mat4& weaponWorld_) const;

		void StoreTransforms(uint32_t slot_, Mat4 const& world_, Mat4 const& viewProj_);
		void StoreShading(
			uint32_t slot_,
			ShowcaseMaterial const& material_,
			Vec3 const& cameraPos_,
			ShowcaseLighting const& lighting_
		);

	private:
		std::unique_ptr<ShowcaseDetail::SkinnedResources> Skinned_{ nullptr };
		std::unique_ptr<ShowcaseDetail::StaticResources> Weapon_{ nullptr };

		std::vector<std::string> AnimationNames_{};
		std::string CurrentAnimationName_{};
		CG3D::MyAnimation const* CurrentAnimation_{ nullptr };
		float AnimationTime_{ 0.0f };
		bool IsLoop_{ true };

		DX12::RootSignature RS_{};
		DX12::Shader VS_Skinned_{};
		DX12::Shader VS_Static_{};
		DX12::Shader PS_{};
		DX12::GraphicsPSO PSO_Skinned_{};
		DX12::GraphicsPSO PSO_Static_{};

		DX12::UploadBuffer UB_Transforms_{};
		DX12::UploadBuffer UB_Shading_{};
	};
}
