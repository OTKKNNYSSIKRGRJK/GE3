module Game.Scene.Title;

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
	template<>
	void Title::Initialize(
		Lumina::DX12::Context const& dxContext_,
		Lumina::AssetManager const& assetMngr_
	) {
		[[maybe_unused]] auto const& device{ dxContext_.Device() };
		[[maybe_unused]] auto& assetMngr{ const_cast<Lumina::AssetManager&>(assetMngr_) };
	}

	Title::Title() = default;
	Title::~Title() = default;
}

namespace Game::Scene {
	template<>
	void Title::Initialize(
		Lumina::DX12::Context const& dxContext_,
		Lumina::AssetManager const& assetMngr_
	) {
		Impl_.reset(new Impl::Title{});
		Impl_->Initialize(dxContext_, assetMngr_);
	}

	Title::Title() = default;
	Title::~Title() = default;
}