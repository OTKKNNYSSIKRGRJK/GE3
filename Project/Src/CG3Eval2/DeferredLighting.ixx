export module Lumina.DeferredLighting;

import <cstdint>;

import <vector>;
import <array>;

import Lumina.Math;

import Lumina.Container.List;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux.View;

namespace Lumina {
	export struct DirectionalLight {
		Float3 Direction;
		Float3 DiffuseRGB;
		float DiffuseIntensity;
		Float3 SpecularRGB;
		float SpecularIntensity;
	};

	export struct PointLight {
		Float4 WorldPosition;
		Float3 DiffuseRGB;
		float DiffuseIntensity;
		Float3 SpecularRGB;
		float SpecularIntensity;
	};

	export struct SpotLight {
		Float3 WorldPosition;
		Float3 Direction;
		float Distance;
		float Decay;
		float Cos_Angle;
		float Cos_FalloffStart;
		Float3 DiffuseRGB;
		float DiffuseIntensity;
		Float3 SpecularRGB;
		float SpecularIntensity;
	};

	export float LightSphereRadius(
		float inv_Threshold_,
		float maxComp_intensity_,
		float factor_Constant_,
		float factor_Linear_,
		float factor_Quadratic_
	) {
		return std::sqrt(
			-factor_Linear_ +
			std::sqrt(
				factor_Linear_ * factor_Linear_ -
				4.0f * factor_Quadratic_ * (factor_Constant_ - maxComp_intensity_ * inv_Threshold_)
			) / (2.0f * factor_Quadratic_)
		);
	}

	export class DeferredLighting {
	public:
		auto RenderTexture() const noexcept -> DX12::RenderTexture2D const&;

	public:
		void Update(
			List<DirectionalLight> const& list_DirectionalLight_,
			List<PointLight> const& list_PointLight_,
			List<Math::Mat4> const& list_WorldMatrix_LightSphere_,
			std::vector<uint32_t> const& arr_Index_ActivePointLight_
		);
		void Render(
			DX12::GraphicsDevice const& device_,
			DX12::CommandList const& cmdList_,
			D3D12_GPU_DESCRIPTOR_HANDLE globalSRV_Arr_GBuffer_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_WorldToNDC_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_Scene_
		);

	public:
		void Initialize(
			DX12::Context const& dxContext_,
			uint32_t canvasWidth_,
			uint32_t canvasHeight_
		);

	private:
		uint32_t MaxNum_PointLights_{ 2048U };
		uint32_t Num_ActivePointLights_{ 0U };


		uint32_t Num_ActiveDirectionalLights_{ 0U };

		DX12::RenderPass RenderPass_{};
		DX12::Canvas Canvas_{};

		DX12::RootSignature RootSignature_{};
		DX12::Shader VS_Shape_{};
		DX12::Shader VS_Fullscreen_{};
		DX12::Shader PS_DirectionalLight_{};
		DX12::Shader PS_PointLight_{};
		DX12::GraphicsPSO GraphicsPSO_DirectionalLight_{};
		DX12::GraphicsPSO GraphicsPSO_PointLight_{};

		DX12::DescriptorTable GlobalTable_{};

		DX12::UploadBuffer UB_Arr_DirectionalLight_{};

		DX12::UploadBuffer UB_Arr_PointLight_{};
		DX12::UploadBuffer UB_Arr_WorldMatrix_LightSphere_{};
		DX12::UploadBuffer UB_Arr_Index_ActivePointLight_{};

		DX12::UploadBuffer UB_Vertices_Rect_{};
		DX12::VBV VBV_Rect_{};

		DX12::UploadBuffer UB_Vertices_LightSphere_{};
		DX12::UploadBuffer UB_Indices_LightSphere_{};
		DX12::VBV VBV_LightSphere_{};
		DX12::IBV IBV_LightSphere_{};
		uint32_t Num_IndicesPerSphere_{};
	};
}