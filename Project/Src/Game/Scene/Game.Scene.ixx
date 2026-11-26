export module Game.Scene;

import <memory>;

import Lumina;

namespace Game::Scene {
	export class Base {
		Base() = default;
		virtual ~Base() = default;
	};

	// シーンが毎フレーム使う、main.cpp が持っているもの（シーンを作るときに渡す。シーンより長生きすること）
	// 2026-11-26：グローバル変数 Flag / BlockEscapeQuit をやめ、シーンの切り替えは Lumina::SceneManager に集めた
	//   （次のシーンはシーン自身が SceneManager::RequestChange<次のシーン>() で頼む。終了は RequestQuit）
	export struct FrameResources {
		Lumina::DX12::Context const& DX12;
		Lumina::DX12::CommandList const& CmdList;
		Lumina::WinApp::RawInput const& Input;
	};
}