module Game.CombatVfx;

// ここで import しているヘッダユニットは、いずれも Game.CombatVfx.ixx で import 済み
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <span>;
import <string>;
import <vector>;

import nlohmann.json;

import Lumina.Math.Numerics;
import Lumina.Math;
import Lumina.Container.List;

import ParticleSystem;
import Lumina.FxMesh;
import Game.CombatEvents;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace {
	using Lumina::Float3;
	using Lumina::Float4;
	using Lumina::FxMesh::Shape;

	constexpr float Pi{ 3.14159265358979f };
	constexpr float TwoPi{ 6.28318530718f };
	constexpr std::size_t MaxLights{ 192U };

	//==== 小さな数学（Float3 のまま計算する。Vec3 は align(16) なのでメンバに持たない方針）====//

	auto Add(Float3 const& a_, Float3 const& b_) noexcept -> Float3 { return { a_.x + b_.x, a_.y + b_.y, a_.z + b_.z }; }
	auto Sub(Float3 const& a_, Float3 const& b_) noexcept -> Float3 { return { a_.x - b_.x, a_.y - b_.y, a_.z - b_.z }; }
	auto Mul(Float3 const& a_, float s_) noexcept -> Float3 { return { a_.x * s_, a_.y * s_, a_.z * s_ }; }
	auto Dot(Float3 const& a_, Float3 const& b_) noexcept -> float { return a_.x * b_.x + a_.y * b_.y + a_.z * b_.z; }
	auto Length(Float3 const& a_) noexcept -> float { return std::sqrt(Dot(a_, a_)); }
	auto Normalize(Float3 const& a_, Float3 const& fallback_) noexcept -> Float3 {
		float const len{ Length(a_) };
		return (len > 1.0e-6f) ? Mul(a_, 1.0f / len) : fallback_;
	}
	auto Lerp(float a_, float b_, float t_) noexcept -> float { return a_ + (b_ - a_) * t_; }
	auto Lerp(Float3 const& a_, Float3 const& b_, float t_) noexcept -> Float3 {
		return { Lerp(a_.x, b_.x, t_), Lerp(a_.y, b_.y, t_), Lerp(a_.z, b_.z, t_) };
	}
	auto Smoothstep(float e0_, float e1_, float x_) noexcept -> float {
		if (!(e1_ > e0_)) { return (x_ >= e1_) ? 1.0f : 0.0f; }
		float const t{ std::clamp((x_ - e0_) / (e1_ - e0_), 0.0f, 1.0f) };
		return t * t * (3.0f - 2.0f * t);
	}
	auto Saturate(float x_) noexcept -> float { return std::clamp(x_, 0.0f, 1.0f); }

	// 行ベクトル（v * M）。平行移動は 3 行目
	auto TransformPoint(Lumina::Mat4 const& m_, Float3 const& p_) noexcept -> Float3 {
		return {
			p_.x * m_[0][0] + p_.y * m_[1][0] + p_.z * m_[2][0] + m_[3][0],
			p_.x * m_[0][1] + p_.y * m_[1][1] + p_.z * m_[2][1] + m_[3][1],
			p_.x * m_[0][2] + p_.y * m_[1][2] + p_.z * m_[2][2] + m_[3][2],
		};
	}

	// Catmull-Rom（p1 → p2 の間、t = 0..1）
	auto CatmullRom(Float3 const& p0_, Float3 const& p1_, Float3 const& p2_, Float3 const& p3_, float t_) noexcept -> Float3 {
		float const t2{ t_ * t_ };
		float const t3{ t2 * t_ };
		auto const f{ [&](float a_, float b_, float c_, float d_) {
			return 0.5f * ((2.0f * b_) + (-a_ + c_) * t_ + (2.0f * a_ - 5.0f * b_ + 4.0f * c_ - d_) * t2 + (-a_ + 3.0f * b_ - 3.0f * c_ + d_) * t3);
		} };
		return { f(p0_.x, p1_.x, p2_.x, p3_.x), f(p0_.y, p1_.y, p2_.y, p3_.y), f(p0_.z, p1_.z, p2_.z, p3_.z) };
	}

	auto SRGBToLinear(float c_) noexcept -> float {
		return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
	}
	auto SRGBToLinear(Float3 const& c_) noexcept -> Float3 {
		return { SRGBToLinear(c_.x), SRGBToLinear(c_.y), SRGBToLinear(c_.z) };
	}

	// 色相は度
	auto HSVToRGB(float h_, float s_, float v_) noexcept -> Float3 {
		float h{ std::fmod(h_, 360.0f) };
		if (h < 0.0f) { h += 360.0f; }
		float const c{ v_ * s_ };
		float const x{ c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f)) };
		float const m{ v_ - c };
		Float3 rgb{};
		if (h < 60.0f) { rgb = { c, x, 0.0f }; }
		else if (h < 120.0f) { rgb = { x, c, 0.0f }; }
		else if (h < 180.0f) { rgb = { 0.0f, c, x }; }
		else if (h < 240.0f) { rgb = { 0.0f, x, c }; }
		else if (h < 300.0f) { rgb = { x, 0.0f, c }; }
		else { rgb = { c, 0.0f, x }; }
		return { rgb.x + m, rgb.y + m, rgb.z + m };
	}

	auto MakeVertex(Float3 const& p_, float u_, float v_, Shape shape_, Float3 const& rgb_, float intensity_, Float4 const& param_, float depthBias_, float soft_) noexcept
		-> Lumina::FxMesh::Vertex {
		return Lumina::FxMesh::Vertex{
			.Position{ p_ },
			.ShapeID{ static_cast<std::uint32_t>(shape_) },
			.UV{ u_, v_ },
			.DepthBias{ depthBias_ },
			.SoftRange{ soft_ },
			.Param{ param_ },
			.Color{ rgb_.x, rgb_.y, rgb_.z, intensity_ },
		};
	}

	//==== 粒子の更新（ParticleSystem の更新関数）====//

	bool UpdateFxParticle(Game::CombatVfx::FxParticle& p_, void const*) {
		if (p_.Pull > 0.0f || p_.Swirl != 0.0f) {
			float const dx{ p_.Attractor.x - p_.Translate.x };
			float const dy{ p_.Attractor.y - p_.Translate.y };
			float const dz{ p_.Attractor.z - p_.Translate.z };
			p_.Velocity.x += dx * p_.Pull;
			p_.Velocity.y += dy * p_.Pull;
			p_.Velocity.z += dz * p_.Pull;
			float const lenXZ{ std::sqrt(dx * dx + dz * dz) };
			if (lenXZ > 1.0e-4f) {
				p_.Velocity.x += (-dz / lenXZ) * p_.Swirl;
				p_.Velocity.z += (dx / lenXZ) * p_.Swirl;
			}
		}
		p_.Velocity.y -= p_.Gravity;
		p_.Velocity.x *= p_.Drag;
		p_.Velocity.y *= p_.Drag;
		p_.Velocity.z *= p_.Drag;

		p_.Translate.x += p_.Velocity.x;
		p_.Translate.y += p_.Velocity.y;
		p_.Translate.z += p_.Velocity.z;

		p_.Rotate.z += p_.SpinByVelocityZ ? (p_.Velocity.z * p_.Spin) : p_.Spin;
		p_.Scale.x *= p_.ScaleMul;
		p_.Scale.y *= p_.ScaleMul;

		p_.BaseAlpha *= p_.AlphaMul;
		float const age{ p_.MaxLife - p_.Life };
		float const fadeIn{ (p_.FadeIn > 0.0f) ? std::min(age / p_.FadeIn, 1.0f) : 1.0f };
		float const fadeOut{ std::min(p_.Life / 8.0f, 1.0f) };
		p_.RenderData.RGBA.w = p_.BaseAlpha * fadeIn * std::max(fadeOut, 0.0f);

		p_.Life -= 1.0f;
		return p_.Life > 0.0f;
	}

	auto MakeParticle() noexcept -> Game::CombatVfx::FxParticle {
		Game::CombatVfx::FxParticle p{};
		p.Velocity = { 0.0f, 0.0f, 0.0f };
		p.RenderData.DiffuseID = 0U;
		p.RenderData.DiffuseAtlasID = 0U;
		return p;
	}

	//==== 保存・読み込み（Writer / Reader に同じ Visit を通す。キーの一覧は 1 か所）====//

	using JSON = nlohmann::json;

	class Writer {
	public:
		explicit Writer(JSON& j_) : J_{ j_ } {}
		void operator()(char const* key_, float& v_) { J_[key_] = v_; }
		void operator()(char const* key_, int& v_) { J_[key_] = v_; }
		void operator()(char const* key_, bool& v_) { J_[key_] = v_; }
		void operator()(char const* key_, Float3& v_) { J_[key_] = JSON::array({ v_.x, v_.y, v_.z }); }
		template<typename F>
		void Group(char const* key_, F&& f_) {
			JSON sub = JSON::object();
			Writer w{ sub };
			f_(w);
			J_[key_] = std::move(sub);
		}
	private:
		JSON& J_;
	};

	// あるキーだけ上書きする（型が違うキーは無視）
	class Reader {
	public:
		explicit Reader(JSON const& j_) : J_{ j_ } {}
		void operator()(char const* key_, float& v_) {
			auto const it{ J_.find(key_) };
			if (it != J_.end() && it->is_number()) { v_ = it->get<float>(); }
		}
		void operator()(char const* key_, int& v_) {
			auto const it{ J_.find(key_) };
			if (it != J_.end() && it->is_number_integer()) { v_ = it->get<int>(); }
		}
		void operator()(char const* key_, bool& v_) {
			auto const it{ J_.find(key_) };
			if (it != J_.end() && it->is_boolean()) { v_ = it->get<bool>(); }
		}
		void operator()(char const* key_, Float3& v_) {
			auto const it{ J_.find(key_) };
			if (it == J_.end() || !it->is_array() || it->size() != 3U) { return; }
			for (auto const& e : *it) { if (!e.is_number()) { return; } }
			v_ = { (*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>() };
		}
		template<typename F>
		void Group(char const* key_, F&& f_) {
			auto const it{ J_.find(key_) };
			if (it == J_.end() || !it->is_object()) { return; }
			Reader r{ *it };
			f_(r);
		}
	private:
		JSON const& J_;
	};

	template<typename Ar>
	void VisitTrail(Ar& ar_, Game::CombatVfx::TrailStyle& s_) {
		ar_("Enabled", s_.Enabled);
		ar_("BladeBase", s_.BladeBase);
		ar_("BladeTip", s_.BladeTip);
		ar_("Lifetime", s_.Lifetime);
		ar_("Subdivide", s_.Subdivide);
		ar_("MinSpeed", s_.MinSpeed);
		ar_("FullSpeed", s_.FullSpeed);
		ar_("HeadColor", s_.HeadColor);
		ar_("TailColor", s_.TailColor);
		ar_("Intensity", s_.Intensity);
		ar_("AgeFalloff", s_.AgeFalloff);
		ar_("TipBias", s_.TipBias);
		ar_("CoreWhite", s_.CoreWhite);
		ar_("Streak", s_.Streak);
	}

	template<typename Ar>
	void VisitWard(Ar& ar_, Game::CombatVfx::WardStyle& s_) {
		ar_("Enabled", s_.Enabled);
		ar_("Radius", s_.Radius);
		ar_("Bottom", s_.Bottom);
		ar_("Top", s_.Top);
		ar_("Segments", s_.Segments);
		ar_("Color", s_.Color);
		ar_("DeflectColor", s_.DeflectColor);
		ar_("Intensity", s_.Intensity);
		ar_("DeflectIntensity", s_.DeflectIntensity);
		ar_("RiseRate", s_.RiseRate);
		ar_("FallRate", s_.FallRate);
		ar_("FlashDecay", s_.FlashDecay);
	}

	template<typename Ar>
	void VisitBurst(Ar& ar_, Game::CombatVfx::BurstStyle& s_) {
		ar_("Enabled", s_.Enabled);
		ar_("Color", s_.Color);
		ar_("Life", s_.Life);
		ar_("RingSize", s_.RingSize);
		ar_("GlowSize", s_.GlowSize);
		ar_("Intensity", s_.Intensity);
		ar_("StarSize", s_.StarSize);
		ar_("GroundRing", s_.GroundRing);
		ar_("Sparks", s_.Sparks);
		ar_("SparkSpeed", s_.SparkSpeed);
		ar_("LightIntensity", s_.LightIntensity);
	}

	template<typename Ar>
	void VisitTelegraph(Ar& ar_, Game::CombatVfx::TelegraphStyle& s_) {
		ar_("Enabled", s_.Enabled);
		ar_("SlashColor", s_.SlashColor);
		ar_("WaveColor", s_.WaveColor);
		ar_("DecalHeight", s_.DecalHeight);
		ar_("DecalDepthBias", s_.DecalDepthBias);
		ar_("SlashWidth", s_.SlashWidth);
		ar_("SlashReach", s_.SlashReach);
		ar_("LaneIntensity", s_.LaneIntensity);
		ar_("FillAlpha", s_.FillAlpha);
		ar_("FadeFrames", s_.FadeFrames);
		ar_("GroundRingRadius", s_.GroundRingRadius);
		ar_("GlintSize", s_.GlintSize);
		ar_("GlintIntensity", s_.GlintIntensity);
		ar_("GlintFrames", s_.GlintFrames);
		ar_("ConvergeRate", s_.ConvergeRate);
		ar_("ConvergeRadius", s_.ConvergeRadius);
		ar_("PetalRate", s_.PetalRate);
		ar_("PetalRadius", s_.PetalRadius);
	}

	template<typename Ar>
	void VisitFootsteps(Ar& ar_, Game::CombatVfx::FootstepStyle& s_) {
		ar_("Enabled", s_.Enabled);
		ar_("StepDistance", s_.StepDistance);
		ar_("PerStep", s_.PerStep);
		ar_("FootHeight", s_.FootHeight);
		ar_("Spread", s_.Spread);
		ar_("Size", s_.Size);
		ar_("Rise", s_.Rise);
		ar_("Life", s_.Life);
		ar_("Alpha", s_.Alpha);
		ar_("PlayerColor", s_.PlayerColor);
		ar_("BossColor", s_.BossColor);
		ar_("LightChance", s_.LightChance);
		ar_("LightScale", s_.LightScale);
	}

	template<typename Ar>
	void VisitAura(Ar& ar_, Game::CombatVfx::AuraStyle& s_) {
		ar_("Enabled", s_.Enabled);
		ar_("HueCenter", s_.HueCenter);
		ar_("HueRange", s_.HueRange);
		ar_("Saturation", s_.Saturation);
		ar_("Alpha", s_.Alpha);
		ar_("Size", s_.Size);
		ar_("LightScale", s_.LightScale);
		ar_("Dash", s_.Dash);
		ar_("DashSize", s_.DashSize);
	}

	template<typename Ar>
	void VisitStun(Ar& ar_, Game::CombatVfx::StunStyle& s_) {
		ar_("Enabled", s_.Enabled);
		ar_("Count", s_.Count);
		ar_("Height", s_.Height);
		ar_("Radius", s_.Radius);
		ar_("Size", s_.Size);
		ar_("SpinSpeed", s_.SpinSpeed);
		ar_("Bob", s_.Bob);
		ar_("Color", s_.Color);
		ar_("Intensity", s_.Intensity);
		ar_("Glow", s_.Glow);
		ar_("RiseRate", s_.RiseRate);
		ar_("FallRate", s_.FallRate);
	}

	template<typename Ar>
	void Visit(Ar& ar_, Game::CombatVfx::Style& s_) {
		ar_.Group("PlayerTrail", [&](auto& g_) { VisitTrail(g_, s_.PlayerTrail); });
		ar_.Group("BossTrail", [&](auto& g_) { VisitTrail(g_, s_.BossTrail); });
		ar_.Group("Ward", [&](auto& g_) { VisitWard(g_, s_.Ward); });
		ar_.Group("Impact", [&](auto& g_) {
			g_.Group("PlayerHit", [&](auto& b_) { VisitBurst(b_, s_.Impact.PlayerHit); });
			g_.Group("BossHit", [&](auto& b_) { VisitBurst(b_, s_.Impact.BossHit); });
			g_.Group("Guard", [&](auto& b_) { VisitBurst(b_, s_.Impact.Guard); });
			g_.Group("Deflect", [&](auto& b_) { VisitBurst(b_, s_.Impact.Deflect); });
			g_.Group("Dodge", [&](auto& b_) { VisitBurst(b_, s_.Impact.Dodge); });
		});
		ar_.Group("Telegraph", [&](auto& g_) { VisitTelegraph(g_, s_.Telegraph); });
		ar_.Group("Footsteps", [&](auto& g_) { VisitFootsteps(g_, s_.Footsteps); });
		ar_.Group("BossAura", [&](auto& g_) { VisitAura(g_, s_.BossAura); });
		ar_.Group("Stun", [&](auto& g_) { VisitStun(g_, s_.Stun); });
	}
}

