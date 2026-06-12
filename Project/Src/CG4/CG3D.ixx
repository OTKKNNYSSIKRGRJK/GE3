module;

#include<array>
#include<vector>
#include<map>

export module Lumina.CG3D;

import <algorithm>;

import <d3d12.h>;

import <span>;
import <ranges>;
import <optional>;

import <string>;
import <format>;

import Lumina.CG3D.ASSIMP;
import Lumina.CG3D.Struct;
import Lumina.CG3D.Animation;

import Lumina.Math;
import Lumina.Utils.Debug;

import Lumina.DX12;
import Lumina.DX12.Aux.View;

/// TODO: study the structure of glTF

namespace Lumina::CG3D {
	/*namespace {
		template <uint32_t _N, uint32_t..._IDXs>
		constexpr auto MakeIndexSequence(std::integer_sequence<uint32_t, _IDXs...>)
			noexcept -> std::array<uint32_t, _N> { return { _IDXs... }; }

		/// Makes a sequence `{0, 1, ..., _N-1}` for small `_N`s.
		template<uint32_t _N> requires ((0U < _N) && (_N <= 1024U))
		auto IndexSequence() noexcept -> std::array<uint32_t, _N> {
			constexpr static std::array<uint32_t, _N> seq{
				MakeIndexSequence<_N>(std::make_integer_sequence<uint32_t, _N>())
			};
			return seq;
		}
	}*/
}

namespace Lumina::CG3D {
	namespace {
		void ProcessNode(Node& node_OUT_, ASSIMP::Node const& node_IN_) {
			auto const& transform{ node_IN_.mTransformation };

			std::memcpy(&node_OUT_.Transform_Local, &transform, sizeof(float) * 16U);
			/// We are using row-major matrices and the ASSIMP ones are column-major.
			Mat4::Transpose(
				node_OUT_.Transform_Local,
				node_OUT_.Transform_Local
			);

			ASSIMP::Vec3 scale{};
			ASSIMP::Quaternion rotate{};
			ASSIMP::Vec3 translate{};
			transform.Decompose(scale, rotate, translate);
			node_OUT_.Transform.Scale = { scale.x, scale.y, scale.z };
			node_OUT_.Transform.Rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w };
			node_OUT_.Transform.Translate = { -translate.x, translate.y, translate.z };

			node_OUT_.Name = node_IN_.mName.data;
			
			node_OUT_.Indices_Mesh.reserve(node_IN_.mNumMeshes);
			for (uint32_t i = 0; i < node_IN_.mNumMeshes; ++i) {
				node_OUT_.Indices_Mesh.push_back(node_IN_.mMeshes[i]);
			}

			node_OUT_.Children.reserve(node_IN_.mNumChildren);

			for (
				auto const* child :
				std::span{ node_IN_.mChildren, node_IN_.mNumChildren }
			) {
				ProcessNode(node_OUT_.Children.emplace_back(), *child);
			}
		}

