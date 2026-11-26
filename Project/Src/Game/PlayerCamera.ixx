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

		// 視点の左右の向き（ラジアン）。Player::Angle と同じ測り方（前方 = (cos, sin)、右へ回すと減る）。
		// マウス・右スティックで回す（InGame の Update）。以前はプレイヤーの向きをそのまま使っていた
		float Yaw{ 0.0f };

		// カメラが地面の上で見ている向き（XZ、長さ 1）。プレイヤーの移動の基準に使う。
		// OffsetFromTarget の横ずれ（右へ 5）も含めた「画面の奥」の向きで、UpdatePlayerCamera の回し方
		// （OffsetFromTarget × RotateAround(Up, -Yaw + π/2)、行ベクトル）と同じ式。揺れ（Sway）と追従の遅れは含めない
		Lumina::Vec2 GroundForward() const noexcept {
			float const theta{ -Yaw + std::numbers::pi_v<float> * 0.5f };
			float const c{ std::cos(theta) };
			float const s{ std::sin(theta) };
			float const offX{ OffsetFromTarget.x * c + OffsetFromTarget.z * s };
			float const offZ{ -OffsetFromTarget.x * s + OffsetFromTarget.z * c };
			float const len{ std::sqrt(offX * offX + offZ * offZ) };
			if (!(len > 0.0f)) { return Lumina::Vec2{ std::cos(Yaw), std::sin(Yaw) }; }
			return Lumina::Vec2{ -offX / len, -offZ / len };
		}

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