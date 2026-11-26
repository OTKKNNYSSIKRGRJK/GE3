module;

#include<assimp/Importer.hpp>
#include<assimp/scene.h>
#include<assimp/postprocess.h>

export module Lumina.CG3D.ASSIMP;

export namespace Lumina::CG3D {
	namespace ASSIMP {
		using Importer = Assimp::Importer;
	}

	namespace ASSIMP {
		using Scene = ::aiScene;
		using Animation = ::aiAnimation;
		using Mesh = ::aiMesh;
		using Face = ::aiFace;
		using Node = ::aiNode;
		using Bone = ::aiBone;
		using VertexWeight = ::aiVertexWeight;
		using Material = ::aiMaterial;

		using Vec2 = ::aiVector2D;
		using Vec3 = ::aiVector3D;
		using Mat4x4 = ::aiMatrix4x4;
		using Quaternion = ::aiQuaternion;

		using String = ::aiString;
	}

	namespace ASSIMP::PostProcessStep {
		enum {
			FlipWindingOrder = aiProcess_FlipWindingOrder,
			FlipUVs = aiProcess_FlipUVs,
		};
	}
}