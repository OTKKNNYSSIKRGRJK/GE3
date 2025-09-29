module;

#include<d3d12.h>

#include<External/nlohmann.JSON/single_include/nlohmann/json.hpp>

#include<External/ImGui/imgui.h>

export module Game.Scene_InGame;

import <cstdint>;

import <cmath>;
import <numbers>;

import <memory>;

import <vector>;

import <string>;
import <format>;

import Lumina.Math.Numerics;
import Lumina.Math.Vector;
import Lumina.Math.Matrix;
import Lumina.Math.Random;

import Lumina.WinApp.Context;

import Lumina.DX12;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.RenderTextureEX;
import Lumina.DX12.Context;

import Lumina.Container.List;
import Lumina.Container.Bitset;

import Lumina.Utils.Data;
import Lumina.Utils.Data.Model;
import Lumina.Utils.Debug;

import Game.MapGenerator;

namespace Game {
	namespace {
		constinit float const MapBlockWidth{ 2.0f };
		constinit float const MapBlockHeight{ 2.0f };

		auto& RndGen{ Lumina::Random::Generator() };

		struct Bounds {
			float Left;
			float Right;
			float Top;
			float Bottom;
		};

		struct RotationAnimation {
			float InitialAngle;
			float FinalAngle;
			int TotalFrames;
			int CurrentFrame;
			float Inv_TotalFrames;
		};

		template<typename T>
		T Lerp(T const& a_, T const & b_, float t_) {
			return (a_ * (1.0f - t_) + b_ * t_);
		}

		constexpr float EaseOut(float x_) { return 1.0f - (1.0f - x_) * (1.0f - x_) * (1.0f - x_); }

		struct Camera {
			Lumina::Vec3 Scale_{ 1.0f, 1.0f, 1.0f };
			Lumina::Vec3 Rotate_{ 0.0f, 0.0f, 0.0f };
			Lumina::Vec3 Translate_{ 0.0f, 0.0f, -50.0f };
			Lumina::Mat4 SRT_{};
			Lumina::Mat4 View_{};
			Lumina::Mat4 Projection_{};
			Lumina::Mat4 VP_{};

			Lumina::DX12::DefaultBuffer DB_{};
			Lumina::DX12::UploadBuffer UB_{};

			Lumina::DX12::DescriptorTable CSUTable_{};

			D3D12_RESOURCE_BARRIER Barriers_[2]{};

			void Initialize(
				Lumina::DX12::Context const& dx12Context_
			) {
				auto const& device{ dx12Context_.Device() };
				DB_.Initialize(device, 256U, "Camera");
				UB_.Initialize(device, 256U, "Camera Upload");

				dx12Context_.GlobalDescriptorHeap().Allocate(CSUTable_, 1U);
				Lumina::DX12::CBV::Create(device, CSUTable_.CPUHandle(0U), DB_);

				Barriers_[0] = {
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				};
				Barriers_[1] = {
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
					},
				};

				SRT_ = Lumina::Mat4::SRT(Scale_, Rotate_, Translate_);
				View_ = SRT_.Inv();
				Projection_ = Lumina::Mat4::PerspectiveFOV(
					0.45f,
					1280.0f / 720.0f,
					0.1f,
					200.0f
				);
			}

			void Update(
				Lumina::DX12::CommandList const& directList_
			) {
				Lumina::Mat4::Multiply(VP_, View_, Projection_);
				UB_.Store(&VP_, sizeof(Lumina::Mat4), 0LLU);
				directList_->ResourceBarrier(1U, &Barriers_[0]);
				directList_->CopyBufferRegion(DB_.Get(), 0LLU, UB_.Get(), 0LLU, sizeof(Lumina::Mat4));
				directList_->ResourceBarrier(1U, &Barriers_[1]);
			}
		};

		enum ELEMENT {
			TREE,
			FIRE,
			EARTH,
			METAL,
			WATER,
			NO_ELEMENT,
		};

		int MapBlockType(int mapBlock_) {
			return (mapBlock_ & 0xFF);
		}

		int MapBlockElement(int mapBlock_) {
			return ((mapBlock_ >> 8) & 0x7);
		}

		int MapBlockDamage(int mapBlock_) {
			return ((mapBlock_ >> 11) & 0x1FF);
		}

		Lumina::Int2 MapPos(Lumina::Vec3 const& Pos_) {
			return { static_cast<int>(std::round(Pos_.x * 0.5f)), static_cast<int>(std::round(Pos_.y * 0.5f)) };
		}

		int GetMapBlock(Lumina::Int2 const& mapPos_, std::vector<std::vector<int>> const& map_) {
			if (mapPos_.x > -1 && mapPos_.x < static_cast<int>(map_[0].size()) &&
				mapPos_.y > -1 && mapPos_.y < static_cast<int>(map_.size())) {
				return map_[mapPos_.y][mapPos_.x];
			}
			return 0;
		}

