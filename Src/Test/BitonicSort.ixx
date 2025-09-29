module;

#include<cstdint>

#include<chrono>

#include<format>

#include<fstream>

#include<d3d12.h>

#include<External/nlohmann.JSON/single_include/nlohmann/json.hpp>

export module Lumina.BitonicSort;

import Lumina.DX12;
import Lumina.DX12.Aux;

import Lumina.Math;
import Lumina.Utils.Data;

struct testBufStruct {
	uint32_t GroupThreadID[3];
	uint32_t GroupID[3];
	uint32_t DispatchThreadID[3];
	uint32_t GroupIndex;
};

namespace Lumina::DX12 {
	struct BitonicLoopMetadata {
		uint32_t Offset_EntryID;
		uint32_t Mask_ReverseDir;
	};

	template<typename T, uint32_t Num_Entries>
	struct SortData {
		constexpr uint32_t ArrayLength() const noexcept { return Num_Entries; }

		UploadBuffer TempBuffer{};
		UnorderedAccessBuffer DataBuffer{};
		ReadbackBuffer RBBuffer{};
		DefaultBuffer BitonicLoopMetadataBuffer{};
		UploadBuffer BitonicLoopMetadataArrayBuffer{};
		BitonicLoopMetadata* BitonicLoopMetadataArray;
		DescriptorTable Table{};