namespace Game::CombatVfx {
	//====	======	======	======	======	====//
	//	初期化・リセット						//
	//====	======	======	======	======	====//

	void Presenter::Initialize(Lumina::DX12::Context const& dxContext_, DXGI_FORMAT rtvFormat_, DXGI_FORMAT dsvFormat_) {
		Mesh_.Initialize(dxContext_, rtvFormat_, dsvFormat_);
		Particles_.Initialize(dxContext_, 2048U);
		Lights_.reserve(MaxLights + 64U);
		Bursts_.reserve(32U);
		Glints_.reserve(16U);
		Load();
		if (!Subscribed_) {
			Game::CombatEvents::Subscribe(this, &Presenter::OnHit);
			Subscribed_ = true;
		}
	}

	void Presenter::Clear() {
		PlayerTrail_ = {};
		BossTrail_ = {};
		PlayerWard_ = {};
		BossWard_ = {};
		Bursts_.clear();
		Glints_.clear();
		SlashLane_ = {};
		WaveLanes_ = {};
		PlayerStun_ = {};
		BossStun_ = {};
		PlayerSteps_ = {};
		BossSteps_ = {};
		PrevCharging_ = false;
		PrevSlamWindup_ = false;
		PrevWaveWindup_ = false;
		PrevWaveSlashing_ = false;
		BossDashFrames_ = 0;
		Particles_.Clear();
		Lights_.clear();
		Mesh_.Clear();
	}

