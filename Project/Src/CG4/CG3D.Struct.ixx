module;

#include<array>
#include<vector>
#include<map>

export module Lumina.CG3D.Struct;

import <cstdint>;
import <algorithm>;

import <d3d12.h>;

import <span>;
import <ranges>;
import <optional>;

import <string>;
import <format>;

import Lumina.CG3D.ASSIMP;
import Lumina.DX12;
import Lumina.Utils.Data;
import Lumina.Math;

namespace Lumina::CG3D {
	export class TRANSFORM {
	public:
		Float3 Scale;
		Float4 Rotate;
		Float3 Translate;
	};

	export class Animation {
	public:
		template<typename _ATTR>
		struct Keyframe {
			_ATTR Value;
			float TimepointInSecond;
		};

		template<typename _ATTR>
		struct Curve {
			std::vector<Keyframe<_ATTR>> Keyframes;
		};
	};

	export struct MyAnimation {
		struct Node {
			Animation::Curve<Float3> Scale;
			Animation::Curve<Float4> Rotate;
			Animation::Curve<Float3> Translate;
		};

		std::map<std::string, Node> Nodes;
		float DurationInSeconds;
	};

	export class Node {
	public:
		Mat4 Transform_Local;
		TRANSFORM Transform;
		std::string Name;
		std::vector<uint32_t> Indices_Mesh;
		std::vector<Node> Children;
	};

	export struct Joint {
		Mat4 Local;
		Mat4 SkeletonSpace;
		TRANSFORM Transform;
		std::string Name;
		uint32_t ID;
		/// ID of the parent Joint, std::nullopt if this Joint is the root
		std::optional<uint32_t> ID_Parent;
		std::vector<uint32_t> IDs_Child;
	};

	export struct Skeleton {
		uint32_t ID_Root;
		std::map<std::string, uint32_t> IDX_Joint;
		std::vector<Joint> ARR_Joint;
	};

	export class Material {
	public:
		std::string FilePath_Diffuse;
	};

	export class VertexWeightData {
	public:
		float Weight;
		uint32_t VertexID;
	};

	export class JointWeightData {
	public:
		Mat4 INV_BindPose;
		std::vector<VertexWeightData> VertexWeights;
	};

	export class VertexInfluence {
	public:
		constexpr static uint32_t MAXNUM_Influences{ 4U };

	public:
		std::array<float, MAXNUM_Influences> ARR_Weight;
		std::array<uint32_t, MAXNUM_Influences> ARR_JointID;
	};

	export class WellForGPU {
	public:
		Mat4 SkeletonSpace;
		Mat4 TR_INV_SkeletonSpace;
	};

	export class SkinCluster {
	public:
		std::vector<Mat4> ARR_INV_BindPose;

		DX12::UploadBuffer InfluenceResource;
		D3D12_VERTEX_BUFFER_VIEW InfluenceBufferView;
		std::span<VertexInfluence> MappedInfluence;

		DX12::UploadBuffer PaletteResource;
		std::span<WellForGPU> MappedPalette;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> PaletteSRVHandle;
	};

	export class Mesh {
	public:
		struct Vertex {
			Float3 Position;
			Float2 TexCoord;
			Float3 Normal;
		};
	public:
		std::string Name;

		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;

		std::map<std::string, JointWeightData> SkinClusterData;

		uint32_t Index_Material;
	};

	export class Collection {
	public:
		class Importer;

	public:
		Node Root;
		std::vector<Mesh> Meshes;
		std::vector<Material> Materials;
	};

	class Collection::Importer {
	public:
		auto ReadFromFile(
			Collection& out_,
			std::string_view fileName_,
			std::string_view dirPath_
		) const -> void;
	};
}