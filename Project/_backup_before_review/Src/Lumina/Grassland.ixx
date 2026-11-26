export module Lumina.Grassland;

import <memory>;

import <vector>;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;

import Lumina.AssetManager;

import Lumina.Utils.Data;
import Lumina.Utils.Data.Mesh;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

import Lumina.Math;

namespace Lumina {
	export class Grassland {
		struct Constant_System {
			float Time;
		};
		struct Constant_Scene {
			Mat4 WorldToNDC;
			Float3 PlayerWorldPos;
			float PlayerRadius;
			Float3 EnemyWorldPos;
			float EnemyRadius;
		};

	public:
		void Initialize(
			DX12::Context const& dxContext_,
			uint32_t mapWidth_,
			uint32_t mapHeight_,
			AssetManager& assetMngr_
		) {
			auto const& device{ dxContext_.Device() };

			MapWidth_ = mapWidth_;
			MapHeight_ = mapHeight_;

			Num_Maps_ = 5U;

			Arr_MapResourceFormats_.resize(Num_Maps_);
			Arr_MapResourceFormats_[static_cast<uint32_t>(MAP::BEND)] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			Arr_MapResourceFormats_[static_cast<uint32_t>(MAP::TRAMPLING)] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			Arr_MapResourceFormats_[static_cast<uint32_t>(MAP::NOISE)] = DXGI_FORMAT_R8G8B8A8_UNORM;
			Arr_MapResourceFormats_[static_cast<uint32_t>(MAP::TINT)] = DXGI_FORMAT_R8G8B8A8_UNORM;
			Arr_MapResourceFormats_[4U] = DXGI_FORMAT_R8G8B8A8_UNORM;

			Arr_Maps_.resize(Num_Maps_);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(Arr_Maps_.size()); ++idx) {
				Arr_Maps_[idx].reset(new DX12::ComputeTexture2D{});
				Arr_Maps_[idx]->Initialize(
					device,
					MapWidth_,
					MapHeight_,
					Arr_MapResourceFormats_[idx],
					std::format("Map #{}", idx).data()
				);
			}

