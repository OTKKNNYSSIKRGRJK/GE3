export module CG3Eval2;

import <cstdint>;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;

import Lumina.Math;

import Lumina.DeferredLighting;
import Lumina.Container.List;

import Lumina.Skybox;
import Lumina.SimpleFX;
import Lumina.SimpleFX2;
import Lumina.SimpleFX3;


namespace CG3Eval2 {
	struct Constant_Scene {
		Lumina::Mat4 WorldToProjective;
	};

	export class Scene {
	public:
		void Update();
		void Render(
			[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
			Lumina::DX12::CommandList const& cmdList_
		);

	public:
		template<typename...Args>
		void Initialize(Args const&...args);

	private:
		Lumina::Mat4 WorldToView_{};
		Lumina::Mat4 ViewToNDC_{};

		Constant_Scene Constant_Scene_{};

		Lumina::DX12::DefaultBuffer DB_Constant_Scene_{};
		Lumina::DX12::UploadBuffer UB_Constant_Scene_{};
		Lumina::DX12::UploadBuffer UB_Constant_Model_{};
		Lumina::DX12::UploadBuffer UB_LightingScene_{};

		Lumina::DX12::DefaultBuffer DB_VB_Mesh_Sphere_{};
		Lumina::DX12::DefaultBuffer DB_IB_Mesh_Sphere_{};
		Lumina::DX12::VBV VBV_Mesh_Sphere_{};
		Lumina::DX12::IBV IBV_Mesh_Sphere_{};
		uint32_t Num_Vertices_Sphere_{};
		uint32_t Num_Indices_Sphere_{};

		Lumina::DX12::DefaultBuffer DB_VB_Mesh_AssimpTest_{};
		Lumina::DX12::VBV VBV_Mesh_AssimpTest_{};
		uint32_t Num_Vertices_AssimpTest_{};

		Lumina::DX12::RootSignature RS_{};
		Lumina::DX12::GraphicsPSO PSO_{};
		Lumina::DX12::Shader VertexShader_{};
		Lumina::DX12::Shader PixelShader_{};

		Lumina::DX12::DescriptorTable GlobalTable_Graphics_{};
		Lumina::DX12::DescriptorTable GlobalTable_ImageTextures_{};

		Lumina::DX12::DescriptorHeap LocalHeap_CBV_{};

		Lumina::DX12::Canvas Canvas_{};
		Lumina::DX12::RenderPass RenderPass_{};

		Lumina::DeferredLighting Lighting_{};
		Lumina::List<Lumina::DirectionalLight> List_DirectionalLight_;
		Lumina::List<Lumina::PointLight> List_PointLight_;
		Lumina::List<Lumina::Mat4> List_Matrix_World_LightSphere_;

		std::unique_ptr<Lumina::Skybox> Skybox_;
		std::unique_ptr<Lumina::SimpleFX> SimpleFX_;
		std::unique_ptr<Lumina::SimpleFX2> SimpleFX2_;
		std::unique_ptr<Lumina::SimpleFX3> SimpleFX3_;
	};
}