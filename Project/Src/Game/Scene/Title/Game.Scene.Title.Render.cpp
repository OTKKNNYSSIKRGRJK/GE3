module Game.Scene.Title;

import : Impl;

import Lumina;

namespace Game::Scene::Impl {
	template<>
	void Title::Render(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_
	) {
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