			LocalHeap_SRV_Maps_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Num_Maps_, false);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(Arr_Maps_.size()); ++idx) {
				DX12::SRV<void>::Create(device, LocalHeap_SRV_Maps_.CPUHandle(idx), *Arr_Maps_[idx]);
			}

			LocalHeap_UAV_Maps_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Num_Maps_, false);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(Arr_Maps_.size()); ++idx) {
				DX12::UAV<void>::Create(device, LocalHeap_UAV_Maps_.CPUHandle(idx), *Arr_Maps_[idx]);
			}

			GlobalTable_Graphics_ = dxContext_.GlobalDescriptorHeap().Allocate(192U);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(Arr_Maps_.size()); ++idx) {
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_Graphics_.CPUHandle(idx + 32U),
					LocalHeap_SRV_Maps_.CPUHandle(idx),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_Graphics_.CPUHandle(idx + 32U + 96U),
					LocalHeap_SRV_Maps_.CPUHandle(idx),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}

			GlobalTable_Compute_ = dxContext_.GlobalDescriptorHeap().Allocate(96U);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(Arr_Maps_.size()); ++idx) {
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_Compute_.CPUHandle(idx + 32U),
					LocalHeap_SRV_Maps_.CPUHandle(idx),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_Compute_.CPUHandle(idx + 64U),
					LocalHeap_UAV_Maps_.CPUHandle(idx),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}

			DirectCommandAllocator0_.Initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
			DirectCommandList0_.Initialize(device, DirectCommandAllocator0_);
			DirectCommandAllocator1_.Initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
			DirectCommandList1_.Initialize(device, DirectCommandAllocator1_);
			ComputeCommandAllocator_.Initialize(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
			ComputeCommandList_.Initialize(device, ComputeCommandAllocator_);

			auto settings{ Utils::LoadFromFile<nlohmann::json>("Settings.json", "Assets/Grassland") };
			auto graphicsRSSetup{ DX12::LoadRootSignatureSetup(settings.at("General Graphics RS Test")) };
			GraphicsRS_.Initialize(device, graphicsRSSetup, "Graphics RS");
			auto computeRSSetup{ DX12::LoadRootSignatureSetup(settings.at("General Compute RS Test")) };
			ComputeRS_.Initialize(device, computeRSSetup, "Compute RS");

			dxContext_.Compile(
				VertexShader_,
				L"Assets/Grassland/VS.hlsl",
				L"vs_6_6",
				L"main",
				"Grassland::VS"
			);
			dxContext_.Compile(
				PixelShader_,
				L"Assets/Grassland/PS.hlsl",
				L"ps_6_6",
				L"main",
				"Grassland::PS"
			);
			DX12::GraphicsPipelineState::Setup graphicsPSOSetup{};
			DX12::BlendState blendState{ .IndependentBlendEnable{ true }, };
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ONE },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
			blendState.RenderTarget[1] = {
				.BlendEnable{ false },
				.LogicOpEnable{ false },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
			DX12::RasterizerState rasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_NONE },
			};
			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ true },
				.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
				.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
			};
			DX12::GraphicsPipelineState::InputLayout inputLayout{};
			inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			inputLayout.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
			inputLayout.Append("NORMAL", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			inputLayout.Append("TANGENT", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			std::vector<DXGI_FORMAT> rtvFormats{
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				DXGI_FORMAT_R8G8B8A8_UNORM,
			};
			graphicsPSOSetup <<
				GraphicsRS_ <<
				VertexShader_ <<
				PixelShader_ <<
				blendState <<
				rasterizerState <<
				depthStencilState <<
				inputLayout <<
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE <<
				rtvFormats <<
				DX12::GraphicsPipelineState::DefaultDSVFormat;
			GraphicsPSO_.Initialize(
				device,
				graphicsPSOSetup,
				"Grassland::GraphicsPSO::GrassBlade"
			);

			Num_ComputeShaders_ = 5U;
			Arr_ComputeShaders_.resize(Num_ComputeShaders_);
			for (auto& cs : Arr_ComputeShaders_) {
				cs.reset(new DX12::Shader{});
			}
			dxContext_.Compile(
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::BEND)],
				L"Assets/Grassland/Maps.CS.hlsl",
				L"cs_6_6",
				L"GenerateBendMap",
				"Grassland::CS::Bend"
			);
			dxContext_.Compile(
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::TRAMPLING)],
				L"Assets/Grassland/Maps.CS.hlsl",
				L"cs_6_6",
				L"GenerateTramplingMap",
				"Grassland::CS::Trampling"
			);
			dxContext_.Compile(
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::NOISE)],
				L"Assets/Grassland/Maps.CS.hlsl",
				L"cs_6_6",
				L"GenerateNoiseMap",
				"Grassland::CS::Noise"
			);
			dxContext_.Compile(
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::TINT)],
				L"Assets/Grassland/Maps.CS.hlsl",
				L"cs_6_6",
				L"GenerateTintMap",
				"Grassland::CS::Tint"
			);
			dxContext_.Compile(
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::INIT)],
				L"Assets/Grassland/Maps.CS.hlsl",
				L"cs_6_6",
				L"InitMaps",
				"Grassland::CS::Init"
			);

			Num_ComputePSOs_ = 5U;
			Arr_ComputePSOs_.resize(Num_ComputePSOs_);
			for (auto& pso : Arr_ComputePSOs_) {
				pso.reset(new DX12::ComputePipelineState{});
			}
			Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::BEND)]->Initialize(
				device,
				ComputeRS_,
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::BEND)],
				"Grassland::ComputePSO::Bend"
			);
			Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::TRAMPLING)]->Initialize(
				device,
				ComputeRS_,
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::TRAMPLING)],
				"Grassland::ComputePSO::Trampling"
			);
			Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::NOISE)]->Initialize(
				device,
				ComputeRS_,
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::NOISE)],
				"Grassland::ComputePSO::Noise"
			);
			Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::TINT)]->Initialize(
				device,
				ComputeRS_,
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::TINT)],
				"Grassland::ComputePSO::Tint"
			);
			Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::INIT)]->Initialize(
				device,
				ComputeRS_,
				*Arr_ComputeShaders_[static_cast<uint32_t>(COMPUTE_SHADER::INIT)],
				"Grassland::ComputePSO::Init"
			);

			ID3D12DescriptorHeap* descriptorHeaps[]{ dxContext_.GlobalDescriptorHeap().Get(), };
			ComputeCommandList_->SetDescriptorHeaps(1U, descriptorHeaps);
			ComputeCommandList_->SetComputeRootSignature(ComputeRS_.Get());
			ComputeCommandList_->SetComputeRootDescriptorTable(0U, GlobalTable_Compute_.GPUHandle(0U));
			ComputeCommandList_->SetPipelineState(
				Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::INIT)]->Get()
			);
			ComputeCommandList_->Dispatch(MapWidth_, MapHeight_, 1U);
			ComputeCommandList_->SetPipelineState(
				Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::NOISE)]->Get()
			);
			ComputeCommandList_->Dispatch(MapWidth_, MapHeight_, 1U);
			ComputeCommandList_->SetPipelineState(
				Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::TINT)]->Get()
			);
			ComputeCommandList_->Dispatch(MapWidth_, MapHeight_, 1U);
			dxContext_.ComputeQueue() << ComputeCommandList_;
			dxContext_.ComputeQueue().CPUWait(dxContext_.ComputeQueue().ExecuteBatchedCommandLists());
			ComputeCommandList_.Reset(ComputeCommandAllocator_);


			auto obj_GrassBlade{ Utils::LoadFromFile<Utils::WavefrontOBJ>("GrassBlade.obj", "Assets/Grassland") };
			auto&& meshes_GrassBlade{ Utils::Mesh::Load(obj_GrassBlade) };
			auto const& mesh_GrassBlade{ meshes_GrassBlade[0] };

			struct Vertex {
				Float3 LocalPos;
				Float2 TexCoord;
				Float3 Normal;
				Float3 Tangent;
			};
			std::vector<Vertex> vertices_GrassBlade{};
			for (auto const& vert : mesh_GrassBlade.Vertices) {
				auto& vertData{ vertices_GrassBlade.emplace_back() };
				vertData.LocalPos = mesh_GrassBlade.Positions[vert.Index_Position];
				vertData.TexCoord = mesh_GrassBlade.TexCoords[vert.Index_TexCoord];
				vertData.Normal = mesh_GrassBlade.Normals[vert.Index_Normal];
				vertData.Tangent = mesh_GrassBlade.Tangents[vert.Index_Tangent];
			}
			Num_Vertices_GrassBlade_ = static_cast<uint32_t>(vertices_GrassBlade.size());

			DB_Mesh_GrassBlade_.Initialize(
				device,
				sizeof(Vertex) * Num_Vertices_GrassBlade_,
				"Grassland::DB::Mesh::GrassBlade"
			);
			VBV_Mesh_GrassBlade_ = DX12::VBV::Create<Vertex>(DB_Mesh_GrassBlade_);
			DX12::UploadBuffer ub_Mesh_GrassBlade{};
			ub_Mesh_GrassBlade.Initialize(device, DB_Mesh_GrassBlade_.SizeInBytes());
			ub_Mesh_GrassBlade.Store(
				vertices_GrassBlade.data(),
				sizeof(Vertex) * Num_Vertices_GrassBlade_,
				0LLU
			);
			DirectCommandList0_->CopyResource(DB_Mesh_GrassBlade_.Get(), ub_Mesh_GrassBlade.Get());

			DB_Constant_System_.Initialize(device, (sizeof(Constant_Scene) + 0xFFLLU) & ~0xFFLLU);
			UB_Constant_System_.Initialize(device, DB_Constant_System_.SizeInBytes());

			DB_Constant_Scene_.Initialize(device, (sizeof(Constant_Scene) + 0xFFLLU) & ~0xFFLLU);
			UB_Constant_Scene_.Initialize(device, DB_Constant_Scene_.SizeInBytes());
			UB_Constant_Scene_.Store(&Constant_Scene_, sizeof(Constant_Scene), 0LLU);
			DirectCommandList0_->CopyResource(DB_Constant_Scene_.Get(), UB_Constant_Scene_.Get());

			LocalHeap_CBV_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32U, false);
			DX12::CBV::Create(device, LocalHeap_CBV_.CPUHandle(0U), DB_Constant_System_);
			DX12::CBV::Create(device, LocalHeap_CBV_.CPUHandle(1U), DB_Constant_Scene_);
			for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_Graphics_.CPUHandle(idx),
					LocalHeap_CBV_.CPUHandle(idx),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_Graphics_.CPUHandle(idx + 96U),
					LocalHeap_CBV_.CPUHandle(idx),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_Compute_.CPUHandle(idx),
					LocalHeap_CBV_.CPUHandle(idx),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}

			std::vector<D3D12_RESOURCE_BARRIER> const barriers{
				DX12::Barrier::Transition(
					*Arr_Maps_[static_cast<uint32_t>(MAP::BEND)],
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
				),
				DX12::Barrier::Transition(
					*Arr_Maps_[static_cast<uint32_t>(MAP::TRAMPLING)],
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
				),
				DX12::Barrier::Transition(
					*Arr_Maps_[static_cast<uint32_t>(MAP::NOISE)],
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
				),
				DX12::Barrier::Transition(
					*Arr_Maps_[static_cast<uint32_t>(MAP::TINT)],
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				),
				DX12::Barrier::Transition(
					DB_Mesh_GrassBlade_,
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
				),
				DX12::Barrier::Transition(
					DB_Constant_System_,
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
				),
				DX12::Barrier::Transition(
					DB_Constant_Scene_,
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
				),
			};
			DirectCommandList0_->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());
			dxContext_.DirectQueue() << DirectCommandList0_;
			dxContext_.DirectQueue().CPUWait(dxContext_.DirectQueue().ExecuteBatchedCommandLists());
			DirectCommandList0_.Reset(DirectCommandAllocator0_);

			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "GrassBlade", "Assets/Grassland/GrassBlade.png" },
				}
			);

			GlobalTable_ImageTextures_ = dxContext_.GlobalDescriptorHeap().Allocate(32U);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
				device->CopyDescriptorsSimple(
					1U,
					GlobalTable_ImageTextures_.CPUHandle(idx),
					assetMngr_.Graphics().CPUHandle(texIDs.at(idx)),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}
		}

		void Update(
			DX12::Context const& dxContext_,
			Mat4 const& worldToNDC_,
			Vec3 const& playerWorldPos_,
			float playerRadius_,
			Vec3 const& enemyWorldPos_,
			float enemyRadius_
		) {
			static std::vector<D3D12_RESOURCE_BARRIER> const barriers0{
				DX12::Barrier::Transition(
					*Arr_Maps_[static_cast<uint32_t>(MAP::BEND)],
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				),
				DX12::Barrier::Transition(
					*Arr_Maps_[static_cast<uint32_t>(MAP::TRAMPLING)],
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				),
			};

			static std::vector<D3D12_RESOURCE_BARRIER> const barriers1{
				DX12::Barrier::Transition(
					DB_Constant_System_,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_COPY_DEST
				),
				DX12::Barrier::Transition(
					DB_Constant_Scene_,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_COPY_DEST
				),
			};

			static std::vector<D3D12_RESOURCE_BARRIER> const barriers2{
				DX12::Barrier::Transition(
					DB_Constant_System_,
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
				),
				DX12::Barrier::Transition(
					DB_Constant_Scene_,
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
				),
				DX12::Barrier::Transition(
					*Arr_Maps_[static_cast<uint32_t>(MAP::BEND)],
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
				),
				DX12::Barrier::Transition(
					*Arr_Maps_[static_cast<uint32_t>(MAP::TRAMPLING)],
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
				),
			};

			static D3D12_RESOURCE_BARRIER const uavBarrier_Map_Trampling{
				.Type{ D3D12_RESOURCE_BARRIER_TYPE_UAV },
				.UAV{
					.pResource{
						Arr_Maps_[static_cast<uint32_t>(MAP::TRAMPLING)]->Get()
					},
				},
			};

			DirectCommandList0_->ResourceBarrier(static_cast<uint32_t>(barriers0.size()), barriers0.data());
			dxContext_.DirectQueue() << DirectCommandList0_;
			dxContext_.ComputeQueue().GPUWait(
				dxContext_.DirectQueue(),
				dxContext_.DirectQueue().ExecuteBatchedCommandLists()
			);

			ID3D12DescriptorHeap* descriptorHeaps[]{ dxContext_.GlobalDescriptorHeap().Get(), };
			ComputeCommandList_->SetDescriptorHeaps(1U, descriptorHeaps);
			ComputeCommandList_->SetComputeRootSignature(ComputeRS_.Get());
			ComputeCommandList_->SetComputeRootDescriptorTable(0U, GlobalTable_Compute_.GPUHandle(0U));
			ComputeCommandList_->SetPipelineState(
				Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::TRAMPLING)]->Get()
			);
			ComputeCommandList_->Dispatch(MapWidth_ >> 1U, MapHeight_ >> 1U, 1U);
			ComputeCommandList_->ResourceBarrier(1U, &uavBarrier_Map_Trampling);
			ComputeCommandList_->SetPipelineState(
				Arr_ComputePSOs_[static_cast<uint32_t>(COMPUTE_SHADER::BEND)]->Get()
			);
			ComputeCommandList_->Dispatch(MapWidth_, MapHeight_, 1U);
			dxContext_.ComputeQueue() << ComputeCommandList_;
			dxContext_.DirectQueue().GPUWait(
				dxContext_.ComputeQueue(),
				dxContext_.ComputeQueue().ExecuteBatchedCommandLists()
			);

			Constant_System_.Time += 1.0f;
			UB_Constant_System_.Store(&Constant_System_, sizeof(Constant_System), 0LLU);

			Constant_Scene_.WorldToNDC = worldToNDC_;
			std::memcpy(&Constant_Scene_.PlayerWorldPos, playerWorldPos_(), sizeof(Float3));
			Constant_Scene_.PlayerRadius = playerRadius_;
			std::memcpy(&Constant_Scene_.EnemyWorldPos, enemyWorldPos_(), sizeof(Float3));
			Constant_Scene_.EnemyRadius = enemyRadius_;
			UB_Constant_Scene_.Store(&Constant_Scene_, sizeof(Constant_Scene), 0LLU);

			DirectCommandList1_->ResourceBarrier(static_cast<uint32_t>(barriers1.size()), barriers1.data());

			DirectCommandList1_->CopyResource(DB_Constant_System_.Get(), UB_Constant_System_.Get());
			DirectCommandList1_->CopyResource(DB_Constant_Scene_.Get(), UB_Constant_Scene_.Get());

			DirectCommandList1_->ResourceBarrier(static_cast<uint32_t>(barriers2.size()), barriers2.data());

			dxContext_.DirectQueue() << DirectCommandList1_;
			dxContext_.DirectQueue().CPUWait(dxContext_.DirectQueue().ExecuteBatchedCommandLists());

			DirectCommandList0_.Reset(DirectCommandAllocator0_);
			DirectCommandList1_.Reset(DirectCommandAllocator1_);
			ComputeCommandList_.Reset(ComputeCommandAllocator_);
			
			/*ImGui::Begin("Grassland");
			ImGui::Image(
				GlobalTable_Compute_.GPUHandle(static_cast<uint32_t>(MAP::BEND) + 32U).ptr,
				{ static_cast<float>(MapWidth_), static_cast<float>(MapHeight_) }
			);
			ImGui::Image(
				GlobalTable_Compute_.GPUHandle(static_cast<uint32_t>(MAP::TRAMPLING) + 32U).ptr,
				{ static_cast<float>(MapWidth_), static_cast<float>(MapHeight_) }
			);
			ImGui::End();*/
		}

		void Render(
			[[maybe_unused]] DX12::Context const& dxContext_,
			DX12::CommandList const& cmdList_
		) {
			/*auto rtv{ dxContext_.SwapChain().BackBufferRTVCPUHandle() };
			auto dsv{ dxContext_.SwapChain().DSVCPUHandle() };
			static D3D12_VIEWPORT const viewport{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f, };
			static D3D12_RECT const scissorRect{ 0U, 0U, 1280U, 720U };

			cmdList_->OMSetRenderTargets(1U, &rtv, false, &dsv);
			cmdList_->RSSetViewports(1U, &viewport);
			cmdList_->RSSetScissorRects(1U, &scissorRect);*/

			//ID3D12DescriptorHeap* descriptorHeaps[]{ dxContext_.GlobalDescriptorHeap().Get(), };
			//DirectCommandList0_->SetDescriptorHeaps(1U, descriptorHeaps);

			cmdList_->SetGraphicsRootSignature(GraphicsRS_.Get());
			cmdList_->SetGraphicsRootDescriptorTable(0U, GlobalTable_Graphics_.GPUHandle(0U));
			cmdList_->SetGraphicsRootDescriptorTable(1U, GlobalTable_Graphics_.GPUHandle(96U));
			cmdList_->SetGraphicsRootDescriptorTable(2U, GlobalTable_ImageTextures_.GPUHandle(0U));
			cmdList_->SetPipelineState(GraphicsPSO_.Get());
			cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList_->IASetVertexBuffers(0U, 1U, &VBV_Mesh_GrassBlade_);
			cmdList_->DrawInstanced(Num_Vertices_GrassBlade_, 512U * 512U, 0U, 0U);
		}

	private:
		uint32_t MapWidth_;
		uint32_t MapHeight_;

		std::vector<std::unique_ptr<DX12::ComputeTexture2D>> Arr_Maps_{};
		std::vector<DXGI_FORMAT> Arr_MapResourceFormats_{};
		uint32_t Num_Maps_{};

		DX12::DescriptorTable GlobalTable_Graphics_{};
		DX12::DescriptorTable GlobalTable_ImageTextures_{};
		DX12::DescriptorTable GlobalTable_Compute_{};
		DX12::DescriptorHeap LocalHeap_CBV_{};
		DX12::DescriptorHeap LocalHeap_SRV_Maps_{};
		DX12::DescriptorHeap LocalHeap_UAV_Maps_{};

		DX12::Shader VertexShader_{};
		DX12::Shader PixelShader_{};
		std::vector<std::unique_ptr<DX12::Shader>> Arr_ComputeShaders_{};
		uint32_t Num_ComputeShaders_{};

		DX12::GraphicsPipelineState GraphicsPSO_{};
		std::vector<std::unique_ptr<DX12::ComputePipelineState>> Arr_ComputePSOs_{};
		uint32_t Num_ComputePSOs_{};

		DX12::CommandAllocator DirectCommandAllocator0_{};
		DX12::CommandList DirectCommandList0_{};
		DX12::CommandAllocator DirectCommandAllocator1_{};
		DX12::CommandList DirectCommandList1_{};
		DX12::CommandAllocator ComputeCommandAllocator_{};
		DX12::CommandList ComputeCommandList_{};

		DX12::RootSignature GraphicsRS_{};
		DX12::RootSignature ComputeRS_{};

		Constant_System Constant_System_{};
		DX12::DefaultBuffer DB_Constant_System_{};
		DX12::UploadBuffer UB_Constant_System_{};

		Constant_Scene Constant_Scene_{};
		DX12::DefaultBuffer DB_Constant_Scene_{};
		DX12::UploadBuffer UB_Constant_Scene_{};

		DX12::DefaultBuffer DB_Mesh_GrassBlade_{};
		DX12::VBV VBV_Mesh_GrassBlade_{};
		uint32_t Num_Vertices_GrassBlade_{};

	private:
		enum class MAP : uint32_t {
			BEND = 0U,
			TRAMPLING = 1U,
			NOISE = 2U,
			TINT = 3U,
		};

		enum class COMPUTE_SHADER : uint32_t {
			BEND = 0U,
			TRAMPLING = 1U,
			NOISE = 2U,
			TINT = 3U,
			INIT = 4U,
		};
	};
}