		namespace {
			Mat4 QuaternionToMatrix(Float4 const& q_) noexcept {
				Mat4 ret{};

				Float4 tmp_0{}, tmp_1{}, tmp_2{};

				//----	Row 0	------	------	------	------	----//
				{
					tmp_0 = { q_.x, -q_.y, -q_.z, q_.w };
					tmp_1 = { q_.y, q_.x, q_.w, q_.z };
					tmp_2 = { q_.z, -q_.w, q_.x, -q_.y };

					//....	......	......	......	......	......	....//

					ret[0][0] = q_.x * tmp_0.x + q_.y * tmp_0.y + q_.z * tmp_0.z + q_.w * tmp_0.w;
					ret[0][1] = q_.x * tmp_1.x + q_.y * tmp_1.y + q_.z * tmp_1.z + q_.w * tmp_1.w;
					ret[0][2] = q_.x * tmp_2.x + q_.y * tmp_2.y + q_.z * tmp_2.z + q_.w * tmp_2.w;
					ret[0][3] = 0.0f;
				}

				//----	Row 1	------	------	------	------	----//
				{
					tmp_0 = { q_.y, q_.x, -q_.w, -q_.z };
					tmp_1 = { -q_.x, q_.y, -q_.z, q_.w };
					tmp_2 = { q_.w, q_.z, q_.y, q_.x };

					//....	......	......	......	......	......	....//

					ret[1][0] = q_.x * tmp_0.x + q_.y * tmp_0.y + q_.z * tmp_0.z + q_.w * tmp_0.w;
					ret[1][1] = q_.x * tmp_1.x + q_.y * tmp_1.y + q_.z * tmp_1.z + q_.w * tmp_1.w;
					ret[1][2] = q_.x * tmp_2.x + q_.y * tmp_2.y + q_.z * tmp_2.z + q_.w * tmp_2.w;
					ret[1][3] = 0.0f;
				}

				//----	Row 2	------	------	------	------	----//
				{
					tmp_0 = { q_.z, q_.w, q_.x, q_.y };
					tmp_1 = { -q_.w, q_.z, q_.y, -q_.x };
					tmp_2 = { -q_.x, -q_.y, q_.z, q_.w };

					//....	......	......	......	......	......	....//

					ret[2][0] = q_.x * tmp_0.x + q_.y * tmp_0.y + q_.z * tmp_0.z + q_.w * tmp_0.w;
					ret[2][1] = q_.x * tmp_1.x + q_.y * tmp_1.y + q_.z * tmp_1.z + q_.w * tmp_1.w;
					ret[2][2] = q_.x * tmp_2.x + q_.y * tmp_2.y + q_.z * tmp_2.z + q_.w * tmp_2.w;
					ret[2][3] = 0.0f;
				}

				return ret;
			}

			auto SRT(
				Float3 const& scale,
				Float4 const& rotate_,
				Float3 const& translate_
			) noexcept -> Mat4 {
				Mat4 ret{ QuaternionToMatrix(rotate_) };
				ret[0][0] *= scale.x;
				ret[0][1] *= scale.x;
				ret[0][2] *= scale.x;
				ret[1][0] *= scale.y;
				ret[1][1] *= scale.y;
				ret[1][2] *= scale.y;
				ret[2][0] *= scale.z;
				ret[2][1] *= scale.z;
				ret[2][2] *= scale.z;
				ret[3][0] = translate_.x;
				ret[3][1] = translate_.y;
				ret[3][2] = translate_.z;
				ret[3][3] = 1.0f;
				return ret;
			}
		}

		void ProcessBones(Mesh& mesh_OUT_, ASSIMP::Mesh const& mesh_IN_) {
			for (ASSIMP::Bone const* bone_IN : std::span{ mesh_IN_.mBones, mesh_IN_.mNumBones }) {
				std::string_view jointName{ bone_IN->mName.data };
				JointWeightData& jointWeightData{ mesh_OUT_.SkinClusterData[jointName.data()] };

				ASSIMP::Mat4x4 bonePoseMat_IN{ bone_IN->mOffsetMatrix };
				bonePoseMat_IN.Inverse();

				ASSIMP::Vec3 scale{};
				ASSIMP::Quaternion rotate{};
				ASSIMP::Vec3 translate{};
				bonePoseMat_IN.Decompose(scale, rotate, translate);

				Mat4 bonePoseMat_OUT{
					SRT(
						{ scale.x, scale.y, scale.z },
						{ rotate.x, -rotate.y, -rotate.z, rotate.w },
						{ -translate.x, translate.y, translate.z }
					)
				};
				jointWeightData.INV_BindPose = bonePoseMat_OUT.Inv();
				for (
					ASSIMP::VertexWeight const& vertexWeight :
					std::span{ bone_IN->mWeights, bone_IN->mNumWeights }
				) {
					jointWeightData.VertexWeights.emplace_back(
						vertexWeight.mWeight,
						vertexWeight.mVertexId
					);
				}
			}
		}