	auto Presenter::Random01() noexcept -> float {
		// xorshift32
		Rng_ ^= Rng_ << 13U;
		Rng_ ^= Rng_ >> 17U;
		Rng_ ^= Rng_ << 5U;
		return static_cast<float>(Rng_ >> 8U) * (1.0f / 16777216.0f);
	}

	//====	======	======	======	======	====//
	//	命中（Game.CombatEvents から）			//
	//====	======	======	======	======	====//

	void Presenter::OnHit(void* self_, Game::CombatEvents::Hit const& e_) {
		auto& self{ *static_cast<Presenter*>(self_) };
		using Game::CombatEvents::Outcome;
		using Game::CombatEvents::Side;

		ImpactStyle const& look{ self.Look.Impact };
		BurstStyle const* style{ nullptr };
		switch (e_.Result) {
			case Outcome::Hit:		 style = (e_.Victim == Side::Player) ? &look.PlayerHit : &look.BossHit; break;
			case Outcome::Guarded:	 style = &look.Guard; break;
			case Outcome::Deflected: style = &look.Deflect; break;
			case Outcome::Dodged:	 style = &look.Dodge; break;
		}

		// 受けた側の構えをひらめかせる（ガード・弾き）
		Ward& ward{ (e_.Victim == Side::Player) ? self.PlayerWard_ : self.BossWard_ };
		if (e_.Result == Outcome::Deflected) { ward.Flash = 1.0f; }
		else if (e_.Result == Outcome::Guarded) { ward.Flash = std::max(ward.Flash, 0.6f); }

		if (style == nullptr || !style->Enabled) { return; }

		FighterView const& victim{ (e_.Victim == Side::Player) ? self.LastInput_.Player : self.LastInput_.Boss };
		Burst b{};
		b.Point = e_.Point;
		b.Ground = { victim.Position.x, victim.Position.y + self.Look.Telegraph.DecalHeight, victim.Position.z };
		b.Age = 0;
		b.Look = *style;
		b.Spin = self.Random01() * TwoPi;
		if (self.Bursts_.size() >= 32U) { self.Bursts_.erase(self.Bursts_.begin()); }
		self.Bursts_.push_back(b);
		++self.NumBurstsTotal_;

		self.EmitSparks(e_.Point, e_.Dir, *style);
	}

	void Presenter::EmitSparks(Float3 const& p_, Lumina::Float2 const& dir_, BurstStyle const& s_) {
		float const dl{ std::sqrt(dir_.x * dir_.x + dir_.y * dir_.y) };
		Float3 const push{ (dl > 1.0e-5f) ? Float3{ dir_.x / dl, 0.0f, dir_.y / dl } : Float3{ 0.0f, 0.0f, 0.0f } };
		int const n{ std::clamp(s_.Sparks, 0, 64) };
		for (int i = 0; i < n; ++i) {
			// 球の上の向き（一様）＋押す向き＋少し上
			float const z{ Random01() * 2.0f - 1.0f };
			float const a{ Random01() * TwoPi };
			float const r{ std::sqrt(std::max(1.0f - z * z, 0.0f)) };
			Float3 const d{ Normalize(Add(Add(Float3{ r * std::cos(a), z, r * std::sin(a) }, Mul(push, 0.9f)), Float3{ 0.0f, 0.35f, 0.0f }), Float3{ 0.0f, 1.0f, 0.0f }) };
			float const speed{ s_.SparkSpeed * (0.45f + 0.9f * Random01()) };

			FxParticle p{ MakeParticle() };
			p.Translate = p_;
			p.Velocity = Mul(d, speed);
			p.Drag = 0.88f;
			p.Gravity = 0.012f;
			p.Scale = { 0.9f + 0.6f * Random01(), 0.9f + 0.6f * Random01(), 1.0f };
			p.ScaleMul = 0.96f;
			p.Life = 14.0f + 12.0f * Random01();
			p.MaxLife = p.Life;
			p.BaseAlpha = 0.9f;
			p.AlphaMul = 0.95f;
			p.Rotate.z = Random01() * TwoPi;
			p.Spin = (Random01() - 0.5f) * 0.3f;
			float const w{ 0.3f * Random01() };
			p.RenderData.RGBA = { s_.Color.x + w, s_.Color.y + w, s_.Color.z + w, 0.0f };
			p.RenderData.DiffuseAtlasID = (Random01() < 0.5f) ? 4U : 2U;	// 四芒星 / 星
			Particles_.Emit(std::move(p));
		}
	}

	//====	======	======	======	======	====//
	//	毎フレーム								//
	//====	======	======	======	======	====//

	void Presenter::Update(Lumina::DX12::CommandList const& cmdList_, FrameInput const& in_) {
		LastInput_ = in_;
		ViewToWorld_ = in_.ViewToWorld;
		if (ViewToWorld_ != nullptr) {
			Lumina::Mat4 const& v{ *ViewToWorld_ };
			CamRight_ = Normalize(Float3{ v[0][0], v[0][1], v[0][2] }, Float3{ 1.0f, 0.0f, 0.0f });
			CamUp_ = Normalize(Float3{ v[1][0], v[1][1], v[1][2] }, Float3{ 0.0f, 1.0f, 0.0f });
		}
		Time_ += 1.0f / 60.0f;
		AuraTime_ += 0.5f;

		UpdateTrail(PlayerTrail_, Look.PlayerTrail, in_.Player, in_.Suspended);
		UpdateTrail(BossTrail_, Look.BossTrail, in_.Boss, in_.Suspended);
		UpdateWard(PlayerWard_, in_.Player, in_.Suspended);
		UpdateWard(BossWard_, in_.Boss, in_.Suspended);
		UpdateTelegraph(in_);
		UpdateStun(PlayerStun_, in_.Player, in_.Suspended);
		UpdateStun(BossStun_, in_.Boss, in_.Suspended);
		UpdateFootsteps(PlayerSteps_, in_.Player, Look.Footsteps.PlayerColor, in_.Suspended);
		UpdateFootsteps(BossSteps_, in_.Boss, Look.Footsteps.BossColor, in_.Suspended);
		EmitBossAura(in_);

		for (Burst& b : Bursts_) { ++b.Age; }
		std::erase_if(Bursts_, [](Burst const& b_) { return b_.Age >= std::max(b_.Look.Life, 1); });
		for (Glint& g : Glints_) { ++g.Age; }
		std::erase_if(Glints_, [](Glint const& g_) { return g_.Age >= std::max(g_.Life, 1); });

		static Lumina::Mat4 const identity{};
		Particles_.Update(cmdList_, (ViewToWorld_ != nullptr) ? *ViewToWorld_ : identity, UpdateFxParticle);
		CollectLights();

		// 帯・輪・弧はここで組み立てておく（ポーズ中は Update が止まるので、止まった形のまま描かれる）
		Mesh_.Clear();
		BuildTrail(PlayerTrail_, Look.PlayerTrail);
		BuildTrail(BossTrail_, Look.BossTrail);
		BuildWard(PlayerWard_, in_.Player);
		BuildWard(BossWard_, in_.Boss);
		BuildTelegraph(in_);
		BuildStun(PlayerStun_, in_.Player);
		BuildStun(BossStun_, in_.Boss);
		BuildBursts();
	}

	//==== 剣の軌跡 ====//

	void Presenter::UpdateTrail(Trail& t_, TrailStyle const& s_, FighterView const& f_, bool suspended_) {
		if (suspended_ || !s_.Enabled) {
			t_.Samples.clear();
			t_.WasSwinging = false;
			return;
		}

		int const lifetime{ std::clamp(s_.Lifetime, 1, 60) };
		for (TrailSample& s : t_.Samples) { ++s.Age; }
		std::erase_if(t_.Samples, [lifetime](TrailSample const& x_) { return x_.Age > lifetime; });

		bool const swinging{ f_.Swinging && f_.WeaponWorld != nullptr };
		if (swinging) {
			TrailSample s{};
			s.Base = TransformPoint(*f_.WeaponWorld, Float3{ 0.0f, s_.BladeBase, 0.0f });
			s.Tip = TransformPoint(*f_.WeaponWorld, Float3{ 0.0f, s_.BladeTip, 0.0f });
			s.Age = 0;
			s.Break = !t_.WasSwinging;
			float speed{ 0.0f };
			if (!s.Break && !t_.Samples.empty() && t_.Samples.back().Age == 1) {
				speed = Length(Sub(s.Tip, t_.Samples.back().Tip));
			}
			s.Alpha = Smoothstep(s_.MinSpeed, s_.FullSpeed, speed);
			t_.Samples.push_back(s);
		}
		t_.WasSwinging = swinging;
	}

