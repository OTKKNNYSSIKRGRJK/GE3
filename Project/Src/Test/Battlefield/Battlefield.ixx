export module Game.Battlefield;

import <memory>;

import <array>;

import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux.View;

import Lumina.ProceduralTerrain;
import Lumina.Wave2D;

import Lumina.Container.List;

namespace Game {
	struct DrawCallData {
		Lumina::DX12::UploadBuffer UB_Vertices;
		Lumina::DX12::UploadBuffer UB_Indices;
		Lumina::DX12::VBV VBV;
		Lumina::DX12::IBV IBV;
	};

	struct Wall {
		Lumina::Mat4 WorldMatrix{};
		Lumina::Mat4 WorldToLocal{};
		Lumina::DX12::UploadBuffer UB_WorldMatrix{};
		std::unique_ptr<Lumina::Wave2D> WaveTexture{ nullptr };
	};

	struct WaveSource {
		uint32_t WallID{};

		Lumina::Float2 Position{ 0.0f };
		float Amplitude{ 1.0f };
		float PhaseOffset{ 0.0f };
		float AngularFrequency{ 0.0f };
		float Timer{ 0.0f };

		using UpdateFunc = bool(*)(WaveSource&);
		UpdateFunc Update{ nullptr };
	};

	class Battlefield {
	public:
		auto Borders() noexcept -> Lumina::Rect&;
		auto Borders() const noexcept -> Lumina::Rect const&;
		auto VisualBorders() noexcept -> Lumina::Rect&;
		auto VisualBorders() const noexcept -> Lumina::Rect const&;

	public:
		void OnHitWall(
			uint32_t idx_Wall_,
			Lumina::Vec3 const& worldPos_,
			float strength_
		);

	public:
		void Update(Lumina::DX12::Context const& dx12Context_);
		void RenderGround(
			Lumina::DX12::GraphicsDevice const& device_,
			Lumina::DX12::CommandList const& cmdList_,
			Lumina::DX12::DescriptorHeap const& localHeap_WorldToNDC_,
			Lumina::DX12::DescriptorTable const& texSRVTable_
		);
		void RenderWalls(
			Lumina::DX12::GraphicsDevice const& device_,
			Lumina::DX12::CommandList const& cmdList_,
			Lumina::DX12::DescriptorHeap const& localHeap_WorldToNDC_,
			Lumina::DX12::DescriptorHeap const& localHeap_UAV_,
			D3D12_GPU_DESCRIPTOR_HANDLE globalSRV_depthTexture_,
			float timer_
		);

	public:
		void Initialize(Lumina::DX12::Context const& dx12Context_);

	private:
		Lumina::Rect Borders_{};
		Lumina::Rect VisualBorders_{};

		std::unique_ptr<Lumina::Terrain> Ground_{};
		std::array<std::unique_ptr<Wall>, 4LLU> Walls_{};
		float WallHeight_{};

		Lumina::DX12::RootSignature GraphicsRS_Walls_{};
		Lumina::DX12::Shader VS_Walls_{};
		Lumina::DX12::Shader PS_Walls_{};
		Lumina::DX12::GraphicsPSO GraphicsPSO_Walls_{};
		DrawCallData Plane_{};

		Lumina::DX12::DescriptorTable GlobalTable_Target_{};
		D3D12_VIEWPORT Viewport_{};

		Lumina::DX12::DescriptorTable GlobalTable_WorldToNDC_{};

		Lumina::List<WaveSource> WaveSources_{};
	};
}