		void SetMapBlockElement(int& mapBlock_, ELEMENT element_) {
			mapBlock_ &= ~(0x7 << 8);
			mapBlock_ |= (element_ << 8);
		}
	}

	namespace {
		struct MeshTest {
			Lumina::DX12::DefaultBuffer VertexBuffer{};
			D3D12_VERTEX_BUFFER_VIEW VBV{};
			uint32_t Num_Vertices{};

			Lumina::DX12::DefaultBuffer Buffer_CubePositions{};
			Lumina::DX12::DefaultBuffer Buffer_CubeTexCoords{};
			Lumina::DX12::DefaultBuffer Buffer_CubeNormals{};

			void Initialize(
				Lumina::DX12::GraphicsDevice const& device_,
				Lumina::DX12::CommandQueue& cmdQueue_,
				std::string_view fileName_,
				std::string_view directory_
			) {
				Lumina::DX12::CommandAllocator cmdAlloc{};
				cmdAlloc.Initialize(device_, cmdQueue_.Type());
				Lumina::DX12::CommandList cmdList{};
				cmdList.Initialize(device_, cmdAlloc);

				auto&& objData{ Lumina::Utils::LoadFromFile<Lumina::Utils::WavefrontOBJ>(fileName_, directory_) };
				Lumina::Utils::Model2 modelData{ objData };
				Num_Vertices = static_cast<uint32_t>(modelData.Vertices.size());
				Lumina::DX12::UploadBuffer vertBuf{};
				vertBuf.Initialize(
					device_,
					sizeof(Lumina::Int3) * modelData.Vertices.size()
				);
				vertBuf.Store(
					modelData.Vertices.data(),
					sizeof(Lumina::Int3) * modelData.Vertices.size(),
					0LLU
				);
				VertexBuffer.Initialize(device_, vertBuf.SizeInBytes(), std::format("VB {}", fileName_));
				cmdList->CopyResource(VertexBuffer.Get(), vertBuf.Get());

				Buffer_CubePositions.Initialize(device_, sizeof(Lumina::Int4) * modelData.Positions.size());
				Buffer_CubeTexCoords.Initialize(device_, sizeof(Lumina::Int2) * modelData.TexCoords.size());
				Buffer_CubeNormals.Initialize(device_, sizeof(Lumina::Int3) * modelData.Normals.size());
				Lumina::DX12::UploadBuffer uploadBuf_Poses{};
				Lumina::DX12::UploadBuffer uploadBuf_TexCoords{};
				Lumina::DX12::UploadBuffer uploadBuf_Norms{};
				uploadBuf_Poses.Initialize(device_, Buffer_CubePositions.SizeInBytes());
				uploadBuf_Poses.Store(modelData.Positions.data(), uploadBuf_Poses.SizeInBytes(), 0LLU);
				uploadBuf_TexCoords.Initialize(device_, Buffer_CubeTexCoords.SizeInBytes());
				uploadBuf_TexCoords.Store(modelData.TexCoords.data(), uploadBuf_TexCoords.SizeInBytes(), 0LLU);
				uploadBuf_Norms.Initialize(device_, Buffer_CubeNormals.SizeInBytes());
				uploadBuf_Norms.Store(modelData.Normals.data(), uploadBuf_Norms.SizeInBytes(), 0LLU);
				cmdList->CopyResource(Buffer_CubePositions.Get(), uploadBuf_Poses.Get());
				cmdList->CopyResource(Buffer_CubeTexCoords.Get(), uploadBuf_TexCoords.Get());
				cmdList->CopyResource(Buffer_CubeNormals.Get(), uploadBuf_Norms.Get());

				std::vector<D3D12_RESOURCE_BARRIER> barriers{
					D3D12_RESOURCE_BARRIER{
						.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
						.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
						.Transition{
							.pResource{ VertexBuffer.Get() },
							.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						},
					},
					D3D12_RESOURCE_BARRIER{
						.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
						.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
						.Transition{
							.pResource{ Buffer_CubePositions.Get() },
							.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						},
					},
					D3D12_RESOURCE_BARRIER{
						.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
						.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
						.Transition{
							.pResource{ Buffer_CubeTexCoords.Get() },
							.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						},
					},
					D3D12_RESOURCE_BARRIER{
						.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
						.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
						.Transition{
							.pResource{ Buffer_CubeNormals.Get() },
							.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						},
					},
				};
				cmdList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());
				cmdQueue_ << cmdList;
				cmdQueue_.CPUWait(cmdQueue_.ExecuteBatchedCommandLists());

				VBV = Lumina::DX12::VBV<Lumina::Int3>{ VertexBuffer };
			}
		};

		struct Player {
			Lumina::Vec3 Position;
			Lumina::Vec3 Rotate{ 0.0f, 0.0f, 0.0f };
			Lumina::Vec3 Scale{ 0.75f, 0.75f, 0.75f };
			Lumina::Vec3 Velocity;
			RotationAnimation RotAnimY;
			RotationAnimation RotAnimZ;
			float Attack{ 1.0f };
			float Defense{ 0.0f };
			float HP{ 128.0f };
			float ElementPowers[6]{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
			ELEMENT ElementInUse{ ELEMENT::NO_ELEMENT };
			int DirectionY;
			int DirectionZ;

			Lumina::DX12::ComputeTexture Texture_{};
			Lumina::DX12::DefaultBuffer DB_TextureParams_{};
			Lumina::DX12::UploadBuffer UB_TextureParams_{};
			struct TextureParams {
				float Time;
			};
			TextureParams TexParams_{};
			//std::unique_ptr<Lumina::DX12::ImageTexture> Texture{ nullptr };
			MeshTest* Mesh{ nullptr };

			Lumina::DX12::DescriptorTable CSUTable_{};

			Lumina::DX12::RootSignature GraphicsRS_{};
			Lumina::DX12::Shader VS_{};
			Lumina::DX12::Shader PS_{};
			Lumina::DX12::GraphicsPipelineState GraphicsPSO_{};

			Lumina::DX12::RootSignature TexRS_{};
			Lumina::DX12::Shader TexCS_{};
			Lumina::DX12::ComputePipelineState TexPSO_{};

			struct RenderData {
				Lumina::Mat4 Transform;
			};
			Lumina::DX12::DefaultBuffer DB_RenderData{};
			Lumina::DX12::UploadBuffer UB_RenderData{};
			RenderData RenderData_;

			void Initialize(
				Lumina::DX12::Context const& dx12Context_,
				NLohmannJSON const& config_
			) {
				auto const& device{ dx12Context_.Device() };

				/*auto img{ Lumina::DX12::ImageSet::Create("Assets/Player.png") };
				auto mip{ Lumina::DX12::MipChain::Create(*img) };
				Texture = Lumina::DX12::ImageTexture::Create(device, *mip, "PlayerTexture");
				Lumina::DX12::ImageTextureUploader uploader{};
				uploader.Initialize(device);
				uploader.Begin();
				uploader << (*Texture);
				uploader.End(dx12Context_.DirectQueue());*/

				Texture_.Initialize(device, 64U, 64U);
				dx12Context_.GlobalDescriptorHeap().Allocate(CSUTable_, 4U);
				Lumina::DX12::SRV<void>::Create(
					device,
					CSUTable_.CPUHandle(0U),
					Texture_
				);
				Lumina::DX12::UAV<Lumina::Float4>::Create(
					device,
					CSUTable_.CPUHandle(1U),
					Texture_
				);

				DB_RenderData.Initialize(device, 256LLU);
				UB_RenderData.Initialize(device, DB_RenderData.SizeInBytes());
				Lumina::DX12::CBV::Create(
					device,
					CSUTable_.CPUHandle(2U),
					DB_RenderData
				);

				DB_TextureParams_.Initialize(device, 256LLU);
				UB_TextureParams_.Initialize(device, DB_TextureParams_.SizeInBytes());
				Lumina::DX12::CBV::Create(
					device,
					CSUTable_.CPUHandle(3U),
					DB_TextureParams_
				);
				
				auto&& rsSetup{ Lumina::DX12::LoadRootSignatureSetup(config_.at("PlayerGraphicsRS")) };
				GraphicsRS_.Initialize(device, rsSetup, "PlayerRS");
				dx12Context_.Compile(
					VS_,
					L"Assets/Shaders/Player.VS.hlsl",
					L"vs_6_6",
					L"main",
					"PlayerVS"
				);
				dx12Context_.Compile(
					PS_,
					L"Assets/Shaders/Player.PS.hlsl",
					L"ps_6_6",
					L"main",
					"PlayerPS"
				);
				Lumina::DX12::BlendState blendState{};
				{
					blendState.AlphaToCoverageEnable = false;
					blendState.IndependentBlendEnable = false;
					blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
						.BlendEnable{ true },
						.LogicOpEnable{ false },
						.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
						.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
						.BlendOp{ D3D12_BLEND_OP_ADD },
						.SrcBlendAlpha{ D3D12_BLEND_SRC_ALPHA },
						.DestBlendAlpha{ D3D12_BLEND_INV_SRC_ALPHA },
						.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
						.LogicOp{ D3D12_LOGIC_OP_NOOP },
						.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
					};
				}
				auto&& rasterizerState{ Lumina::DX12::LoadRasterizerState(config_.at("PlayerGraphicsPSO")) };
				Lumina::DX12::DepthStencilState depthStencilState{
					.DepthEnable{ true },
					.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
					.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
					.StencilEnable{ false },
				};
				auto&& inputLayout{ Lumina::DX12::LoadInputLayout(config_.at("PlayerGraphicsPSO")) };
				GraphicsPSO_.Initialize(
					device,
					GraphicsRS_,
					VS_,
					PS_,
					blendState,
					rasterizerState,
					depthStencilState,
					inputLayout,
					D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
					Lumina::DX12::GraphicsPSO::DefaultRTVFormats,
					Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
					"PlayerPSO"
				);

				TexRS_.Initialize(
					device,
					Lumina::DX12::LoadRootSignatureSetup(config_.at("PlayerTexComputeRS")),
					"PlayerTexRS"
				);
				dx12Context_.Compile(
					TexCS_,
					L"Assets/Shaders/PlayerTex.CS.hlsl",
					L"cs_6_6",
					L"main",
					"PlayerTexCS"
				);
				TexPSO_.Initialize(
					device,
					TexRS_,
					TexCS_,
					"PlayerTexPSO"
				);

				DirectionY = 1;
				DirectionZ = 0;

				RotAnimY = {
					.InitialAngle{ 0.0f },
					.FinalAngle{ 0.0f },
					.TotalFrames{ 30 },
					.CurrentFrame{ 30 },
				};
				RotAnimY.Inv_TotalFrames = 1.0f / static_cast<float>(RotAnimY.TotalFrames);

				RotAnimZ = {
					.InitialAngle{ 0.0f },
					.FinalAngle{ 0.0f },
					.TotalFrames{ 30 },
					.CurrentFrame{ 30 },
				};
				RotAnimZ.Inv_TotalFrames = 1.0f / static_cast<float>(RotAnimZ.TotalFrames);

				TexParams_.Time = 0.0f;
			}

			void Update(Lumina::DX12::CommandList const& directList_) {
				if (RotAnimY.CurrentFrame <= RotAnimY.TotalFrames) {
					Rotate.y = Lerp(
						RotAnimY.InitialAngle,
						RotAnimY.FinalAngle,
						EaseOut(RotAnimY.CurrentFrame * RotAnimY.Inv_TotalFrames)
					);
					++RotAnimY.CurrentFrame;
				}
				if (RotAnimZ.CurrentFrame <= RotAnimZ.TotalFrames) {
					Rotate.z = Lerp(
						RotAnimZ.InitialAngle,
						RotAnimZ.FinalAngle,
						EaseOut(RotAnimZ.CurrentFrame * RotAnimZ.Inv_TotalFrames)
					);
					++RotAnimZ.CurrentFrame;
				}

				TexParams_.Time += 0.005f;
				UB_TextureParams_.Store(&TexParams_, sizeof(TextureParams), 0LLU);

				directList_->CopyBufferRegion(
					DB_TextureParams_.Get(),
					0LLU,
					UB_TextureParams_.Get(),
					0LLU,
					sizeof(TextureParams)
				);

				RenderData_.Transform = Lumina::Mat4::SRT(Scale, Rotate, Position);
				UB_RenderData.Store(&RenderData_, sizeof(RenderData), 0LLU);

				directList_->CopyBufferRegion(
					DB_RenderData.Get(),
					0LLU,
					UB_RenderData.Get(),
					0LLU,
					sizeof(Lumina::Mat4)
				);
			}
		};

		struct Bullet {
			Lumina::Vec3 Position;
			Lumina::Vec3 Velocity;
			Lumina::Vec3 Rotate;
			Lumina::Vec3 Scale;
			uint32_t FrameCount{ 0U };
			int32_t Life{ 1 };

			float Size{};

			ELEMENT ElementType;

			struct RenderData {
				float Transform[4][4];
				uint32_t ElementType;
				float Opacity;
			};

			static void Update_TreeType(Bullet& pb_) {
				pb_.Velocity.x += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.002f;
				pb_.Velocity.y += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.002f;
			}
			static void Update_FireType(Bullet& pb_) {
				pb_.Velocity.x += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.001f;
				pb_.Velocity.x *= 0.98f;
				pb_.Velocity.y += 0.05f;
				pb_.Size *= 0.98f;
			}
			static void Update_EarthType(Bullet& pb_) {
				pb_.Velocity.x += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0001f;
				pb_.Velocity.y += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0001f;
				pb_.Velocity *= 0.95f;
				pb_.Size *= 1.01f;
				--pb_.Life;
			}
			static void Update_MetalType(Bullet& pb_) {
				pb_.Velocity.x *= 1.02f;
				pb_.Velocity.y += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0002f;
				pb_.Velocity.y *= 0.9f;
				--pb_.Life;
			}
			static void Update_WaterType(Bullet& pb_) {
				pb_.Velocity.x += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.001f;
				pb_.Velocity.x *= 0.98f;
				pb_.Velocity.y -= 0.05f;
				pb_.Size *= 0.99f;
			}

			static void OnHitBlock_TreeType(Bullet& pb_, int&) {
				pb_.Velocity *= 0.0f;
				pb_.Life -= 30;
			}
			static void OnHitBlock_FireType(Bullet& pb_, int& mapBlock_) {
				if (MapBlockDamage(mapBlock_) < 400) {
					mapBlock_ += (1 << 11);
				}
				pb_.Velocity *= 0.0f;
				pb_.Life -= 30;
			}
			static void OnHitBlock_EarthType(Bullet& pb_, int&) {
				pb_.Velocity *= 0.0f;
				pb_.Life -= 30;
			}
			static void OnHitBlock_MetalType(Bullet& pb_, int&) {
				pb_.Velocity *= 0.9f;
				pb_.Life -= 9;
			}
			static void OnHitBlock_WaterType(Bullet& pb_, int& mapBlock_) {
				if (MapBlockType(mapBlock_) == 2) {
					pb_.Velocity.y *= -1.0f;
				}
				else {
					pb_.Velocity.x *= -1.0f;
				}
				pb_.Velocity *= 0.8f;
				pb_.Life -= 30;
			}
			static void OnHitBlock(Bullet& pb_, int&) {
				pb_.Velocity *= 0.0f;
				pb_.Life -= 30;
			}
		};

		struct PlayerBulletManager {
			static constinit inline uint32_t MaxNum_{ 4096U };

			Lumina::List<Bullet> List_{ MaxNum_ };
			uint32_t Count_Alive{ 0U };

			std::unique_ptr<Lumina::DX12::ImageTexture> TextureAtlas{ nullptr };

			Lumina::DX12::DescriptorTable CSUTable_{};

			Lumina::DX12::RootSignature GraphicsRS_{};
			Lumina::DX12::Shader VS_{};
			Lumina::DX12::Shader PS_{};
			Lumina::DX12::GraphicsPipelineState GraphicsPSO_{};

			Lumina::DX12::DefaultBuffer DB_RenderData{};
			Lumina::DX12::UploadBuffer UB_RenderData{};
			Lumina::DX12::DefaultBuffer DB_AliveBulletIndices{};
			Lumina::DX12::UploadBuffer UB_AliveBulletIndices{};

			MeshTest* Square{ nullptr };

			enum class VIEW_NAME : uint32_t {
				TEXTURE_ATLAS,
				RENDER_DATA,
				ALIVE_BULLET_INDICES,
			};

			void Initialize(
				Lumina::DX12::Context const& dx12Context_,
				NLohmannJSON const& config_
			) {
				auto const& device{ dx12Context_.Device() };

				auto img{ Lumina::DX12::ImageSet::Create("Assets/Particles.png") };
				auto mip{ Lumina::DX12::MipChain::Create(*img) };
				TextureAtlas = Lumina::DX12::ImageTexture::Create(device, *mip, "BulletTextureAtlas");
				Lumina::DX12::ImageTextureUploader uploader{};
				uploader.Initialize(dx12Context_.Device());
				uploader.Begin();
				uploader << (*TextureAtlas);
				uploader.End(dx12Context_.DirectQueue());

				DB_RenderData.Initialize(device, sizeof(Bullet::RenderData) * MaxNum_);
				UB_RenderData.Initialize(device, DB_RenderData.SizeInBytes());
				DB_AliveBulletIndices.Initialize(device, sizeof(uint32_t) * MaxNum_);
				UB_AliveBulletIndices.Initialize(device, DB_AliveBulletIndices.SizeInBytes());

				dx12Context_.GlobalDescriptorHeap().Allocate(CSUTable_, 16U);
				Lumina::DX12::SRV<void>::Create(
					device,
					CSUTable_.CPUHandle(VIEW_NAME::TEXTURE_ATLAS),
					*TextureAtlas
				);
				Lumina::DX12::SRV<Bullet::RenderData>::Create(
					device,
					CSUTable_.CPUHandle(VIEW_NAME::RENDER_DATA),
					DB_RenderData
				);
				Lumina::DX12::SRV<uint32_t>::Create(
					device,
					CSUTable_.CPUHandle(VIEW_NAME::ALIVE_BULLET_INDICES),
					DB_AliveBulletIndices
				);

				auto&& rsSetup{ Lumina::DX12::LoadRootSignatureSetup(config_.at("BulletRS")) };
				GraphicsRS_.Initialize(device, rsSetup, "BulletRS");
				dx12Context_.Compile(
					VS_,
					L"Assets/Shaders/Bullet.VS.hlsl",
					L"vs_6_6",
					L"main",
					"BulletVS"
				);
				dx12Context_.Compile(
					PS_,
					L"Assets/Shaders/Bullet.PS.hlsl",
					L"ps_6_6",
					L"main",
					"BulletPS"
				);
				Lumina::DX12::BlendState blendState{};
				{
					blendState.AlphaToCoverageEnable = false;
					blendState.IndependentBlendEnable = false;
					blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
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
				}
				auto&& rasterizerState{ Lumina::DX12::LoadRasterizerState(config_.at("BulletPSO")) };
				Lumina::DX12::DepthStencilState depthStencilState{
					.DepthEnable{ false },
					.StencilEnable{ false },
				};
				auto&& inputLayout{ Lumina::DX12::LoadInputLayout(config_.at("BulletPSO")) };
				GraphicsPSO_.Initialize(
					device,
					GraphicsRS_,
					VS_,
					PS_,
					blendState,
					rasterizerState,
					depthStencilState,
					inputLayout,
					D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
					Lumina::DX12::GraphicsPSO::DefaultRTVFormats,
					Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
					"BulletPSO"
				);
			}

			void Update(
				Lumina::DX12::CommandList const& directList_
			) {
				static Bullet::RenderData renderData{};
				Count_Alive = 0U;
				decltype(List_)::Iterator it{ List_ };
				for (it.Begin(); !it.End(); it.Next()) {
					int idx = it.Index();
					UB_AliveBulletIndices.Store(&idx, sizeof(int), sizeof(int) * Count_Alive);
					auto& bullet = (*it);
					if (bullet.Life <= 0) {
						List_.Delete(it);
						continue;
					}
					bullet.Position += bullet.Velocity;
					switch (bullet.ElementType) {
						case ELEMENT::TREE: {
							Bullet::Update_TreeType(bullet);
							break;
						}
						case ELEMENT::FIRE: {
							Bullet::Update_FireType(bullet);
							break;
						}
						case ELEMENT::EARTH: {
							Bullet::Update_EarthType(bullet);
							break;
						}
						case ELEMENT::METAL: {
							Bullet::Update_MetalType(bullet);
							break;
						}
						case ELEMENT::WATER: {
							Bullet::Update_WaterType(bullet);
							break;
						}
					}
					++bullet.FrameCount;
					auto&& srt{ Lumina::Mat4::SRT(bullet.Scale * bullet.Size, bullet.Rotate, bullet.Position) };
					std::memcpy(
						&renderData.Transform,
						&srt,
						sizeof(Lumina::Mat4)
					);
					renderData.ElementType = bullet.ElementType;
					renderData.Opacity = static_cast<float>(bullet.Life) / 180.0f;
					UB_RenderData.Store(
						&renderData,
						sizeof(Bullet::RenderData),
						sizeof(Bullet::RenderData) * idx
					);
					++Count_Alive;
				}

				D3D12_RESOURCE_BARRIER const barriers[]{
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_RenderData.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
							.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_AliveBulletIndices.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
							.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_RenderData.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_AliveBulletIndices.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
						},
					},
				};
				directList_->ResourceBarrier(2U, &barriers[0]);
				directList_->CopyResource(DB_RenderData.Get(), UB_RenderData.Get());
				directList_->CopyResource(DB_AliveBulletIndices.Get(), UB_AliveBulletIndices.Get());
				directList_->ResourceBarrier(2U, &barriers[2]);
			}

			void Render(
				Lumina::DX12::CommandList const& directList_,
				Lumina::DX12::DescriptorTable const& vpCBVTable_,
				D3D12_GPU_DESCRIPTOR_HANDLE meshViewTableStart_
			) {
				directList_->SetPipelineState(GraphicsPSO_.Get());
				directList_->SetGraphicsRootSignature(GraphicsRS_.Get());
				directList_->SetGraphicsRootDescriptorTable(0U, meshViewTableStart_);
				directList_->SetGraphicsRootDescriptorTable(1U, vpCBVTable_.GPUHandle(0U));
				directList_->SetGraphicsRootDescriptorTable(2U, CSUTable_.GPUHandle(1U));
				directList_->IASetVertexBuffers(0U, 1U, &(Square->VBV));
				directList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				if (Count_Alive) {
					directList_->DrawInstanced(Square->Num_Vertices, Count_Alive, 0U, 0U);
				}
			}
		};

		struct Enemy {
			Lumina::Vec3 Position;
			Lumina::Vec3 Rotate{ 0.0f, 0.0f, 0.0f };
			Lumina::Vec3 Scale{ 0.75f, 0.75f, 0.75f };
			Lumina::Vec3 Velocity;

			int FrameCount;

			float Life{ 50.0f };

			ELEMENT ElementType;

			struct RenderData {
				float Transform[4][4];
				uint32_t ElementType;
			};

			static void Update_TreeType(Enemy& e_) {
				e_.Velocity.x += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0005f;
				e_.Velocity.y += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0005f;
			}
			static void Update_FireType(Enemy& e_) {
				e_.Velocity.x += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0002f;
				e_.Velocity.y = std::min<float>(e_.Velocity.y + 0.01f, 0.25f);
			}
			static void Update_EarthType(Enemy& e_) {
				e_.Velocity.x += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0001f;
				e_.Velocity.y += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0001f;
				e_.Velocity *= 0.95f;
			}
			static void Update_MetalType(Enemy& e_) {
				e_.Velocity *= 1.01f;
			}
			static void Update_WaterType(Enemy& e_) {
				e_.Velocity.x += (static_cast<int32_t>(RndGen() & 127U) - 64) * 0.0002f;
				e_.Velocity.y = std::max<float>(e_.Velocity.y - 0.01f, -0.25f);
			}
		};

		struct EnemyManager {
			static constinit inline uint32_t MaxNum_{ 128U };

			Lumina::List<Enemy> List_{ MaxNum_ };
			uint32_t Count_Alive{ 0U };

			MeshTest* Mesh{ nullptr };

			Lumina::DX12::DescriptorTable CSUTable_{};

			Lumina::DX12::ComputeTexture Texture_{};
			Lumina::DX12::DefaultBuffer DB_TextureParams_{};
			Lumina::DX12::UploadBuffer UB_TextureParams_{};
			struct TextureParams {
				float Time;
			};
			TextureParams TexParams_{};

			Lumina::DX12::RootSignature GraphicsRS_{};
			Lumina::DX12::Shader VS_{};
			Lumina::DX12::Shader PS_{};
			Lumina::DX12::GraphicsPipelineState GraphicsPSO_{};

			Lumina::DX12::RootSignature TexRS_{};
			Lumina::DX12::Shader TexCS_{};
			Lumina::DX12::ComputePipelineState TexPSO_{};

			Lumina::DX12::DefaultBuffer DB_RenderData{};
			Lumina::DX12::UploadBuffer UB_RenderData{};
			Lumina::DX12::DefaultBuffer DB_AliveEnemyIndices{};
			Lumina::DX12::UploadBuffer UB_AliveEnemyIndices{};

			void Initialize(
				Lumina::DX12::Context const& dx12Context_,
				NLohmannJSON const& config_
			) {
				auto const& device{ dx12Context_.Device() };

				Texture_.Initialize(device, 64U, 64U);

				DB_RenderData.Initialize(device, sizeof(Enemy::RenderData) * MaxNum_);
				UB_RenderData.Initialize(device, DB_RenderData.SizeInBytes());
				DB_AliveEnemyIndices.Initialize(device, sizeof(uint32_t) * MaxNum_);
				UB_AliveEnemyIndices.Initialize(device, DB_AliveEnemyIndices.SizeInBytes());

				dx12Context_.GlobalDescriptorHeap().Allocate(CSUTable_, 16U);

				Lumina::DX12::SRV<void>::Create(
					device,
					CSUTable_.CPUHandle(0U),
					Texture_
				);
				Lumina::DX12::UAV<Lumina::Float4>::Create(
					device,
					CSUTable_.CPUHandle(1U),
					Texture_
				);

				Lumina::DX12::SRV<Enemy::RenderData>::Create(
					device,
					CSUTable_.CPUHandle(2U),
					DB_RenderData
				);
				Lumina::DX12::SRV<uint32_t>::Create(
					device,
					CSUTable_.CPUHandle(3U),
					DB_AliveEnemyIndices
				);

				DB_TextureParams_.Initialize(device, 256LLU);
				UB_TextureParams_.Initialize(device, DB_TextureParams_.SizeInBytes());
				Lumina::DX12::CBV::Create(
					device,
					CSUTable_.CPUHandle(4U),
					DB_TextureParams_
				);

				auto&& rsSetup{ Lumina::DX12::LoadRootSignatureSetup(config_.at("EnemyRS")) };
				GraphicsRS_.Initialize(device, rsSetup, "EnemyRS");
				dx12Context_.Compile(
					VS_,
					L"Assets/Shaders/Enemy.VS.hlsl",
					L"vs_6_6",
					L"main",
					"EnemyVS"
				);
				dx12Context_.Compile(
					PS_,
					L"Assets/Shaders/Enemy.PS.hlsl",
					L"ps_6_6",
					L"main",
					"EnemyPS"
				);
				Lumina::DX12::BlendState blendState{};
				{
					blendState.AlphaToCoverageEnable = false;
					blendState.IndependentBlendEnable = false;
					blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
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
				}
				auto&& rasterizerState{ Lumina::DX12::LoadRasterizerState(config_.at("EnemyPSO")) };
				Lumina::DX12::DepthStencilState depthStencilState{
					.DepthEnable{ true },
					.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
					.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
					.StencilEnable{ false },
				};
				auto&& inputLayout{ Lumina::DX12::LoadInputLayout(config_.at("EnemyPSO")) };
				GraphicsPSO_.Initialize(
					device,
					GraphicsRS_,
					VS_,
					PS_,
					blendState,
					rasterizerState,
					depthStencilState,
					inputLayout,
					D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
					Lumina::DX12::GraphicsPSO::DefaultRTVFormats,
					Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
					"EnemyPSO"
				);

				TexRS_.Initialize(
					device,
					Lumina::DX12::LoadRootSignatureSetup(config_.at("EnemyTexComputeRS")),
					"EnemyTexRS"
				);
				dx12Context_.Compile(
					TexCS_,
					L"Assets/Shaders/EnemyTex.CS.hlsl",
					L"cs_6_6",
					L"main",
					"EnemyTexCS"
				);
				TexPSO_.Initialize(
					device,
					TexRS_,
					TexCS_,
					"EnemyTexPSO"
				);
			}

			void Update(
				Lumina::DX12::CommandList const& directList_,
				Player const& player_
			) {
				static Enemy::RenderData renderData{};
				Count_Alive = 0U;
				decltype(List_)::Iterator it{ List_ };
				for (it.Begin(); !it.End(); it.Next()) {
					int idx = it.Index();
					UB_AliveEnemyIndices.Store(&idx, sizeof(int), sizeof(int) * Count_Alive);
					auto& enemy = (*it);
					if (enemy.Life <= 0.0f) {
						List_.Delete(it);
						continue;
					}
					enemy.Position += enemy.Velocity;
					switch (enemy.ElementType) {
						case ELEMENT::TREE: {
							auto&& dPos{ player_.Position - enemy.Position };
							float acc = 0.05f / Lumina::Vec3::Dot(dPos, dPos);
							enemy.Velocity += acc * dPos.Norm();
							Enemy::Update_TreeType(enemy);
							break;
						}
						case ELEMENT::FIRE: {
							Enemy::Update_FireType(enemy);
							break;
						}
						case ELEMENT::EARTH: {
							auto&& dPos{ player_.Position - enemy.Position };
							enemy.Velocity += dPos * 0.001f;
							Enemy::Update_EarthType(enemy);
							break;
						}
						case ELEMENT::METAL: {
							Enemy::Update_MetalType(enemy);
							break;
						}
						case ELEMENT::WATER: {
							Enemy::Update_WaterType(enemy);
							break;
						}
					}
					++enemy.FrameCount;
					enemy.Rotate.x += (RndGen() & 127U) * 0.0001f;
					enemy.Rotate.y += (RndGen() & 127U) * 0.0001f;
					auto&& srt{ Lumina::Mat4::SRT(enemy.Scale, enemy.Rotate, enemy.Position) };
					std::memcpy(
						&renderData.Transform,
						&srt,
						sizeof(Lumina::Mat4)
					);
					renderData.ElementType = enemy.ElementType;
					UB_RenderData.Store(
						&renderData,
						sizeof(Enemy::RenderData),
						sizeof(Enemy::RenderData) * idx
					);
					++Count_Alive;
				}

				D3D12_RESOURCE_BARRIER const barriers[]{
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_RenderData.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
							.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_AliveEnemyIndices.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
							.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_RenderData.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_AliveEnemyIndices.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
						},
					},
				};
				directList_->ResourceBarrier(2U, &barriers[0]);
				directList_->CopyResource(DB_RenderData.Get(), UB_RenderData.Get());
				directList_->CopyResource(DB_AliveEnemyIndices.Get(), UB_AliveEnemyIndices.Get());
				directList_->ResourceBarrier(2U, &barriers[2]);
			}

			void Render(
				Lumina::DX12::CommandList const& directList_,
				Lumina::DX12::DescriptorTable const& vpCBVTable_,
				D3D12_GPU_DESCRIPTOR_HANDLE meshViewTableStart_
			) {
				directList_->SetPipelineState(GraphicsPSO_.Get());
				directList_->SetGraphicsRootSignature(GraphicsRS_.Get());
				directList_->SetGraphicsRootDescriptorTable(0U, meshViewTableStart_);
				directList_->SetGraphicsRootDescriptorTable(1U, vpCBVTable_.GPUHandle(0U));
				directList_->SetGraphicsRootDescriptorTable(2U, CSUTable_.GPUHandle(2U));
				directList_->SetGraphicsRootDescriptorTable(3U, CSUTable_.GPUHandle(0U));
				directList_->IASetVertexBuffers(0U, 1U, &(Mesh->VBV));
				directList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				if (Count_Alive) {
					directList_->DrawInstanced(Mesh->Num_Vertices, Count_Alive, 0U, 0U);
				}
			}
		};

		struct EnemyBulletManager {
			static constinit inline uint32_t MaxNum_{ 4096U };

			Lumina::List<Bullet> List_{ MaxNum_ };
			uint32_t Count_Alive{ 0U };

			std::unique_ptr<Lumina::DX12::ImageTexture> TextureAtlas{ nullptr };

			Lumina::DX12::DescriptorTable CSUTable_{};

			Lumina::DX12::RootSignature GraphicsRS_{};
			Lumina::DX12::Shader VS_{};
			Lumina::DX12::Shader PS_{};
			Lumina::DX12::GraphicsPipelineState GraphicsPSO_{};

			Lumina::DX12::DefaultBuffer DB_RenderData{};
			Lumina::DX12::UploadBuffer UB_RenderData{};
			Lumina::DX12::DefaultBuffer DB_AliveBulletIndices{};
			Lumina::DX12::UploadBuffer UB_AliveBulletIndices{};

			MeshTest* Square{ nullptr };

			enum class VIEW_NAME : uint32_t {
				TEXTURE_ATLAS,
				RENDER_DATA,
				ALIVE_BULLET_INDICES,
			};

			void Initialize(
				Lumina::DX12::Context const& dx12Context_,
				NLohmannJSON const& config_
			) {
				auto const& device{ dx12Context_.Device() };

				auto img{ Lumina::DX12::ImageSet::Create("Assets/Particles.png") };
				auto mip{ Lumina::DX12::MipChain::Create(*img) };
				TextureAtlas = Lumina::DX12::ImageTexture::Create(device, *mip, "BulletTextureAtlas");
				Lumina::DX12::ImageTextureUploader uploader{};
				uploader.Initialize(dx12Context_.Device());
				uploader.Begin();
				uploader << (*TextureAtlas);
				uploader.End(dx12Context_.DirectQueue());

				DB_RenderData.Initialize(device, sizeof(Bullet::RenderData) * MaxNum_);
				UB_RenderData.Initialize(device, DB_RenderData.SizeInBytes());
				DB_AliveBulletIndices.Initialize(device, sizeof(uint32_t) * MaxNum_);
				UB_AliveBulletIndices.Initialize(device, DB_AliveBulletIndices.SizeInBytes());

				dx12Context_.GlobalDescriptorHeap().Allocate(CSUTable_, 16U);
				Lumina::DX12::SRV<void>::Create(
					device,
					CSUTable_.CPUHandle(VIEW_NAME::TEXTURE_ATLAS),
					*TextureAtlas
				);
				Lumina::DX12::SRV<Bullet::RenderData>::Create(
					device,
					CSUTable_.CPUHandle(VIEW_NAME::RENDER_DATA),
					DB_RenderData
				);
				Lumina::DX12::SRV<uint32_t>::Create(
					device,
					CSUTable_.CPUHandle(VIEW_NAME::ALIVE_BULLET_INDICES),
					DB_AliveBulletIndices
				);

				auto&& rsSetup{ Lumina::DX12::LoadRootSignatureSetup(config_.at("BulletRS")) };
				GraphicsRS_.Initialize(device, rsSetup, "BulletRS");
				dx12Context_.Compile(
					VS_,
					L"Assets/Shaders/Bullet.VS.hlsl",
					L"vs_6_6",
					L"main",
					"BulletVS"
				);
				dx12Context_.Compile(
					PS_,
					L"Assets/Shaders/Bullet.PS.hlsl",
					L"ps_6_6",
					L"main",
					"BulletPS"
				);
				Lumina::DX12::BlendState blendState{};
				{
					blendState.AlphaToCoverageEnable = false;
					blendState.IndependentBlendEnable = false;
					blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
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
				}
				auto&& rasterizerState{ Lumina::DX12::LoadRasterizerState(config_.at("BulletPSO")) };
				Lumina::DX12::DepthStencilState depthStencilState{
					.DepthEnable{ false },
					.StencilEnable{ false },
				};
				auto&& inputLayout{ Lumina::DX12::LoadInputLayout(config_.at("BulletPSO")) };
				GraphicsPSO_.Initialize(
					device,
					GraphicsRS_,
					VS_,
					PS_,
					blendState,
					rasterizerState,
					depthStencilState,
					inputLayout,
					D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
					Lumina::DX12::GraphicsPSO::DefaultRTVFormats,
					Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
					"BulletPSO"
				);
			}

			void Update(
				Lumina::DX12::CommandList const& directList_
			) {
				static Bullet::RenderData renderData{};
				Count_Alive = 0U;
				decltype(List_)::Iterator it{ List_ };
				for (it.Begin(); !it.End(); it.Next()) {
					int idx = it.Index();
					UB_AliveBulletIndices.Store(&idx, sizeof(int), sizeof(int) * Count_Alive);
					auto& bullet = (*it);
					if (bullet.Life <= 0) {
						List_.Delete(it);
						continue;
					}
					bullet.Position += bullet.Velocity;
					++bullet.FrameCount;
					auto&& srt{ Lumina::Mat4::SRT(bullet.Scale, bullet.Rotate, bullet.Position) };
					std::memcpy(
						&renderData.Transform,
						&srt,
						sizeof(Lumina::Mat4)
					);
					renderData.ElementType = bullet.ElementType;
					renderData.Opacity = static_cast<float>(bullet.Life) / 180.0f;
					UB_RenderData.Store(
						&renderData,
						sizeof(Bullet::RenderData),
						sizeof(Bullet::RenderData) * idx
					);
					++Count_Alive;
				}

				D3D12_RESOURCE_BARRIER const barriers[]{
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_RenderData.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
							.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_AliveBulletIndices.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
							.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_RenderData.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
						},
					},
					{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition{
							.pResource{ DB_AliveBulletIndices.Get() },
							.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
							.StateAfter{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
						},
					},
				};
				directList_->ResourceBarrier(2U, &barriers[0]);
				directList_->CopyResource(DB_RenderData.Get(), UB_RenderData.Get());
				directList_->CopyResource(DB_AliveBulletIndices.Get(), UB_AliveBulletIndices.Get());
				directList_->ResourceBarrier(2U, &barriers[2]);
			}

			void Render(
				Lumina::DX12::CommandList const& directList_,
				Lumina::DX12::DescriptorTable const& vpCBVTable_,
				D3D12_GPU_DESCRIPTOR_HANDLE meshViewTableStart_
			) {
				directList_->SetPipelineState(GraphicsPSO_.Get());
				directList_->SetGraphicsRootSignature(GraphicsRS_.Get());
				directList_->SetGraphicsRootDescriptorTable(0U, meshViewTableStart_);
				directList_->SetGraphicsRootDescriptorTable(1U, vpCBVTable_.GPUHandle(0U));
				directList_->SetGraphicsRootDescriptorTable(2U, CSUTable_.GPUHandle(1U));
				directList_->IASetVertexBuffers(0U, 1U, &(Square->VBV));
				directList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				if (Count_Alive) {
					directList_->DrawInstanced(Square->Num_Vertices, Count_Alive, 0U, 0U);
				}
			}
		};

		struct ElementPowerIndicator {
			Lumina::Vec4 Vertices[6]{};
			Lumina::Mat4 World{};
			Lumina::Float2 TexCoords[6]{};

			Lumina::DX12::UploadBuffer UB_MeshVertices_{};
			Lumina::DX12::UploadBuffer UB_MeshIndices_{};
			Lumina::DX12::UploadBuffer UB_World_{};
			D3D12_VERTEX_BUFFER_VIEW VBV_{};
			D3D12_INDEX_BUFFER_VIEW IBV_{};

			struct Vertex {
				Lumina::Float4 Position;
				Lumina::Float4 Color;
				Lumina::Float2 TexCoord;
			};
		};

		struct UIManager{
			ElementPowerIndicator ElementPowerIndicator_;
			Lumina::Mat4 OrthoProj_{};

			Lumina::DX12::ComputeTexture Texture_{};
			Lumina::DX12::DefaultBuffer DB_TextureParams_{};
			Lumina::DX12::UploadBuffer UB_TextureParams_{};
			struct TextureParams {
				float Time;
			};
			TextureParams TexParams_{};

			Lumina::DX12::DescriptorTable CSUTable_{};
			Lumina::DX12::UploadBuffer UB_OrthoProj_{};

			Lumina::DX12::RootSignature RS_{};
			Lumina::DX12::Shader VS_{};
			Lumina::DX12::Shader PS_{};
			Lumina::DX12::GraphicsPipelineState PSO_{};

			Lumina::DX12::RootSignature TexRS_{};
			Lumina::DX12::Shader TexCS_{};
			Lumina::DX12::ComputePipelineState TexPSO_{};

			void Initialize(
				Lumina::DX12::Context const& dx12Context_,
				NLohmannJSON const& config_
			) {
				auto const& device{ dx12Context_.Device() };

				ElementPowerIndicator_.World[3][0] = 100.0f;
				ElementPowerIndicator_.World[3][1] = 150.0f;
				ElementPowerIndicator_.World[3][2] = 0.0f;
				ElementPowerIndicator_.TexCoords[5] = { 0.5f, 0.5f };
				ElementPowerIndicator_.UB_MeshVertices_.Initialize(
					device,
					sizeof(ElementPowerIndicator::Vertex) * 6LLU
				);
				ElementPowerIndicator_.UB_MeshVertices_.Store(
					&ElementPowerIndicator_.Vertices[5],
					sizeof(Lumina::Vec4),
					sizeof(ElementPowerIndicator::Vertex) * 5
				);
				ElementPowerIndicator_.UB_MeshVertices_.Store(
					&ElementPowerIndicator_.TexCoords[5],
					sizeof(Lumina::Float2),
					sizeof(ElementPowerIndicator::Vertex) * 5 + sizeof(Lumina::Vec4) * 2
				);
				ElementPowerIndicator_.VBV_ = Lumina::DX12::VBV<ElementPowerIndicator::Vertex>{ ElementPowerIndicator_.UB_MeshVertices_ };
				ElementPowerIndicator_.UB_MeshIndices_.Initialize(
					device,
					sizeof(uint32_t) * 15LLU
				);
				uint32_t indices[15] = { 5, 0, 1, 5, 1, 2, 5, 2, 3, 5, 3, 4, 5, 4, 0, };
				ElementPowerIndicator_.UB_MeshIndices_.Store(indices, sizeof(uint32_t) * 15, 0);
				ElementPowerIndicator_.IBV_ = Lumina::DX12::IBV{ ElementPowerIndicator_.UB_MeshIndices_ };

				ElementPowerIndicator_.UB_World_.Initialize(device, 256LLU);
				ElementPowerIndicator_.UB_World_.Store(
					&ElementPowerIndicator_.World,
					sizeof(Lumina::Mat4),
					0LLU
				);

				Texture_.Initialize(device, 64U, 64U);
				dx12Context_.GlobalDescriptorHeap().Allocate(CSUTable_, 4U);
				Lumina::DX12::SRV<void>::Create(
					device,
					CSUTable_.CPUHandle(0U),
					Texture_
				);
				Lumina::DX12::UAV<Lumina::Float4>::Create(
					device,
					CSUTable_.CPUHandle(1U),
					Texture_
				);
				OrthoProj_ = Lumina::Mat4::Orthographic(0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 100.0f);
				UB_OrthoProj_.Initialize(device, 256LLU);
				UB_OrthoProj_.Store(&OrthoProj_, sizeof(Lumina::Mat4), 0LLU);
				Lumina::DX12::CBV::Create(
					device,
					CSUTable_.CPUHandle(2U),
					UB_OrthoProj_
				);

				DB_TextureParams_.Initialize(device, 256LLU);
				UB_TextureParams_.Initialize(device, DB_TextureParams_.SizeInBytes());
				Lumina::DX12::CBV::Create(
					device,
					CSUTable_.CPUHandle(3U),
					DB_TextureParams_
				);

				auto&& rsSetup{ Lumina::DX12::LoadRootSignatureSetup(config_.at("UIRS")) };
				RS_.Initialize(device, rsSetup, "UIRS");
				dx12Context_.Compile(
					VS_,
					L"Assets/Shaders/UI.VS.hlsl",
					L"vs_6_6",
					L"main",
					"UIVS"
				);
				dx12Context_.Compile(
					PS_,
					L"Assets/Shaders/UI.PS.hlsl",
					L"ps_6_6",
					L"main",
					"UIPS"
				);
				Lumina::DX12::BlendState blendState{};
				{
					blendState.AlphaToCoverageEnable = false;
					blendState.IndependentBlendEnable = false;
					blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
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
				}
				auto&& rasterizerState{ Lumina::DX12::LoadRasterizerState(config_.at("UIPSO")) };
				Lumina::DX12::DepthStencilState depthStencilState{
					.DepthEnable{ true },
					.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
					.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
					.StencilEnable{ false },
				};
				auto&& inputLayout{ Lumina::DX12::LoadInputLayout(config_.at("UIPSO")) };
				PSO_.Initialize(
					device,
					RS_,
					VS_,
					PS_,
					blendState,
					rasterizerState,
					depthStencilState,
					inputLayout,
					D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
					Lumina::DX12::GraphicsPSO::DefaultRTVFormats,
					Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
					"UIPSO"
				);

				TexRS_.Initialize(
					device,
					Lumina::DX12::LoadRootSignatureSetup(config_.at("UITexComputeRS")),
					"UITexRS"
				);
				dx12Context_.Compile(
					TexCS_,
					L"Assets/Shaders/UITex.CS.hlsl",
					L"cs_6_6",
					L"main",
					"UITexCS"
				);
				TexPSO_.Initialize(
					device,
					TexRS_,
					TexCS_,
					"PlayerTexPSO"
				);
			}

			void Update(Lumina::DX12::CommandList const& directList_, Player const& player_) {
				static Lumina::Float4 const colors[5] = {
					{ 0.3f, 0.7f, 0.2f, 1.0f },
					{ 0.8f, 0.1f, 0.1f, 1.0f },
					{ 0.6f, 0.3f, 0.5f, 1.0f },
					{ 0.9f, 0.9f, 0.3f, 1.0f },
					{ 0.1f, 0.2f, 0.8f, 1.0f },
				};

				for (int i = 0; i < 5; ++i) {
					ElementPowerIndicator_.Vertices[i] = {
						std::cos(i * 3.14159265f * (0.4f)) * player_.ElementPowers[i] * 100.0f,
						std::sin(i * 3.14159265f * (0.4f)) * player_.ElementPowers[i] * 100.0f,
						0.0f,
						1.0f
					};

					ElementPowerIndicator_.UB_MeshVertices_.Store(
						&ElementPowerIndicator_.Vertices[i],
						sizeof(Lumina::Vec4),
						sizeof(ElementPowerIndicator::Vertex) * i
					);
					ElementPowerIndicator_.UB_MeshVertices_.Store(
						&colors[i],
						sizeof(Lumina::Float4),
						sizeof(ElementPowerIndicator::Vertex) * i + sizeof(Lumina::Vec4)
					);

					ElementPowerIndicator_.TexCoords[i] = {
						std::cos(i * 3.14159265f * (0.4f)) * player_.ElementPowers[i] * 0.5f + 0.5f,
						std::sin(i * 3.14159265f * (0.4f)) * player_.ElementPowers[i] * 0.5f + 0.5f
					};

					ElementPowerIndicator_.UB_MeshVertices_.Store(
						&ElementPowerIndicator_.TexCoords[i],
						sizeof(Lumina::Float2),
						sizeof(ElementPowerIndicator::Vertex) * i + sizeof(Lumina::Vec4) + sizeof(Lumina::Float4)
					);
				}

				TexParams_.Time += 0.05f;
				UB_TextureParams_.Store(&TexParams_, sizeof(TextureParams), 0LLU);

				directList_->CopyBufferRegion(
					DB_TextureParams_.Get(),
					0LLU,
					UB_TextureParams_.Get(),
					0LLU,
					sizeof(TextureParams)
				);
			}

			void Render(
				Lumina::DX12::CommandList const& directList_
			) {
				directList_->SetPipelineState(PSO_.Get());
				directList_->SetGraphicsRootSignature(RS_.Get());
				directList_->SetGraphicsRootDescriptorTable(0U, CSUTable_.GPUHandle(2U));
				directList_->SetGraphicsRootDescriptorTable(1U, CSUTable_.GPUHandle(0U));
				directList_->SetGraphicsRootConstantBufferView(2U, ElementPowerIndicator_.UB_World_->GetGPUVirtualAddress());
				directList_->IASetVertexBuffers(0U, 1U, &ElementPowerIndicator_.VBV_);
				directList_->IASetIndexBuffer(&ElementPowerIndicator_.IBV_);
				directList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				directList_->DrawIndexedInstanced(15U, 1U, 0U, 0U, 0U);
			}
		};
	}

	export class Scene_InGame {
		struct MapMetadata {
			uint32_t Width;
			uint32_t Height;
		};

	private:
		void UpdateMap(Lumina::DX12::CommandList const& directList_) {
			static D3D12_RESOURCE_BARRIER const barriers[]{
				{
					.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
					.Transition{
						.pResource{ Buffer_MapData_.Get() },
						.StateBefore{ D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				},
				{
					.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
					.Transition{
						.pResource{ Buffer_MapData_.Get() },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE },
					},
				},
			};

			directList_->ResourceBarrier(1U, &barriers[0]);
			for (uint32_t y = 0; y < MapMetadata_.Height; ++y) {
				for (uint32_t x = 0; x < MapMetadata_.Width; ++x) {
					if (MapBlockDamage(Map_[y][x]) >= 400 &&
						y != 0 && y != MapMetadata_.Height - 1 &&
						x != 0 && x != MapMetadata_.Width - 1) {
						Map_[y][x] = 0;
						UB_MapData_.Store(
							&Map_[y][x],
							sizeof(int),
							sizeof(int) * (MapMetadata_.Width * y + x)
						);
						directList_->CopyBufferRegion(
							Buffer_MapData_.Get(),
							sizeof(int) * (MapMetadata_.Width * y + x),
							UB_MapData_.Get(),
							sizeof(int) * (MapMetadata_.Width * y + x),
							sizeof(int)
						);
					}
				}
			}
			directList_->ResourceBarrier(1U, &barriers[1]);
		}

		void UpdatePlayer(
			Lumina::WinApp::Context const& winAppContext_,
			Lumina::DX12::CommandList const& directList_
		) {
			static auto const& keyboard{
				winAppContext_.RawInputContext(
					winAppContext_.WindowInstance(L"Main")
				).Keyboard()
			};
			Keyboard_Previous_.Set(Keyboard_Current_);
			keyboard.CurrentState(Keyboard_Current_);

			if (keyboard.IsPressed(Lumina::WinApp::KEY::SPACE)) {
				if (Player_->ElementPowers[Player_->ElementInUse] > 0.0f) {
					for (int i = -2; i < 3; ++i) {
						if (!PlayerBulletManager_->List_.IsFull()) {
							[[maybe_unused]] auto& bullet = PlayerBulletManager_->List_.NewElement();
							bullet.Position = {
								std::cos(0.2f * i) * 1.0f * Player_->DirectionY,
								std::sin(0.2f * i) * 1.0f,
								0.0f
							};
							auto&& bulletPos{
								static_cast<Lumina::Vec4>(bullet.Position) *
								Lumina::Mat4::Rotate({ 0.0f, 0.0f, Player_->Rotate.z })
							};
							std::memcpy(&bullet.Position, &bulletPos, sizeof(Lumina::Vec4));
							bullet.Position += Player_->Position;
							bullet.Velocity = {
								bulletPos.x * 0.75f,
								bulletPos.y * 0.75f,
								0.0f
							};
							bullet.Rotate = { 0.0f, 0.0f, 0.0f };
							bullet.Scale = { 0.25f, 0.25f, 0.25f };
							bullet.Size = 1.0f;
							bullet.FrameCount = 0U;
							bullet.Life = 180;
							bullet.ElementType = Player_->ElementInUse;
						}
					}
					if (Player_->ElementInUse != ELEMENT::NO_ELEMENT) {
						Player_->ElementPowers[Player_->ElementInUse] -= 0.005f;
					}
				}
			}

			if (keyboard.IsPressed(Lumina::WinApp::KEY::ARROW_UP)) {
				if (Player_->DirectionZ != 1 ||
					Player_->DirectionY * Player_->RotAnimZ.FinalAngle <= 0.0f) {
					Player_->RotAnimZ.InitialAngle = Player_->Rotate.z;
					Player_->RotAnimZ.FinalAngle = 0.3f * Player_->DirectionY;
					Player_->RotAnimZ.CurrentFrame = 0;
				}

				Player_->Position.y += 0.25f;
				if (MapBlockType(GetMapBlock(MapPos(Player_->Position + Lumina::Vec3{ 0.0f, 1.0f, 0.0f }), Map_)) != 0) {
					Player_->Position.y -= 0.25f;
				}
				Player_->DirectionZ = 1;
			}
			else if (keyboard.IsPressed(Lumina::WinApp::KEY::ARROW_DOWN)) {
				if (Player_->DirectionZ != -1 ||
					Player_->DirectionY * Player_->RotAnimZ.FinalAngle >= 0.0f) {
					Player_->RotAnimZ.InitialAngle = Player_->Rotate.z;
					Player_->RotAnimZ.FinalAngle = -0.3f * Player_->DirectionY;
					Player_->RotAnimZ.CurrentFrame = 0;
				}

				Player_->Position.y -= 0.25f;
				if (MapBlockType(GetMapBlock(MapPos(Player_->Position + Lumina::Vec3{ 0.0f, -1.0f, 0.0f }), Map_)) != 0) {
					Player_->Position.y += 0.25f;
				}
				Player_->DirectionZ = -1;
			}
			else {
				if (Player_->DirectionZ != 0) {
					Player_->RotAnimZ.InitialAngle = Player_->Rotate.z;
					Player_->RotAnimZ.FinalAngle = 0.0f;
					Player_->RotAnimZ.CurrentFrame = 0;
				}

				Player_->DirectionZ = 0;
			}
			if (keyboard.IsPressed(Lumina::WinApp::KEY::ARROW_LEFT)) {
				if (Player_->DirectionY != -1) {
					Player_->RotAnimY.InitialAngle = Player_->Rotate.y;
					Player_->RotAnimY.FinalAngle = std::numbers::pi_v<float>;
					Player_->RotAnimY.CurrentFrame = 0;
				}

				Player_->Position.x -= 0.25f;

				if (MapBlockType(GetMapBlock(MapPos(Player_->Position + Lumina::Vec3{ -1.0f, 0.0f, 0.0f }), Map_)) != 0) {
					Player_->Position.x += 0.25f;
				}
				Player_->DirectionY = -1;
			}
			if (keyboard.IsPressed(Lumina::WinApp::KEY::ARROW_RIGHT)) {
				if (Player_->DirectionY != 1) {
					Player_->RotAnimY.InitialAngle = Player_->Rotate.y;
					Player_->RotAnimY.FinalAngle = 0.0f;
					Player_->RotAnimY.CurrentFrame = 0;
				}

				Player_->Position.x += 0.25f;

				if (MapBlockType(GetMapBlock(MapPos(Player_->Position + Lumina::Vec3{ 1.0f, 0.0f, 0.0f }), Map_)) != 0) {
					Player_->Position.x -= 0.25f;
				}
				Player_->DirectionY = 1;
			}

			if (
				Keyboard_Current_[static_cast<int>(Lumina::WinApp::KEY::Z)] &&
				!Keyboard_Previous_[static_cast<int>(Lumina::WinApp::KEY::Z)]
			) {
				Player_->ElementInUse = static_cast<ELEMENT>((static_cast<int>(Player_->ElementInUse) + 1) % 6);
			}
			if (
				Keyboard_Current_[static_cast<int>(Lumina::WinApp::KEY::X)] &&
				!Keyboard_Previous_[static_cast<int>(Lumina::WinApp::KEY::X)]
			) {
				Player_->ElementInUse = static_cast<ELEMENT>((static_cast<int>(Player_->ElementInUse) + 5) % 6);
			}

			Player_->Update(directList_);
		}

		void RenderPlayer(Lumina::DX12::CommandList const& directList_) {
			directList_->SetPipelineState(Player_->GraphicsPSO_.Get());
			directList_->SetGraphicsRootSignature(Player_->GraphicsRS_.Get());
			directList_->SetGraphicsRootDescriptorTable(0U, CSUTable_.GPUHandle(4U));
			directList_->SetGraphicsRootDescriptorTable(1U, Camera_->CSUTable_.GPUHandle(0U));
			directList_->SetGraphicsRootDescriptorTable(2U, Player_->CSUTable_.GPUHandle(2U));
			directList_->SetGraphicsRootDescriptorTable(3U, Player_->CSUTable_.GPUHandle(0U));
			directList_->IASetVertexBuffers(0U, 1U, &Mesh_Cube_.VBV);
			directList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			directList_->DrawInstanced(Mesh_Cube_.Num_Vertices, 1U, 0U, 0U);
		}

		void UpdatePlayerBullets(Lumina::DX12::CommandList const& directList_) {
			PlayerBulletManager_->Update(directList_);

			decltype(PlayerBulletManager_->List_)::Iterator it{ PlayerBulletManager_->List_ };
			for (it.Begin(); !it.End(); it.Next()) {
				auto& bullet = (*it);
				auto pos = MapPos(bullet.Position);
				if (MapBlockType(GetMapBlock(MapPos(bullet.Position), Map_)) != 0) {
					switch (bullet.ElementType) {
						case ELEMENT::TREE: {
							Bullet::OnHitBlock_TreeType(bullet, Map_[pos.y][pos.x]);
							break;
						}
						case ELEMENT::FIRE: {
							Bullet::OnHitBlock_FireType(bullet, Map_[pos.y][pos.x]);
							break;
						}
						case ELEMENT::EARTH: {
							Bullet::OnHitBlock_EarthType(bullet, Map_[pos.y][pos.x]);
							break;
						}
						case ELEMENT::METAL: {
							Bullet::OnHitBlock_MetalType(bullet, Map_[pos.y][pos.x]);
							break;
						}
						case ELEMENT::WATER: {
							Bullet::OnHitBlock_WaterType(bullet, Map_[pos.y][pos.x]);
							break;
						}
						default: {
							Bullet::OnHitBlock(bullet, Map_[pos.y][pos.x]);
							break;
						}
					}
				}
			}
		}

		void UpdateEnemies(
			Lumina::DX12::CommandList const& directList_
		) {
			static int cnt = 0;

			if (cnt % 16 == 15 && !EnemyManager_->List_.IsFull()) {
				[[maybe_unused]] auto& enemy = EnemyManager_->List_.NewElement();
				enemy.Position = Player_->Position;
				enemy.Velocity = { 0.0f, 0.0f, 0.0f };
				enemy.Rotate = { 0.0f, 0.0f, 0.0f };
				enemy.Scale = { 0.5f, 0.5f, 0.5f };
				enemy.FrameCount = 0U;
				enemy.Life = 50.0f;
				//enemy.ElementType = ELEMENT::WATER;
				enemy.ElementType = static_cast<ELEMENT>(RndGen() % 5U);

				switch (enemy.ElementType) {
					case ELEMENT::WATER: {
						enemy.Position.y = MapMetadata_.Height * 2.0f + static_cast<float>(RndGen() % 10U);
						break;
					}
					case ELEMENT::FIRE: {
						enemy.Position.y = -static_cast<float>(RndGen() % 5U);
						break;
					}
					case ELEMENT::TREE:
					case ELEMENT::EARTH: {
						float theta = static_cast<float>(RndGen() % 360U) * 0.0174532925f;
						enemy.Position.x += std::cos(theta) * 30.0f;
						enemy.Position.y += std::sin(theta) * 30.0f;
						break;
					}
					case ELEMENT::METAL: {
						float theta = static_cast<float>(RndGen() % 360U) * 0.0174532925f;
						enemy.Position.x += std::cos(theta) * 30.0f;
						enemy.Position.y += std::sin(theta) * 30.0f;
						if (RndGen() & 1U) {
							enemy.Velocity.x = 0.2f;
							if (enemy.Position.x > Player_->Position.x) { enemy.Velocity.x *= -1.0f; }
						}
						else {
							enemy.Velocity.y = 0.2f;
							if (enemy.Position.y > Player_->Position.y) { enemy.Velocity.y *= -1.0f; }
						}
						break;
					}
				}
			}

			++cnt;

			EnemyManager_->Update(directList_, *Player_);

			decltype(EnemyManager_->List_)::Iterator it_Enemy{ EnemyManager_->List_ };
			for (it_Enemy.Begin(); !it_Enemy.End(); it_Enemy.Next()) {
				auto& enemy = (*it_Enemy);

				if (
					enemy.FrameCount > 60 &&
					(enemy.Position.x < 0.0f || enemy.Position.x > MapMetadata_.Width * 2.0f ||
					enemy.Position.y < 0.0f || enemy.Position.y > MapMetadata_.Height * 2.0f)
				) {
					EnemyManager_->List_.Delete(it_Enemy);
				}

				if (enemy.ElementType == ELEMENT::EARTH && enemy.FrameCount % 128 == 127) {
					for (int i = 0; i < 12; ++i) {
						if (!EnemyBulletManager_->List_.IsFull()) {
							auto& bullet = EnemyBulletManager_->List_.NewElement();
							bullet.Position = {
								std::cos(0.5235988f * i) * 0.5f,
								std::sin(0.5235988f * i) * 0.5f,
								0.0f
							};
							bullet.Velocity = {
								bullet.Position.x * 0.5f,
								bullet.Position.y * 0.5f,
								0.0f
							};
							bullet.Position += enemy.Position;
							bullet.Rotate = { 0.0f, 0.0f, 0.0f };
							bullet.Scale = { 0.25f, 0.25f, 0.25f };
							bullet.Size = 1.0f;
							bullet.FrameCount = 0U;
							bullet.Life = 180;
							bullet.ElementType = ELEMENT::NO_ELEMENT;
						}
					}
				}
			}

			EnemyBulletManager_->Update(directList_);

			decltype(EnemyBulletManager_->List_)::Iterator it{ EnemyBulletManager_->List_ };
			for (it.Begin(); !it.End(); it.Next()) {
				auto& bullet = (*it);
				auto pos = MapPos(bullet.Position);
				if (MapBlockType(GetMapBlock(MapPos(bullet.Position), Map_)) != 0) {
					Bullet::OnHitBlock(bullet, Map_[pos.y][pos.x]);
				}
				else if (
					bullet.Position.x < 0.0f || bullet.Position.x > MapMetadata_.Width * 2.0f ||
					bullet.Position.y < 0.0f || bullet.Position.y > MapMetadata_.Height * 2.0f
				) {
					EnemyBulletManager_->List_.Delete(it);
				}
			}
		}

		void UpdateCamera(Lumina::DX12::CommandList const& directList_) {
			static Bounds const bounds = {
				.Left = 20.0f,
				.Right = 234.0f,
				.Top = 115.0f,
				.Bottom = 10.0f,
			};
			auto const& playerPos{ Player_->Position };
			float dx{ playerPos.x - Camera_->Translate_.x };
			float dy{ playerPos.y - Camera_->Translate_.y };
			Camera_->Translate_.x += dx * 0.0625f;
			Camera_->Translate_.y += dy * 0.0625f;
			Camera_->Translate_.x = std::clamp(Camera_->Translate_.x, bounds.Left, bounds.Right);
			Camera_->Translate_.y = std::clamp(Camera_->Translate_.y, bounds.Bottom, bounds.Top);

			/*ImGui::Begin("Camera");
			ImGui::DragFloat3("Rotate##Camera", Camera_->Rotate_(), 0.01f);
			ImGui::DragFloat3("Translate##Camera", Camera_->Translate_(), 0.01f);
			ImGui::DragFloat3("PlayerPos##Camera", Player_->Position(), 0.01f);
			ImGui::End();*/

			Camera_->SRT_ = Lumina::Mat4::SRT(
				Camera_->Scale_,
				Camera_->Rotate_,
				Camera_->Translate_
			);
			Camera_->View_ = Camera_->SRT_.Inv();
			Camera_->Update(directList_);
		}

		void CheckCollision() {
			// Player vs. enemies

			decltype(EnemyManager_->List_)::Iterator it_Enemy{ EnemyManager_->List_ };
			for (it_Enemy.Begin(); !it_Enemy.End(); it_Enemy.Next()) {
				auto& enemy = *it_Enemy;
				float dx = Player_->Position.x - enemy.Position.x;
				float dy = Player_->Position.y - enemy.Position.y;
				float d = Player_->Scale.x + enemy.Scale.x;
				if (dx * dx + dy * dy <= d * d) {
					EnemyManager_->List_.Delete(it_Enemy);
				}
			}

			// Player vs. enemy bullets
			
			decltype(EnemyBulletManager_->List_)::Iterator it_EnemyBullet{ EnemyBulletManager_->List_ };
			for (it_EnemyBullet.Begin(); !it_EnemyBullet.End(); it_EnemyBullet.Next()) {
				auto& bullet = *it_EnemyBullet;
				float dx = Player_->Position.x - bullet.Position.x;
				float dy = Player_->Position.y - bullet.Position.y;
				float d = Player_->Scale.x + bullet.Scale.x;
				if (dx * dx + dy * dy <= d * d) {
					EnemyBulletManager_->List_.Delete(it_EnemyBullet);
				}
			}

			// Player bullets vs. enemies
			
			decltype(PlayerBulletManager_->List_)::Iterator it_PlayerBullet{ PlayerBulletManager_->List_ };
			for (it_PlayerBullet.Begin(); !it_PlayerBullet.End(); it_PlayerBullet.Next()) {
				auto& bullet = *it_PlayerBullet;
				for (it_Enemy.Begin(); !it_Enemy.End(); it_Enemy.Next()) {
					auto& enemy = *it_Enemy;
					float dx = bullet.Position.x - enemy.Position.x;
					float dy = bullet.Position.y - enemy.Position.y;
					float d = bullet.Scale.x + enemy.Scale.x;
					if (dx * dx + dy * dy <= d * d) {
						PlayerBulletManager_->List_.Delete(it_PlayerBullet);
						EnemyManager_->List_.Delete(it_Enemy);
						break;
					}
				}
			}
		}

	public:
		void Update(
			Lumina::WinApp::Context const& winAppContext_,
			Lumina::DX12::CommandList const& directList_
		) {
			UpdateMap(directList_);
			UpdatePlayer(winAppContext_, directList_);
			UpdatePlayerBullets(directList_);
			UpdateEnemies(directList_);
			CheckCollision();
			UpdateCamera(directList_);
			UIManager_->Update(directList_, *Player_);
		}

		void Render(
			Lumina::DX12::Context const& dx12Context_,
			Lumina::DX12::CommandList const& directList_
		);

	private:
		void InitializeMap(
			Lumina::DX12::Context const& dx12Context_,
			NLohmannJSON const& config_
		);

	public:
		void Initialize(
			Lumina::DX12::Context const& dx12Context_
		);

	private:
		std::vector<std::vector<int>> Map_;
		MapMetadata MapMetadata_{};

		Lumina::DX12::DefaultBuffer Buffer_MapData_{};
		Lumina::DX12::UploadBuffer UB_MapData_{};
		Lumina::DX12::DefaultBuffer Buffer_MapMetadata_{};

		Lumina::DX12::UnorderedAccessBuffer UAB_Indices_Active_{};
		Lumina::DX12::UnorderedAccessBuffer UAB_ActiveCounter_{};
		Lumina::DX12::UploadBuffer UB_ActiveCounterReset_{};
		Lumina::DX12::ReadbackBuffer RBB_ActiveCounter_{};

		MeshTest Mesh_Cube_{};
		MeshTest Mesh_Square_{};

		Lumina::DX12::DescriptorTable CSUTable_{};

		Lumina::DX12::RootSignature ComputeRS_{};
		Lumina::DX12::Shader CS_{};
		Lumina::DX12::ComputePipelineState ComputePSO_{};

		Lumina::DX12::RootSignature MapBlockGraphicsRS_{};
		Lumina::DX12::Shader MapBlockVS_{};
		Lumina::DX12::Shader MapBlockPS_{};
		Lumina::DX12::GraphicsPipelineState MapBlockGraphicsPSO_{};

		Lumina::DX12::CommandAllocator ComputeAllocator_{};
		Lumina::DX12::CommandList ComputeList_{};

		Lumina::DX12::RootSignature GraphicsRS_Player_{};
		Lumina::DX12::Shader VS_Player_{};
		Lumina::DX12::Shader PS_Player_{};
		Lumina::DX12::GraphicsPipelineState GraphicsPSO_Player_{};

	private:
		std::unique_ptr<Player> Player_{ nullptr };
		Lumina::Int2 PlayerInitialTile_{};
		std::unique_ptr<PlayerBulletManager> PlayerBulletManager_{ nullptr };
		std::unique_ptr<EnemyManager> EnemyManager_{ nullptr };
		std::unique_ptr<EnemyBulletManager> EnemyBulletManager_{ nullptr };
		std::unique_ptr<UIManager> UIManager_{ nullptr };
		

		std::unique_ptr<Camera> Camera_{ nullptr };

	private:
		Lumina::Bitset<256U> Keyboard_Current_{};
		Lumina::Bitset<256U> Keyboard_Previous_{};
	};

	void Scene_InGame::InitializeMap(
		Lumina::DX12::Context const& dx12Context_,
		NLohmannJSON const& config_
	) {
		MapMetadata_.Width = 128U;
		MapMetadata_.Height = 64U;

		std::unique_ptr<CellularAutomata> mapGen{ new CellularAutomata{} };
		mapGen->Run(MapMetadata_.Width, MapMetadata_.Height);
		mapGen->GetMap(Map_);
		for (size_t i_Row{ 0LLU }; i_Row < (Map_.size() >> 1U); ++i_Row) {
			Map_.at(i_Row).swap(Map_.at((Map_.size() - 1LLU) - i_Row));
		}
		auto const& caves{ mapGen->GetCaves() };
		auto caveID{ Lumina::Random::Generator()() % static_cast<uint32_t>(caves.size()) };
		auto tileID{ Lumina::Random::Generator()() % static_cast<uint32_t>(caves[caveID].size()) };
		PlayerInitialTile_ = caves[caveID][tileID];
		mapGen.reset();

		/*auto& logger{ Lumina::Utils::Debug::Logger::Instance() };
		for (auto& mapRow : Map_) {
			for (auto& mapTile : mapRow) {
				if ((mapTile & ((1 << 20U) - 1)) != 0) {
					if ((mapTile & ((1 << 20U) - 1)) == 2) {
						logger.ConsolePrint(".");
					}
					else { logger.ConsolePrint("#"); }
				}
				else {
					logger.ConsolePrint(" ");
				}
			}
			logger.ConsolePrint("\n");
		}*/

		auto const& device{ dx12Context_.Device() };
		auto& cmdQueue{ dx12Context_.DirectQueue() };

		Buffer_MapData_.Initialize(device, sizeof(int) * MapMetadata_.Width * MapMetadata_.Height, "MapData");
		Buffer_MapMetadata_.Initialize(device, (sizeof(MapMetadata) + 0xFF) & ~0xFF, "MapMetadata");

		UB_MapData_.Initialize(device, Buffer_MapData_.SizeInBytes());
		for (size_t i_Row{ 0LLU }; i_Row < Map_.size(); ++i_Row) {
			auto const& mapRow{ Map_.at(i_Row) };
			UB_MapData_.Store(
				mapRow.data(),
				sizeof(int) * mapRow.size(),
				sizeof(int) * mapRow.size() * i_Row
			);
		}
		Lumina::DX12::UploadBuffer uploadBuf_MapMetadata{};
		uploadBuf_MapMetadata.Initialize(device, Buffer_MapMetadata_.SizeInBytes());
		uploadBuf_MapMetadata.Store(&MapMetadata_, sizeof(MapMetadata), 0LLU);

		Lumina::DX12::CommandAllocator cmdAlloc{};
		cmdAlloc.Initialize(device, cmdQueue.Type());
		Lumina::DX12::CommandList cmdList{};
		cmdList.Initialize(device, cmdAlloc);
		cmdList->CopyResource(Buffer_MapData_.Get(), UB_MapData_.Get());
		cmdList->CopyResource(Buffer_MapMetadata_.Get(), uploadBuf_MapMetadata.Get());

		UAB_Indices_Active_.Initialize(device, sizeof(uint32_t) * MapMetadata_.Width * MapMetadata_.Height);
		UAB_ActiveCounter_.Initialize(device, sizeof(uint32_t) * 1LLU);
		UB_ActiveCounterReset_.Initialize(device, sizeof(uint32_t) * 1LLU);
		uint32_t const counterResetVal{ 0U };
		UB_ActiveCounterReset_.Store(&counterResetVal, sizeof(uint32_t), 0LLU);
		RBB_ActiveCounter_.Initialize(device, sizeof(uint32_t) * 1LLU);

		std::vector<D3D12_RESOURCE_BARRIER> barriers{
			D3D12_RESOURCE_BARRIER{
				.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
				.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
				.Transition{
					.pResource{ Buffer_MapData_.Get() },
					.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
					.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
					.StateAfter{ D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE },
				},
			},
			D3D12_RESOURCE_BARRIER{
				.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
				.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
				.Transition{
					.pResource{ Buffer_MapMetadata_.Get() },
					.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
					.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
					.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
				},
			},
		};
		cmdList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());

		cmdQueue.BatchCommandList(cmdList);
		cmdQueue.CPUWait(cmdQueue.ExecuteBatchedCommandLists());

		Mesh_Cube_.Initialize(device, cmdQueue, "cube.obj", "Assets");
		Mesh_Square_.Initialize(device, cmdQueue, "plane.obj", "Assets");

		auto const& gpuDH{ dx12Context_.GlobalDescriptorHeap() };
		CSUTable_ = gpuDH.Allocate(10U);
		Lumina::DX12::SRV<int>::Create(device, CSUTable_.CPUHandle(0U), Buffer_MapData_);
		Lumina::DX12::CBV::Create(device, CSUTable_.CPUHandle(1U), Buffer_MapMetadata_);
		Lumina::DX12::UAV<uint32_t>::Create(device, CSUTable_.CPUHandle(2U), UAB_Indices_Active_, UAB_ActiveCounter_);
		Lumina::DX12::SRV<uint32_t>::Create(device, CSUTable_.CPUHandle(3U), UAB_Indices_Active_);
		Lumina::DX12::SRV<Lumina::Float4>::Create(device, CSUTable_.CPUHandle(4U), Mesh_Cube_.Buffer_CubePositions);
		Lumina::DX12::SRV<Lumina::Float2>::Create(device, CSUTable_.CPUHandle(5U), Mesh_Cube_.Buffer_CubeTexCoords);
		Lumina::DX12::SRV<Lumina::Float3>::Create(device, CSUTable_.CPUHandle(6U), Mesh_Cube_.Buffer_CubeNormals);
		Lumina::DX12::SRV<Lumina::Float4>::Create(device, CSUTable_.CPUHandle(7U), Mesh_Square_.Buffer_CubePositions);
		Lumina::DX12::SRV<Lumina::Float2>::Create(device, CSUTable_.CPUHandle(8U), Mesh_Square_.Buffer_CubeTexCoords);
		Lumina::DX12::SRV<Lumina::Float3>::Create(device, CSUTable_.CPUHandle(9U), Mesh_Square_.Buffer_CubeNormals);

		auto&& rsSetup{ Lumina::DX12::LoadRootSignatureSetup(config_.at("RS")) };
		MapBlockGraphicsRS_.Initialize(device, rsSetup, "RS@InGame");
		dx12Context_.Compile(
			MapBlockVS_,
			L"Assets/Shaders/InGameMap.VS.hlsl",
			L"vs_6_6",
			L"main",
			"VS@InGameMap"
		);
		dx12Context_.Compile(
			MapBlockPS_,
			L"Assets/Shaders/InGameMap.PS.hlsl",
			L"ps_6_6",
			L"main",
			"PS@InGameMap"
		);
		Lumina::DX12::BlendState blendState{};
		{
			blendState.AlphaToCoverageEnable = false;
			blendState.IndependentBlendEnable = false;
			blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_SRC_ALPHA },
				.DestBlendAlpha{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
		}
		auto&& rasterizerState{ Lumina::DX12::LoadRasterizerState(config_.at("PSO")) };
		Lumina::DX12::DepthStencilState depthStencilState{
			.DepthEnable{ true },
			.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
			.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
			.StencilEnable{ false },
		};
		auto&& inputLayout{ Lumina::DX12::LoadInputLayout(config_.at("PSO")) };
		MapBlockGraphicsPSO_.Initialize(
			device,
			MapBlockGraphicsRS_,
			MapBlockVS_,
			MapBlockPS_,
			blendState,
			rasterizerState,
			depthStencilState,
			inputLayout,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			Lumina::DX12::GraphicsPSO::DefaultRTVFormats,
			Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
			"PSO@InGameMap"
		);

		auto&& computeRSSetup{ Lumina::DX12::LoadRootSignatureSetup(config_.at("Compute RS")) };
		ComputeRS_.Initialize(device, computeRSSetup, "RS@InGame");
		dx12Context_.Compile(
			CS_,
			L"Assets/Shaders/InGameMap.CS.hlsl",
			L"cs_6_6",
			L"Update",
			"CS@InGameMap"
		);
		ComputePSO_.Initialize(device, ComputeRS_, CS_);

		ComputeAllocator_.Initialize(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		ComputeList_.Initialize(device, ComputeAllocator_);
	}

	void Scene_InGame::Initialize(
		Lumina::DX12::Context const& dx12Context_
	) {
		NLohmannJSON config{ Lumina::Utils::LoadFromFile<NLohmannJSON>("Scene_InGame.json") };

		InitializeMap(dx12Context_, config);

		//auto const& device{ dx12Context_.Device() };
		//auto& cmdQueue{ dx12Context_.DirectQueue() };

		Camera_.reset(new Camera{});
		Camera_->Initialize(dx12Context_);

		Player_.reset(new Player{});
		Player_->Initialize(dx12Context_, config);
		Player_->Position = {
			PlayerInitialTile_.x * 2.0f,
			((MapMetadata_.Height - 1U) - PlayerInitialTile_.y) * 2.0f,
			0.0f
		};
		Player_->Mesh = &Mesh_Cube_;

		PlayerBulletManager_.reset(new PlayerBulletManager{});
		PlayerBulletManager_->Initialize(dx12Context_, config);
		PlayerBulletManager_->Square = &Mesh_Square_;

		EnemyManager_.reset(new EnemyManager{});
		EnemyManager_->Initialize(dx12Context_, config);
		EnemyManager_->Mesh = &Mesh_Cube_;

		EnemyBulletManager_.reset(new EnemyBulletManager{});
		EnemyBulletManager_->Initialize(dx12Context_, config);
		EnemyBulletManager_->Square = &Mesh_Square_;

		UIManager_.reset(new UIManager{});
		UIManager_->Initialize(dx12Context_, config);
	}

	void Scene_InGame::Render(
		[[maybe_unused]] Lumina::DX12::Context const& dx12Context_,
		Lumina::DX12::CommandList const& directList_
	) {
		static D3D12_RESOURCE_BARRIER const barriers[]{
			 {
				 .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				 .Transition{
					 .pResource{ UAB_ActiveCounter_.Get() },
					 .StateBefore{ D3D12_RESOURCE_STATE_COPY_SOURCE },
					 .StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
				 },
			 },
			 {
				 .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				 .Transition{
					 .pResource{ UAB_ActiveCounter_.Get() },
					 .StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
					 .StateAfter{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
				 },
			 },
			 {
				 .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				 .Transition{
					 .pResource{ UAB_ActiveCounter_.Get() },
					 .StateBefore{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
					 .StateAfter{ D3D12_RESOURCE_STATE_COPY_SOURCE },
				 },
			 },
			 {
				 .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
				 .UAV{.pResource{ nullptr } },
			 },
			 {
				 .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				 .Transition{
					 .pResource{ Player_->Texture_.Get() },
					 .StateBefore{ D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
					 .StateAfter{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
				 },
			 },
			 {
				 .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				 .Transition{
					 .pResource{ Player_->Texture_.Get() },
					 .StateBefore{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
					 .StateAfter{ D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
				 },
			 }
		};

		ID3D12DescriptorHeap* descriptorHeaps[]{ dx12Context_.GlobalDescriptorHeap().Get()};
		ComputeList_->SetDescriptorHeaps(1U, descriptorHeaps);

		ComputeList_->SetPipelineState(ComputePSO_.Get());
		ComputeList_->SetComputeRootSignature(ComputeRS_.Get());
		ComputeList_->SetComputeRootDescriptorTable(0U, CSUTable_.GPUHandle(0U));
		ComputeList_->ResourceBarrier(1U, barriers + 0U);
		ComputeList_->CopyResource(UAB_ActiveCounter_.Get(), UB_ActiveCounterReset_.Get());
		ComputeList_->ResourceBarrier(1U, barriers + 1U);
		ComputeList_->Dispatch((MapMetadata_.Width * MapMetadata_.Height) >> 8U, 1U, 1U);
		ComputeList_->ResourceBarrier(2U, barriers + 2U);
		ComputeList_->CopyResource(RBB_ActiveCounter_.Get(), UAB_ActiveCounter_.Get());

		ComputeList_->SetPipelineState(Player_->TexPSO_.Get());
		ComputeList_->SetComputeRootSignature(Player_->TexRS_.Get());
		ComputeList_->SetComputeRootDescriptorTable(0U, Player_->CSUTable_.GPUHandle(1U));
		ComputeList_->SetComputeRootDescriptorTable(1U, Player_->CSUTable_.GPUHandle(3U));
		ComputeList_->Dispatch(64U >> 4U, 64U >> 4U, 1U);

		ComputeList_->SetPipelineState(EnemyManager_->TexPSO_.Get());
		ComputeList_->SetComputeRootSignature(EnemyManager_->TexRS_.Get());
		ComputeList_->SetComputeRootDescriptorTable(0U, EnemyManager_->CSUTable_.GPUHandle(1U));
		ComputeList_->SetComputeRootDescriptorTable(1U, EnemyManager_->CSUTable_.GPUHandle(4U));
		ComputeList_->Dispatch(64U >> 4U, 64U >> 4U, 1U);

		ComputeList_->SetPipelineState(UIManager_->TexPSO_.Get());
		ComputeList_->SetComputeRootSignature(UIManager_->TexRS_.Get());
		ComputeList_->SetComputeRootDescriptorTable(0U, UIManager_->CSUTable_.GPUHandle(1U));
		ComputeList_->SetComputeRootDescriptorTable(1U, UIManager_->CSUTable_.GPUHandle(3U));
		ComputeList_->Dispatch(64U >> 4U, 64U >> 4U, 1U);

		dx12Context_.ComputeQueue() << ComputeList_;
		auto fenceVal_CQ{ dx12Context_.ComputeQueue().ExecuteBatchedCommandLists() };
		dx12Context_.ComputeQueue().CPUWait(fenceVal_CQ);
		ComputeList_.Reset(ComputeAllocator_);

		//directList_->SetDescriptorHeaps(1U, descriptorHeaps);
		directList_->SetPipelineState(MapBlockGraphicsPSO_.Get());
		directList_->SetGraphicsRootSignature(MapBlockGraphicsRS_.Get());
		directList_->SetGraphicsRootDescriptorTable(0U, CSUTable_.GPUHandle(0U));
		directList_->SetGraphicsRootDescriptorTable(1U, Camera_->CSUTable_.GPUHandle(0U));
		directList_->IASetVertexBuffers(0U, 1U, &Mesh_Cube_.VBV);
		directList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto cnt{ *reinterpret_cast<uint32_t const*>(RBB_ActiveCounter_()) };
		directList_->DrawInstanced(Mesh_Cube_.Num_Vertices, cnt, 0U, 0U);

		RenderPlayer(directList_);
		PlayerBulletManager_->Render(directList_, Camera_->CSUTable_, CSUTable_.GPUHandle(7U));
		EnemyManager_->Render(directList_, Camera_->CSUTable_, CSUTable_.GPUHandle(4U));
		EnemyBulletManager_->Render(directList_, Camera_->CSUTable_, CSUTable_.GPUHandle(7U));
		UIManager_->Render(directList_);
	}
}