	void Presenter::BuildTrail(Trail const& t_, TrailStyle const& s_) {
		auto const& samples{ t_.Samples };
		std::size_t const n{ samples.size() };
		if (!s_.Enabled || n < 2U) { return; }

		float const invLife{ 1.0f / static_cast<float>(std::clamp(s_.Lifetime, 1, 60)) };
		int const sub{ std::clamp(s_.Subdivide, 1, 16) };
		Float3 const head{ SRGBToLinear(s_.HeadColor) };
		Float3 const tail{ SRGBToLinear(s_.TailColor) };
		Float4 const param{ s_.AgeFalloff, s_.TipBias, s_.CoreWhite, s_.Streak };

		for (std::size_t i = 1U; i < n; ++i) {
			TrailSample const& a{ samples[i - 1U] };
			TrailSample const& b{ samples[i] };
			if (b.Break) { continue; }
			if (a.Alpha <= 0.0f && b.Alpha <= 0.0f) { continue; }
			TrailSample const& p0{ (i >= 2U && !a.Break) ? samples[i - 2U] : a };
			TrailSample const& p3{ (i + 1U < n && !samples[i + 1U].Break) ? samples[i + 1U] : b };

			for (int k = 0; k < sub; ++k) {
				float const t0{ static_cast<float>(k) / static_cast<float>(sub) };
				float const t1{ static_cast<float>(k + 1) / static_cast<float>(sub) };
				Float3 const base0{ CatmullRom(p0.Base, a.Base, b.Base, p3.Base, t0) };
				Float3 const base1{ CatmullRom(p0.Base, a.Base, b.Base, p3.Base, t1) };
				Float3 const tip0{ CatmullRom(p0.Tip, a.Tip, b.Tip, p3.Tip, t0) };
				Float3 const tip1{ CatmullRom(p0.Tip, a.Tip, b.Tip, p3.Tip, t1) };
				float const u0{ Saturate(Lerp(static_cast<float>(a.Age), static_cast<float>(b.Age), t0) * invLife) };
				float const u1{ Saturate(Lerp(static_cast<float>(a.Age), static_cast<float>(b.Age), t1) * invLife) };
				float const al0{ Lerp(a.Alpha, b.Alpha, t0) * s_.Intensity };
				float const al1{ Lerp(a.Alpha, b.Alpha, t1) * s_.Intensity };
				Float3 const c0{ Lerp(head, tail, u0) };
				Float3 const c1{ Lerp(head, tail, u1) };
				Mesh_.Quad(
					MakeVertex(base0, u0, 0.0f, Shape::Trail, c0, al0, param, 0.0f, 0.5f),
					MakeVertex(base1, u1, 0.0f, Shape::Trail, c1, al1, param, 0.0f, 0.5f),
					MakeVertex(tip0, u0, 1.0f, Shape::Trail, c0, al0, param, 0.0f, 0.5f),
					MakeVertex(tip1, u1, 1.0f, Shape::Trail, c1, al1, param, 0.0f, 0.5f)
				);
			}
		}
	}

	//==== ガードの弧 ====//

	void Presenter::UpdateWard(Ward& w_, FighterView const& f_, bool suspended_) {
		WardStyle const& s{ Look.Ward };
		float const target{ (f_.Guarding && !suspended_) ? 1.0f : 0.0f };
		float const rate{ (target > w_.Level) ? s.RiseRate : s.FallRate };
		w_.Level += (target - w_.Level) * Saturate(rate);
		float const deflectTarget{ (f_.Guarding && f_.CanDeflect && !suspended_) ? 1.0f : 0.0f };
		w_.Deflect += (deflectTarget - w_.Deflect) * 0.5f;
		w_.Flash *= Saturate(s.FlashDecay);
		if (w_.Flash < 0.01f) { w_.Flash = 0.0f; }
		if (w_.Level < 0.005f) { w_.Level = 0.0f; }
	}

	void Presenter::BuildWard(Ward const& w_, FighterView const& f_) {
		WardStyle const& s{ Look.Ward };
		if (!s.Enabled || (w_.Level + w_.Flash) < 0.01f) { return; }

		int const segs{ std::clamp(s.Segments, 4, 64) };
		float const half{ std::clamp(f_.GuardHalfAngle, 0.05f, Pi) };
		Float3 const color{ Lerp(SRGBToLinear(s.Color), SRGBToLinear(s.DeflectColor), Saturate(w_.Deflect)) };
		// 構えが消えていく途中でも、ひらめきは見せる
		float const level{ std::max(w_.Level, w_.Flash * 0.5f) };
		Float4 const param{ s.Intensity * level, s.DeflectIntensity * w_.Deflect * w_.Level, w_.Flash, 0.0f };
		float const y0{ f_.Position.y + s.Bottom };
		float const y1{ f_.Position.y + s.Top };

		auto const at{ [&](float th_, float y_) {
			return Float3{ f_.Position.x + s.Radius * std::cos(th_), y_, f_.Position.z + s.Radius * std::sin(th_) };
		} };
		for (int j = 0; j < segs; ++j) {
			float const u0{ static_cast<float>(j) / static_cast<float>(segs) };
			float const u1{ static_cast<float>(j + 1) / static_cast<float>(segs) };
			float const th0{ f_.Angle - half + 2.0f * half * u0 };
			float const th1{ f_.Angle - half + 2.0f * half * u1 };
			Mesh_.Quad(
				MakeVertex(at(th0, y0), u0, 0.0f, Shape::Ward, color, 1.0f, param, 0.0f, 0.6f),
				MakeVertex(at(th1, y0), u1, 0.0f, Shape::Ward, color, 1.0f, param, 0.0f, 0.6f),
				MakeVertex(at(th0, y1), u0, 1.0f, Shape::Ward, color, 1.0f, param, 0.0f, 0.6f),
				MakeVertex(at(th1, y1), u1, 1.0f, Shape::Ward, color, 1.0f, param, 0.0f, 0.6f)
			);
		}
	}

	//==== 予兆 ====//

	void Presenter::UpdateTelegraph(FrameInput const& in_) {
		TelegraphStyle const& s{ Look.Telegraph };
		BossSkillView const& sk{ in_.Skills };
		FighterView const& boss{ in_.Boss };
		bool const enabled{ s.Enabled && !in_.Suspended };
		float const fadeStep{ 1.0f / static_cast<float>(std::max(s.FadeFrames, 1)) };

		Float3 tip{ boss.Visual };
		if (boss.WeaponWorld != nullptr) {
			tip = TransformPoint(*boss.WeaponWorld, Float3{ 0.0f, Look.BossTrail.BladeTip, 0.0f });
		}

		//---- 溜め斬り ----//
		bool const charging{ enabled && sk.ChargeProgress >= 0.0f };
		bool const windup{ enabled && sk.SlamWindup };
		if (charging || windup) {
			float const dx{ in_.Player.Position.x - boss.Position.x };
			float const dz{ in_.Player.Position.z - boss.Position.z };
			float const dist{ std::sqrt(dx * dx + dz * dz) };
			float const lunge{ std::clamp(dist - sk.LungeStop, 0.0f, std::max(sk.LungeMax, 0.0f)) };
			SlashLane_.Active = true;
			SlashLane_.Origin = boss.Position;
			SlashLane_.Angle = boss.Angle;
			SlashLane_.Length = lunge + s.SlashReach;
			SlashLane_.Fill = charging ? sk.ChargeProgress : 1.0f;
			SlashLane_.Pulse = windup ? (0.5f + 0.5f * std::sin(Time_ * 40.0f)) : 0.0f;
			SlashLane_.Fade = std::min(SlashLane_.Fade + 0.25f, 1.0f);

			// 刃先へ集まる光（溜めの間）
			if (charging) {
				Float3 const c{ s.SlashColor };
				for (int i = 0; i < std::clamp(s.ConvergeRate, 0, 16); ++i) {
					float const z{ Random01() * 2.0f - 1.0f };
					float const a{ Random01() * TwoPi };
					float const r{ std::sqrt(std::max(1.0f - z * z, 0.0f)) };
					Float3 const d{ r * std::cos(a), z, r * std::sin(a) };
					FxParticle p{ MakeParticle() };
					p.Translate = Add(tip, Mul(d, s.ConvergeRadius * (0.6f + 0.4f * Random01())));
					p.Velocity = Mul(d, -0.05f);
					p.Attractor = tip;
					p.Pull = 0.012f;
					p.Drag = 0.88f;
					p.Scale = { 0.9f, 0.9f, 1.0f };
					p.ScaleMul = 0.97f;
					p.Life = 22.0f;
					p.MaxLife = p.Life;
					p.FadeIn = 5.0f;
					p.BaseAlpha = 0.7f;
					p.RenderData.RGBA = { c.x + 0.2f, c.y + 0.2f, c.z + 0.2f, 0.0f };
					p.RenderData.DiffuseAtlasID = 0U;
					Particles_.Emit(std::move(p));
				}
			}
		}
		else if (SlashLane_.Active) {
			SlashLane_.Fade -= fadeStep;
			SlashLane_.Pulse = 0.0f;
			if (SlashLane_.Fade <= 0.0f) { SlashLane_ = {}; }
		}

		auto const glint{ [&](Float3 const& at_, float size_, Float3 const& color_, int life_) {
			if (Glints_.size() >= 16U) { Glints_.erase(Glints_.begin()); }
			Glints_.push_back(Glint{ at_, 0, std::max(life_, 1), size_, color_, Random01() * TwoPi });
		} };
		if (charging && !PrevCharging_) { glint(tip, s.GlintSize * 0.6f, s.SlashColor, s.GlintFrames); }
		if (windup && !PrevSlamWindup_) { glint(tip, s.GlintSize, Float3{ 1.0f, 0.55f, 0.5f }, s.GlintFrames + 6); }	// 振り下ろしの直前
		PrevCharging_ = charging;
		PrevSlamWindup_ = windup;

		//---- 茨の波 ----//
		bool const waveWindup{ enabled && sk.WindupProgress >= 0.0f };
		bool const waveSlash{ enabled && sk.WaveSlashing };
		if (waveWindup || waveSlash) {
			WaveLanes_.Active = true;
			WaveLanes_.Origin = boss.Position;
			WaveLanes_.Angle = boss.Angle;
			WaveLanes_.Fill = waveWindup ? sk.WindupProgress : 1.0f;
			WaveLanes_.Pulse = waveSlash ? (0.5f + 0.5f * std::sin(Time_ * 40.0f)) : 0.0f;
			WaveLanes_.Fade = std::min(WaveLanes_.Fade + 0.25f, 1.0f);

			// 足元を回りながら寄ってくる花びら（溜めの間）
			if (waveWindup) {
				Float3 const c{ s.WaveColor };
				for (int i = 0; i < std::clamp(s.PetalRate, 0, 16); ++i) {
					float const th{ Random01() * TwoPi };
					float const r{ s.PetalRadius * (0.7f + 0.5f * Random01()) };
					FxParticle p{ MakeParticle() };
					p.Translate = { boss.Position.x + r * std::cos(th), boss.Position.y + Look.Footsteps.FootHeight + Random01() * 0.6f, boss.Position.z + r * std::sin(th) };
					p.Velocity = { 0.0f, 0.02f, 0.0f };
					p.Attractor = { boss.Position.x, p.Translate.y + 1.5f, boss.Position.z };
					p.Pull = 0.004f;
					p.Swirl = 0.05f;
					p.Drag = 0.93f;
					p.Scale = { 1.2f, 1.2f, 1.0f };
					p.ScaleMul = 0.99f;
					p.Life = 40.0f;
					p.MaxLife = p.Life;
					p.FadeIn = 6.0f;
					p.BaseAlpha = 0.45f;
					p.Rotate.z = Random01() * TwoPi;
					p.Spin = 0.08f;
					p.RenderData.RGBA = { c.x, c.y, c.z, 0.0f };
					p.RenderData.DiffuseAtlasID = (Random01() < 0.5f) ? 3U : 0U;
					Particles_.Emit(std::move(p));
				}
			}
		}
		else if (WaveLanes_.Active) {
			WaveLanes_.Fade -= fadeStep;
			WaveLanes_.Pulse = 0.0f;
			if (WaveLanes_.Fade <= 0.0f) { WaveLanes_ = {}; }
		}
		if (waveWindup && !PrevWaveWindup_) { glint(tip, s.GlintSize * 0.6f, s.WaveColor, s.GlintFrames); }
		if (waveSlash && !PrevWaveSlashing_) { glint(tip, s.GlintSize, Float3{ 1.0f, 0.6f, 0.8f }, s.GlintFrames + 6); }	// 撃つ直前
		PrevWaveWindup_ = waveWindup;
		PrevWaveSlashing_ = waveSlash;
	}

