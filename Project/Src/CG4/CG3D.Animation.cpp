module Lumina.CG3D.Animation;

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
import Lumina.Utils.Debug;

import Lumina.DX12;
import Lumina.DX12.Aux.View;

/// TODO: study the structure of glTF

namespace Lumina::CG3D {
	namespace {
		// *  Creates joints from ALL nodes regardless of being related to the animation.
		auto CreateJoint(
			std::vector<Joint>& joints_,
			std::optional<uint32_t> const& id_Parent_,
			Node const& node_
		) -> uint32_t {
			uint32_t const id = static_cast<uint32_t>(joints_.size());

			joints_.emplace_back();
			joints_[id].Name = node_.Name;
			joints_[id].Local = node_.Transform_Local;
			joints_[id].SkeletonSpace = Mat4{};
			joints_[id].Transform = node_.Transform;
			joints_[id].ID = id;
			joints_[id].ID_Parent = id_Parent_;
			joints_[id].IDs_Child = {};

			if (!node_.Children.empty()) {
				for (auto const& child : node_.Children) {
					uint32_t const id_Child{ CreateJoint(joints_, id, child) };
					joints_[id].IDs_Child.emplace_back(id_Child);
				}
			}

			return id;
		}
	}

	auto CreateSkeleton(Node const& rootNode_) -> Skeleton {
		Skeleton ret{};
		for (auto const& child : rootNode_.Children) {
			ret.ID_Root = CreateJoint(ret.ARR_Joint, std::nullopt, child);
		}

		for (Joint const& joint : ret.ARR_Joint) {
			ret.IDX_Joint.emplace(joint.Name, joint.ID);
		}

		return ret;
	}
}

namespace Lumina::CG3D {
	namespace {
		void ProcessAnimation(
			MyAnimation& anim_OUT_,
			ASSIMP::Animation const& anim_IN_
		) {
			anim_OUT_.DurationInSeconds = static_cast<float>(
				anim_IN_.mDuration /
				anim_IN_.mTicksPerSecond
			);
			for (
				auto const* nodeAnim_IN :
				std::span{ anim_IN_.mChannels, anim_IN_.mNumChannels }
			) {
				auto& nodeAnim_OUT{ anim_OUT_.Nodes[nodeAnim_IN->mNodeName.data] };
				for (
					auto const& keyframe_IN :
					std::span{ nodeAnim_IN->mPositionKeys, nodeAnim_IN->mNumPositionKeys }
				) {
					auto& keyframe_OUT{ nodeAnim_OUT.Translate.Keyframes.emplace_back() };
					auto const& value_IN{ keyframe_IN.mValue };
					keyframe_OUT.Value = { -value_IN.x, value_IN.y, value_IN.z };
					keyframe_OUT.TimepointInSecond = static_cast<float>(
						keyframe_IN.mTime /
						anim_IN_.mTicksPerSecond
					);
				}
				for (
					auto const& keyframe_IN :
					std::span{ nodeAnim_IN->mRotationKeys, nodeAnim_IN->mNumRotationKeys }
				) {
					auto& keyframe_OUT{ nodeAnim_OUT.Rotate.Keyframes.emplace_back() };
					auto const& value_IN{ keyframe_IN.mValue };
					/// Right-hand system to left-hand system;
					/// the rotation orientation and the axis is thus reversed. 
					keyframe_OUT.Value = { value_IN.x, -value_IN.y, -value_IN.z, value_IN.w };
					keyframe_OUT.TimepointInSecond = static_cast<float>(
						keyframe_IN.mTime /
						anim_IN_.mTicksPerSecond
					);
				}
			}
		}
	}

