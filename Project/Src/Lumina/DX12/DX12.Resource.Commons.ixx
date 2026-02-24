module;

#include<d3d12.h>
#include<dxgi1_6.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : Resource.Commons;

//****	******	******	******	******	****//

import <type_traits>;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	consteval bool IsAllocatedInSystemRAM(
		D3D12_HEAP_PROPERTIES const& heapProperties_
	) noexcept {
		return (
			(heapProperties_.Type == D3D12_HEAP_TYPE_UPLOAD) ||
			(heapProperties_.Type == D3D12_HEAP_TYPE_GPU_UPLOAD) ||
			(heapProperties_.Type == D3D12_HEAP_TYPE_READBACK) ||
			(heapProperties_.MemoryPoolPreference == D3D12_MEMORY_POOL_L0)
		);
	}

	struct ResourceSettings {
		D3D12_HEAP_PROPERTIES HeapProperties;
		D3D12_RESOURCE_FLAGS ResourceFlags;
		D3D12_RESOURCE_STATES InitialState;
	};

	// No multisampling
	export constexpr DXGI_SAMPLE_DESC SampleDesc_NoMultisampling{
		.Count{ 1U },
		.Quality{ 0U },
	};
}

//****	******	******	******	******	****//

namespace Lumina::DX12 {
	export class IResource {};

	class IBuffer : public IResource {};
	export template<typename T>
		concept Concept_Buffer = std::is_base_of_v<IBuffer, T>;

	class ITexture2D : public IResource {};
	export template<typename T>
		concept Concept_Texture2D = std::is_base_of_v<ITexture2D, T>;
}