	void Presenter::BuildLane(Float3 const& origin_, float angle_, float start_, float length_, float width_, Float3 const& rgb_, float intensity_, float fill_) {
		if (!(length_ > 0.05f) || !(intensity_ > 0.0f)) { return; }
		TelegraphStyle const& s{ Look.Telegraph };
		Float3 const dir{ std::cos(angle_), 0.0f, std::sin(angle_) };
		Float3 const side{ -dir.z, 0.0f, dir.x };
		float const hw{ width_ * 0.5f };
		float const y{ origin_.y + s.DecalHeight };
		auto const at{ [&](float along_, float across_) {
			return Float3{ origin_.x + dir.x * along_ + side.x * across_, y, origin_.z + dir.z * along_ + side.z * across_ };
		} };
		Float4 const param{ Saturate(fill_), length_, 0.0f, s.FillAlpha };
		float const e{ start_ + length_ };
		Mesh_.Quad(
			MakeVertex(at(start_, -hw), 0.0f, 0.0f, Shape::Lane, rgb_, intensity_, param, s.DecalDepthBias, 0.5f),
			MakeVertex(at(e, -hw), 1.0f, 0.0f, Shape::Lane, rgb_, intensity_, param, s.DecalDepthBias, 0.5f),
			MakeVertex(at(start_, hw), 0.0f, 1.0f, Shape::Lane, rgb_, intensity_, param, s.DecalDepthBias, 0.5f),
			MakeVertex(at(e, hw), 1.0f, 1.0f, Shape::Lane, rgb_, intensity_, param, s.DecalDepthBias, 0.5f)
		);
	}
	void Presenter::BuildTelegraph(FrameInput const& in_) {
		TelegraphStyle const& s{ Look.Telegraph };
		BossSkillView const& sk{ in_.Skills };

		if (SlashLane_.Active) {
			Float3 const c{ SRGBToLinear(s.SlashColor) };
			float const intensity{ s.LaneIntensity * SlashLane_.Fade * (1.0f + 0.6f * SlashLane_.Pulse) };
			BuildLane(SlashLane_.Origin, SlashLane_.Angle, 0.5f, SlashLane_.Length, s.SlashWidth, c, intensity, SlashLane_.Fill);
			// 溜めの間、足元で縮む輪
			if (sk.ChargeProgress >= 0.0f) {
				float const p{ sk.ChargeProgress };
				Float3 const center{ SlashLane_.Origin.x, SlashLane_.Origin.y + s.DecalHeight, SlashLane_.Origin.z };
				BuildFlat(Shape::Ring, center, Lerp(s.GroundRingRadius, 1.6f, p), c, s.LaneIntensity * (0.4f + 0.8f * p), Float4{ 0.86f, 0.06f, 0.12f * p, 0.0f });
			}
		}

		if (WaveLanes_.Active) {
			Float3 const c{ SRGBToLinear(s.WaveColor) };
			float const intensity{ s.LaneIntensity * WaveLanes_.Fade * (1.0f + 0.6f * WaveLanes_.Pulse) };
			int const n{ std::clamp(sk.WaveLines, 1, 16) };
			for (int i = 0; i < n; ++i) {
				float const a{ WaveLanes_.Angle + (static_cast<float>(i) - static_cast<float>(n - 1) * 0.5f) * sk.WaveSpread };
				// 闘技場の端までで切る（茨は外へ出たら消える）
				float const cx{ std::cos(a) };
				float const cz{ std::sin(a) };
				float const ox{ WaveLanes_.Origin.x + cx * sk.WaveStart };
				float const oz{ WaveLanes_.Origin.z + cz * sk.WaveStart };
				float reach{ std::max(sk.WaveMaxDistance, 0.0f) };
				if (cx > 1.0e-4f) { reach = std::min(reach, (in_.ArenaMax - ox) / cx); }
				if (cx < -1.0e-4f) { reach = std::min(reach, (in_.ArenaMin - ox) / cx); }
				if (cz > 1.0e-4f) { reach = std::min(reach, (in_.ArenaMax - oz) / cz); }
				if (cz < -1.0e-4f) { reach = std::min(reach, (in_.ArenaMin - oz) / cz); }
				BuildLane(WaveLanes_.Origin, a, sk.WaveStart, std::max(reach, 0.0f), sk.WaveHalfWidth * 2.0f, c, intensity, WaveLanes_.Fill);
			}
		}

		for (Glint const& g : Glints_) {
			float const t{ static_cast<float>(g.Age) / static_cast<float>(std::max(g.Life, 1)) };
			float const k{ (t < 0.25f) ? (t / 0.25f) : (1.0f - (t - 0.25f) / 0.75f) };
			Float3 const c{ SRGBToLinear(g.Color) };
			BuildBillboard(Shape::Star, g.Point, g.Size * (0.4f + 0.6f * k), c, s.GlintIntensity * k, Float4{ 14.0f, 1.2f, 40.0f, g.Spin + t * 0.8f }, 1.5f);
			BuildBillboard(Shape::Glow, g.Point, g.Size * 0.6f, c, s.GlintIntensity * 0.6f * k * k, Float4{ 2.0f, 30.0f, 0.5f, 0.0f }, 1.5f);
		}
	}

	//==== スタン（頭の上で回る星）====//

	void Presenter::UpdateStun(Stun& s_, FighterView const& f_, bool suspended_) {
		StunStyle const& st{ Look.Stun };
		float const target{ (st.Enabled && f_.Stunned && !suspended_) ? 1.0f : 0.0f };
		float const rate{ (target > s_.Level) ? st.RiseRate : st.FallRate };
		s_.Level += (target - s_.Level) * Saturate(rate);
		if (s_.Level < 0.005f) { s_.Level = 0.0f; }
		if (s_.Level > 0.0f) { s_.Angle += st.SpinSpeed; }
		else { s_.Angle = Random01() * TwoPi; }		// 次に出るときは別の角度から
	}

