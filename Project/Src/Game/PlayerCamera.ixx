export module Game.PlayerCamera;

import Lumina.DX12;
import Lumina.DX12.Aux.View;

import Lumina.Math;

namespace Game {
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

		float FovY{ 0.45f };		// 垂直 FOV（ラジアン）。Projection はこの値で作る（SetFov で変える）

		TrackingCamera(
			Lumina::DX12::GraphicsDevice const& device_
		) {
			SetFov(FovY);

			constexpr uint64_t vpBufferSize{ (sizeof(Lumina::Mat4) + 0xFF) & ~0xFF };
			VPUploadBuffer.Initialize(device_, vpBufferSize, "ViewProjection");

			CBVHeap.Initialize(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1U, false);
			Lumina::DX12::CBV::Create(device_, CBVHeap.CPUHandle(0U), VPUploadBuffer);
		}

		void SetFov(float fovY_) {
			FovY = fovY_;
			Projection = Lumina::Mat4::PerspectiveFOV(
				fovY_,
				1280.0f / 720.0f,
				0.1f,
				200.0f
			);
		}

		void Update() {

		}
	};
}