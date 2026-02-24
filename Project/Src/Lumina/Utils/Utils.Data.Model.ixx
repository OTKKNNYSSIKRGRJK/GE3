export module Lumina.Utils.Data.Model;

import <vector>;

import Lumina.Math.Numerics;

import Lumina.Utils.Data;

namespace Lumina::Utils {
	export struct Model {
		struct Vertex {
			Float4 Position;
			Float4 Color;
			Float2 TexCoord;
			Float3 Normal;
		};

		std::vector<Vertex> Vertices;

		// Since the vertices are specified by a right-hand coordinate system in the OBJ format,
		// some tweaks are needed so that the model data could be represented properly in the D3D12 context.
		// Triangulation algorithm would need revision?
		// (In most cases it should be sufficient
		// since modelers without questioned sanity should only make faces convex polygons) 
		Model(const WavefrontOBJ& obj_) {
			const auto& faces{ obj_.Faces() };
			const auto& verts{ obj_.Vertices() };

			for (const auto& face : faces) {
				int32_t idx_Verts[3]{
					static_cast<int32_t>(face.Index_Vertex_Last),
					static_cast<int32_t>(face.Index_Vertex_Last) - 1,
					static_cast<int32_t>(face.Index_Vertex_Last) - 2,
				};

				do {
					for (int32_t idx_Vert : idx_Verts) {
						const auto& vert{ verts[idx_Vert] };

						Float4 pos{ obj_.Position(vert.Index_Position) };
						pos.x = -pos.x;
						Float3 norm{ obj_.Normal(vert.Index_Normal) };
						norm.x = -norm.x;
						Float2 texCoord{ obj_.TexCoord(vert.Index_TexCoord) };
						texCoord.y = 1.0f - texCoord.y;

						Vertices.emplace_back(
							pos,
							Float4{ (norm.x + 1.0f) * 0.5f, (norm.y + 1.0f) * 0.5f, (norm.z + 1.0f) * 0.5f, 1.0f },
							texCoord,
							norm
						);
					}

					--idx_Verts[1];
					--idx_Verts[2];
				} while (idx_Verts[2] >= static_cast<int32_t>(face.Index_Vertex_First));
			}
		}
	};

	export struct Model2 {
		std::vector<Float4> Positions;
		std::vector<Float2> TexCoords;
		std::vector<Float3> Normals;

		std::vector<Int3> Vertices;

		Model2(WavefrontOBJ const& obj_) {
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