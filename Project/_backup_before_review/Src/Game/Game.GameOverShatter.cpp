module Game.GameOverShatter;

// ここで import しているヘッダユニットは、いずれも Game.GameOverShatter.ixx で import 済み
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <memory>;
import <span>;
import <string>;
import <vector>;
import <d3d12.h>;

import Lumina;
import Lumina.CG3D.Struct;
import nlohmann.json;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace Game::GameOverShatter {
	auto DefaultGroups() -> std::array<Group, NumGroups> {
		std::array<Group, NumGroups> groups{};

		{	// 細かい塵。ふわっと上へ
			Group& g{ groups[0] };
			g.Name = "Dust";
			g.AtlasID = 0U;
			g.Rate = 900.0f;
			g.BandBehind = 0.18f;
			g.BandAhead = 0.0f;
			g.LifeMin = 1.8f;	g.LifeMax = 3.2f;
			g.SizeMin = 0.10f;	g.SizeMax = 0.22f;
			g.SpeedMin = 0.2f;	g.SpeedMax = 0.8f;
			g.Rise = 0.6f;		g.Drag = 1.0f;
			g.Turbulence = 1.2f;	g.TurbulenceFreq = 1.3f;
			g.SpinMax = 0.0f;
			g.ColourA = { 0.95f, 0.90f, 0.85f };
			g.ColourB = { 0.80f, 0.75f, 0.70f };
			g.Alpha = 0.45f;
			g.FadeIn = 0.1f;	g.FadeOut = 0.6f;
			g.EndScale = 0.4f;	g.Flicker = 0.0f;
		}
		{	// 灰の欠片。ひらひら回りながら落ちる
			Group& g{ groups[1] };
			g.Name = "Ash";
			g.AtlasID = 6U;
			g.Rate = 160.0f;
			g.BandBehind = 0.12f;
			g.BandAhead = 0.0f;
			g.LifeMin = 2.5f;	g.LifeMax = 4.5f;
			g.SizeMin = 0.30f;	g.SizeMax = 0.60f;
			g.SpeedMin = 0.3f;	g.SpeedMax = 1.0f;
			g.Rise = -0.8f;		g.Drag = 1.2f;
			g.Turbulence = 1.5f;	g.TurbulenceFreq = 1.0f;
			g.SpinMax = 3.0f;
			g.ColourA = { 0.80f, 0.78f, 0.75f };
			g.ColourB = { 0.55f, 0.53f, 0.50f };
			g.Alpha = 0.55f;
			g.FadeIn = 0.05f;	g.FadeOut = 0.5f;
			g.EndScale = 0.7f;	g.Flicker = 0.0f;
		}
		{	// 火の粉。縁のすぐ前後から、速く短く
			Group& g{ groups[2] };
			g.Name = "Embers";
			g.AtlasID = 4U;
			g.Rate = 260.0f;
			g.BandBehind = 0.0f;
			g.BandAhead = 0.06f;
			g.LifeMin = 0.8f;	g.LifeMax = 1.8f;
			g.SizeMin = 0.18f;	g.SizeMax = 0.38f;
			g.SpeedMin = 0.8f;	g.SpeedMax = 2.2f;
			g.Rise = 4.0f;		g.Drag = 1.2f;
			g.Turbulence = 2.5f;	g.TurbulenceFreq = 2.5f;
			g.SpinMax = 0.0f;
			g.ColourA = { 1.00f, 0.55f, 0.20f };
			g.ColourB = { 1.00f, 0.85f, 0.50f };
			g.Alpha = 0.9f;
			g.FadeIn = 0.05f;	g.FadeOut = 0.6f;
			g.EndScale = 0.1f;	g.Flicker = 0.5f;
		}
		{	// もや。大きくて薄い
			Group& g{ groups[3] };
			g.Name = "Mist";
			g.AtlasID = 0U;
			g.Rate = 50.0f;
			g.Continuous = true;
			g.BandBehind = 0.25f;
			g.BandAhead = 0.0f;
			g.LifeMin = 2.5f;	g.LifeMax = 4.0f;
			g.SizeMin = 1.8f;	g.SizeMax = 3.0f;
			g.SpeedMin = 0.1f;	g.SpeedMax = 0.4f;
			g.Rise = 0.4f;		g.Drag = 1.0f;
			g.Turbulence = 0.6f;	g.TurbulenceFreq = 0.7f;
			g.SpinMax = 0.0f;
			g.ColourA = { 0.85f, 0.85f, 0.90f };
			g.ColourB = { 0.70f, 0.70f, 0.75f };
			g.Alpha = 0.07f;
			g.FadeIn = 0.4f;	g.FadeOut = 0.5f;
			g.EndScale = 1.3f;	g.Flicker = 0.0f;
		}
		return groups;
	}

	namespace {
		constexpr float StepSeconds{ 1.0f / 60.0f };		// InGame の Update は固定の 1/60 秒
		constexpr std::array<uint32_t, NumGroups> Capacity{ 1536U, 512U, 768U, 128U };	// Dust / Ash / Embers / Mist の最大数

		auto Saturate(float x_) -> float { return std::clamp(x_, 0.0f, 1.0f); }
		auto SmoothStep(float edge0_, float edge1_, float x_) -> float {
			float const t{ Saturate((x_ - edge0_) / std::max(edge1_ - edge0_, 1.0e-4f)) };
			return t * t * (3.0f - 2.0f * t);
		}

		//////	//////	//////	//////	//////	//////
		//	ディゾルブのノイズ（HLSL と同じ式）		//
		//////	//////	//////	//////	//////	//////
		// MeshSkinning.PS.hlsl の Hash / Lattice / ValueNoise / DissolveNoise と同じ。式を変えるときは両方そろえること
		auto Hash(uint32_t x_) -> uint32_t {
			x_ ^= x_ >> 16;
			x_ *= 0x7feb352dU;
			x_ ^= x_ >> 15;
			x_ *= 0x846ca68bU;
			x_ ^= x_ >> 16;
			return x_;
		}

		auto Lattice(int32_t x_, int32_t y_, int32_t z_, uint32_t seed_) -> float {
			uint32_t const h{
				Hash(
					static_cast<uint32_t>(x_) * 73856093U ^
					static_cast<uint32_t>(y_) * 19349663U ^
					static_cast<uint32_t>(z_) * 83492791U ^
					seed_
				)
			};
			return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
		}

		auto ValueNoise(float px_, float py_, float pz_, uint32_t seed_) -> float {
			float const fx{ std::floor(px_) };
			float const fy{ std::floor(py_) };
			float const fz{ std::floor(pz_) };
			float const tx{ px_ - fx };
			float const ty{ py_ - fy };
			float const tz{ pz_ - fz };
			float const ux{ tx * tx * (3.0f - 2.0f * tx) };
			float const uy{ ty * ty * (3.0f - 2.0f * ty) };
			float const uz{ tz * tz * (3.0f - 2.0f * tz) };
			int32_t const cx{ static_cast<int32_t>(fx) };
			int32_t const cy{ static_cast<int32_t>(fy) };
			int32_t const cz{ static_cast<int32_t>(fz) };

			float const v000{ Lattice(cx + 0, cy + 0, cz + 0, seed_) };
			float const v100{ Lattice(cx + 1, cy + 0, cz + 0, seed_) };
			float const v010{ Lattice(cx + 0, cy + 1, cz + 0, seed_) };
			float const v110{ Lattice(cx + 1, cy + 1, cz + 0, seed_) };
			float const v001{ Lattice(cx + 0, cy + 0, cz + 1, seed_) };
			float const v101{ Lattice(cx + 1, cy + 0, cz + 1, seed_) };
			float const v011{ Lattice(cx + 0, cy + 1, cz + 1, seed_) };
			float const v111{ Lattice(cx + 1, cy + 1, cz + 1, seed_) };

			float const x00{ v000 + (v100 - v000) * ux };
			float const x10{ v010 + (v110 - v010) * ux };
			float const x01{ v001 + (v101 - v001) * ux };
			float const x11{ v011 + (v111 - v011) * ux };
			float const y0{ x00 + (x10 - x00) * uy };
			float const y1{ x01 + (x11 - x01) * uy };
			return y0 + (y1 - y0) * uz;
		}

		auto DissolveNoise(float px_, float py_, float pz_, uint32_t seed_, float contrast_) -> float {
			float const n{
				ValueNoise(px_, py_, pz_, seed_) * 0.5f +
				ValueNoise(px_ * 2.03f, py_ * 2.03f, pz_ * 2.03f, seed_ + 1U) * 0.3f +
				ValueNoise(px_ * 4.11f, py_ * 4.11f, pz_ * 4.11f, seed_ + 2U) * 0.2f
			};
			return Saturate((n - 0.5f) * contrast_ + 0.5f);
		}

		//////	//////	//////	//////	//////	//////
		//	スキニング（CPU）						//
		//////	//////	//////	//////	//////	//////
		struct Skinned {
			Lumina::Float3 Position;
			Lumina::Float3 Normal;
		};

		// GPU（MeshSkinning.VS.hlsl の Skinning）と同じく、頂点ごとにパレットで変形する。法線は回転部分だけで近似
		auto SkinVertex(
			Lumina::CG3D::Mesh::Vertex const& vertex_,
			Lumina::CG3D::VertexInfluence const& influence_,
			std::span<Lumina::CG3D::WellForGPU const> palette_
		) -> Skinned {
			Lumina::Float3 const& p{ vertex_.Position };
			Lumina::Float3 const& n{ vertex_.Normal };
			Skinned out{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
			float weightSum{ 0.0f };

			for (uint32_t k{ 0U }; k < Lumina::CG3D::VertexInfluence::MAXNUM_Influences; ++k) {
				float const w{ influence_.ARR_Weight[k] };
				uint32_t const joint{ influence_.ARR_JointID[k] };
				if (w <= 0.0f || joint >= palette_.size()) { continue; }

				auto const& m{ palette_[joint].SkeletonSpace };
				out.Position.x += w * (p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0]);
				out.Position.y += w * (p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1]);
				out.Position.z += w * (p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2]);
				out.Normal.x += w * (n.x * m[0][0] + n.y * m[1][0] + n.z * m[2][0]);
				out.Normal.y += w * (n.x * m[0][1] + n.y * m[1][1] + n.z * m[2][1]);
				out.Normal.z += w * (n.x * m[0][2] + n.y * m[1][2] + n.z * m[2][2]);
				weightSum += w;
			}
			if (weightSum <= 0.0f) { out = Skinned{ p, n }; }
			return out;
		}

		//////	//////	//////	//////	//////	//////
		//	粒子の 1 フレーム						//
		//////	//////	//////	//////	//////	//////
		// ParticleSystem<T>::Update に渡す更新関数。groupPtr_ は Group const*。false を返すと粒子は消える
		auto StepParticle(ShardParticle& p_, void const* groupPtr_) -> bool {
			Group const& g{ *static_cast<Group const*>(groupPtr_) };

			p_.Age += StepSeconds;
			if (p_.Age >= p_.MaxLife) { return false; }
			float const u{ p_.Age / p_.MaxLife };

			// ゆらぎ（位相は粒子ごとにずらす）と、上向きの加速度、速度の減衰
			constexpr float Tau{ 6.2831853f };
			float const t{ p_.Age * g.TurbulenceFreq + p_.Seed * 40.0f };
			p_.Velocity.x += std::sin(t + p_.Seed * Tau) * g.Turbulence * StepSeconds;
			p_.Velocity.y += std::sin(t * 0.73f + p_.Seed * 17.0f) * 0.6f * g.Turbulence * StepSeconds;
			p_.Velocity.z += std::cos(t * 1.31f + p_.Seed * 3.1f) * g.Turbulence * StepSeconds;
			p_.Velocity.y += g.Rise * StepSeconds;
			float const damping{ std::max(0.0f, 1.0f - g.Drag * StepSeconds) };
			p_.Velocity.x *= damping;
			p_.Velocity.y *= damping;
			p_.Velocity.z *= damping;

			p_.Translate.x += p_.Velocity.x * StepSeconds;
			p_.Translate.y += p_.Velocity.y * StepSeconds;
			p_.Translate.z += p_.Velocity.z * StepSeconds;
			p_.Rotate.z += p_.Spin * StepSeconds;

			float const size{ p_.BaseSize * (1.0f + (g.EndScale - 1.0f) * u) };
			p_.Scale.x = size;
			p_.Scale.y = size;

			float const fadeIn{ g.FadeIn > 1.0e-4f ? SmoothStep(0.0f, g.FadeIn, u) : 1.0f };
			float const fadeOut{ g.FadeOut > 1.0e-4f ? 1.0f - SmoothStep(1.0f - g.FadeOut, 1.0f, u) : 1.0f };
			float const flicker{ 1.0f - g.Flicker * 0.5f * (1.0f + std::sin(p_.Age * 40.0f + p_.Seed * 100.0f)) };
			float const alpha{ p_.BaseAlpha * fadeIn * fadeOut * flicker };

			p_.Life = p_.MaxLife - p_.Age;
			p_.RenderData.RGBA = { p_.Colour.x, p_.Colour.y, p_.Colour.z, alpha };
			return true;
		}

		//////	//////	//////	//////	//////	//////
		//	JSON									//
		//////	//////	//////	//////	//////	//////
		using JSON = nlohmann::json;

		void Read(JSON const& obj_, char const* key_, float& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = itr->get<float>(); }
		}
		void Read(JSON const& obj_, char const* key_, int& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = itr->get<int>(); }
		}
		void Read(JSON const& obj_, char const* key_, uint32_t& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = static_cast<uint32_t>(std::max(itr->get<int>(), 0)); }
		}
		void Read(JSON const& obj_, char const* key_, bool& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_boolean()) { out_ = itr->get<bool>(); }
		}
		void Read(JSON const& obj_, char const* key_, std::string& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_string()) { out_ = itr->get<std::string>(); }
		}
		void Read(JSON const& obj_, char const* key_, Lumina::Float3& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_array() && itr->size() == 3U) {
				out_ = { (*itr)[0].get<float>(), (*itr)[1].get<float>(), (*itr)[2].get<float>() };
			}
		}
		auto ToJSON(Lumina::Float3 const& v_) -> JSON {
			return JSON::array({ v_.x, v_.y, v_.z });
		}

