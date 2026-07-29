module Game.Scene.Title;

import <string>;

import : Impl;

import Lumina;
import Game.Scene;

namespace Game::Scene::Impl {
	template<>
	void Title::Update(
		[[maybe_unused]] Lumina::DX12::Context const& dxContext_,
		[[maybe_unused]] Lumina::DX12::CommandList const& cmdList_,
		[[maybe_unused]] Lumina::WinApp::RawInput const& input_
	) {
		auto const& keyboard{ input_.Keyboard() };
		using Lumina::WinApp::KEY;
		if (keyboard.IsPressed(KEY::SPACE)) {
			Game::Scene::Flag = 1;
		}
	}
}

namespace Game::Scene {
	template<>
	void Title::Update(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::WinApp::RawInput const& input_
	) {
		Impl_->Update(dxContext_, cmdList_, input_);
	}
}