	void Presenter::BuildStun(Stun const& s_, FighterView const& f_) {
		StunStyle const& st{ Look.Stun };
		if (!(s_.Level > 0.0f)) { return; }
		int const n{ std::clamp(st.Count, 1, 12) };
		Float3 const c{ SRGBToLinear(st.Color) };
		float const ease{ s_.Level * s_.Level * (3.0f - 2.0f * s_.Level) };
		for (int i = 0; i < n; ++i) {
			float const fi{ static_cast<float>(i) };
			float const a{ s_.Angle + TwoPi * fi / static_cast<float>(n) };
			// 少し傾いた輪（奥の星は少し下がる）＋ 1 つずつずれた上下の揺れ
			Float3 const at{
				f_.Position.x + st.Radius * std::cos(a),
				f_.Position.y + st.Height + st.Radius * 0.25f * std::sin(a) + st.Bob * std::sin(Time_ * 6.0f + fi * 1.3f),
				f_.Position.z + st.Radius * std::sin(a),
			};
			float const size{ st.Size * (0.6f + 0.4f * ease) };
			BuildBillboard(Shape::Star5, at, size, c, st.Intensity * ease, Float4{ a * 1.5f, 0.42f, st.Glow, 0.0f }, 0.5f);
		}
	}

	//==== 命中 ====//

