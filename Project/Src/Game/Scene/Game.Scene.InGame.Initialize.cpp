module Game.Scene.InGame;

import <vector>;
import <unordered_map>;
import <string>;

import nlohmann.json;

import Lumina.Math;

import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;

import Lumina.AssetManager;

import Lumina.Utils.Data;
import Lumina.Utils.Data.Mesh;

import : Impl;

import Lumina.CG3D;
import Lumina.CG3D.Animation;

namespace Game::Scene::Impl {
	namespace {
		void InitializeCanvas(
			Lumina::DX12::Canvas& canvas_,
			Lumina::DX12::Context const& dxContext_
		) {
			auto const& device{ dxContext_.Device() };

			canvas_.AllocateTextures(2U, true);
			canvas_.RenderTexture(0U).Initialize(device, 1280U, 720U, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
			canvas_.RenderTexture(1U).Initialize(device, 1280U, 720U, DXGI_FORMAT_R8G8B8A8_UNORM);
			canvas_.DepthTexture().Initialize(device, 1280U, 720U);
			canvas_.TransitionResourceStates(device, dxContext_.DirectQueue());
			canvas_.CreateViews(device);
			canvas_.Viewport(0U) = D3D12_VIEWPORT{
				.TopLeftX{ 0.0f },
				.TopLeftY{ 0.0f },
				.Width{ 1280.0f },
				.Height{ 720.0f },
				.MinDepth{ 0.0f },
				.MaxDepth{ 1.0f },
			};
			canvas_.ScissorRect(0U) = D3D12_RECT{
				.left{ 0 },
				.top{ 0 },
				.right{ 1280 },
				.bottom{ 720 },
			};
			canvas_.Viewport(1U) = D3D12_VIEWPORT{
				.TopLeftX{ 0.0f },
				.TopLeftY{ 0.0f },
				.Width{ 0.0f },
				.Height{ 0.0f },
				.MinDepth{ 0.0f },
				.MaxDepth{ 1.0f },
			};
			canvas_.ScissorRect(1U) = D3D12_RECT{
				.left{ 640 },
				.top{ 360 },
				.right{ 1280 },
				.bottom{ 720 },
			};
		}
	}

	template<>
	void InGame::Initialize(
		Lumina::DX12::Context const& dxContext_,
		Lumina::AssetManager const& assetMngr_
	) {
		auto const& device{ dxContext_.Device() };
		auto& assetMngr{ const_cast<Lumina::AssetManager&>(assetMngr_) };

		CurrentPhase_ = PHASE_STARTUP;
		NextPhaseCountDown_ = 90;

		// Player
		{
			Player_.reset(new Player{});
			Player_->Initialize(Lumina::Vec3{ 10.0f, 0.25f, 0.0f });
		}

		// PlayerCamera
		{
			PlayerCamera_.reset(new TrackingCamera{ device });
			PlayerCamera_->OffsetFromTarget = { 5.0f, 15.0f, -40.0f };
			PlayerCamera_->DelayFactor = 0.15f;
			PlayerCamera_->SwayAmpFactor = 0.02f;
			PlayerCamera_->SwayFreqFactor = 0.01f;
			PlayerCamera_->SwayTimeFactor = 0.0f;
		}

		{
			Boss_Kinoko_.reset(new Boss_Kinoko{});
			Boss_Kinoko_->Initialize(Lumina::Vec3{ -10.0f, 0.0f, 0.0f });
		}

		// ImageTextures
		{
			std::vector<uint32_t> texIDs{};
			assetMngr.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "Particles", "Assets/Img/Particles.png" },
					{ "OCEAN", "Assets/Img/OCEAN.png" },
					{ "Kinoko", "Assets/Kinoko/Kinoko.png" },
					{ "Blank", "Assets/Img/Blank.png" },
					{ "Win", "Assets/Img/YouWin.png" },
					{ "Lose", "Assets/Img/YouLose.png" },
				}
			);

