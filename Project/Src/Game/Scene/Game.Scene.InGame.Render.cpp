module Game.Scene.InGame;

import : Impl;

import Lumina;

namespace Game::Scene::Impl {
	template<>
	void InGame::Render(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_
	) {
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

		auto&& playerLocalToWorld{
			Lumina::Mat4::SRT(Player_->ModelScale(), Player_->ModelRotate(), Player_->ModelTranslate())
		};

		Lumina::Mat4 kinokoShear{};
		kinokoShear[1][0] = Boss_Kinoko_->ModelShearOnXZPlane().x;
		kinokoShear[1][2] = Boss_Kinoko_->ModelShearOnXZPlane().z;
		auto&& bossLocalToWorld{
			kinokoShear * Lumina::Mat4::SRT(Boss_Kinoko_->ModelScale(), Boss_Kinoko_->ModelRotate(), Boss_Kinoko_->ModelTranslate())
		};

		MeshManager_->Begin(cmdList_);
		MeshManager_->BatchBegin();
		MeshManager_->Batch(
			MeshShaderAssets_[0],
			1U,
			LocalHeap_Materials_.CPUHandle(0U),
			playerLocalToWorld
		);
		MeshManager_->Batch(
			MeshShaderAssets_[1],
			1U,
			LocalHeap_Materials_.CPUHandle(1U),
			bossLocalToWorld
		);
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
			PlayerBullets_->Render(
				cmdList_,
				RS_ParticleSystem_,
				GraphicsPSO_BasicParticle_AdditiveMode_,
				LocalHeap_Scene_.CPUHandle(1U),
				LocalHeap_Scene_.CPUHandle(0U),
				GlobalTable_SRV_ImageTexture_,
				GlobalTable_SRV_CanvasTexture_
			);

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

		SpriteRenderer_->Begin(cmdList_);
		SpriteRenderer_->BatchBegin();
		SpriteRenderer_->Batch(UI_PlayerHPBar_);
		SpriteRenderer_->Batch(UI_BossHPBar_);
		if (CurrentPhase_ == PHASE_WIN) {
			SpriteRenderer_->Batch(UI_WIN_);
		}
		else if (CurrentPhase_ == PHASE_LOSE) {
			SpriteRenderer_->Batch(UI_LOSE_);
		}
		SpriteRenderer_->BatchEnd();

		/*auto const& swapChain{ dxContext_.SwapChain() };
		auto rtv{ swapChain.BackBufferRTVCPUHandle() };
		auto dsv{ swapChain.DSVCPUHandle() };
		PostProcessingPass_.RenderTarget(0U).View() = rtv;
		PostProcessingPass_.DepthStencil().View() = dsv;*/

		UIPass_.RenderTarget(0U).View() = swapChain.BackBufferRTVCPUHandle();

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
		UIPass_.End();

		SpriteRenderer_->End();
	}
}

namespace Game::Scene {
	template<>
	void InGame::Render(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_
	) {
		Impl_->Render(dxContext_, cmdList_);
	}
}