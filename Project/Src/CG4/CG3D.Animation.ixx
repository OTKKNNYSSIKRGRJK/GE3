export module Lumina.CG3D.Animation;

import <algorithm>;

import <d3d12.h>;

import <vector>;

import <span>;
import <ranges>;
import <optional>;

import <string>;
import <format>;

import Lumina.CG3D.ASSIMP;
import Lumina.CG3D.Struct;

import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Aux.View;

namespace Lumina::CG3D {
	export auto CreateSkeleton(Node const& rootNode_) -> Skeleton;

	export auto LoadAnimationFile(
		std::string_view fileName_,
		std::string_view directoryPath_
	) -> std::vector<MyAnimation>;

	export void Update(Skeleton& skeleton_);

	export void ApplyAnimation(Skeleton& skeleton_, MyAnimation const& anim_, float time_);

	export auto CreateSkinCluster(
		SkinCluster& skinCluster_,
		DX12::GraphicsDevice const& d3d12Device_,
		DX12::DescriptorHeap const& globalHeap_,
		Skeleton const& skeleton_,
		Mesh const& mesh_
	) -> void;

	export void Update(SkinCluster& skinCluster_, Skeleton const& skeleton_);

	export void Update(
		SkinCluster& skinCluster_,
		Skeleton& skeleton_,
		MyAnimation const& anim_,
		float time_
	);
}