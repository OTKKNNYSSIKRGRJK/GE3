module Game.Scene.InGame;

import : Impl;

import Lumina;
import Game.GameOverShatter;
import Game.CombatVfx;
import Game.SceneLight;

namespace Game::Scene::Impl {
	template<>
	void InGame::Render(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_
	) {
		{
			Lumina::Mat4 meshWorld{
				Lumina::Mat4::SRT(
					Player_->ModelScale() * 2.0f,
					Player_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
					Player_->ModelTranslate()
				)
			};
			Lumina::Mat4 tr_INV_MeshWorld{
				meshWorld.Inv()
			};
			Lumina::Mat4::Transpose(tr_INV_MeshWorld, tr_INV_MeshWorld);

			Lumina::Mat4 wvp{
				meshWorld * PlayerCamera_->VP
			};

			UB_Transforms_.Store(
				&wvp,
				sizeof(Lumina::Mat4),
				0LLU
			);
			UB_Transforms_.Store(
				&meshWorld,
				sizeof(Lumina::Mat4),
				sizeof(Lumina::Mat4)
			);
			UB_Transforms_.Store(
				&tr_INV_MeshWorld,
				sizeof(Lumina::Mat4),
				sizeof(Lumina::Mat4) * 2
			);
		}

		{
			Lumina::Mat4 meshWorld{
				Lumina::Mat4::SRT(
					Boss_Kinoko_->ModelScale() * 2.0f,
					Boss_Kinoko_->ModelRotate() + Lumina::Vec3{ 0.0f, 1.57f, 0.0f },
					Boss_Kinoko_->ModelTranslate()
				)
			};
			Lumina::Mat4 tr_INV_MeshWorld{
				meshWorld.Inv()
			};
			Lumina::Mat4::Transpose(tr_INV_MeshWorld, tr_INV_MeshWorld);

			Lumina::Mat4 wvp{
				meshWorld * PlayerCamera_->VP
			};
			UB_Transforms2_.Store(
				&wvp,
				sizeof(Lumina::Mat4),
				0LLU
			);
			UB_Transforms2_.Store(
				&meshWorld,
				sizeof(Lumina::Mat4),
				sizeof(Lumina::Mat4)
			);
			UB_Transforms2_.Store(
				&tr_INV_MeshWorld,
				sizeof(Lumina::Mat4),
				sizeof(Lumina::Mat4) * 2
			);
		}

		D3D12_RESOURCE_BARRIER const barriers_PreGeometryPass[]{
			 Lumina::DX12::Barrier::Transition(
				 Canvas_.RenderTexture(0U),
				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				 D3D12_RESOURCE_STATE_RENDER_TARGET
			 ),
			 Lumina::DX12::Barrier::Transition(
				 Canvas_.RenderTexture(1U),
				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				 D3D12_RESOURCE_STATE_RENDER_TARGET
			 ),
			 Lumina::DX12::Barrier::Transition(
				 Canvas_.DepthTexture(),
				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				 D3D12_RESOURCE_STATE_DEPTH_WRITE
			 ),
		};
		cmdList_->ResourceBarrier(3U, barriers_PreGeometryPass);

		cmdList_->RSSetViewports(
			Canvas_.Num_RenderTargets(),
			Canvas_.Viewports().data()
		);
		cmdList_->RSSetScissorRects(
			Canvas_.Num_RenderTargets(),
			Canvas_.ScissorRects().data()
		);

		/*auto&& playerLocalToWorld{
			Lumina::Mat4::SRT(Player_->ModelScale(), Player_->ModelRotate(), Player_->ModelTranslate())
		};*/

		/*Lumina::Mat4 kinokoShear{};
		kinokoShear[1][0] = Boss_Kinoko_->ModelShearOnXZPlane().x;
		kinokoShear[1][2] = Boss_Kinoko_->ModelShearOnXZPlane().z;
		auto&& bossLocalToWorld{
			kinokoShear * Lumina::Mat4::SRT(Boss_Kinoko_->ModelScale(), Boss_Kinoko_->ModelRotate(), Boss_Kinoko_->ModelTranslate())
		};*/

		Lumina::Mat4 weaponWorld{ World_Hands[1] };
		weaponWorld =
			Lumina::Mat4::SRT({ 0.125f, 0.5f, 0.125f }, { -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }) *
			weaponWorld;

		// VfxLoop の窓のエフェクト。ソケット（刃先）は武器メッシュと同じ行列で変換する（刃と光がずれないように）
		// （倒れている間はアニメーションイベントの時計が止まり、途中の光線が出たまま残るので、空にして渡す）
		AnimEventVfx_.Prepare(
			GameOver_.IsActive() ? decltype(PlayerAnimEvents_.VfxTracks()){} : PlayerAnimEvents_.VfxTracks(),
			weaponWorld,
			PlayerCamera_->VP
		);

		MeshManager_->Begin(cmdList_);
		MeshManager_->BatchBegin();
		/*MeshManager_->Batch(
			MeshShaderAssets_[0],
			1U,
			LocalHeap_Materials_.CPUHandle(0U),
			playerLocalToWorld
		);*/
		MeshManager_->Batch(
			MeshShaderAssets_[1],
			1U,
			LocalHeap_Materials_.CPUHandle(1U),
			weaponWorld
		);
		// ボスの武器（プレイヤーと同じメッシュ・同じ SRT。右手は Update の Tag_HoldBossWeapon で作ってある）
		Lumina::Mat4 const bossWeaponWorld{
			Lumina::Mat4::SRT({ 0.125f, 0.5f, 0.125f }, { -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }) *
			World_BossHand_
		};
		MeshManager_->Batch(
			MeshShaderAssets_[1],
			1U,
			LocalHeap_Materials_.CPUHandle(1U),
			bossWeaponWorld
		);
		/*weaponWorld[3][0] = WorldPos_Weapon_.x;
		weaponWorld[3][1] = WorldPos_Weapon_.y;
		weaponWorld[3][2] = WorldPos_Weapon_.z;
		MeshManager_->Batch(
			MeshShaderAssets_[1],
			1U,
			LocalHeap_Materials_.CPUHandle(1U),
			weaponWorld
		);*/
		MeshManager_->BatchEnd();

		DeferredGeometryPass_.Begin(cmdList_);
		{
			MeshManager_->Render(
				GraphicsPSO_MeshDeferredGeometry_,
				GlobalTable_SRV_ImageTexture_.GPUHandle(0U),
				LocalHeap_Scene_.CPUHandle(0U)
			);

			Grassland_->Render(dxContext_, cmdList_);
		}
		DeferredGeometryPass_.End();

		MeshManager_->End();

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{
			DeferredGeometryPass_.RenderTarget(0).View(),
			DeferredGeometryPass_.RenderTarget(1).View()
		};
		cmdList_->OMSetRenderTargets(2U, rtvs, false, &DeferredGeometryPass_.DepthStencil().View());

		cmdList_->SetGraphicsRootSignature(RS_Skinning_.Get());
		cmdList_->SetPipelineState(GraphicsPSO_SkinnedMeshDeferredGeometry_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (Player_) {
			cmdList_->SetGraphicsRootDescriptorTable(0U, GlobalTable_CBV_Scene_.GPUHandle(0U));
			cmdList_->SetGraphicsRootDescriptorTable(1U, PlayerSkinnedInstance_->SkinCluster_.PaletteSRVHandle.second);
			cmdList_->SetGraphicsRootDescriptorTable(2U, GlobalTable_Materials_.GPUHandle(0U));
			cmdList_->SetGraphicsRootDescriptorTable(3U, GlobalTable_SRV_ImageTexture_.GPUHandle(0U));
			cmdList_->SetGraphicsRootDescriptorTable(5U, Skybox_->GlobalTable().GPUHandle(0U));
			// カメラ位置＋ゲームオーバーのディゾルブ（12 個）。ディゾルブが無いときは閾値が -10 で何も消えない
			auto const playerConstants{ Shatter_.PlayerConstants(PlayerCamera_->Position) };
			cmdList_->SetGraphicsRoot32BitConstants(6U, Game::GameOverShatter::NumConstants, &playerConstants, 0U);

			D3D12_VERTEX_BUFFER_VIEW const vbvs[2]{
				reinterpret_cast<D3D12_VERTEX_BUFFER_VIEW const&>(PlayerSkinnedModel_->VBV_),
				reinterpret_cast<D3D12_VERTEX_BUFFER_VIEW const&>(PlayerSkinnedInstance_->SkinCluster_.InfluenceBufferView)
			};
			cmdList_->IASetVertexBuffers(0, 2, vbvs);
			cmdList_->IASetIndexBuffer(reinterpret_cast<D3D12_INDEX_BUFFER_VIEW const*>(&PlayerSkinnedModel_->IBV_));
			cmdList_->DrawIndexedInstanced(
				static_cast<uint32_t>(PlayerSkinnedModel_->Collection_.Meshes[0].Indices.size()),
				1U, 0U, 0U, 0U
			);
		}

		if (Boss_Kinoko_) {
			cmdList_->SetGraphicsRootDescriptorTable(0U, GlobalTable_CBV_Scene_.GPUHandle(1U));
			cmdList_->SetGraphicsRootDescriptorTable(1U, BossSkinnedInstance_->SkinCluster_.PaletteSRVHandle.second);
			cmdList_->SetGraphicsRootDescriptorTable(2U, GlobalTable_Materials_.GPUHandle(1U));
			cmdList_->SetGraphicsRootDescriptorTable(3U, GlobalTable_SRV_ImageTexture_.GPUHandle(0U));
			cmdList_->SetGraphicsRootDescriptorTable(5U, Skybox_->GlobalTable().GPUHandle(0U));
			// 勝利演出のディゾルブ（演出中でなければ閾値 -10 で何も消えない）
			auto const bossConstants{ BossShatter_.PlayerConstants(PlayerCamera_->Position) };
			cmdList_->SetGraphicsRoot32BitConstants(6U, Game::GameOverShatter::NumConstants, &bossConstants, 0U);

			D3D12_VERTEX_BUFFER_VIEW const vbvs[2]{
				reinterpret_cast<D3D12_VERTEX_BUFFER_VIEW const&>(PlayerSkinnedModel_->VBV_),
				reinterpret_cast<D3D12_VERTEX_BUFFER_VIEW const&>(BossSkinnedInstance_->SkinCluster_.InfluenceBufferView)
			};
			cmdList_->IASetVertexBuffers(0, 2, vbvs);
			cmdList_->IASetIndexBuffer(reinterpret_cast<D3D12_INDEX_BUFFER_VIEW const*>(&PlayerSkinnedModel_->IBV_));
			cmdList_->DrawIndexedInstanced(
				static_cast<uint32_t>(PlayerSkinnedModel_->Collection_.Meshes[0].Indices.size()),
				1U, 0U, 0U, 0U
			);
		}

		Skybox_->Render(cmdList_, GlobalTable_CBV_Scene_);

		SimpleFX3_->Render(cmdList_, PlayerCamera_->VP, 1U);

		D3D12_RESOURCE_BARRIER const barriers_PostGeometryPass[]{
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(0U),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(1U),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
			Lumina::DX12::Barrier::Transition(
				Canvas_.DepthTexture(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
		};
		cmdList_->ResourceBarrier(3U, barriers_PostGeometryPass);

		//----	------	------	------	------	----//

		DeferredLighting_->Render(
			dxContext_.Device(),
			cmdList_,
			GlobalTable_SRV_CanvasTexture_,
			LocalHeap_Scene_.CPUHandle(0U),
			LocalHeap_Scene_.CPUHandle(2U)
		);

		//----	------	------	------	------	----//

		/*D3D12_RESOURCE_BARRIER const barriers_PrePostProcessingPass[]{
			Lumina::DX12::Barrier::Transition(
				DeferredLighting_.RenderTexture(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
		};
		cmdList_->ResourceBarrier(1U, barriers_PrePostProcessingPass);

		D3D12_RESOURCE_BARRIER const barriers_PostPostProcessing[]{
			Lumina::DX12::Barrier::Transition(
				RCT_PostProcessing_,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS
			),
			Lumina::DX12::Barrier::Transition(
				RCT_PostProcessing_,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
		};*/

		PrimitiveManager_->Begin(cmdList_);
		PrimitiveManager_->BatchTriangle(
			{ { -1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, 3U },
			{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, 3U },
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, 3U }
		);
		PrimitiveManager_->BatchTriangle(
			{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, 3U },
			{ { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, 3U },
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, 3U }
		);
		PrimitiveManager_->End(cmdList_);

		D3D12_RESOURCE_BARRIER const barriers_PreMergePass[]{
			 Lumina::DX12::Barrier::Transition(
				 Canvas_Merge_.RenderTexture(0U),
				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				 D3D12_RESOURCE_STATE_RENDER_TARGET
			 ),
		};
		D3D12_RESOURCE_BARRIER const barriers_PostMergePass[]{
			 Lumina::DX12::Barrier::Transition(
				 Canvas_Merge_.RenderTexture(0U),
				 D3D12_RESOURCE_STATE_RENDER_TARGET,
				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			 ),
		};
		cmdList_->ResourceBarrier(1U, barriers_PreMergePass);

		cmdList_->RSSetViewports(
			Canvas_Merge_.Num_RenderTargets(),
			Canvas_Merge_.Viewports().data()
		);
		cmdList_->RSSetScissorRects(
			Canvas_Merge_.Num_RenderTargets(),
			Canvas_Merge_.ScissorRects().data()
		);

		MergePass_.Begin(cmdList_);
		{
			PrimitiveManager_->Render(
				cmdList_,
				GlobalTable_SRV_CanvasTexture_,
				{},
				1,
				GlobalTable_CBV_PostProcessing_.GPUHandle(1U)
			);

			// 平行光 1 本＋環境光（加算）。ライティングの結果（点光源だけ）に足す。G-buffer は SRV 0〜2（アルベド・法線・深度）
			SceneLight_.Render(cmdList_, GlobalTable_SRV_CanvasTexture_.GPUHandle(0U));
			
			AmbientSparkles_->Render(
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			PlayerEffects_->Render(
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			KnockEffects_->Render(
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			// ゲームオーバーで体が砕けて舞う粒子（グループごとに別の ParticleSystem。同じ加算 PSO）
			// 色抜けを掛ける設定のグループだけ、ここ（MergePass）で描く。既定の「掛けない」グループは UIPass の後ろ
			Shatter_.Render(
				Game::GameOverShatter::Layer::Graded,
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			BossShatter_.Render(		// 勝利演出でボスが砕ける粒子（同じ設定）
				Game::GameOverShatter::Layer::Graded,
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			PlayerBullets_->Render(
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			BossBullets_->Render(
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);

			// 戦闘の仮エフェクト: 粒子（本編の粒子と同じ RS / PSO）→ 帯・輪・弧（FxMesh、加算。深度は SRV 2 番を読む）
			CombatVfx_.RenderParticles(
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			CombatVfx_.RenderMesh(cmdList_, PlayerCamera_->VP, 0.1f, 200.0f, GlobalTable_SRV_CanvasTexture_.GPUHandle(2U));

			// 刃先の放射光線（加算・スクリーン空間）。この後のポストでヒット時のゆがみが掛かる
			AnimEventVfx_.Render(cmdList_);
		}
		MergePass_.End();

		cmdList_->ResourceBarrier(1U, barriers_PostMergePass);

		PrimitiveManager1_->Begin(cmdList_);
		PrimitiveManager1_->BatchTriangle(
			{ { -1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, 4U },
			{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, 4U },
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, 4U }
		);
		PrimitiveManager1_->BatchTriangle(
			{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, 4U },
			{ { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, 4U },
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, 4U }
		);
		PrimitiveManager1_->End(cmdList_);

		D3D12_RESOURCE_BARRIER const barriers_PrePostProcessingPass[]{
			 Lumina::DX12::Barrier::Transition(
				 Canvas_PostProcessing_.RenderTexture(0U),
				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				 D3D12_RESOURCE_STATE_RENDER_TARGET
			 ),
		};
		D3D12_RESOURCE_BARRIER const barriers_PostPostProcessingPass[]{
			 Lumina::DX12::Barrier::Transition(
				 Canvas_PostProcessing_.RenderTexture(0U),
				 D3D12_RESOURCE_STATE_RENDER_TARGET,
				 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			 ),
		};

		cmdList_->ResourceBarrier(1U, barriers_PrePostProcessingPass);

		cmdList_->RSSetViewports(
			Canvas_PostProcessing_.Num_RenderTargets(),
			Canvas_PostProcessing_.Viewports().data()
		);
		cmdList_->RSSetScissorRects(
			Canvas_PostProcessing_.Num_RenderTargets(),
			Canvas_PostProcessing_.ScissorRects().data()
		);

		PostProcessingPass_.Begin(cmdList_);
		{
			PrimitiveManager1_->Render(
				cmdList_,
				GlobalTable_SRV_CanvasTexture_,
				{},
				1,
				GlobalTable_CBV_PostProcessing_.GPUHandle(0U)
			);
		}
		PostProcessingPass_.End();

		cmdList_->ResourceBarrier(1U, barriers_PostPostProcessingPass);

		// VS バナーの中身（キャラの寄り）を専用の RT に描く（バナーが見えている間だけ）。
		// RT・ビューポート・シザーを切り替えるので、UIPass の前に呼んでビューポートとシザーを画面全体に戻す
		VsBanner_.RenderPortraits(cmdList_);
		cmdList_->RSSetViewports(1U, &Canvas_PostProcessing_.Viewport(0U));
		cmdList_->RSSetScissorRects(1U, &Canvas_PostProcessing_.ScissorRect(0U));

		PrimitiveManager2_->Begin(cmdList_);
		PrimitiveManager2_->BatchTriangle(
			{ { -1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, 5U },
			{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, 5U },
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, 5U }
		);
		PrimitiveManager2_->BatchTriangle(
			{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, 5U },
			{ { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, 5U },
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, 5U }
		);
		PrimitiveManager2_->End(cmdList_);

		auto const& swapChain{ dxContext_.SwapChain() };

		// ゲームオーバーの演出中は HP バーをフェードアウトして消す（元の不透明度は Initialize.cpp の RGBA と同じ 0.25）
		// 勝利演出の色抜けでも消える（小さい方。<algorithm> を入れていないので手で）
		float const uiAlpha{ (GradeFx_.UiAlpha() < VictoryGradeFx_.UiAlpha()) ? GradeFx_.UiAlpha() : VictoryGradeFx_.UiAlpha() };
		UI_PlayerHPBar_.RGBA({ 1.0f, 1.0f, 1.0f, 0.25f * uiAlpha });
		UI_BossHPBar_.RGBA({ 1.0f, 1.0f, 1.0f, 0.25f * uiAlpha });
		UI_PlayerHPLabel_.RGBA({ 1.0f, 1.0f, 1.0f, 0.8f * uiAlpha });
		UI_BossHPLabel_.RGBA({ 1.0f, 1.0f, 1.0f, 0.8f * uiAlpha });

		SpriteRenderer_->Begin(cmdList_);
		// バッチが 1 つも無いときは BatchBegin しない（BatchBegin は転送先を COPY_DEST にし、BatchEnd は 0 個だと戻さないため）
		bool const showHPBars{ uiAlpha > 0.0f && !GameOver_.IsRewinding() };		// 巻き戻し中は出さない（再開してからの VS バナーで伸びてくる）
		bool const showWin{ false };		// 旧「YOU WIN」画像（UI_WIN_）は使わない。勝利演出のラベル（ResultRenderer_）に置き換えた
		if (showHPBars || showWin) {
			SpriteRenderer_->BatchBegin();
			if (showHPBars) {
				SpriteRenderer_->Batch(UI_PlayerHPBar_);
				SpriteRenderer_->Batch(UI_BossHPBar_);
				SpriteRenderer_->Batch(UI_PlayerHPLabel_);
				SpriteRenderer_->Batch(UI_BossHPLabel_);
			}
			if (showWin) {
				SpriteRenderer_->Batch(UI_WIN_);
			}
			SpriteRenderer_->BatchEnd();
		}

		// 結果表示（LOSE）は別のレンダラで、色抜けとぼかしの後ろ（バックバッファ直描き）に描く。中間 RT に入れるとぼかしでぼけるため。
		// 演出の時計に合わせてフェードイン（演出中でなければ不透明度 1）。ポーズメニューが開いている間は出さない（メニューと重なるため）
		// 「Game Over」と「Press Space to Continue」は、それぞれ演出の時計に合わせてフェードイン（演出中でなければ不透明度 1）
		float const gameOverAlpha{ GameOver_.ResultAlpha() };
		float const promptAlpha{ GameOver_.PromptAlpha() };
		bool const showResult{ (CurrentPhase_ == PHASE_LOSE || GameOver_.IsActive() || GameOver_.ForceLabels) && (gameOverAlpha > 0.0f || promptAlpha > 0.0f) && !PauseMenu_.IsBlockingGame() };
		// 勝利演出のラベル（「Victory」「Press Space to Return to Title」）も同じ作り。値は Victory_ の Look（GameOverY = ラベルの y）
		float const victoryAlpha{ Victory_.ResultAlpha() };
		float const victoryPromptAlpha{ Victory_.PromptAlpha() };
		bool const showVictory{ (CurrentPhase_ == PHASE_WIN || Victory_.IsActive() || Victory_.ForceLabels) && (victoryAlpha > 0.0f || victoryPromptAlpha > 0.0f) && !PauseMenu_.IsBlockingGame() };
		// 暗転（勝利演出の続行）。ラベルより手前、一番最後に重ねる
		float const exitAlpha{ Victory_.ExitAlpha() };
		bool const showExitFade{ exitAlpha > 0.0f };
		bool const anyResultSprite{ showResult || showVictory || showExitFade };
		ResultRenderer_->Begin(cmdList_);
		if (anyResultSprite) {
			ResultRenderer_->BatchBegin();
			if (showResult) {
				auto const& look{ GameOver_.Look };
				auto const labelSize{ GameOver_.LabelSize() };
				auto const promptSize{ GameOver_.PromptSize() };
				UI_GAMEOVER_.Translate({ 640.0f, look.GameOverY });
				UI_GAMEOVER_.Scale({ labelSize.x, labelSize.y });
				UI_GAMEOVER_.RGBA({ 1.0f, 1.0f, 1.0f, gameOverAlpha });
				UI_GAMEOVER_PROMPT_.Translate({ 640.0f, look.PromptY });
				UI_GAMEOVER_PROMPT_.Scale({ promptSize.x, promptSize.y });
				UI_GAMEOVER_PROMPT_.RGBA({ 1.0f, 1.0f, 1.0f, promptAlpha });
				ResultRenderer_->Batch(UI_GAMEOVER_);
				ResultRenderer_->Batch(UI_GAMEOVER_PROMPT_);
			}
			if (showVictory) {
				auto const& look{ Victory_.Look };
				auto const labelSize{ Victory_.LabelSize() };
				auto const promptSize{ Victory_.PromptSize() };
				UI_VICTORY_.Translate({ 640.0f, look.GameOverY });
				UI_VICTORY_.Scale({ labelSize.x, labelSize.y });
				UI_VICTORY_.RGBA({ 1.0f, 1.0f, 1.0f, victoryAlpha });
				UI_VICTORY_PROMPT_.Translate({ 640.0f, look.PromptY });
				UI_VICTORY_PROMPT_.Scale({ promptSize.x, promptSize.y });
				UI_VICTORY_PROMPT_.RGBA({ 1.0f, 1.0f, 1.0f, victoryPromptAlpha });
				ResultRenderer_->Batch(UI_VICTORY_);
				ResultRenderer_->Batch(UI_VICTORY_PROMPT_);
			}
			if (showExitFade) {
				UI_EXIT_FADE_.RGBA({ 0.0f, 0.0f, 0.0f, exitAlpha });
				ResultRenderer_->Batch(UI_EXIT_FADE_);
			}
			ResultRenderer_->BatchEnd();
		}

		// ポーズメニューのスプライトも、コピーを積むのでレンダーパスの外で写しておく。
		// 閉じているときは右下の「ESC : ポーズ」（STARTUP の間は出さない）
		// 右下のヒント。「ESC : ポーズ」は STARTUP 以外、「Q : 自害」は Q が効く IN_BATTLE の間だけ
		PauseMenu_.Prepare(cmdList_, CurrentPhase_ != PHASE_STARTUP && uiAlpha >= 1.0f && !GameOver_.IsRewinding(), CurrentPhase_ == PHASE_IN_BATTLE && uiAlpha >= 1.0f);

		/*auto const& swapChain{ dxContext_.SwapChain() };
		auto rtv{ swapChain.BackBufferRTVCPUHandle() };
		auto dsv{ swapChain.DSVCPUHandle() };
		PostProcessingPass_.RenderTarget(0U).View() = rtv;
		PostProcessingPass_.DepthStencil().View() = dsv;*/

		// ゲームオーバーの色抜けの間だけ、UI まで中間 RT に描き、UIPass の後ろでバックバッファへ色抜けして写す。
		// それ以外は今までどおりバックバッファへ直接描く
		// ポーズメニューが開いている間は、チャンネルスプリットとぼかしを止める（メニューの文字が読めなくなるため）
		// 勝利演出の間は 2 台目の色抜け（VictoryGradeFx_）を掛ける。以下の grade はどちらか一方
		auto& grade{ ActiveGrade() };
		grade.SuppressFilters(PauseMenu_.IsBlockingGame());
		GradeFx_.SetSpin(GameOver_.SpinRadians(), GameOver_.Look.SpinFocus);		// 巻き戻し中の回転ぼかし（それ以外は 0）

		// チャンネルスプリットの中心：プレイヤー（体の真ん中あたり）の画面上の位置。見えない（カメラの後ろ）ときは前の値のまま。
		// 勝利演出の間はボス（倒れる方）
		{
			auto const focusFeet{ Victory_.IsActive() ? Boss_Kinoko_->ModelTranslate() : Player_->ModelTranslate() };
			Lumina::Vec4 const world{ focusFeet.x, focusFeet.y + 2.7f, focusFeet.z, 1.0f };
			Lumina::Vec4 const clip{ world * PlayerCamera_->VP };
			if (clip.w > 1.0e-4f) {
				grade.SetFocus((clip.x / clip.w) * 0.5f + 0.5f, 0.5f - (clip.y / clip.w) * 0.5f);
			}
		}
		bool const gradeActive{ grade.IsActive() };
		if (gradeActive) {
			UIPass_.RenderTarget(0U).View() = grade.BeginSource(cmdList_);
		}
		else {
			UIPass_.RenderTarget(0U).View() = swapChain.BackBufferRTVCPUHandle();
		}

		UIPass_.Begin(cmdList_);
		PrimitiveManager2_->Render(
			cmdList_,
			GlobalTable_SRV_CanvasTexture_,
			{},
			1,
			GlobalTable_CBV_PostProcessing_.GPUHandle(1U)
		);
		SpriteRenderer_->Render(
			PSO_SpriteUI_,
			GlobalTable_SRV_ImageTexture_.GPUHandle(0U),
			LocalHeap_OrthoProj_.CPUHandle(0U)
		);
		// バトル開始の VS バナー（プリマルチプライド α）。HP バーより手前
		VsBanner_.Render(cmdList_);
#if defined(_DEBUG)
		if (ShowColliders_) {
			CollisionDebug_.Draw(CollisionWorld_);
			CollisionDebug_.Render(cmdList_, PlayerCamera_->VP);
		}
#endif
		// ポーズメニュー（開いている間だけ。暗幕 → ラベル、アルファ合成）。一番手前
		PauseMenu_.Render();
		UIPass_.End();

		SpriteRenderer_->End();

		if (gradeActive) {
			grade.Apply(dxContext_, cmdList_);
		}

		// 色抜けを掛けない粒子（Group::BypassGrade）は、色抜けのあとにバックバッファへ直接描く（粒子の色がそのまま見える）。
		// 深度は MergePass と同じ SRV で読む（遠くの物に隠れる判定）
		{
			D3D12_CPU_DESCRIPTOR_HANDLE const backBufferRTV{ swapChain.BackBufferRTVCPUHandle() };
			cmdList_->OMSetRenderTargets(1U, &backBufferRTV, false, nullptr);
			cmdList_->RSSetViewports(1U, Canvas_Merge_.Viewports().data());
			cmdList_->RSSetScissorRects(1U, Canvas_Merge_.ScissorRects().data());
			Shatter_.Render(
				Game::GameOverShatter::Layer::Direct,
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			BossShatter_.Render(
				Game::GameOverShatter::Layer::Direct,
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);
			// ラベル（Game Over / Victory と続行の案内）と暗転。色抜け・ぼかしを通さず、バックバッファへそのまま描く。
			// 2026-11-17：粒子の後ろへ移した（暗転が粒子も覆うように。ラベルは粒子より手前になった）
			if (anyResultSprite) {
				ResultRenderer_->Render(
					PSO_SpriteResult_,
					GlobalTable_SRV_ImageTexture_.GPUHandle(0U),
					LocalHeap_OrthoProj_.CPUHandle(0U)
				);
			}
			ResultRenderer_->End();
		}
	}
}

namespace Game::Scene {
	void InGame::Render() {
		Impl_->Render(Frame_.DX12, Frame_.CmdList);
	}
}
