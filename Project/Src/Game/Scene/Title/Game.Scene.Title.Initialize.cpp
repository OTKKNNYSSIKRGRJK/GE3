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

import Lumina.FirePillar;
import Lumina.MagicCircle;
import Lumina.ShowcaseCharacter;

namespace Game::Scene::Impl {
	namespace {
		// 既定の魔法陣の構成（外周 → 中間 → 内側 → 浮遊リング）
		void SetupDefaultMagicCircle(std::vector<Lumina::MagicCircle::Layer>& layers_) {
			using MC = Lumina::MagicCircle;
			layers_.clear();

			// 外周：二重リングでルーン帯を挟む＋床の照り返し
			{
				auto& layer{ layers_.emplace_back() };
				layer.Elements = MC::Element_Ring0 | MC::Element_Ring1 | MC::Element_Runes | MC::Element_Fill;
				layer.Radius = 6.0f;
				layer.Height = 0.02f;
				layer.RotateSpeed = 0.08f;
				layer.Ring0 = 0.98f;
				layer.Ring1 = 0.84f;
				layer.RuneCount = 36;
				layer.RuneInner = 0.86f;
				layer.RuneOuter = 0.96f;
				layer.RuneSeed = 7;
				layer.Color = { 1.00f, 0.62f, 0.25f };
				layer.FillAmount = 0.06f;
			}

			// 中間：八芒星 {8/3}＋頂点ノード＋目盛り（逆回転）
			{
				auto& layer{ layers_.emplace_back() };
				layer.Elements =
					MC::Element_Ring0 | MC::Element_Ring1 | MC::Element_Polygon |
					MC::Element_Nodes | MC::Element_Ticks;
				layer.Radius = 4.2f;
				layer.Height = 0.03f;
				layer.RotateSpeed = -0.15f;
				layer.Ring0 = 0.98f;
				layer.Ring1 = 0.76f;
				layer.PolygonVertices = 8;
				layer.PolygonStep = 3;
				layer.PolygonRadius = 0.98f;
				layer.NodeRadius = 0.05f;
				layer.TickCount = 64;
				layer.TickInner = 0.82f;
				layer.TickOuter = 0.88f;
				layer.Color = { 1.00f, 0.45f, 0.15f };
			}

			// 内側：六芒星 {6/2}＋短い放射線（速めに回す）
			{
				auto& layer{ layers_.emplace_back() };
				layer.Elements =
					MC::Element_Ring0 | MC::Element_Ring1 | MC::Element_Polygon | MC::Element_Spokes;
				layer.Radius = 2.4f;
				layer.Height = 0.04f;
				layer.RotateSpeed = 0.35f;
				layer.Ring0 = 0.97f;
				layer.Ring1 = 0.45f;
				layer.PolygonVertices = 6;
				layer.PolygonStep = 2;
				layer.PolygonRadius = 0.97f;
				layer.SpokeCount = 6;
				layer.SpokeInner = 0.20f;
				layer.SpokeOuter = 0.45f;
				layer.Color = { 1.00f, 0.80f, 0.45f };
			}

			// 浮遊リング：腰の高さで回る輪（縦方向に重ねる案の試し用。既定はオフ）
			{
				auto& layer{ layers_.emplace_back() };
				layer.Enabled = false;
				layer.Elements = MC::Element_Ring0 | MC::Element_Ticks | MC::Element_Runes;
				layer.Radius = 2.2f;
				layer.Height = 2.6f;
				layer.RotateSpeed = 0.6f;
				layer.Ring0 = 0.95f;
				layer.TickCount = 48;
				layer.TickInner = 0.86f;
				layer.TickOuter = 0.92f;
				layer.RuneCount = 20;
				layer.RuneInner = 0.68f;
				layer.RuneOuter = 0.80f;
				layer.RuneSeed = 3;
				layer.Intensity = 0.7f;
				layer.GlowAmount = 0.25f;
			}
		}
	}

	template<>
	void Title::Initialize(
		Lumina::DX12::Context const& dxContext_,
		Lumina::AssetManager const& assetMngr_
	) {
		[[maybe_unused]] auto const& device{ dxContext_.Device() };
		[[maybe_unused]] auto& assetMngr{ const_cast<Lumina::AssetManager&>(assetMngr_) };

		// いずれもバックバッファ（SRGB RTV）＋ スワップチェインの深度（D24S8）に描く

		//----	------	------	------	------	----//
		// Character（原点に立たせる）

		Character_.Initialize(dxContext_);
		Character_.Position = { 0.0f, 0.0f, 0.0f };

		//----	------	------	------	------	----//
		// FirePillar：既定はキャラを包む 1 本（設定ファイルがあればそちらで上書き）

		FirePillarRenderer_.Initialize(dxContext_, assetMngr);

		FirePillars_.clear();
		{
			auto& entry{ FirePillars_.emplace_back() };
			std::char_traits<char>::copy(entry.Name, "Outer", 6U);
			entry.Desc.Radius = 2.0f;
			entry.Desc.Height = 8.0f;
		}

		//----	------	------	------	------	----//
		// MagicCircle

		MagicCircleRenderer_.Initialize(dxContext_);
		SetupDefaultMagicCircle(MagicCircleLayers_);

		//----	------	------	------	------	----//
		// Particles（本編の ParticleSystem を流用）

		InitializeParticles(dxContext_, assetMngr);

		//----	------	------	------	------	----//
		// Menu UI（Start / Exit。本編の SpriteRenderer を流用）

		InitializeUI(dxContext_, assetMngr);

		//----	------	------	------	------	----//
		// 調整結果の読み込み（無ければ上の既定値のまま）

		LoadSettings();
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