	auto LoadAnimationFile(
		std::string_view fileName_,
		std::string_view directoryPath_
	) -> std::vector<MyAnimation> {
		std::string filePath{ directoryPath_.data() };
		filePath += '/';
		filePath += fileName_.data();

		ASSIMP::Importer importer{};
		ASSIMP::Scene const* scene{
			importer.ReadFile(
				filePath.data(),
				ASSIMP::PostProcessStep::FlipWindingOrder |
				ASSIMP::PostProcessStep::FlipUVs
			)
		};
		(scene->HasAnimations()) ||
		Utils::Debug::ThrowIfFalse<>{ "No animations in the scene!\n" };

		std::vector<MyAnimation> animations{};

		for (
			auto const* anim_IN :
			std::span{ scene->mAnimations, scene->mNumAnimations }
		) {
			auto& anim_OUT{ animations.emplace_back() };
			ProcessAnimation(anim_OUT, *anim_IN);
		}

		return animations;
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

		auto SRT(TRANSFORM const& transform_) noexcept -> Mat4 {
			Mat4 ret{ QuaternionToMatrix(transform_.Rotate) };
			
			ret[0][0] *= transform_.Scale.x;
			ret[0][1] *= transform_.Scale.x;
			ret[0][2] *= transform_.Scale.x;
			
			ret[1][0] *= transform_.Scale.y;
			ret[1][1] *= transform_.Scale.y;
			ret[1][2] *= transform_.Scale.y;
			
			ret[2][0] *= transform_.Scale.z;
			ret[2][1] *= transform_.Scale.z;
			ret[2][2] *= transform_.Scale.z;
			
			ret[3][0] = transform_.Translate.x;
			ret[3][1] = transform_.Translate.y;
			ret[3][2] = transform_.Translate.z;
			ret[3][3] = 1.0f;
			
			return ret;
		}
	}

	void Update(Skeleton& skeleton_) {
		for (auto& joint : skeleton_.ARR_Joint) {
			joint.Local = SRT(joint.Transform);
			if (joint.ID_Parent) {
				auto const& jointParent{ skeleton_.ARR_Joint[*joint.ID_Parent] };
				joint.SkeletonSpace = joint.Local * jointParent.SkeletonSpace;
			}
			else {
				joint.SkeletonSpace = joint.Local;
			}
		}
	}

	namespace {
		void Calculate(
			Float3& out_,
			Animation::Curve<Float3> const& in_,
			float time_
		) {
			if (in_.Keyframes.size() == 1 || time_ <= in_.Keyframes[0].TimepointInSecond) {
				out_ = in_.Keyframes[0].Value;
			}
			else {
				for (auto it{ in_.Keyframes.cbegin() }; it != in_.Keyframes.cend() - 1; ++it) {
					auto const& cur{ *it };
					auto const& next{ *(it + 1) };
					if (cur.TimepointInSecond <= time_ && time_ <= next.TimepointInSecond) {
						float const t{
							(time_ - cur.TimepointInSecond) /
							(next.TimepointInSecond - cur.TimepointInSecond)
						};

						out_ = {
							std::lerp(cur.Value.x, next.Value.x, t),
							std::lerp(cur.Value.y, next.Value.y, t),
							std::lerp(cur.Value.z, next.Value.z, t)
						};
						return;
					}
				}
			}
		}
		void Calculate(
			Float4& out_,
			Animation::Curve<Float4> const& in_,
			float time_
		) {
			if (in_.Keyframes.size() == 1 || time_ <= in_.Keyframes[0].TimepointInSecond) {
				out_ = in_.Keyframes[0].Value;
			}
			else {
				for (auto it{ in_.Keyframes.cbegin() }; it != in_.Keyframes.cend() - 1; ++it) {
					auto const& cur{ *it };
					auto const& next{ *(it + 1) };
					if (cur.TimepointInSecond <= time_ && time_ <= next.TimepointInSecond) {
						float const t{
							(time_ - cur.TimepointInSecond) /
							(next.TimepointInSecond - cur.TimepointInSecond)
						};
						/*auto rotate = Math::SLERP{
							Math::Quaternion{ cur.Value.X, cur.Value.Y, cur.Value.Z, cur.Value.W },
							Math::Quaternion{ next.Value.X, next.Value.Y, next.Value.Z, next.Value.W }
						}(t);*/

						out_ = {
							std::lerp(cur.Value.x, next.Value.x, t),
							std::lerp(cur.Value.y, next.Value.y, t),
							std::lerp(cur.Value.z, next.Value.z, t),
							std::lerp(cur.Value.w, next.Value.w, t),
						};
						return;
					}
				}
			}
		}
		void Calculate(
			TRANSFORM& out_,
			MyAnimation::Node const& in_,
			float time_
		) {
			Calculate(out_.Rotate, in_.Rotate, time_);
			Calculate(out_.Translate, in_.Translate, time_);
		}
	}

