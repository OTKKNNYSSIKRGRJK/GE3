export module Lumina.DX12 : Barrier;

import <d3d12.h>;

import : Resource;

namespace Lumina::DX12 {
	// Resource barrier helper class
	export class Barrier {
	public:
		template<Concept_Resource ResourceType>
		static auto Transition(
			ResourceType const& res_,
			D3D12_RESOURCE_STATES state_Before_,
			D3D12_RESOURCE_STATES state_After_,
			D3D12_RESOURCE_BARRIER_FLAGS flags_ = D3D12_RESOURCE_BARRIER_FLAG_NONE
		) -> D3D12_RESOURCE_BARRIER {
			return D3D12_RESOURCE_BARRIER{
				.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
				.Flags{ flags_ },
				.Transition{
					// Target of the barrier
					.pResource{ res_.Get() },
					// Resource state before transition
					.StateBefore{ state_Before_ },
					// Resource state after transition
					.StateAfter{ state_After_ },
				},
			};
		}
	};
}