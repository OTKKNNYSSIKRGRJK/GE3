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
	export struct PointLight {
		Lumina::Float4 WorldPosition;
		Lumina::Float3 RGB;
		float Intensity;
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
			List<PointLight> const& list_PointLight_,
			List<Mat4> const& list_WorldMatrix_LightSphere_,
			std::vector<uint32_t> const& arr_Index_ActivePointLight_
		);
		void Render(
			DX12::GraphicsDevice const& device_,
			DX12::CommandList const& cmdList_,
			DX12::DescriptorTable const& globalSRV_Arr_GBuffer_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_WorldToNDC_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_ScreenToWorld_
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

		DX12::RenderPass RenderPass_{};
		DX12::Canvas Canvas_{};

		DX12::RootSignature RootSignature_{};
		DX12::Shader VertexShader_{};
		DX12::Shader PixelShader_{};
		DX12::GraphicsPSO GraphicsPSO_{};

		DX12::DescriptorTable GlobalTable_{};

		DX12::UploadBuffer UB_Arr_PointLight_{};
		DX12::UploadBuffer UB_Arr_WorldMatrix_LightSphere_{};
		DX12::UploadBuffer UB_Arr_Index_ActivePointLight_{};

		DX12::UploadBuffer UB_Vertices_LightSphere_{};
		DX12::UploadBuffer UB_Indices_LightSphere_{};
		DX12::VBV VBV_LightSphere_{};
		DX12::IBV IBV_LightSphere_{};
		uint32_t Num_IndicesPerSphere_{};
	};
}