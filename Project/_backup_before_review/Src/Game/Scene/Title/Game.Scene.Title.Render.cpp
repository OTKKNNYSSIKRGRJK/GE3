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
			Character_.Position.x + CameraShake_.x,
			Character_.Position.y + CameraTargetY_ + CameraShake_.y,
			Character_.Position.z + CameraShake_.z,
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
		// 1〜4.5 は Title 用 Canvas（SRGB ＋ D24S8）に描く。黒と深度 1.0 でクリア済みになる
		// （Game.Scene.Title.Post.cpp。Viewport / Scissor も Canvas のものに設定する）

		BeginSceneCanvas(cmdList_);

		//----	------	------	------	------	----//
		// 1. Character（不透明・深度書き込みあり）

		Character_.Render(cmdList_, viewProj, eye, Lighting_);

		//----	------	------	------	------	----//
		// 2. MagicCircle（加算・深度テストのみ。キャラの足が手前なら隠れる）

		MagicCircleRenderer_.Render(
			cmdList_,
			std::span<Lumina::MagicCircle::Layer const>{ MagicCircleRenderLayers_ },
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
		// 4.5 Outro の放射光線（スクリーン空間・加算・深度なし。Outro 中で強さがあるときだけ。ポストが掛かる）

		RenderRays(cmdList_, RaysSlot_Tip);

		//----	------	------	------	------	----//
		// ポスト：Canvas → バックバッファ（割れ目・ズームブラー・色収差・簡易ブルーム）
		// ここから先の RT はバックバッファ、DSV はスワップチェインの深度

		EndSceneCanvasAndPost(dxContext_, cmdList_);

		//----	------	------	------	------	----//
		// 4.6 割れ目からの放射光線（バックバッファ。割れ目でずれないようにポストの後。区間 4 だけ）

		RenderRays(cmdList_, RaysSlot_Crack);

		//----	------	------	------	------	----//
		// 5. Menu UI（バックバッファ。アルファ合成・深度なし。黒のフェード・白のフラッシュ・白への抜けもここ）

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