		SortData(
			const GraphicsDevice& device_,
			DescriptorHeap& cbvsrvuavHeap_
		) {
			Table = cbvsrvuavHeap_.Allocate(2U);

			TempBuffer.Initialize(device_, sizeof(T) * Num_Entries);
			DataBuffer.Initialize(device_, sizeof(T) * Num_Entries);
			DX12::RawUAV::Create(device_, Table.CPUHandle(0U), DataBuffer);

			RBBuffer.Initialize(device_, sizeof(T) * Num_Entries);

			BitonicLoopMetadataBuffer.Initialize(device_, sizeof(BitonicLoopMetadata));
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
				.Format{ DXGI_FORMAT_UNKNOWN },
				.ViewDimension{ D3D12_SRV_DIMENSION_BUFFER },
				.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
				.Buffer{
					.NumElements{ 1U },
					.StructureByteStride{ sizeof(uint32_t) },
				}
			};
			device_->CreateShaderResourceView(
				BitonicLoopMetadataBuffer.Get(),
				&srvDesc,
				Table.CPUHandle(1U)
			);
			BitonicLoopMetadataArrayBuffer.Initialize(device_, sizeof(BitonicLoopMetadata) * 256LLU);
			BitonicLoopMetadataArray = reinterpret_cast<BitonicLoopMetadata*>(BitonicLoopMetadataArrayBuffer());
		}
	};

	template<uint32_t Log2_Num_ThreadsX = 9U>
	class BitonicSorter final {
	public:
		template<typename T, uint32_t Num_Entries>
		void Sort(
			SortData<T, Num_Entries>& data_,
			CommandQueue& computeQueue_,
			const DescriptorHeap& cbvsrvuavHeap_
		);

		BitonicSorter(
			const GraphicsDevice& device_,
			const Shader::Compiler& shaderCompiler_,
			const NLohmannJSON& config_
		);
		~BitonicSorter() noexcept;

	private:
		CommandAllocator ComputeAllocator_{};
		CommandList ComputeList_{};

		Shader ComputeShader_{};
		RootSignature ComputeRS_{};
		ComputePipelineState ComputePSO_{};
	};

	template<uint32_t Log2_Num_ThreadsX>
	template<typename T, uint32_t Num_Entries>
	void BitonicSorter<Log2_Num_ThreadsX>::Sort(
		SortData<T, Num_Entries>& data_,
		CommandQueue& computeQueue_,
		const DescriptorHeap& cbvsrvuavHeap_
	) {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier = D3D12_RESOURCE_BARRIER{
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition{
				.pResource{ data_.DataBuffer.Get() },
				.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
				.StateBefore{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
				.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
			}
		};
		ComputeList_->ResourceBarrier(1U, &barrier);
		ComputeList_->CopyResource(data_.DataBuffer.Get(), data_.TempBuffer.Get());
		barrier = D3D12_RESOURCE_BARRIER{
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition{
				.pResource{ data_.DataBuffer.Get() },
				.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
				.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
				.StateAfter{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
			}
		};
		ComputeList_->ResourceBarrier(1U, &barrier);

		//----	------	------	------	------	----//

		ID3D12DescriptorHeap* descriptorHeaps[]{ cbvsrvuavHeap_.Get() };
		ComputeList_->SetDescriptorHeaps(1U, descriptorHeaps);
		ComputeList_->SetPipelineState(ComputePSO_.Get());
		ComputeList_->SetComputeRootSignature(ComputeRS_.Get());
		ComputeList_->SetComputeRootDescriptorTable(0U, data_.Table.GPUHandle(0U));

		uint32_t copyOffset{ 0U };
		for (
			uint32_t i_Mask_ReverseDir{ 2U };
			i_Mask_ReverseDir <= data_.ArrayLength();
			i_Mask_ReverseDir <<= 1U
		) {
			for (
				uint32_t i_Offset_EntryID{ i_Mask_ReverseDir >> 1U };
				i_Offset_EntryID >= 1U;
				i_Offset_EntryID >>= 1U
			) {
				data_.BitonicLoopMetadataArray[copyOffset] = BitonicLoopMetadata{
					.Offset_EntryID{ i_Offset_EntryID },
					.Mask_ReverseDir{ i_Mask_ReverseDir },
				};
				ComputeList_->CopyBufferRegion(
					data_.BitonicLoopMetadataBuffer.Get(),
					0LLU,
					data_.BitonicLoopMetadataArrayBuffer.Get(),
					sizeof(BitonicLoopMetadata) * copyOffset,
					sizeof(BitonicLoopMetadata)
				);
				ComputeList_->Dispatch((data_.ArrayLength() >> 1U) >> Log2_Num_ThreadsX, 1U, 1U);
				++copyOffset;
			}
		}

		//----	------	------	------	------	----//

		barrier = D3D12_RESOURCE_BARRIER{
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition{
				.pResource{ data_.DataBuffer.Get() },
				.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
				.StateBefore{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
				.StateAfter{ D3D12_RESOURCE_STATE_COPY_SOURCE },
			}
		};
		ComputeList_->ResourceBarrier(1U, &barrier);
		ComputeList_->CopyResource(data_.RBBuffer.Get(), data_.DataBuffer.Get());
		barrier = D3D12_RESOURCE_BARRIER{
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition{
				.pResource{ data_.DataBuffer.Get() },
				.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
				.StateBefore{ D3D12_RESOURCE_STATE_COPY_SOURCE },
				.StateAfter{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
			}
		};
		ComputeList_->ResourceBarrier(1U, &barrier);

		//----	------	------	------	------	----//

		computeQueue_ << ComputeList_;
		computeQueue_.CPUWait(computeQueue_.ExecuteBatchedCommandLists());
		ComputeList_.Reset(ComputeAllocator_);
	}

	template<uint32_t Log2_Num_ThreadsX>
	BitonicSorter<Log2_Num_ThreadsX>::BitonicSorter(
		const GraphicsDevice& device_,
		const Shader::Compiler& shaderCompiler_,
		const NLohmannJSON& config_
	) {
		ComputeAllocator_.Initialize(device_, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		ComputeList_.Initialize(device_, ComputeAllocator_);

		ComputeShader_.Initialize(
			shaderCompiler_,
			L"Assets/Shaders/Test.CS.hlsl",
			L"cs_6_6",
			L"main"
		);
		auto&& rsSetup{ LoadRootSignatureSetup(config_) };
		ComputeRS_.Initialize(device_, rsSetup);
		ComputePSO_.Initialize(device_, ComputeRS_, ComputeShader_);
	}

	template<uint32_t Log2_Num_ThreadsX>
	BitonicSorter<Log2_Num_ThreadsX>::~BitonicSorter() noexcept = default;
}

struct Particle {
	uint32_t Index;
	int Val;
};

export namespace Lumina::DX12::Test{
	void Foo(
		[[maybe_unused]] const Lumina::DX12::GraphicsDevice& device_,
		[[maybe_unused]] Lumina::DX12::DescriptorHeap& cbvsrvuavHeap_,
		[[maybe_unused]] const Lumina::DX12::Shader::Compiler& shaderCompiler_,
		[[maybe_unused]] const NLohmannJSON& config_
	) {
		std::chrono::steady_clock::time_point timeStamps[2]{};
		std::chrono::nanoseconds totalTime{ 0 };
		int cnt_TimeStamp = 0;

		std::ofstream ofs{ "test.txt" };
		ofs << "Quick sort: << \n";

		CommandQueue computeQueue{};
		computeQueue.Initialize(device_, D3D12_COMMAND_LIST_TYPE_COMPUTE, "Foo");
		BitonicSorter<9U> sorter{ device_, shaderCompiler_, config_ };

		SortData<Particle, (1U << 22U)> sortData{ device_, cbvsrvuavHeap_ };

		//auto* unsortedData{ reinterpret_cast<Particle*>(sortData.TempBuffer()) };

		Particle tmpP;
		for (int k = 0; k < 100; ++k) {
			for (uint32_t i{ 0U }; i < sortData.ArrayLength(); ++i) {
				tmpP = {
					.Index{ i },
					.Val{ static_cast<int>(Lumina::Random::Generator()()) & 0xFFFF },
				};
				sortData.TempBuffer.Store(&tmpP, sizeof(Particle), sizeof(Particle) * i);
			}

			timeStamps[cnt_TimeStamp] = std::chrono::steady_clock::now();
			sorter.Sort(sortData, computeQueue, cbvsrvuavHeap_);
			timeStamps[cnt_TimeStamp ^ 1] = std::chrono::steady_clock::now();

			if (k >= 50) {
				auto frameTime = timeStamps[cnt_TimeStamp ^ 1] - timeStamps[cnt_TimeStamp];
				ofs << "#" << k << ": " << frameTime << "\n";
				totalTime += frameTime;
			}
			cnt_TimeStamp ^= 1;
		}
		ofs << "Total: " << totalTime << "\n";
		ofs << "Avg: " << totalTime / 50 << " = " <<
			std::chrono::duration_cast<std::chrono::milliseconds>(totalTime) / 50 << "\n";

		//const auto* computeData{ reinterpret_cast<const Particle*>(sortData.RBBuffer()) };

		//for (uint32_t i = 0; i < 128; ++i) {
		//	std::string&& outputStr = std::format(
		//		"#{}: Index = {}, Val = {}\n",
		//		i,
		//		computeData[i].Index,
		//		computeData[i].Val
		//	);
		//	/*std::string&& outputStr = std::format(
		//		"#{}:\nGroupThreadID = ({}, {}, {})\nGroupID = ({}, {}, {})\nDispatchThreadID = ({}, {}, {})\nGroupIndex = {}\n\n",
		//		i,
		//		computeData[i].GroupThreadID[0], computeData[i].GroupThreadID[1], computeData[i].GroupThreadID[2],
		//		computeData[i].GroupID[0], computeData[i].GroupID[1], computeData[i].GroupID[2],
		//		computeData[i].DispatchThreadID[0], computeData[i].DispatchThreadID[1], computeData[i].DispatchThreadID[2],
		//		computeData[i].GroupIndex
		//	);*/
		//	ofs << outputStr;
		//}

		/*Particle* particles = new Particle[1U << 22U];
		for (int k = 0; k < 100; ++k) {
			for (uint32_t i = 0; i < 1U << 22U; ++i) {
				particles[i].Index = i;
				particles[i].Val = Lumina::RndEngine() & 0xFFFF;
			}
			timeStamps[cnt_TimeStamp] = std::chrono::steady_clock::now();
			std::qsort(
				particles, 1U << 22U, sizeof(Particle),
				[](const void* p0_, const void* p1_)->int {
					return (
						reinterpret_cast<const Particle*>(p0_)->Val >
						reinterpret_cast<const Particle*>(p1_)->Val
					);
				}
			);
			timeStamps[cnt_TimeStamp ^ 1] = std::chrono::steady_clock::now();

			if (k >= 50) {
				auto frameTime = timeStamps[cnt_TimeStamp ^ 1] - timeStamps[cnt_TimeStamp];
				ofs << "#" << k << ": " << frameTime << "\n";
				totalTime += frameTime;
			}
			cnt_TimeStamp ^= 1;
		}
		ofs << "Total: " << totalTime << "\n";
		ofs << "Avg: " << totalTime / 50 << " = " <<
			std::chrono::duration_cast<std::chrono::milliseconds>(totalTime) / 50 << "\n";
		delete[] particles;*/

		ofs.close();
	}
}