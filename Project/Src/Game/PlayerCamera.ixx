export module Game.PlayerCamera;

import Lumina.DX12;
import Lumina.DX12.Aux.View;

import Lumina.Math;

namespace Game {
	/// <summary>
	/// Camera that tracks a target
	/// </summary>
	export struct TrackingCamera {
		Lumina::Mat4 View;
		Lumina::Mat4 Projection;
		Lumina::Mat4 VP;

		Lumina::Vec3 Position;
		Lumina::Vec3 Target;
		Lumina::Vec3 OffsetFromTarget;
		Lumina::Vec3 Up{ 0.0f, 1.0f, 0.0f };

		float DelayFactor;
		float SwayAmpFactor;
		float SwayFreqFactor;
		float SwayTimeFactor;

		Lumina::DX12::UploadBuffer VPUploadBuffer{};
		Lumina::DX12::DescriptorHeap CBVHeap{};

		TrackingCamera(
			Lumina::DX12::GraphicsDevice const& device_
		) {
			Projection = Lumina::Mat4::PerspectiveFOV(
				0.45f,
				1280.0f / 720.0f,
				0.1f,
				200.0f
			);

			constexpr uint64_t vpBufferSize{ (sizeof(Lumina::Mat4) + 0xFF) & ~0xFF };
			VPUploadBuffer.Initialize(device_, vpBufferSize, "ViewProjection");

			CBVHeap.Initialize(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1U, false);
			Lumina::DX12::CBV::Create(device_, CBVHeap.CPUHandle(0U), VPUploadBuffer);
		}

		void Update() {

		}
	};
}