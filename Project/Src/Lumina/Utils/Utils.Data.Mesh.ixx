export module Lumina.Utils.Data.Mesh;

import <vector>;

import <string>;

import Lumina.Math.Numerics;
import Lumina.Math.Vector;

import Lumina.Utils.Data;

namespace Lumina::Utils {

	/// @class Mesh
	/// @brief mesh data (in SoA form, maybe)

	export class Mesh {
	public:
		struct Vertex {
			uint32_t Index_Position;
			uint32_t Index_TexCoord;
			uint32_t Index_Normal;
			uint32_t Index_Tangent;
		};

	public:
		static Vec3 CalculateTangent(
			Float3 const& pos0_, Float2 const& uv0_,
			Float3 const& pos1_, Float2 const& uv1_,
			Float3 const& pos2_, Float2 const& uv2_
		) {
			Vec3 const dPos01{ Vec3{ pos1_ } - Vec3{ pos0_ } };
			Vec3 const dPos02{ Vec3{ pos2_ } - Vec3{ pos0_ } };
			Vec2 const dUV01{ Vec2{ uv1_ } - Vec2{ uv0_ } };
			Vec2 const dUV02{ Vec2{ uv2_ } - Vec2{ uv0_ } };

			float const inv_Det{ 1.0f / Vec2::Cross(dUV01, dUV02) };

			return inv_Det * (dUV02.y * dPos01 - dUV01.y * dPos02);
		}

	public:
		static auto Load(WavefrontOBJ const& obj_) -> std::vector<Mesh> {
			std::vector<Mesh> meshes{};
			meshes.resize(obj_.Num_Objects());

			auto const& names{ obj_.ObjectNames() };
			auto const& offsets{ obj_.ObjectOffsets() };

			for (uint32_t idx_Mesh{ 0U }; idx_Mesh < obj_.Num_Objects(); ++idx_Mesh) {
				Mesh& mesh{ meshes[idx_Mesh] };

				mesh.Name = names[idx_Mesh];

				// Positions
				{
					uint32_t const idx_Position_Begin{ offsets[idx_Mesh].Index_Position };
					uint32_t const idx_Position_End{ offsets[idx_Mesh + 1U].Index_Position };
					uint32_t const num_Positions{ idx_Position_End - idx_Position_Begin };
					mesh.Positions.resize(num_Positions);
					mesh.Positions.assign(
						obj_.Positions().cbegin() + idx_Position_Begin,
						obj_.Positions().cbegin() + idx_Position_End
					);
					for (auto& pos : mesh.Positions) {
						pos.z = -pos.z;
					}
				}

				// TexCoords
				{
					uint32_t const idx_TexCoord_Begin{ offsets[idx_Mesh].Index_TexCoord };
					uint32_t const idx_TexCoord_End{ offsets[idx_Mesh + 1U].Index_TexCoord };
					uint32_t const num_TexCoords{ idx_TexCoord_End - idx_TexCoord_Begin };
					mesh.TexCoords.resize(num_TexCoords);
					mesh.TexCoords.assign(
						obj_.TexCoords().cbegin() + idx_TexCoord_Begin,
						obj_.TexCoords().cbegin() + idx_TexCoord_End
					);
					for (auto& texCoord : mesh.TexCoords) {
						texCoord.y = 1.0f - texCoord.y;
					}
				}

				// Normals
				{
					uint32_t const idx_Normal_Begin{ offsets[idx_Mesh].Index_Normal };
					uint32_t const idx_Normal_End{ offsets[idx_Mesh + 1U].Index_Normal };
					uint32_t const num_Normals{ idx_Normal_End - idx_Normal_Begin };
					mesh.Normals.resize(num_Normals);
					mesh.Normals.assign(
						obj_.Normals().cbegin() + idx_Normal_Begin,
						obj_.Normals().cbegin() + idx_Normal_End
					);
					for (auto& norm : mesh.Normals) {
						norm.z = -norm.z;
					}
				}

				// Vertices & tangents
				{
					auto const& faces{ obj_.Faces().data() };
					auto const& verts{ obj_.Vertices().data() };

					auto const& positions{ obj_.Positions().data() };
					auto const& texCoords{ obj_.TexCoords().data() };

					uint32_t const idx_Face_Begin{ offsets[idx_Mesh].Index_Face };
					uint32_t const idx_Face_End{ offsets[idx_Mesh + 1U].Index_Face };

					uint32_t const idxOffset_Position{ offsets[idx_Mesh].Index_Position };
					uint32_t const idxOffset_TexCoord{ offsets[idx_Mesh].Index_TexCoord };
					uint32_t const idxOffset_Normal{ offsets[idx_Mesh].Index_Normal };

					for (
						uint32_t idx_Face{ idx_Face_Begin };
						idx_Face < idx_Face_End;
						++idx_Face
					) {
						auto const& face{ faces[idx_Face] };

						int32_t idx_Verts[3]{
							static_cast<int32_t>(face.Index_Vertex_Last),
							static_cast<int32_t>(face.Index_Vertex_Last) - 1,
							static_cast<int32_t>(face.Index_Vertex_Last) - 2,
						};

						uint32_t const idx_Tangent{ idx_Face - idx_Face_Begin };
						Vec3&& tangent{
							CalculateTangent(
								positions[verts[idx_Verts[0]].Index_Position],
								texCoords[verts[idx_Verts[0]].Index_TexCoord],
								positions[verts[idx_Verts[1]].Index_Position],
								texCoords[verts[idx_Verts[1]].Index_TexCoord],
								positions[verts[idx_Verts[2]].Index_Position],
								texCoords[verts[idx_Verts[2]].Index_TexCoord]
							)
						};
						mesh.Tangents.emplace_back(tangent.x, tangent.y, tangent.z);

						do {
							for (int32_t idx_Vert : idx_Verts) {
								auto const& vert{ verts[idx_Vert] };
								mesh.Vertices.emplace_back(
									vert.Index_Position - idxOffset_Position,
									vert.Index_TexCoord - idxOffset_TexCoord,
									vert.Index_Normal - idxOffset_Normal,
									idx_Tangent
								);
							}

							--idx_Verts[1];
							--idx_Verts[2];
						} while (idx_Verts[2] >= static_cast<int32_t>(face.Index_Vertex_First));
					}
				}
			}

			return meshes;
		}

	public:
		std::string Name;

		std::vector<Float3> Positions;
		std::vector<Float2> TexCoords;
		std::vector<Float3> Normals;
		std::vector<Float3> Tangents;

		std::vector<Vertex> Vertices;
	};
}