		void ProcessMesh(Mesh& mesh_OUT_, ASSIMP::Mesh const& mesh_IN_) {
			for (uint32_t idx{ 0U }; idx < mesh_IN_.mNumVertices; ++idx) {
				auto const& position{ mesh_IN_.mVertices[idx] };
				auto const& normal{ mesh_IN_.mNormals[idx] };
				auto const& texCoord{ mesh_IN_.mTextureCoords[0][idx] };

				auto& vert{ mesh_OUT_.Vertices.emplace_back() };
				vert.Position = { -position.x, position.y, position.z };
				vert.TexCoord = { texCoord.x, texCoord.y };
				vert.Normal = { -normal.x, normal.y, normal.z };
			}

			for (auto const& face : std::span{ mesh_IN_.mFaces, mesh_IN_.mNumFaces }) {
				(face.mNumIndices == 3) ||
				Utils::Debug::ThrowIfFalse<>{ "Faces must be triangulated!" };

				mesh_OUT_.Indices.emplace_back(face.mIndices[0]);
				mesh_OUT_.Indices.emplace_back(face.mIndices[1]);
				mesh_OUT_.Indices.emplace_back(face.mIndices[2]);
			}

			ProcessBones(mesh_OUT_, mesh_IN_);
			
			mesh_OUT_.Index_Material = mesh_IN_.mMaterialIndex;
		}

		void ProcessMeshes(std::vector<Mesh>& out_, ASSIMP::Scene const& scene_) {
			for (
				ASSIMP::Mesh const* mesh_IN :
				std::span{ scene_.mMeshes, scene_.mNumMeshes }
			) {
				(mesh_IN->HasTextureCoords(0U)) ||
				Utils::Debug::ThrowIfFalse<>{ "Mesh has no texture coordinates!\n" };
				(mesh_IN->HasNormals()) ||
				Utils::Debug::ThrowIfFalse<>{ "Mesh has no normals!\n" };
				
				auto& mesh_OUT{ out_.emplace_back() };
				ProcessMesh(mesh_OUT, *mesh_IN);
			}
		}
	}

	export auto Import(
		std::string_view fileName_,
		std::string_view dirPath_
	) -> Collection {
		std::string filePath{ dirPath_ };
		filePath += '/';
		filePath += fileName_;

		ASSIMP::Importer importer{};
		ASSIMP::Scene const* scene{
			importer.ReadFile(
				filePath.data(),
				ASSIMP::PostProcessStep::FlipWindingOrder |
				ASSIMP::PostProcessStep::FlipUVs
			)
		};
		(scene != nullptr) ||
		Utils::Debug::ThrowIfFalse<>{ std::format("Failed to load scene {}: {}\n", filePath, importer.GetErrorString()) };

		(scene->HasMeshes()) ||
		Utils::Debug::ThrowIfFalse<>{ "No meshes in the scene!\n" };
		
		Collection ret{};

		ProcessMeshes(ret.Meshes, *scene);

		for (
			ASSIMP::Material const* material :
			std::span{ scene->mMaterials, scene->mNumMaterials }
		) {
			auto& material_OUT{ ret.Materials.emplace_back() };
			if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0U) {
				ASSIMP::String filePath_Tex0{};
				material->GetTexture(aiTextureType_DIFFUSE, 0, &filePath_Tex0);
				if (filePath_Tex0.length > 0 && filePath_Tex0.data[0] != '*') {
					material_OUT.FilePath_Diffuse = filePath_Tex0.data;
				}
			}
			// Fallback: glTF PBR uses baseColorTexture which Assimp may report as BASE_COLOR
			if (material_OUT.FilePath_Diffuse.empty() && material->GetTextureCount(aiTextureType_BASE_COLOR) != 0U) {
				ASSIMP::String filePath_Tex0{};
				material->GetTexture(aiTextureType_BASE_COLOR, 0, &filePath_Tex0);
				if (filePath_Tex0.length > 0 && filePath_Tex0.data[0] != '*') {
					material_OUT.FilePath_Diffuse = filePath_Tex0.data;
				}
			}
		}

		ProcessNode(ret.Root, *(scene->mRootNode));

		return ret;
	}
}