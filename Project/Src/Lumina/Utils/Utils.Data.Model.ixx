export module Lumina.Utils.Data.Model;

import <vector>;

import Lumina.Math.Numerics;

import Lumina.Utils.Data;

namespace Lumina::Utils {
	export struct Model {
		std::vector<Float3> Positions;
		std::vector<Float2> TexCoords;
		std::vector<Float3> Normals;

		std::vector<Int3> Vertices;

		Model(WavefrontOBJ const& obj_) {
			Positions.resize(obj_.Positions().size());
			Positions.assign(obj_.Positions().cbegin(), obj_.Positions().cend());
			for (auto& pos : Positions) {
				pos.z = -pos.z;
			}
			TexCoords.resize(obj_.TexCoords().size());
			TexCoords.assign(obj_.TexCoords().cbegin(), obj_.TexCoords().cend());
			for (auto& texCoord : TexCoords) {
				texCoord.y = 1.0f - texCoord.y;
			}
			Normals.resize(obj_.Normals().size());
			Normals.assign(obj_.Normals().cbegin(), obj_.Normals().cend());
			for (auto& norm : Normals) {
				norm.z = -norm.z;
			}

			auto const& faces{ obj_.Faces() };
			auto const& verts{ obj_.Vertices() };

			for (auto const& face : faces) {
				int32_t idx_Verts[3]{
					static_cast<int32_t>(face.Index_Vertex_Last),
					static_cast<int32_t>(face.Index_Vertex_Last) - 1,
					static_cast<int32_t>(face.Index_Vertex_Last) - 2,
				};

				do {
					for (int32_t idx_Vert : idx_Verts) {
						auto const& vert{ verts[idx_Vert] };
						Vertices.emplace_back(
							vert.Index_Position,
							vert.Index_TexCoord,
							vert.Index_Normal
						);
					}

					--idx_Verts[1];
					--idx_Verts[2];
				} while (idx_Verts[2] >= static_cast<int32_t>(face.Index_Vertex_First));
			}
		}
	};
}