			GlobalTable_SRV_ImageTexture_ = dxContext_.GlobalDescriptorHeap().Allocate(32U);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_SRV_ImageTexture_.CPUHandle(idx),
					assetMngr.Graphics().CPUHandle(texIDs.at(idx)),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}
		}

		// Shader Constants
		{
			UB_ViewToWorld_.Initialize(device, 256LLU);
			UB_ScreenToWorld_.Initialize(device, 256LLU);

			LocalHeap_Scene_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16U, false);
			Lumina::DX12::CBV::Create(device, LocalHeap_Scene_.CPUHandle(0U), PlayerCamera_->VPUploadBuffer);
			Lumina::DX12::CBV::Create(device, LocalHeap_Scene_.CPUHandle(1U), UB_ViewToWorld_);
			Lumina::DX12::CBV::Create(device, LocalHeap_Scene_.CPUHandle(2U), UB_ScreenToWorld_);
		}

		// Materials
		{
			auto const rgb_Player = Lumina::Utils::Color::Convert(
				Lumina::Utils::Color::HSV{ 195.0f, 0.25f, 0.5f }
			);
			PlayerMaterial_.RGBA = { rgb_Player.R, rgb_Player.G, rgb_Player.B, 1.0f };
			PlayerMaterial_.ID_DiffuseMap = 1U;
			UB_PlayerMaterial_.Initialize(device, 256LLU, "Player Material");
			UB_PlayerMaterial_.Store(&PlayerMaterial_, sizeof(MeshMaterial), 0LLU);

			auto const rgb_Boss = Lumina::Utils::Color::Convert(
				Lumina::Utils::Color::HSV{ -5.0f, 0.25f, 0.5f }
			);
			BossMaterial_.RGBA = { rgb_Boss.R, rgb_Boss.G, rgb_Boss.B, 1.0f };
			BossMaterial_.ID_DiffuseMap = 2U;
			UB_BossMaterial_.Initialize(device, 256LLU, "Boss Material");
			UB_BossMaterial_.Store(&BossMaterial_, sizeof(MeshMaterial), 0LLU);

			LocalHeap_Materials_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16U, false);
			Lumina::DX12::CBV::Create(device, LocalHeap_Materials_.CPUHandle(0U), UB_PlayerMaterial_);
			Lumina::DX12::CBV::Create(device, LocalHeap_Materials_.CPUHandle(1U), UB_BossMaterial_);
		}

		// Grassland
		{
			Grassland_.reset(new Lumina::Grassland{});
			Grassland_->Initialize(dxContext_, 256U, 256U, const_cast<Lumina::AssetManager&>(assetMngr_));
		}

		// Mesh
		{
			auto&& playerModel{
				Lumina::Utils::Mesh::Load(
					Lumina::Utils::LoadFromFile<Lumina::Utils::WavefrontOBJ>(
						"Box.obj", "Assets"
					)
				)
			};
			auto&& bossModel{
				Lumina::Utils::Mesh::Load(
					Lumina::Utils::LoadFromFile<Lumina::Utils::WavefrontOBJ>(
						"Kinoko.obj", "Assets/Kinoko"
					)
				)
			};

			std::vector<Lumina::Utils::Mesh> meshes{};
			meshes.insert(meshes.cend(), playerModel.cbegin(), playerModel.cend());
			meshes.insert(meshes.cend(), bossModel.cbegin(), bossModel.cend());
			Lumina::MeshUploader meshUploader{};
			meshUploader.Initialize(dxContext_);
			meshUploader.Begin();
			for (auto const& mesh : meshes) {
				meshUploader.Batch(mesh);
			}
			meshUploader.End(MeshShaderAssets_);
		}

		// MeshManager
		{
			MeshManager_.reset(new Lumina::MeshManager{});
			MeshManager_->Initialize(dxContext_, 2048U, 1048576U);

			dxContext_.Compile(
				VS_MeshDeferredGeometry_,
				L"Assets/Shaders/MeshCommon.VS.hlsl",
				L"vs_6_6",
				L"main",
				"Mesh.DeferredGeometry.VS"
			);
			dxContext_.Compile(
				PS_MeshDeferredGeometry_,
				L"Assets/Shaders/MeshCommon.PS.hlsl",
				L"ps_6_6",
				L"main",
				"Mesh.DeferredGeometry.PS"
			);

			Lumina::DX12::BlendState blendState_None{};
			blendState_None.RenderTarget[0].BlendEnable = false;
			blendState_None.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendState_None.RenderTarget[1].BlendEnable = false;
			blendState_None.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			Lumina::DX12::GraphicsPSO::InputLayout inputLayout_Mesh{};
			inputLayout_Mesh.Append("IDX_POSITION", 0U, DXGI_FORMAT_R32_UINT);
			inputLayout_Mesh.Append("IDX_TEXCOORD", 0U, DXGI_FORMAT_R32_UINT);
			inputLayout_Mesh.Append("IDX_NORMAL", 0U, DXGI_FORMAT_R32_UINT);
			inputLayout_Mesh.Append("IDX_TANGENT", 0U, DXGI_FORMAT_R32_UINT);

			GraphicsPSO_MeshDeferredGeometry_.Initialize(
				device,
				MeshManager_->RootSignature(),
				VS_MeshDeferredGeometry_,
				PS_MeshDeferredGeometry_,
				blendState_None,
				Lumina::DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_BACK },
				},
				Lumina::DX12::DepthStencilState{
					.DepthEnable{ true },
					.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
					.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
					.StencilEnable{ false },
				},
				inputLayout_Mesh,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM, },
				Lumina::DX12::GraphicsPSO::DefaultDSVFormat
				);
		}

		// PrimitiveManager
		{
			PrimitiveManager_.reset(new Lumina::PrimitiveManager{});
			PrimitiveManager_->Initialize(dxContext_);
			PrimitiveManager1_.reset(new Lumina::PrimitiveManager{});
			PrimitiveManager1_->Initialize(dxContext_);
			PrimitiveManager2_.reset(new Lumina::PrimitiveManager{});
			PrimitiveManager2_->Initialize(dxContext_);
		}

		// Particles
		{
			auto config_ParticleSystem{
				Lumina::Utils::LoadFromFile<nlohmann::json>(
					"Assets/Configs/ParticleSystem.json"
				)
			};
			RS_ParticleSystem_.Initialize(
				device,
				Lumina::DX12::LoadRootSignatureSetup(
					config_ParticleSystem.at("Common RS")
				)
			);

			dxContext_.Compile(
				VS_BasicParticle_,
				L"Assets/Shaders/BasicParticle.VS.hlsl",
				L"vs_6_6",
				L"main",
				"BasicParticle.VS"
			);
			dxContext_.Compile(
				PS_BasicParticle_,
				L"Assets/Shaders/BasicParticle.PS.hlsl",
				L"ps_6_6",
				L"main",
				"BasicParticle.PS"
			);

			Lumina::DX12::BlendState blendState_AdditiveMode{};
			blendState_AdditiveMode.RenderTarget[0] = {
				.BlendEnable{ true },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_ONE },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_SRC_ALPHA },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			Lumina::DX12::GraphicsPSO::InputLayout inputLayout_Particle{};
			inputLayout_Particle.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);
			inputLayout_Particle.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
			GraphicsPSO_BasicParticle_AdditiveMode_.Initialize(
				device,
				RS_ParticleSystem_,
				VS_BasicParticle_,
				PS_BasicParticle_,
				blendState_AdditiveMode,
				Lumina::DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_NONE },
				},
				Lumina::DX12::DepthStencilState{
					.DepthEnable{ false },
					.StencilEnable{ false },
				},
				inputLayout_Particle,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, },
				Lumina::DX12::GraphicsPSO::DefaultDSVFormat
				);

			AmbientSparkles_.reset(new ParticleSystem<Particle>{});
			AmbientSparkles_->Initialize(dxContext_, 256U);

			PlayerEffects_.reset(new ParticleSystem<Particle>{});
			PlayerEffects_->Initialize(dxContext_, 512U);

			KnockEffects_.reset(new ParticleSystem<Particle>{});
			KnockEffects_->Initialize(dxContext_, 256U);
		}

		{
			PlayerBullets_.reset(new ParticleSystem<PlayerBullet>{});
			PlayerBullets_->Initialize(dxContext_, 512U);
		}

		// DeferredLighting
		{
			DeferredLighting_.reset(new Lumina::DeferredLighting{});
			DeferredLighting_->Initialize(dxContext_, 1280U, 720U);

			List_PointLight_.Initialize(2048U);
			List_LocalToWorld_LightSphere_.Initialize(2048U);
		}

		// Canvas
		{
			InitializeCanvas(Canvas_, dxContext_); 
			{
				Canvas_Merge_.AllocateTextures(1U, false);
				Canvas_Merge_.RenderTexture(0U).Initialize(
					device,
					1280U,
					720U,
					DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
				);
				Canvas_Merge_.TransitionResourceStates(device, dxContext_.DirectQueue());
				Canvas_Merge_.CreateViews(device);
				Canvas_Merge_.Viewport(0U) = D3D12_VIEWPORT{
					.TopLeftX{ 0.0f },
					.TopLeftY{ 0.0f },
					.Width{ 1280.0f },
					.Height{ 720.0f },
					.MinDepth{ 0.0f },
					.MaxDepth{ 1.0f },
				};
				Canvas_Merge_.ScissorRect(0U) = D3D12_RECT{
					.left{ 0 },
					.top{ 0 },
					.right{ 1280 },
					.bottom{ 720 },
				};
			}
			{
				Canvas_PostProcessing_.AllocateTextures(1U, false);
				Canvas_PostProcessing_.RenderTexture(0U).Initialize(
					device,
					1280U,
					720U,
					DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
				);
				Canvas_PostProcessing_.TransitionResourceStates(device, dxContext_.DirectQueue());
				Canvas_PostProcessing_.CreateViews(device);
				Canvas_PostProcessing_.Viewport(0U) = D3D12_VIEWPORT{
					.TopLeftX{ 0.0f },
					.TopLeftY{ 0.0f },
					.Width{ 1280.0f },
					.Height{ 720.0f },
					.MinDepth{ 0.0f },
					.MaxDepth{ 1.0f },
				};
				Canvas_PostProcessing_.ScissorRect(0U) = D3D12_RECT{
					.left{ 0 },
					.top{ 0 },
					.right{ 1280 },
					.bottom{ 720 },
				};
			}

			GlobalTable_SRV_CanvasTexture_ = dxContext_.GlobalDescriptorHeap().Allocate(8U);
			Lumina::DX12::SRV<void>::Create(
				device,
				GlobalTable_SRV_CanvasTexture_.CPUHandle(0U),
				Canvas_.RenderTexture(0U)
			);
			Lumina::DX12::SRV<void>::Create(
				device,
				GlobalTable_SRV_CanvasTexture_.CPUHandle(1U),
				Canvas_.RenderTexture(1U)
			);
			Lumina::DX12::SRV<void>::Create<DXGI_FORMAT_R24_UNORM_X8_TYPELESS>(
				device,
				GlobalTable_SRV_CanvasTexture_.CPUHandle(2U),
				Canvas_.DepthTexture()
			);

			Lumina::DX12::SRV<void>::Create(
				device,
				GlobalTable_SRV_CanvasTexture_.CPUHandle(3U),
				DeferredLighting_->RenderTexture()
			);

			Lumina::DX12::SRV<void>::Create(
				device,
				GlobalTable_SRV_CanvasTexture_.CPUHandle(4U),
				Canvas_Merge_.RenderTexture(0U)
			);
			Lumina::DX12::SRV<void>::Create(
				device,
				GlobalTable_SRV_CanvasTexture_.CPUHandle(5U),
				Canvas_PostProcessing_.RenderTexture(0U)
			);
		}

		// DeferredGeometryPass
		{
			DeferredGeometryPass_.Initialize(2U, true);
			DeferredGeometryPass_.RenderTarget(0).BeginningEvent().ClearTarget(
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				{ 0.0f, 0.0f, 0.0f, 0.0f }
			);
			DeferredGeometryPass_.RenderTarget(0).EndingEvent().Preserve();
			DeferredGeometryPass_.RenderTarget(1).BeginningEvent().ClearTarget(
				DXGI_FORMAT_R8G8B8A8_UNORM,
				{ 0.0f, 0.0f, 0.0f, 0.0f }
			);
			DeferredGeometryPass_.RenderTarget(1).EndingEvent().Preserve();
			DeferredGeometryPass_.DepthStencil().DepthBeginningEvent().ClearTarget(
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				{ .Depth{ 1.0f }, }
			);
			DeferredGeometryPass_.DepthStencil().DepthEndingEvent().Preserve();
			DeferredGeometryPass_.DepthStencil().StencilBeginningEvent().NoAccess();
			DeferredGeometryPass_.DepthStencil().StencilEndingEvent().NoAccess();

			for (uint32_t idx{ 0U }; idx < Canvas_.Num_RenderTargets(); ++idx) {
				DeferredGeometryPass_.RenderTarget(idx).View() = Canvas_.RTV(idx);
			}
			DeferredGeometryPass_.DepthStencil().View() = Canvas_.DSV();
		}

		// MergePass
		{
			MergePass_.Initialize(1U, false);
			MergePass_.RenderTarget(0).BeginningEvent().ClearTarget(
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				{ 0.0f, 0.0f, 0.0f, 0.0f }
			);
			MergePass_.RenderTarget(0).EndingEvent().Preserve();
			MergePass_.RenderTarget(0U).View() = Canvas_Merge_.RTV(0U);
		}

		// PostProcessingPass
		{
			PostProcessingPass_.Initialize(1U, false);
			PostProcessingPass_.RenderTarget(0).BeginningEvent().ClearTarget(
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				{ 0.0f, 0.0f, 0.0f, 0.0f }
			);
			PostProcessingPass_.RenderTarget(0).EndingEvent().Preserve();
			/*PostProcessingPass_.DepthStencil().DepthBeginningEvent().ClearTarget(
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				{ .Depth{ 1.0f }, }
			);
			PostProcessingPass_.DepthStencil().DepthEndingEvent().Preserve();
			PostProcessingPass_.DepthStencil().StencilBeginningEvent().NoAccess();
			PostProcessingPass_.DepthStencil().StencilEndingEvent().NoAccess();*/

			PostProcessingPass_.RenderTarget(0U).View() = Canvas_PostProcessing_.RTV(0U);
		}

		// UIPass
		{
			UIPass_.Initialize(1U, false);
			UIPass_.RenderTarget(0).BeginningEvent().Preserve();
			UIPass_.RenderTarget(0).EndingEvent().Preserve();
		}

		auto worldToNDC = Lumina::Mat4::Orthographic(0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 1.0f);
		{
			UB_OrthoProj_.Initialize(device, 256LLU, "OrthoProj");
			UB_OrthoProj_.Store(&worldToNDC, sizeof(Lumina::Mat4), 0U);
			LocalHeap_OrthoProj_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1U, false);
			Lumina::DX12::CBV::Create(device, LocalHeap_OrthoProj_.CPUHandle(0U), UB_OrthoProj_);
		}

		UB_PostProcessingConstants_.Initialize(device, 256LLU);
		GlobalTable_CBV_PostProcessing_ = dxContext_.GlobalDescriptorHeap().Allocate(2U);
		Lumina::DX12::CBV::Create(
			device,
			GlobalTable_CBV_PostProcessing_.CPUHandle(0U),
			UB_PostProcessingConstants_
		);

		PPConstants_.Time = 0.0f;
		PPConstants_.PlayerNDCPos = {};
		PPConstants_.IsEnemySuccessfullyAttacking = 0;
		/*auto depthTexSRVID{
			(
				GlobalTable_SRV_CanvasTexture_.GPUHandle(2U).ptr -
				dxContext_.GlobalDescriptorHeap().GPUHandle(0U).ptr
			) / dxContext_.GlobalDescriptorHeap().Increment()
		};*/
		UB_PostProcessingConstants_.Store(&PPConstants_, sizeof(PostProcessingConstants), 0LLU);

		UB_Dummy_.Initialize(device, 256LLU);
		Lumina::DX12::CBV::Create(
			device,
			GlobalTable_CBV_PostProcessing_.CPUHandle(1U),
			UB_Dummy_
		);
		PostProcessingConstants dummy{ 0 };
		UB_Dummy_.Store(&dummy, sizeof(PostProcessingConstants), 0LLU);

		// Sprite
		{
			SpriteRenderer_.reset(new Lumina::SpriteRenderer{});
			SpriteRenderer_->Initialize(dxContext_, 128U);

			// Pipeline
			{
				dxContext_.Compile(
					VS_Sprite_,
					L"Assets/Shaders/Sprite2.VS.hlsl",
					L"vs_6_6",
					L"main",
					"Sprite2.VS"
				);
				dxContext_.Compile(
					PS_Sprite_,
					L"Assets/Shaders/Sprite2.PS.hlsl",
					L"ps_6_6",
					L"main",
					"Sprite2.PS"
				);

				Lumina::DX12::BlendState spriteBlendState{};
				spriteBlendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
					.BlendEnable{ true },
					.LogicOpEnable{ false },
					.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
					.DestBlend{ D3D12_BLEND_ONE },
					.BlendOp{ D3D12_BLEND_OP_ADD },
					.SrcBlendAlpha{ D3D12_BLEND_SRC_ALPHA },
					.DestBlendAlpha{ D3D12_BLEND_ONE },
					.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
					.LogicOp{ D3D12_LOGIC_OP_NOOP },
					.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
				};
				Lumina::DX12::GraphicsPSO::InputLayout spriteInputLayout{};
				spriteInputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);
				PSO_Sprite_.Initialize(
					device,
					SpriteRenderer_->RootSignature(),
					VS_Sprite_,
					PS_Sprite_,
					spriteBlendState,
					Lumina::DX12::RasterizerState{
						.FillMode{ D3D12_FILL_MODE_SOLID },
						.CullMode{ D3D12_CULL_MODE_NONE },
					},
					Lumina::DX12::DepthStencilState{
						.DepthEnable{ false },
						.StencilEnable{ false },
					},
					spriteInputLayout,
					D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
					{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM, },
					Lumina::DX12::GraphicsPSO::DefaultDSVFormat
					);


					dxContext_.Compile(
						PS_SpriteUI_,
						L"Assets/Shaders/SpriteUI.PS.hlsl",
						L"ps_6_6",
						L"main",
						"SpriteUI.PS"
					);
					PSO_SpriteUI_.Initialize(
						device,
						SpriteRenderer_->RootSignature(),
						VS_Sprite_,
						PS_SpriteUI_,
						spriteBlendState,
						Lumina::DX12::RasterizerState{
							.FillMode{ D3D12_FILL_MODE_SOLID },
							.CullMode{ D3D12_CULL_MODE_NONE },
						},
						Lumina::DX12::DepthStencilState{
							.DepthEnable{ false },
							.StencilEnable{ false },
						},
						spriteInputLayout,
						D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
						{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, },
						Lumina::DX12::GraphicsPSO::DefaultDSVFormat
						);
			}

			// PlayerHPBar
			{
				UI_PlayerHPBar_.Translate({ 40.0f, 680.0f });
				UI_PlayerHPBar_.Scale({ 0.0f, 40.0f });
				UI_PlayerHPBar_.AnchorPoint({ 0.0f, 1.0f });
				UI_PlayerHPBar_.TextureID(3U);
				UI_PlayerHPBar_.RGBA({ 1.0f, 1.0f, 1.0f, 0.25f });

				MaxWidth_UI_PlayerHPBar_ = 480.0f;
				UI_PlayerHPValue_ = Player_->HP_;
			}
			// BossHPBar
			{
				UI_BossHPBar_.Translate({ 20.0f, 20.0f });
				UI_BossHPBar_.Scale({ 0.0f, 20.0f });
				UI_BossHPBar_.AnchorPoint({ 0.0f, 0.0f });
				UI_BossHPBar_.TextureID(3U);
				UI_BossHPBar_.RGBA({ 1.0f, 1.0f, 1.0f, 0.25f });

				MaxWidth_UI_BossHPBar_ = 1280.0f - 20.0f * 2;
			}

			//// Caption
			//{
			//}

			{
				UI_WIN_.Translate({ 640.0f, 360.0f });
				UI_WIN_.Scale({ 720.0f, 180.0f });
				UI_WIN_.AnchorPoint({ 0.5f, 0.5f });
				UI_WIN_.TextureID(4U);
				UI_WIN_.RGBA({ 1.0f, 1.0f, 1.0f, 1.0f });
				UI_LOSE_.Translate({ 640.0f, 360.0f });
				UI_LOSE_.Scale({ 720.0f, 180.0f });
				UI_LOSE_.AnchorPoint({ 0.5f, 0.5f });
				UI_LOSE_.TextureID(5U);
				UI_LOSE_.RGBA({ 1.0f, 1.0f, 1.0f, 1.0f });
			}

			//// Manual Button
			//{
			//	UI_ManualButton_.Translate({ 1400.0f, 535.0f });
			//	UI_ManualButton_.Scale({ 360.0f, 90.0f });
			//	UI_ManualButton_.AnchorPoint({ 1.0f, 0.5f });
			//	UI_ManualButton_.TextureID(IT_ManualButton);
			//	UI_ManualButton_.RGBA({ 1.0f, 1.0f, 1.0f, 0.0f });
			//}

			//// End Button
			//{
			//	UI_ExitButton_.Translate({ 1400.0f, 610.0f });
			//	UI_ExitButton_.Scale({ 360.0f, 90.0f });
			//	UI_ExitButton_.AnchorPoint({ 1.0f, 0.5f });
			//	UI_ExitButton_.TextureID(IT_ExitButton);
			//	UI_ExitButton_.RGBA({ 1.0f, 1.0f, 1.0f, 0.0f });
			//}

			//SelectedButton_ = 0;
		}

		{
			PlayerSkinnedModel_ = std::make_unique<SkinnedModel>();
			PlayerSkinnedModel_->Collection_ = Lumina::CG3D::Import("Neki.gltf", "Assets/Neki");

			PlayerSkinnedModel_->VertexBuffer_.Initialize(
				device,
				// バッファサイズ＝頂点サイズ×メッシュの頂点数
				sizeof(Lumina::CG3D::Mesh::Vertex)*
				PlayerSkinnedModel_->Collection_.Meshes[0].Vertices.size()
			);
			// 頂点バッファに頂点データを入れる
			PlayerSkinnedModel_->VertexBuffer_.Store(
				// データ
				PlayerSkinnedModel_->Collection_.Meshes[0].Vertices.data(),
				// データサイズ
				sizeof(Lumina::CG3D::Mesh::Vertex)*
				PlayerSkinnedModel_->Collection_.Meshes[0].Vertices.size(),
				// メモリオフセット　気にせんでええ
				0LLU
			);
			// 頂点バッファを使ってビューを作成
			// テンプレートに頂点の変数型を入れる
			PlayerSkinnedModel_->VBV_ =
				Lumina::DX12::VBV::Create<Lumina::CG3D::Mesh::Vertex>(PlayerSkinnedModel_->VertexBuffer_);

			PlayerSkinnedModel_->IndexBuffer_.Initialize(
				device,
				sizeof(uint32_t)*
				PlayerSkinnedModel_->Collection_.Meshes[0].Indices.size()
			);
			PlayerSkinnedModel_->IndexBuffer_.Store(
				PlayerSkinnedModel_->Collection_.Meshes[0].Indices.data(),
				sizeof(uint32_t)*
				PlayerSkinnedModel_->Collection_.Meshes[0].Indices.size(),
				0LLU
			);
			PlayerSkinnedModel_->IBV_ = Lumina::DX12::IBV::Create(PlayerSkinnedModel_->IndexBuffer_);

			PlayerSkinnedInstance_ = std::make_unique<SkinnedInstance>();

			PlayerSkinnedInstance_->Skeleton_ =
				Lumina::CG3D::CreateSkeleton(PlayerSkinnedModel_->Collection_.Root);
			Lumina::CG3D::CreateSkinCluster(
				PlayerSkinnedInstance_->SkinCluster_,
				device,
				dxContext_.GlobalDescriptorHeap(),
				PlayerSkinnedInstance_->Skeleton_,
				// メッシュ
				PlayerSkinnedModel_->Collection_.Meshes[0]
			);

			PlayerSkinnedInstance_->MeshScale_ = { 1.0f, 1.0f, 1.0f };
			PlayerSkinnedInstance_->MeshRotate_ = { 0.0f, 0.0f, 0.0f };
			PlayerSkinnedInstance_->MeshTranslate_ = { 0.0f, 0.0f, 0.0f };

			auto animation_run{ Lumina::CG3D::LoadAnimationFile("Cool_Run.gltf", "Assets/Neki") };
			
			animDatabase_["Run"] = animation_run[0];
			PlayAnimation("Run", true);

			auto config{ Lumina::Utils::LoadFromFile<nlohmann::json>("Assets/Configs/SkinnedMesh.json") };
			auto&& rsSetup{ Lumina::DX12::LoadRootSignatureSetup(config.at("RS")) };
			RS_Skinning_.Initialize(device, rsSetup);

			dxContext_.Compile(
				VS_SkinnedMeshDeferredGeometry_,
				L"Assets/Shaders/MeshSkinning.VS.hlsl",
				L"vs_6_6",
				L"main",
				"SkinnedMesh.DeferredGeometry.VS"
			);
			dxContext_.Compile(
				PS_SkinnedMeshDeferredGeometry_,
				L"Assets/Shaders/MeshSkinning.PS.hlsl",
				L"ps_6_6",
				L"main",
				"SkinnedMesh.DeferredGeometry.PS"
			);

			Lumina::DX12::BlendState blendState_None{};
			blendState_None.RenderTarget[0].BlendEnable = false;
			blendState_None.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			blendState_None.RenderTarget[1].BlendEnable = false;
			blendState_None.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			Lumina::DX12::GraphicsPSO::InputLayout inputLayout_Mesh{};
			inputLayout_Mesh.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			inputLayout_Mesh.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
			inputLayout_Mesh.Append("NORMAL", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			inputLayout_Mesh.Append("WEIGHT", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT, 1U);
			inputLayout_Mesh.Append("PALETTE", 0U, DXGI_FORMAT_R32G32B32A32_SINT, 1U);

			GraphicsPSO_SkinnedMeshDeferredGeometry_.Initialize(
				device,
				RS_Skinning_,
				VS_SkinnedMeshDeferredGeometry_,
				PS_SkinnedMeshDeferredGeometry_,
				blendState_None,
				Lumina::DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_BACK },
				},
				Lumina::DX12::DepthStencilState{
					.DepthEnable{ true },
					.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
					.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
					.StencilEnable{ false },
				},
				inputLayout_Mesh,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{
					DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
					DXGI_FORMAT_R8G8B8A8_UNORM,
					DXGI_FORMAT_R8G8B8A8_UNORM,
				},
				Lumina::DX12::GraphicsPSO::DefaultDSVFormat
			);

			UB_Transforms_.Initialize(device, 256LLU);
			GlobalTable_CBV_Scene_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			Lumina::DX12::CBV::Create(device, GlobalTable_CBV_Scene_.CPUHandle(0U), UB_Transforms_);

			GlobalTable_Materials_ = dxContext_.GlobalDescriptorHeap().Allocate(16U);
			Lumina::DX12::CBV::Create(device, GlobalTable_Materials_.CPUHandle(0U), UB_PlayerMaterial_);
		}
	}

	InGame::InGame() = default;
	InGame::~InGame() = default;
}

namespace Game::Scene {
	template<>
	void InGame::Initialize(
		Lumina::DX12::Context const& dxContext_,
		Lumina::AssetManager const& assetMngr_
	) {
		Impl_.reset(new Impl::InGame{});
		Impl_->Initialize(dxContext_, assetMngr_);
	}

	InGame::InGame() = default;
	InGame::~InGame() = default;
}