#if defined(_DEBUG)
		void DrawGroupEditor(Group& g_, uint32_t index_) {
			ImGui::PushID(static_cast<int>(index_));
			bool const open{ ImGui::TreeNode("group", "%s%s", g_.Name.c_str(), g_.Enabled ? "" : "  (off)") };
			if (open) {
				ImGui::Checkbox("Enabled", &g_.Enabled);
				ImGui::Checkbox("Bypass colour grade (draw as-is, after the grade)", &g_.BypassGrade);

				int atlas{ static_cast<int>(g_.AtlasID) };
				ImGui::SliderInt("Atlas cell", &atlas, 0, 9);
				g_.AtlasID = static_cast<uint32_t>(atlas);
				ImGui::TextDisabled("0 glow  1 ellipse  2 star  3 soft star  4 cross  5 small cross  6 rough flake  7 disc  8 vertical gradient");

				ImGui::SeparatorText("Where");
				ImGui::SliderFloat("Rate (candidates/s)", &g_.Rate, 0.0f, 4000.0f, "%.0f");
				ImGui::Checkbox("Keep emitting (also after the dissolve ends, until restart)", &g_.Continuous);
				ImGui::BeginDisabled(g_.Continuous);
				ImGui::SliderFloat("Band behind", &g_.BandBehind, 0.0f, 0.6f);
				ImGui::EndDisabled();
				ImGui::SliderFloat("Band ahead", &g_.BandAhead, 0.0f, 0.3f);
				ImGui::TextDisabled("Candidates are random surface points; only those whose noise value is in [threshold - behind, threshold + ahead] become particles.");
				ImGui::TextDisabled("Keep emitting: no lower limit (everything already dissolved). After the dissolve ends, every candidate becomes a particle, so Rate = particles/s.");

				ImGui::SeparatorText("Particle");
				ImGui::SliderFloat("Life min (s)", &g_.LifeMin, 0.1f, 8.0f);
				ImGui::SliderFloat("Life max (s)", &g_.LifeMax, 0.1f, 8.0f);
				ImGui::SliderFloat("Size min", &g_.SizeMin, 0.01f, 4.0f);
				ImGui::SliderFloat("Size max", &g_.SizeMax, 0.01f, 4.0f);
				ImGui::SliderFloat("Speed min", &g_.SpeedMin, 0.0f, 6.0f);
				ImGui::SliderFloat("Speed max", &g_.SpeedMax, 0.0f, 6.0f);
				ImGui::SliderFloat("Rise (accel)", &g_.Rise, -6.0f, 8.0f);
				ImGui::SliderFloat("Drag (1/s)", &g_.Drag, 0.0f, 4.0f);
				ImGui::SliderFloat("Turbulence", &g_.Turbulence, 0.0f, 6.0f);
				ImGui::SliderFloat("Turbulence freq", &g_.TurbulenceFreq, 0.1f, 6.0f);
				ImGui::SliderFloat("Spin max (rad/s)", &g_.SpinMax, 0.0f, 12.0f);

				ImGui::SeparatorText("Look");
				ImGui::ColorEdit3("Colour A", &g_.ColourA.x);
				ImGui::ColorEdit3("Colour B", &g_.ColourB.x);
				ImGui::SliderFloat("Alpha", &g_.Alpha, 0.0f, 1.0f);
				ImGui::SliderFloat("Fade in (life)", &g_.FadeIn, 0.0f, 1.0f);
				ImGui::SliderFloat("Fade out (life)", &g_.FadeOut, 0.0f, 1.0f);
				ImGui::SliderFloat("End scale (x)", &g_.EndScale, 0.0f, 3.0f);
				ImGui::SliderFloat("Flicker", &g_.Flicker, 0.0f, 1.0f);

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
#endif
	}

	//////	//////	//////	//////	//////	//////
	//	Initialize / Reset						//
	//////	//////	//////	//////	//////	//////

	void Effect::Initialize(
		Lumina::DX12::Context const& dx12Context_,
		Lumina::CG3D::Mesh const& mesh_,
		Lumina::CG3D::SkinCluster const& skinCluster_
	) {
		Load();

		Mesh_ = &mesh_;
		Influences_.assign(skinCluster_.MappedInfluence.begin(), skinCluster_.MappedInfluence.end());
		if (Influences_.size() != mesh_.Vertices.size() || mesh_.Indices.size() < 3U) {
			Status_ = "Mesh and skin influences do not match. Particles are disabled.";
			return;
		}

		// 体の大きさ（一番長い辺）と、三角形の面積の累積
		Lumina::Float3 lo{ 1.0e30f, 1.0e30f, 1.0e30f };
		Lumina::Float3 hi{ -1.0e30f, -1.0e30f, -1.0e30f };
		for (auto const& vertex : mesh_.Vertices) {
			lo = { std::min(lo.x, vertex.Position.x), std::min(lo.y, vertex.Position.y), std::min(lo.z, vertex.Position.z) };
			hi = { std::max(hi.x, vertex.Position.x), std::max(hi.y, vertex.Position.y), std::max(hi.z, vertex.Position.z) };
		}
		BodySize_ = std::max({ hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, 1.0e-3f });

		std::size_t const numTriangles{ mesh_.Indices.size() / 3U };
		AreaCdf_.clear();
		AreaCdf_.reserve(numTriangles);
		float total{ 0.0f };
		for (std::size_t tri{ 0U }; tri < numTriangles; ++tri) {
			auto const& a{ mesh_.Vertices[mesh_.Indices[tri * 3U + 0U]].Position };
			auto const& b{ mesh_.Vertices[mesh_.Indices[tri * 3U + 1U]].Position };
			auto const& c{ mesh_.Vertices[mesh_.Indices[tri * 3U + 2U]].Position };
			float const abx{ b.x - a.x }, aby{ b.y - a.y }, abz{ b.z - a.z };
			float const acx{ c.x - a.x }, acy{ c.y - a.y }, acz{ c.z - a.z };
			float const cx{ aby * acz - abz * acy };
			float const cy{ abz * acx - abx * acz };
			float const cz{ abx * acy - aby * acx };
			total += 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
			AreaCdf_.push_back(total);
		}
		if (total <= 0.0f) {
			Status_ = "The mesh has no area. Particles are disabled.";
			return;
		}

		for (uint32_t idx{ 0U }; idx < NumGroups; ++idx) {
			Systems_[idx] = std::make_unique<ParticleSystem<ShardParticle>>();
			Systems_[idx]->Initialize(dx12Context_, Capacity[idx]);
		}
		Ready_ = true;
	}

	void Effect::Reset() {
		for (auto& system : Systems_) {
			if (system) { system->Clear(); }
		}
		Accumulator_.fill(0.0f);
		LastElapsed_ = 0.0f;
	}

	//////	//////	//////	//////	//////	//////
	//	Dissolve								//
	//////	//////	//////	//////	//////	//////

	auto Effect::DissolveProgress(float elapsed_) const -> float {
		return SmoothStep(0.0f, 1.0f, (elapsed_ - Look.DissolveDelay) / std::max(Look.DissolveDuration, 1.0e-4f));
	}

	// 進み 0 → 1 で、閾値は「縁も含めて何も消えていない」値から「何も残らない」値まで動く（ノイズ値は 0..1）
	auto Effect::Threshold(float progress_) const -> float {
		float const start{ -Look.EdgeWidth - 0.01f };
		float const end{ 1.01f };
		return start + (end - start) * progress_;
	}

	auto Effect::PlayerConstants(Lumina::Vec3 const& cameraPos_) const -> SkinnedMeshConstants {
		SkinnedMeshConstants constants{
			{ cameraPos_.x, cameraPos_.y, cameraPos_.z },
			-10.0f,
			NoiseScale(),
			Look.NoiseContrast,
			Look.EdgeWidth,
			Look.EdgeBoost,
			Look.EdgeColor,
			static_cast<uint32_t>(Look.Seed)
		};
		if (Ready_ && Active_) {
			constants.Threshold = Threshold(DissolveProgress(Elapsed_));
		}
		return constants;
	}

	auto Effect::NoDissolveConstants(Lumina::Vec3 const& cameraPos_) -> SkinnedMeshConstants {
		return SkinnedMeshConstants{
			{ cameraPos_.x, cameraPos_.y, cameraPos_.z },
			-10.0f,
			1.0f, 1.0f, 0.0f, 0.0f,
			{ 0.0f, 0.0f, 0.0f },
			0U
		};
	}

	//////	//////	//////	//////	//////	//////
	//	Particles								//
	//////	//////	//////	//////	//////	//////

	auto Effect::Random01() -> float {
		// xorshift32
		RngState_ ^= RngState_ << 13;
		RngState_ ^= RngState_ >> 17;
		RngState_ ^= RngState_ << 5;
		return static_cast<float>(RngState_ >> 8) * (1.0f / 16777216.0f);
	}

	void Effect::TryEmit(
		uint32_t groupIndex_,
		float threshold_,
		Lumina::Mat4 const& meshWorld_,
		Lumina::CG3D::SkinCluster const& skinCluster_
	) {
		Group const& g{ Look.Groups[groupIndex_] };
		Lumina::CG3D::Mesh const& mesh{ *Mesh_ };

		// 面積で重みづけして三角形を選び、その中の一様な点を取る
		float const pick{ Random01() * AreaCdf_.back() };
		auto const found{ std::upper_bound(AreaCdf_.begin(), AreaCdf_.end(), pick) };
		std::size_t const tri{ std::min(static_cast<std::size_t>(found - AreaCdf_.begin()), AreaCdf_.size() - 1U) };
		uint32_t const idx0{ mesh.Indices[tri * 3U + 0U] };
		uint32_t const idx1{ mesh.Indices[tri * 3U + 1U] };
		uint32_t const idx2{ mesh.Indices[tri * 3U + 2U] };

		float const ra{ Random01() };
		float const rb{ Random01() };
		float const root{ std::sqrt(ra) };
		float const w0{ 1.0f - root };
		float const w1{ root * (1.0f - rb) };
		float const w2{ root * rb };

		// ディゾルブと同じノイズ値（バインドポーズの位置）。帯に入っていなければ粒子にしない
		auto const& bind0{ mesh.Vertices[idx0].Position };
		auto const& bind1{ mesh.Vertices[idx1].Position };
		auto const& bind2{ mesh.Vertices[idx2].Position };
		float const scale{ NoiseScale() };
		float const noise{
			DissolveNoise(
				(w0 * bind0.x + w1 * bind1.x + w2 * bind2.x) * scale,
				(w0 * bind0.y + w1 * bind1.y + w2 * bind2.y) * scale,
				(w0 * bind0.z + w1 * bind1.z + w2 * bind2.z) * scale,
				static_cast<uint32_t>(Look.Seed),
				Look.NoiseContrast
			)
		};
		// Continuous のグループは「もう消えた所すべて」（下限なし）。ディゾルブが終われば体の全面
		if (noise > threshold_ + g.BandAhead) { return; }
		if (!g.Continuous && noise < threshold_ - g.BandBehind) { return; }

		// 現在のポーズの位置と法線（頂点ごとに変形してから重みで混ぜる。GPU と同じ順）
		std::span<Lumina::CG3D::WellForGPU const> const palette{ skinCluster_.MappedPalette };
		Skinned const s0{ SkinVertex(mesh.Vertices[idx0], Influences_[idx0], palette) };
		Skinned const s1{ SkinVertex(mesh.Vertices[idx1], Influences_[idx1], palette) };
		Skinned const s2{ SkinVertex(mesh.Vertices[idx2], Influences_[idx2], palette) };
		float const lx{ w0 * s0.Position.x + w1 * s1.Position.x + w2 * s2.Position.x };
		float const ly{ w0 * s0.Position.y + w1 * s1.Position.y + w2 * s2.Position.y };
		float const lz{ w0 * s0.Position.z + w1 * s1.Position.z + w2 * s2.Position.z };
		float const nlx{ w0 * s0.Normal.x + w1 * s1.Normal.x + w2 * s2.Normal.x };
		float const nly{ w0 * s0.Normal.y + w1 * s1.Normal.y + w2 * s2.Normal.y };
		float const nlz{ w0 * s0.Normal.z + w1 * s1.Normal.z + w2 * s2.Normal.z };

		// ワールドへ（行ベクトル × 行列）
		auto const& W{ meshWorld_ };
		float const wx{ lx * W[0][0] + ly * W[1][0] + lz * W[2][0] + W[3][0] };
		float const wy{ lx * W[0][1] + ly * W[1][1] + lz * W[2][1] + W[3][1] };
		float const wz{ lx * W[0][2] + ly * W[1][2] + lz * W[2][2] + W[3][2] };
		float nwx{ nlx * W[0][0] + nly * W[1][0] + nlz * W[2][0] };
		float nwy{ nlx * W[0][1] + nly * W[1][1] + nlz * W[2][1] };
		float nwz{ nlx * W[0][2] + nly * W[1][2] + nlz * W[2][2] };
		float const nLen{ std::sqrt(nwx * nwx + nwy * nwy + nwz * nwz) };
		if (nLen > 1.0e-4f) {
			nwx /= nLen;
			nwy /= nLen;
			nwz /= nLen;
		}
		else {
			nwx = 0.0f;
			nwy = 1.0f;
			nwz = 0.0f;
		}

		auto const range{ [this](float lo_, float hi_) -> float { return lo_ + (std::max(hi_, lo_) - lo_) * Random01(); } };

		ShardParticle p{};
		float const speed{ range(g.SpeedMin, g.SpeedMax) };
		p.Translate = { wx + nwx * 0.02f, wy + nwy * 0.02f, wz + nwz * 0.02f };
		// 法線方向に、横へのばらつきを少し足す
		p.Velocity = {
			nwx * speed + (Random01() * 2.0f - 1.0f) * speed * 0.35f,
			nwy * speed + (Random01() * 2.0f - 1.0f) * speed * 0.35f,
			nwz * speed + (Random01() * 2.0f - 1.0f) * speed * 0.35f
		};
		p.MaxLife = std::max(range(g.LifeMin, g.LifeMax), 0.05f);
		p.Life = p.MaxLife;
		p.BaseSize = range(g.SizeMin, g.SizeMax);
		p.Scale = { p.BaseSize, p.BaseSize, 1.0f };
		p.BaseAlpha = g.Alpha;
		p.Seed = Random01();
		p.Spin = (Random01() * 2.0f - 1.0f) * g.SpinMax;
		p.Rotate.z = Random01() * 6.2831853f;

		float const mix{ Random01() };
		p.Colour = {
			g.ColourA.x + (g.ColourB.x - g.ColourA.x) * mix,
			g.ColourA.y + (g.ColourB.y - g.ColourA.y) * mix,
			g.ColourA.z + (g.ColourB.z - g.ColourA.z) * mix
		};
		p.RenderData.RGBA = { p.Colour.x, p.Colour.y, p.Colour.z, 0.0f };
		p.RenderData.DiffuseID = 0U;
		p.RenderData.DiffuseAtlasID = g.AtlasID;

		Systems_[groupIndex_]->Emit(std::move(p));
	}

	void Effect::Update(
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::Mat4 const& viewToWorld_,
		float elapsed_,
		bool active_,
		bool running_,
		Lumina::Mat4 const& meshWorld_,
		Lumina::CG3D::SkinCluster const& skinCluster_
	) {
		Elapsed_ = elapsed_;
		Active_ = active_;
		if (!Ready_) { return; }

		// 時刻が巻き戻った（Replay / リスタート / スクラブで戻した）ら、前の粒子は消す
		if (elapsed_ < LastElapsed_ - 1.0e-4f) { Reset(); }
		LastElapsed_ = elapsed_;

		if (running_) {
			// ディゾルブ中は全グループ、終わったあとは Continuous のグループだけ出す（閾値は 1 を超えたまま）
			float const progress{ DissolveProgress(elapsed_) };
			if (progress > 0.0f) {
				float const threshold{ Threshold(progress) };
				bool const finished{ progress >= 1.0f };
				for (uint32_t idx{ 0U }; idx < NumGroups; ++idx) {
					Group const& g{ Look.Groups[idx] };
					if (!g.Enabled || (finished && !g.Continuous)) { continue; }

					Accumulator_[idx] += g.Rate * StepSeconds;
					for (int tries{ 0 }; Accumulator_[idx] >= 1.0f && tries < 512; ++tries) {
						Accumulator_[idx] -= 1.0f;
						TryEmit(idx, threshold, meshWorld_, skinCluster_);
					}
				}
			}
		}

		for (uint32_t idx{ 0U }; idx < NumGroups; ++idx) {
			Systems_[idx]->Update(cmdList_, viewToWorld_, &StepParticle, &Look.Groups[idx]);
		}
	}

	void Effect::Render(
		Layer layer_,
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::DX12::RootSignature const& rs_,
		Lumina::DX12::GraphicsPSO const& graphicsPSO_,
		D3D12_CPU_DESCRIPTOR_HANDLE localCBV_SceneVars_,
		D3D12_CPU_DESCRIPTOR_HANDLE localCBV_VP_,
		Lumina::DX12::DescriptorTable const& globalTable_Textures_,
		Lumina::DX12::DescriptorTable const& globalTable_Textures2_
	) {
		if (!Ready_) { return; }
		for (uint32_t idx{ 0U }; idx < NumGroups; ++idx) {
			Layer const layer{ Look.Groups[idx].BypassGrade ? Layer::Direct : Layer::Graded };
			if (layer != layer_) { continue; }

			Systems_[idx]->Render(
				cmdList_,
				rs_,
				graphicsPSO_,
				localCBV_SceneVars_,
				localCBV_VP_,
				globalTable_Textures_,
				globalTable_Textures2_
			);
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Save / Load								//
	//////	//////	//////	//////	//////	//////

	auto Effect::Load() -> bool {
		std::ifstream ifs{ ConfigPath };
		if (!ifs.is_open()) { Status_ = "No config file (using defaults)."; return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { Status_ = "Config is not valid JSON."; return false; }

		Style& style{ Look };
		if (auto const dissolve{ root.find("Dissolve") }; dissolve != root.end() && dissolve->is_object()) {
			Read(*dissolve, "Delay", style.DissolveDelay);
			Read(*dissolve, "Duration", style.DissolveDuration);
			Read(*dissolve, "NoiseFrequency", style.NoiseFrequency);
			Read(*dissolve, "NoiseContrast", style.NoiseContrast);
			Read(*dissolve, "EdgeWidth", style.EdgeWidth);
			Read(*dissolve, "EdgeColor", style.EdgeColor);
			Read(*dissolve, "EdgeBoost", style.EdgeBoost);
			Read(*dissolve, "Seed", style.Seed);
		}
		if (auto const groups{ root.find("Groups") }; groups != root.end() && groups->is_array()) {
			for (std::size_t idx{ 0U }; idx < groups->size() && idx < NumGroups; ++idx) {
				JSON const& obj{ (*groups)[idx] };
				if (!obj.is_object()) { continue; }
				Group& g{ style.Groups[idx] };
				Read(obj, "Name", g.Name);
				Read(obj, "Enabled", g.Enabled);
				Read(obj, "BypassGrade", g.BypassGrade);
				Read(obj, "Continuous", g.Continuous);
				Read(obj, "AtlasID", g.AtlasID);
				Read(obj, "Rate", g.Rate);
				Read(obj, "BandBehind", g.BandBehind);
				Read(obj, "BandAhead", g.BandAhead);
				Read(obj, "LifeMin", g.LifeMin);
				Read(obj, "LifeMax", g.LifeMax);
				Read(obj, "SizeMin", g.SizeMin);
				Read(obj, "SizeMax", g.SizeMax);
				Read(obj, "SpeedMin", g.SpeedMin);
				Read(obj, "SpeedMax", g.SpeedMax);
				Read(obj, "Rise", g.Rise);
				Read(obj, "Drag", g.Drag);
				Read(obj, "Turbulence", g.Turbulence);
				Read(obj, "TurbulenceFreq", g.TurbulenceFreq);
				Read(obj, "SpinMax", g.SpinMax);
				Read(obj, "ColourA", g.ColourA);
				Read(obj, "ColourB", g.ColourB);
				Read(obj, "Alpha", g.Alpha);
				Read(obj, "FadeIn", g.FadeIn);
				Read(obj, "FadeOut", g.FadeOut);
				Read(obj, "EndScale", g.EndScale);
				Read(obj, "Flicker", g.Flicker);
			}
		}
		Status_ = "Loaded.";
		return true;
	}

	auto Effect::Save() -> bool {
		Style const& style{ Look };
		JSON root = JSON::object();

		JSON dissolve = JSON::object();
		dissolve["Delay"] = style.DissolveDelay;
		dissolve["Duration"] = style.DissolveDuration;
		dissolve["NoiseFrequency"] = style.NoiseFrequency;
		dissolve["NoiseContrast"] = style.NoiseContrast;
		dissolve["EdgeWidth"] = style.EdgeWidth;
		dissolve["EdgeColor"] = ToJSON(style.EdgeColor);
		dissolve["EdgeBoost"] = style.EdgeBoost;
		dissolve["Seed"] = style.Seed;
		root["Dissolve"] = dissolve;

		JSON groups = JSON::array();
		for (Group const& g : style.Groups) {
			JSON obj = JSON::object();
			obj["Name"] = g.Name;
			obj["Enabled"] = g.Enabled;
			obj["BypassGrade"] = g.BypassGrade;
			obj["Continuous"] = g.Continuous;
			obj["AtlasID"] = g.AtlasID;
			obj["Rate"] = g.Rate;
			obj["BandBehind"] = g.BandBehind;
			obj["BandAhead"] = g.BandAhead;
			obj["LifeMin"] = g.LifeMin;
			obj["LifeMax"] = g.LifeMax;
			obj["SizeMin"] = g.SizeMin;
			obj["SizeMax"] = g.SizeMax;
			obj["SpeedMin"] = g.SpeedMin;
			obj["SpeedMax"] = g.SpeedMax;
			obj["Rise"] = g.Rise;
			obj["Drag"] = g.Drag;
			obj["Turbulence"] = g.Turbulence;
			obj["TurbulenceFreq"] = g.TurbulenceFreq;
			obj["SpinMax"] = g.SpinMax;
			obj["ColourA"] = ToJSON(g.ColourA);
			obj["ColourB"] = ToJSON(g.ColourB);
			obj["Alpha"] = g.Alpha;
			obj["FadeIn"] = g.FadeIn;
			obj["FadeOut"] = g.FadeOut;
			obj["EndScale"] = g.EndScale;
			obj["Flicker"] = g.Flicker;
			groups.push_back(std::move(obj));
		}
		root["Groups"] = groups;

		std::ofstream ofs{ ConfigPath, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { Status_ = "Save failed (cannot open file)."; return false; }
		ofs << root.dump(2) << '\n';
		if (!ofs.good()) { Status_ = "Save failed (write error)."; return false; }
		Status_ = "Saved.";
		return true;
	}

	//////	//////	//////	//////	//////	//////
	//	ImGui									//
	//////	//////	//////	//////	//////	//////

	void Effect::DrawEditor() {
#if defined(_DEBUG)
		if (!ImGui::Begin("GameOver Shatter")) {
			ImGui::End();
			return;
		}
		Style& style{ Look };

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { Look = Style{}; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);
		if (!Ready_) { ImGui::TextColored(ImVec4{ 1.0f, 0.5f, 0.3f, 1.0f }, "Particles are not ready (see the status above)."); }
		ImGui::TextDisabled("Preview: use the 'GameOver Director' window. Scrub shows the dissolve; 'Replay from start' also emits particles.");

		ImGui::SeparatorText("Dissolve");
		ImGui::SliderFloat("Delay (s)", &style.DissolveDelay, 0.0f, 8.0f);
		ImGui::SliderFloat("Duration (s)", &style.DissolveDuration, 0.1f, 8.0f);
		ImGui::SliderFloat("Noise frequency", &style.NoiseFrequency, 1.0f, 24.0f);
		ImGui::SliderFloat("Noise contrast", &style.NoiseContrast, 0.5f, 5.0f);
		ImGui::SliderFloat("Edge width", &style.EdgeWidth, 0.0f, 0.4f);
		ImGui::ColorEdit3("Edge colour", &style.EdgeColor.x);
		ImGui::SliderFloat("Edge boost", &style.EdgeBoost, 0.0f, 4.0f);
		ImGui::InputInt("Noise seed", &style.Seed);
		ImGui::TextDisabled("Progress now: %.2f  (noise frequency = grid cells across the body; the seed changes the pattern)", DissolveProgress(Elapsed_));

		ImGui::SeparatorText("Particle groups");
		for (uint32_t idx{ 0U }; idx < NumGroups; ++idx) {
			DrawGroupEditor(style.Groups[idx], idx);
		}

		ImGui::End();
#endif
	}
}
