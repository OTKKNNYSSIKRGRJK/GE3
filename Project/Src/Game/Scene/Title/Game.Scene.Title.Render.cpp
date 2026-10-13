module Game.Scene.Title;

import <cmath>;
import <span>;
import <d3d12.h>;

import : Impl;

import Lumina;
import Lumina.FirePillar;
import Lumina.MagicCircle;
import Lumina.ShowcaseCharacter;

namespace Game::Scene::Impl {
	namespace {
		// 左手系・行ベクトル（InGame の LookAt と同じ）
		Lumina::Mat4 LookAt(
			Lumina::Vec3 const& src_,
			Lumina::Vec3 const& dst_,
			Lumina::Vec3 const& up_
		) {
			Lumina::Vec3 const forward{ Lumina::Vec3{ dst_ - src_ }.Unit() };
			Lumina::Vec3 const right{ Lumina::Vec3::Cross(up_, forward).Unit() };
			Lumina::Vec3 const up{ Lumina::Vec3::Cross(forward, right) };

			return {
				right.x, up.x, forward.x, 0.0f,
				right.y, up.y, forward.y, 0.0f,
				right.z, up.z, forward.z, 0.0f,
				-Lumina::Vec3::Dot(src_, right),
				-Lumina::Vec3::Dot(src_, up),
				-Lumina::Vec3::Dot(src_, forward),
				1.0f,
			};
		}
	}

	// Camera（Yaw = 0 で -Z 側から +Z を向く。注視点はキャラの足元基準）
	// Update（パーティクルのビルボード用）と Render の両方から呼ぶ
	void Title::BuildCamera(Lumina::Mat4& view_, Lumina::Mat4& viewProj_, Lumina::Vec3& eye_) const {
		Lumina::Vec3 const target{
			Character_.Position.x,
			Character_.Position.y + CameraTargetY_,
			Character_.Position.z,
		};
		eye_ = Lumina::Vec3{
			target.x + std::sin(CameraYaw_) * std::cos(CameraPitch_) * CameraDistance_,
			target.y + std::sin(CameraPitch_) * CameraDistance_,
			target.z - std::cos(CameraYaw_) * std::cos(CameraPitch_) * CameraDistance_,
		};
		view_ = LookAt(eye_, target, Lumina::Vec3{ 0.0f, 1.0f, 0.0f });
		Lumina::Mat4 const proj{ Lumina::Mat4::PerspectiveFOV(CameraFovY_, 1280.0f / 720.0f, 0.1f, 200.0f) };
		viewProj_ = view_ * proj;
	}

	template<>
	void Title::Render(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_
	) {
		Lumina::Mat4 view{};
		Lumina::Mat4 viewProj{};
		Lumina::Vec3 eye{};
		BuildCamera(view, viewProj, eye);

		//----	------	------	------	------	----//
		// バックバッファ＋スワップチェインの深度に直接描く
		// （BeginFrame はクリアしないのでここでクリアする。Viewport/Scissor は BeginFrame で設定済み）

		auto const& swapChain{ dxContext_.SwapChain() };
		D3D12_CPU_DESCRIPTOR_HANDLE const rtv{ swapChain.BackBufferRTVCPUHandle() };
		D3D12_CPU_DESCRIPTOR_HANDLE const dsv{ swapChain.DSVCPUHandle() };

		cmdList_->OMSetRenderTargets(1U, &rtv, false, &dsv);
		cmdList_->ClearRenderTargetView(rtv, ClearColor_, 0U, nullptr);
		cmdList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0U, 0U, nullptr);

		//----	------	------	------	------	----//
		// 1. Character（不透明・深度書き込みあり）

		Character_.Render(cmdList_, viewProj, eye, Lighting_);

		//----	------	------	------	------	----//
		// 2. MagicCircle（加算・深度テストのみ。キャラの足が手前なら隠れる）

		MagicCircleRenderer_.Render(
			cmdList_,
			std::span<Lumina::MagicCircle::Layer const>{ MagicCircleLayers_ },
			Character_.Position,
			Time_,
			MagicCircleIntensity_,
			viewProj
		);

		//----	------	------	------	------	----//
		// 3. FirePillar（深度テストのみ。奥の壁はキャラに隠れ、手前の壁がキャラに半透明で被さる）
		//    FirePillarInstances_ は Update でエントリの並び順（外側→内側）に作ってある

		FirePillarRenderer_.Render(
			cmdList_,
			std::span<Lumina::FlamePillar::Instance const>{ FirePillarInstances_ },
			viewProj,
			eye
		);

		//----	------	------	------	------	----//
		// 4. Particles（加算・深度テストのみ。ビルボード行列と VP は Update で作ってある）

		RenderParticles(cmdList_);

		//----	------	------	------	------	----//
		// 5. Menu UI（アルファ合成・深度なし。黒のフェードもここ）

		RenderUI(cmdList_);
	}
}

namespace Game::Scene {
	template<>
	void Title::Render(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_
	) {
		Impl_->Render(dxContext_, cmdList_);
	}
}