	void Presenter::BuildBursts() {
		for (Burst const& b : Bursts_) {
			BurstStyle const& s{ b.Look };
			float const t{ static_cast<float>(b.Age) / static_cast<float>(std::max(s.Life, 1)) };
			float const ease{ 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t) };
			float const fade{ (1.0f - t) * (1.0f - t) };
			Float3 const c{ SRGBToLinear(s.Color) };

			if (s.RingSize > 0.0f) {
				BuildBillboard(Shape::Ring, b.Point, s.RingSize * (0.35f + 0.65f * ease), c, s.Intensity * fade,
					Float4{ 0.8f, 0.10f * (1.0f - t) + 0.03f, 0.0f, 0.0f }, 1.5f);
			}
			if (s.GlowSize > 0.0f) {
				float const g{ fade * fade };
				BuildBillboard(Shape::Glow, b.Point, s.GlowSize * (0.6f + 0.4f * ease), c, s.Intensity * 1.5f * g, Float4{ 2.0f, 30.0f, 0.6f, 0.0f }, 1.5f);
			}
			if (s.StarSize > 0.0f) {
				BuildBillboard(Shape::Star, b.Point, s.StarSize * (1.0f + 0.3f * ease), c, s.Intensity * 1.5f * fade, Float4{ 14.0f, 1.4f, 40.0f, b.Spin + t * 0.6f }, 1.5f);
			}
			if (s.GroundRing > 0.0f) {
				BuildFlat(Shape::Ring, b.Ground, s.GroundRing * (0.15f + 0.85f * ease), c, s.Intensity * fade, Float4{ 0.85f, 0.06f, 0.0f, 0.0f });
			}
		}
	}

	void Presenter::BuildBillboard(Shape shape_, Float3 const& center_, float size_, Float3 const& rgb_, float intensity_, Float4 const& param_, float depthBias_) {
		if (!(size_ > 0.0f) || !(intensity_ > 0.0f)) { return; }
		Float3 const r{ Mul(CamRight_, size_) };
		Float3 const u{ Mul(CamUp_, size_) };
		Mesh_.Quad(
			MakeVertex(Add(Sub(center_, r), u), 0.0f, 0.0f, shape_, rgb_, intensity_, param_, depthBias_, 0.4f),
			MakeVertex(Add(Add(center_, r), u), 1.0f, 0.0f, shape_, rgb_, intensity_, param_, depthBias_, 0.4f),
			MakeVertex(Sub(Sub(center_, r), u), 0.0f, 1.0f, shape_, rgb_, intensity_, param_, depthBias_, 0.4f),
			MakeVertex(Sub(Add(center_, r), u), 1.0f, 1.0f, shape_, rgb_, intensity_, param_, depthBias_, 0.4f)
		);
	}

	void Presenter::BuildFlat(Shape shape_, Float3 const& center_, float size_, Float3 const& rgb_, float intensity_, Float4 const& param_) {
		if (!(size_ > 0.0f) || !(intensity_ > 0.0f)) { return; }
		float const bias{ Look.Telegraph.DecalDepthBias };
		Mesh_.Quad(
			MakeVertex(Float3{ center_.x - size_, center_.y, center_.z + size_ }, 0.0f, 0.0f, shape_, rgb_, intensity_, param_, bias, 0.5f),
			MakeVertex(Float3{ center_.x + size_, center_.y, center_.z + size_ }, 1.0f, 0.0f, shape_, rgb_, intensity_, param_, bias, 0.5f),
			MakeVertex(Float3{ center_.x - size_, center_.y, center_.z - size_ }, 0.0f, 1.0f, shape_, rgb_, intensity_, param_, bias, 0.5f),
			MakeVertex(Float3{ center_.x + size_, center_.y, center_.z - size_ }, 1.0f, 1.0f, shape_, rgb_, intensity_, param_, bias, 0.5f)
		);
	}

	//==== 足元 ====//

	void Presenter::UpdateFootsteps(Footsteps& fs_, FighterView const& f_, Float3 const& color_, bool suspended_) {
		FootstepStyle const& s{ Look.Footsteps };
		if (!s.Enabled || suspended_) {
			fs_ = {};
			return;
		}
		Float3 const cur{ f_.Position };
		if (fs_.HasPrev) {
			float const dx{ cur.x - fs_.Prev.x };
			float const dz{ cur.z - fs_.Prev.z };
			float const d{ std::sqrt(dx * dx + dz * dz) };
			if (d > 3.0f) { fs_.Accum = 0.0f; }		// 瞬間移動（巻き戻し・やり直し）
			else if (!f_.Airborne && !f_.Dashing) { fs_.Accum += d; }
		}
		fs_.Prev = cur;
		fs_.HasPrev = true;

		float const step{ std::max(s.StepDistance, 0.1f) };
		int emitted{ 0 };
		while (fs_.Accum >= step && emitted < 4) {
			fs_.Accum -= step;
			++emitted;
			for (int i = 0; i < std::clamp(s.PerStep, 0, 16); ++i) {
				float const th{ Random01() * TwoPi };
				float const r{ s.Spread * std::sqrt(Random01()) };
				float const shade{ 0.8f + 0.4f * Random01() };
				FxParticle p{ MakeParticle() };
				p.Translate = { cur.x + r * std::cos(th), cur.y + s.FootHeight, cur.z + r * std::sin(th) };
				p.Velocity = { std::cos(th) * 0.015f, s.Rise * (0.6f + 0.8f * Random01()), std::sin(th) * 0.015f };
				p.Drag = 0.97f;
				p.Gravity = -0.0004f;
				float const size{ s.Size * (0.6f + 0.8f * Random01()) };
				p.Scale = { size, size, 1.0f };
				p.ScaleMul = 0.995f;
				p.Life = static_cast<float>(std::max(s.Life, 1)) * (0.7f + 0.6f * Random01());
				p.MaxLife = p.Life;
				p.FadeIn = 6.0f;
				p.BaseAlpha = s.Alpha;
				p.Rotate.z = Random01() * TwoPi;
				p.Spin = (Random01() - 0.5f) * 0.05f;
				p.RenderData.RGBA = { color_.x * shade, color_.y * shade, color_.z * shade, 0.0f };
				p.RenderData.DiffuseAtlasID = (Random01() < 0.7f) ? 0U : 5U;	// ぼかし点 / 小四芒星
				p.LightScale = (Random01() < s.LightChance) ? s.LightScale : 0.0f;
				Particles_.Emit(std::move(p));
			}
		}
		if (fs_.Accum > step * 4.0f) { fs_.Accum = 0.0f; }
	}

	//==== ボスのオーラ・ダッシュ（InGame の Player effects と同じ動き、色だけ薔薇色）====//

	void Presenter::EmitBossAura(FrameInput const& in_) {
		AuraStyle const& s{ Look.BossAura };
		if (!s.Enabled) { BossDashFrames_ = 0; return; }
		FighterView const& boss{ in_.Boss };
		float const t{ AuraTime_ };

		float const hue{ s.HueCenter + std::sin(Time_ * 0.7f) * s.HueRange * 0.5f + (Random01() - 0.5f) * s.HueRange * 0.5f };
		Float3 const rgb{ HSVToRGB(hue, std::clamp(s.Saturation * (0.85f + 0.3f * Random01()), 0.0f, 1.0f), 0.95f) };

		auto const basic{ [&](Float3 const& at_, Float3 const& velocity_, float size_, float life_, float alpha_, Float3 const& color_) {
			FxParticle p{ MakeParticle() };
			p.Translate = at_;
			p.Velocity = velocity_;
			p.Scale = { size_, size_, 1.0f };
			p.ScaleMul = 0.93f;
			p.AlphaMul = 0.95f;
			p.BaseAlpha = alpha_;
			p.Life = life_;
			p.MaxLife = life_;
			p.Rotate.z = Random01() * TwoPi;
			p.Spin = 0.01f;
			p.SpinByVelocityZ = true;
			p.RenderData.RGBA = { color_.x, color_.y, color_.z, alpha_ };
			bool const star{ Random01() < 0.25f };
			p.RenderData.DiffuseAtlasID = star ? 4U : 3U;
			p.LightScale = star ? s.LightScale : 0.0f;
			Particles_.Emit(std::move(p));
		} };

		// 体のまわり（プレイヤーと同じ式）
		for (int i = 0; i < 2; ++i) {
			float const fi{ static_cast<float>(i) };
			Float3 const offset{
				std::cos(t * 0.3f + fi * 3.6f) * 1.5f,
				std::sin(t * 0.4f * fi) * 1.0f,
				std::sin(t * 0.5f - fi * 1.2f) * 1.5f,
			};
			Float3 const c{ rgb.x + Random01() * 0.1f, rgb.y + Random01() * 0.1f, rgb.z };
			basic(Add(boss.Visual, offset), Float3{ offset.z * -0.05f, 0.0f, offset.x * -0.05f }, s.Size, 64.0f, s.Alpha, c);
		}
		// 刃先（プレイヤーは両手から出している。ボスは右手しか作っていないので刃先から）
		if (boss.WeaponWorld != nullptr) {
			Float3 const tip{ TransformPoint(*boss.WeaponWorld, Float3{ 0.0f, Look.BossTrail.BladeTip, 0.0f }) };
			Float3 const offset{ std::cos(t * 0.3f) * 0.1f, std::sin(t * 0.4f) * 0.1f, std::sin(t * 0.5f) * 0.1f };
			basic(Add(tip, offset), Float3{ offset.z * -0.05f, 0.0f, offset.x * -0.05f }, s.Size, 64.0f, s.Alpha, rgb);
		}

		// ダッシュ（プレイヤーの Dash の粒子と同じ式）
		if (s.Dash && boss.DashTriggered && !in_.Suspended) { BossDashFrames_ = 15; }
		if (BossDashFrames_ > 0) {
			float const c{ std::cos(boss.Angle) };
			float const sn{ std::sin(boss.Angle) };
			for (int i = 0; i < 2; ++i) {
				float const fi{ static_cast<float>(i) };
				Float3 const offset{
					std::cos(t * 2.4f + Pi * fi) * 0.2f + c * 0.9f,
					std::sin(t * 0.8f) * 0.1f,
					std::sin(t * 2.4f + Pi * fi) * 0.2f + sn * 0.9f,
				};
				basic(Add(boss.Visual, offset), Float3{ offset.x * -0.1f, 0.0f, offset.z * -0.1f }, s.DashSize, 32.0f, 0.5f,
					Float3{ 0.1f + rgb.x, 0.1f + rgb.y, 0.1f + rgb.z });
			}
			--BossDashFrames_;
		}
	}

	//==== 点光源 ====//

	void Presenter::CollectLights() {
		Lights_.clear();
		Lumina::List<FxParticle>::Iterator it{ Particles_.InstanceList() };
		for (it.Begin(); !it.End(); it.Next()) {
			if (Lights_.size() >= MaxLights) { break; }
			FxParticle const& p{ *it };
			if (!(p.LightScale > 0.0f) || p.Life <= 0.0f) { continue; }
			float const intensity{ p.RenderData.RGBA.w * p.LightScale };
			if (intensity < 0.05f) { continue; }
			Lights_.push_back({ p.Translate, Float3{ p.RenderData.RGBA.x, p.RenderData.RGBA.y, p.RenderData.RGBA.z }, intensity });
		}
		for (Burst const& b : Bursts_) {
			if (!(b.Look.LightIntensity > 0.0f)) { continue; }
			float const t{ static_cast<float>(b.Age) / static_cast<float>(std::max(b.Look.Life, 1)) };
			Lights_.push_back({ b.Point, b.Look.Color, b.Look.LightIntensity * (1.0f - t) * (1.0f - t) });
		}
		for (Glint const& g : Glints_) {
			float const t{ static_cast<float>(g.Age) / static_cast<float>(std::max(g.Life, 1)) };
			float const k{ (t < 0.25f) ? (t / 0.25f) : (1.0f - (t - 0.25f) / 0.75f) };
			Lights_.push_back({ g.Point, g.Color, Look.Telegraph.GlintIntensity * 30.0f * k });
		}
	}

	//====	======	======	======	======	====//
	//	描画									//
	//====	======	======	======	======	====//

	void Presenter::RenderParticles(
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::DX12::RootSignature const& rs_,
		Lumina::DX12::GraphicsPSO const& pso_,
		D3D12_CPU_DESCRIPTOR_HANDLE localCBV_SceneVars_,
		D3D12_CPU_DESCRIPTOR_HANDLE localCBV_VP_,
		Lumina::DX12::DescriptorTable const& textures_,
		Lumina::DX12::DescriptorTable const& textures2_
	) {
		Particles_.Render(cmdList_, rs_, pso_, localCBV_SceneVars_, localCBV_VP_, textures_, textures2_);
	}

	void Presenter::RenderMesh(
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::Mat4 const& viewProjection_,
		float nearZ_,
		float farZ_,
		D3D12_GPU_DESCRIPTOR_HANDLE depthSRV_
	) {
		Mesh_.Render(cmdList_, viewProjection_, nearZ_, farZ_, Time_, depthSRV_);
	}

	//====	======	======	======	======	====//
	//	保存・読み込み							//
	//====	======	======	======	======	====//

	auto Presenter::Load() -> bool {
		std::ifstream ifs{ ConfigPath };
		if (!ifs.is_open()) { Status_ = "No config file (using defaults)."; return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { Status_ = "Config is not valid JSON."; return false; }
		Reader r{ root };
		Visit(r, Look);
		Status_ = "Loaded.";
		return true;
	}

	auto Presenter::Save() -> bool {
		JSON root = JSON::object();
		Writer w{ root };
		Visit(w, Look);
		std::ofstream ofs{ ConfigPath, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { Status_ = "Save failed (cannot open file)."; return false; }
		ofs << root.dump(2) << '\n';
		if (!ofs.good()) { Status_ = "Save failed (write error)."; return false; }
		Status_ = "Saved.";
		return true;
	}

	//====	======	======	======	======	====//
	//	エディタ（Debug のみ）					//
	//====	======	======	======	======	====//

#if defined(_DEBUG)
	namespace {
		void EditTrail(char const* label_, TrailStyle& s_) {
			if (!ImGui::TreeNode(label_)) { return; }
			ImGui::Checkbox("Enabled", &s_.Enabled);
			ImGui::DragFloat("Blade base (local Y)", &s_.BladeBase, 0.01f, -2.0f, 10.0f, "%.3f");
			ImGui::DragFloat("Blade tip (local Y)", &s_.BladeTip, 0.01f, -2.0f, 10.0f, "%.3f");
			ImGui::SliderInt("Lifetime (frames)", &s_.Lifetime, 1, 30);
			ImGui::SliderInt("Subdivide", &s_.Subdivide, 1, 12);
			ImGui::DragFloat("Min speed", &s_.MinSpeed, 0.005f, 0.0f, 5.0f, "%.3f");
			ImGui::DragFloat("Full speed", &s_.FullSpeed, 0.005f, 0.0f, 5.0f, "%.3f");
			ImGui::ColorEdit3("Head (sRGB)", &s_.HeadColor.x);
			ImGui::ColorEdit3("Tail (sRGB)", &s_.TailColor.x);
			ImGui::DragFloat("Intensity", &s_.Intensity, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("Age falloff", &s_.AgeFalloff, 0.01f, 0.05f, 8.0f);
			ImGui::DragFloat("Tip bias", &s_.TipBias, 0.01f, 0.05f, 8.0f);
			ImGui::DragFloat("Core white", &s_.CoreWhite, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Streak", &s_.Streak, 0.01f, 0.0f, 1.0f);
			ImGui::TreePop();
		}

		void EditBurst(char const* label_, BurstStyle& s_) {
			if (!ImGui::TreeNode(label_)) { return; }
			ImGui::Checkbox("Enabled", &s_.Enabled);
			ImGui::ColorEdit3("Color (sRGB)", &s_.Color.x);
			ImGui::SliderInt("Life (frames)", &s_.Life, 1, 90);
			ImGui::DragFloat("Ring size", &s_.RingSize, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Glow size", &s_.GlowSize, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Star size", &s_.StarSize, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Ground ring", &s_.GroundRing, 0.05f, 0.0f, 30.0f);
			ImGui::DragFloat("Intensity", &s_.Intensity, 0.01f, 0.0f, 10.0f);
			ImGui::SliderInt("Sparks", &s_.Sparks, 0, 64);
			ImGui::DragFloat("Spark speed", &s_.SparkSpeed, 0.01f, 0.0f, 3.0f);
			ImGui::DragFloat("Light intensity", &s_.LightIntensity, 1.0f, 0.0f, 1000.0f);
			ImGui::TreePop();
		}
	}
#endif

	void Presenter::DrawEditor() {
#if defined(_DEBUG)
		if (!ImGui::Begin("Combat VFX")) {
			ImGui::End();
			return;
		}

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { Look = Style{}; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		ImGui::SeparatorText("Status");
		ImGui::Text("Mesh vertices %u / %u (dropped tris %u)", Mesh_.VertexCount(), Mesh_.MaxVertices(), Mesh_.DroppedTriangles());
		ImGui::Text("Lights %u   Bursts %u (total %u)   Glints %u", static_cast<std::uint32_t>(Lights_.size()),
			static_cast<std::uint32_t>(Bursts_.size()), NumBurstsTotal_, static_cast<std::uint32_t>(Glints_.size()));
		ImGui::Text("Ward  player %.2f / deflect %.2f   boss %.2f", PlayerWard_.Level, PlayerWard_.Deflect, BossWard_.Level);
		ImGui::Text("Telegraph  slash %s (fill %.2f)   wave %s (fill %.2f)   Stun  player %.2f / boss %.2f",
			SlashLane_.Active ? "on" : "-", SlashLane_.Fill, WaveLanes_.Active ? "on" : "-", WaveLanes_.Fill, PlayerStun_.Level, BossStun_.Level);

		ImGui::SeparatorText("Test (spawns at the player)");
		{
			auto const test{ [&](Game::CombatEvents::Outcome r_, Game::CombatEvents::Side v_) {
				FighterView const& f{ (v_ == Game::CombatEvents::Side::Player) ? LastInput_.Player : LastInput_.Boss };
				Game::CombatEvents::Hit e{};
				e.Victim = v_;
				e.Result = r_;
				e.Point = { f.Visual.x, f.Visual.y + 1.5f, f.Visual.z };
				e.Dir = { std::cos(f.Angle + Pi), std::sin(f.Angle + Pi) };
				OnHit(this, e);
			} };
			using Game::CombatEvents::Outcome;
			using Game::CombatEvents::Side;
			if (ImGui::Button("Player hit")) { test(Outcome::Hit, Side::Player); }
			ImGui::SameLine();
			if (ImGui::Button("Boss hit")) { test(Outcome::Hit, Side::Boss); }
			ImGui::SameLine();
			if (ImGui::Button("Guard")) { test(Outcome::Guarded, Side::Player); }
			ImGui::SameLine();
			if (ImGui::Button("Deflect")) { test(Outcome::Deflected, Side::Player); }
			ImGui::SameLine();
			if (ImGui::Button("Dodge")) { test(Outcome::Dodged, Side::Player); }
		}

		if (ImGui::CollapsingHeader("Weapon trails")) {
			EditTrail("Player", Look.PlayerTrail);
			EditTrail("Boss", Look.BossTrail);
		}

		if (ImGui::CollapsingHeader("Guard ward")) {
			WardStyle& s{ Look.Ward };
			ImGui::Checkbox("Enabled##Ward", &s.Enabled);
			ImGui::DragFloat("Radius", &s.Radius, 0.01f, 0.1f, 10.0f);
			ImGui::DragFloat("Bottom", &s.Bottom, 0.01f, -2.0f, 10.0f);
			ImGui::DragFloat("Top", &s.Top, 0.01f, 0.0f, 15.0f);
			ImGui::SliderInt("Segments", &s.Segments, 4, 64);
			ImGui::ColorEdit3("Color (sRGB)##Ward", &s.Color.x);
			ImGui::ColorEdit3("Deflect color (sRGB)", &s.DeflectColor.x);
			ImGui::DragFloat("Intensity##Ward", &s.Intensity, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Deflect intensity", &s.DeflectIntensity, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Rise rate", &s.RiseRate, 0.005f, 0.0f, 1.0f);
			ImGui::DragFloat("Fall rate", &s.FallRate, 0.005f, 0.0f, 1.0f);
			ImGui::DragFloat("Flash decay", &s.FlashDecay, 0.005f, 0.0f, 0.99f);
		}

		if (ImGui::CollapsingHeader("Impacts")) {
			EditBurst("Boss -> Player : Hit", Look.Impact.PlayerHit);
			EditBurst("Player -> Boss : Hit", Look.Impact.BossHit);
			EditBurst("Guarded", Look.Impact.Guard);
			EditBurst("Deflected", Look.Impact.Deflect);
			EditBurst("Dodged (i-frames)", Look.Impact.Dodge);
		}

		if (ImGui::CollapsingHeader("Telegraphs (boss skills)")) {
			TelegraphStyle& s{ Look.Telegraph };
			ImGui::Checkbox("Enabled##Tele", &s.Enabled);
			ImGui::ColorEdit3("Charge slash (sRGB)", &s.SlashColor.x);
			ImGui::ColorEdit3("Thorn wave (sRGB)", &s.WaveColor.x);
			ImGui::DragFloat("Decal height", &s.DecalHeight, 0.01f, -1.0f, 3.0f);
			ImGui::DragFloat("Decal depth bias", &s.DecalDepthBias, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("Slash lane width", &s.SlashWidth, 0.01f, 0.1f, 20.0f);
			ImGui::DragFloat("Slash reach", &s.SlashReach, 0.01f, 0.0f, 20.0f);
			ImGui::DragFloat("Lane intensity", &s.LaneIntensity, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Fill alpha", &s.FillAlpha, 0.005f, 0.0f, 2.0f);
			ImGui::SliderInt("Fade frames", &s.FadeFrames, 1, 60);
			ImGui::DragFloat("Ground ring radius", &s.GroundRingRadius, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Glint size", &s.GlintSize, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("Glint intensity", &s.GlintIntensity, 0.01f, 0.0f, 10.0f);
			ImGui::SliderInt("Glint frames", &s.GlintFrames, 1, 60);
			ImGui::SliderInt("Converge rate", &s.ConvergeRate, 0, 16);
			ImGui::DragFloat("Converge radius", &s.ConvergeRadius, 0.05f, 0.0f, 20.0f);
			ImGui::SliderInt("Petal rate", &s.PetalRate, 0, 16);
			ImGui::DragFloat("Petal radius", &s.PetalRadius, 0.05f, 0.0f, 30.0f);
		}

		if (ImGui::CollapsingHeader("Footsteps")) {
			FootstepStyle& s{ Look.Footsteps };
			ImGui::Checkbox("Enabled##Steps", &s.Enabled);
			ImGui::DragFloat("Step distance", &s.StepDistance, 0.01f, 0.1f, 10.0f);
			ImGui::SliderInt("Per step", &s.PerStep, 0, 16);
			ImGui::DragFloat("Foot height", &s.FootHeight, 0.01f, -2.0f, 5.0f);
			ImGui::DragFloat("Spread", &s.Spread, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Size##Steps", &s.Size, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("Rise", &s.Rise, 0.001f, -0.2f, 0.5f, "%.3f");
			ImGui::SliderInt("Life##Steps", &s.Life, 1, 240);
			ImGui::DragFloat("Alpha##Steps", &s.Alpha, 0.005f, 0.0f, 1.0f);
			ImGui::ColorEdit3("Player color", &s.PlayerColor.x);
			ImGui::ColorEdit3("Boss color", &s.BossColor.x);
			ImGui::DragFloat("Light chance", &s.LightChance, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Light scale", &s.LightScale, 0.5f, 0.0f, 1000.0f);
		}

		if (ImGui::CollapsingHeader("Stun stars")) {
			StunStyle& s{ Look.Stun };
			ImGui::Checkbox("Enabled##Stun", &s.Enabled);
			ImGui::SliderInt("Count", &s.Count, 1, 12);
			ImGui::DragFloat("Height##Stun", &s.Height, 0.01f, 0.0f, 15.0f);
			ImGui::DragFloat("Radius##Stun", &s.Radius, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Size##Stun", &s.Size, 0.005f, 0.0f, 3.0f);
			ImGui::DragFloat("Spin speed", &s.SpinSpeed, 0.002f, -1.0f, 1.0f);
			ImGui::DragFloat("Bob", &s.Bob, 0.005f, 0.0f, 2.0f);
			ImGui::ColorEdit3("Color (sRGB)##Stun", &s.Color.x);
			ImGui::DragFloat("Intensity##Stun", &s.Intensity, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("Glow##Stun", &s.Glow, 0.01f, 0.0f, 3.0f);
			ImGui::DragFloat("Rise rate##Stun", &s.RiseRate, 0.005f, 0.0f, 1.0f);
			ImGui::DragFloat("Fall rate##Stun", &s.FallRate, 0.005f, 0.0f, 1.0f);
			ImGui::TextDisabled("Test: hit the boss, or use \"Player hit\" above while the boss attacks");
		}

		if (ImGui::CollapsingHeader("Boss aura / dash")) {
			AuraStyle& s{ Look.BossAura };
			ImGui::Checkbox("Enabled##Aura", &s.Enabled);
			ImGui::DragFloat("Hue center", &s.HueCenter, 0.5f, 0.0f, 360.0f);
			ImGui::DragFloat("Hue range", &s.HueRange, 0.5f, 0.0f, 360.0f);
			ImGui::DragFloat("Saturation", &s.Saturation, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Alpha##Aura", &s.Alpha, 0.005f, 0.0f, 1.0f);
			ImGui::DragFloat("Size##Aura", &s.Size, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("Light scale##Aura", &s.LightScale, 0.5f, 0.0f, 1000.0f);
			ImGui::Checkbox("Dash particles", &s.Dash);
			ImGui::DragFloat("Dash size", &s.DashSize, 0.01f, 0.0f, 10.0f);
		}

		ImGui::End();
#endif
	}
}