	void ApplyAnimation(Skeleton& skeleton_, MyAnimation const& anim_, float time_) {
		for (auto& joint : skeleton_.ARR_Joint) {
			auto it{ anim_.Nodes.find(joint.Name) };
			if (it != anim_.Nodes.cend()) {
				auto const& animNode_Root{ it->second };
				Calculate(joint.Transform, animNode_Root, time_);
			}
		}
	}
}

namespace Lumina::CG3D {
	namespace {
		constexpr auto IndexRange(uint32_t n_) {
			return std::ranges::iota_view{ 0U, n_ };
		}
	}

	auto CreateSkinCluster(
		SkinCluster& skinCluster_,
		DX12::GraphicsDevice const& d3d12Device_,
		DX12::DescriptorHeap const& globalHeap_,
		Skeleton const& skeleton_,
		Mesh const& mesh_
	) -> void {
		skinCluster_.PaletteResource.Initialize(
			d3d12Device_,
			sizeof(WellForGPU) * skeleton_.ARR_Joint.size()
		);
		skinCluster_.MappedPalette = {
			reinterpret_cast<WellForGPU*>(skinCluster_.PaletteResource()),
			skeleton_.ARR_Joint.size()
		};

		auto globalTable{ globalHeap_.Allocate(1U) };
		skinCluster_.PaletteSRVHandle.first = globalTable.CPUHandle(0U);
		skinCluster_.PaletteSRVHandle.second = globalTable.GPUHandle(0U);
		DX12::SRV<WellForGPU>::Create(
			d3d12Device_,
			skinCluster_.PaletteSRVHandle.first,
			skinCluster_.PaletteResource
		);

		skinCluster_.InfluenceResource.Initialize(
			d3d12Device_,
			sizeof(VertexInfluence) * mesh_.Vertices.size()
		);
		skinCluster_.MappedInfluence = {
			reinterpret_cast<VertexInfluence*>(skinCluster_.InfluenceResource()),
			mesh_.Vertices.size()
		};
		auto influenceBufferView{ DX12::VBV::Create<VertexInfluence>(skinCluster_.InfluenceResource) };
		skinCluster_.InfluenceBufferView = *reinterpret_cast<D3D12_VERTEX_BUFFER_VIEW*>(&influenceBufferView);

		skinCluster_.ARR_INV_BindPose.resize(skeleton_.ARR_Joint.size());
		std::generate(
			skinCluster_.ARR_INV_BindPose.begin(),
			skinCluster_.ARR_INV_BindPose.end(),
			[]() { return Mat4{}; }
		);

		for (auto const& jointWeight : mesh_.SkinClusterData) {
			auto it{ skeleton_.IDX_Joint.find(jointWeight.first) };
			if (it == skeleton_.IDX_Joint.cend()) { continue; }

			skinCluster_.ARR_INV_BindPose[it->second] = jointWeight.second.INV_BindPose;
			for (auto const& vertexWeight : jointWeight.second.VertexWeights) {
				auto& influence{ skinCluster_.MappedInfluence[vertexWeight.VertexID] };

				for (uint32_t idx : IndexRange(VertexInfluence::MAXNUM_Influences)) {
					if (influence.ARR_Weight[idx] == 0.0f) {
						influence.ARR_Weight[idx] = vertexWeight.Weight;
						influence.ARR_JointID[idx] = it->second;
						break;
					}
				}
			}
		}
	}

	void Update(SkinCluster& skinCluster_, Skeleton const& skeleton_) {
		for (uint32_t jointID{ 0 }; jointID < skeleton_.ARR_Joint.size(); ++jointID) {
			skinCluster_.MappedPalette[jointID].SkeletonSpace =
				skinCluster_.ARR_INV_BindPose[jointID] *
				skeleton_.ARR_Joint[jointID].SkeletonSpace;

			auto&& inv_SkeletonSpace{ skinCluster_.MappedPalette[jointID].SkeletonSpace.Inv() };
			Mat4::Transpose(
				skinCluster_.MappedPalette[jointID].TR_INV_SkeletonSpace,
				inv_SkeletonSpace
			);
		}
	}

	void Update(
		SkinCluster& skinCluster_,
		Skeleton& skeleton_,
		MyAnimation const& anim_,
		float time_
	) {
		anim_;
		time_;
		ApplyAnimation(skeleton_, anim_, time_);
		Update(skeleton_);
		Update(skinCluster_, skeleton_);
	}
}