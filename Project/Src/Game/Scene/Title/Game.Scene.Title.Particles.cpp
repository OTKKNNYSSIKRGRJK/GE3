module Game.Scene.Title;

import <cstdint>;
import <cmath>;
import <numbers>;
import <algorithm>;
import <random>;
import <memory>;
import <vector>;
import <utility>;
import <d3d12.h>;

import nlohmann.json;

import : Impl;

import Lumina;
import Lumina.MagicCircle;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

//////	//////	//////	//////	//////	//////
//	Title のパーティクル					//
//////	//////	//////	//////	//////	//////
// 本編の ParticleSystem<T> / BasicParticle.VS / ParticleSystem.json の "Common RS" をそのまま使う。
// PS だけタイトル用（本編の PS は G-buffer の深度テクスチャで手動遮蔽しているが、
// タイトルにはそのテクスチャが無いので、ハードウェアの深度テストに置き換えた）。
//
// Sparkles / Aura … 本編の AmbientSparkles / PlayerEffects の移植（フレーム単位の係数 × 経過フレーム数）
// Embers          … 火の粉（秒単位）
// CircleLights    … 魔法陣のリングから立ち昇る光（秒単位）
// WeaponGlow      … 武器の軸に沿って置く、動かない大きめの光（秒単位）。Outro 中は Outro の WeaponGlow 設定で強める
// WeaponFlare     … Outro 中だけ、武器の先端に置く四芒星のフレア（毎フレーム 1 枚だけ置き直す）

namespace Game::Scene::Impl {
	namespace {
		constexpr float Pi{ std::numbers::pi_v<float> };
		constexpr float TwoPi{ Pi * 2.0f };
		constexpr float RadToDeg{ 180.0f * std::numbers::inv_pi_v<float> };

		std::mt19937& Rng() {
			static std::mt19937 rng{ 20261006U };
			return rng;
		}
		float Rand01() {
			return static_cast<float>(Rng()() >> 8U) * (1.0f / 16777216.0f);
		}
		float RandSigned() {
			return Rand01() * 2.0f - 1.0f;
		}

		float Saturate(float x_) {
			return std::clamp(x_, 0.0f, 1.0f);
		}
		float SRGBToLinear(float c_) {
			return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
		}
		Lumina::Float3 SRGBToLinear(Lumina::Float3 const& c_) {
			return { SRGBToLinear(c_.x), SRGBToLinear(c_.y), SRGBToLinear(c_.z) };
		}
		Lumina::Float3 Lerp3(Lumina::Float3 const& a_, Lumina::Float3 const& b_, float t_) {
			return {
				a_.x + (b_.x - a_.x) * t_,
				a_.y + (b_.y - a_.y) * t_,
				a_.z + (b_.z - a_.z) * t_,
			};
		}
		float Dot3(Lumina::Float3 const& a_, Lumina::Float3 const& b_) {
			return a_.x * b_.x + a_.y * b_.y + a_.z * b_.z;
		}

		// 端数を持ち越しつつ、このフレームの発生数を返す
		int SpawnCount(float rate_, float deltaTime_, float& accum_) {
			accum_ += std::max(rate_, 0.0f) * deltaTime_;
			int const count{ static_cast<int>(accum_) };
			accum_ -= static_cast<float>(count);
			return std::min(count, 256);	// 一時停止明けなどで溜まりすぎないように
		}

		uint32_t ClampAtlas(int atlas_) {
			// BasicParticle.VS は AtlasID & 7 を列、>> 3 を行として扱う。Particles.png は 1 行なので 0..7
			return static_cast<uint32_t>(std::clamp(atlas_, 0, 7));
		}

		//----	------	------	------	------	----//
		// Update 関数（ParticleSystem には関数ポインタで渡すので、共通データは引数経由）
		//----	------	------	------	------	----//

		struct UpdateContext {
			float DeltaTime;
			float Frames;					// DeltaTime × 60（本編のフレーム単位の係数用）
			Lumina::Float3 CameraRight;
			Lumina::Float3 CameraUp;
			Lumina::Float3 Center;			// キャラの足元
			void const* Params;				// 各エミッタのパラメータ
		};

		// 本編 UpdateAmbientSparkle の移植（Life はフレーム単位、60 → 0）
		bool UpdateSparkle(TitleParticle& p_, void const* context_) {
			auto const& ctx{ *static_cast<UpdateContext const*>(context_) };
			float const frames{ ctx.Frames };

			p_.Translate.x += (Rand01() - 0.5f) * 0.05f * frames;
			p_.Translate.y += Rand01() * 0.05f * frames;
			p_.Translate.z += (Rand01() - 0.5f) * 0.025f * frames;

			float const k{ 1.0f - (p_.Life - 30.0f) * (p_.Life - 30.0f) * (1.0f / 900.0f) };
			p_.RenderData.RGBA.w = k * p_.BaseAlpha;
			p_.Scale.x = k * p_.BaseSize;
			p_.Scale.y = k * p_.BaseSize;

			p_.Life -= 0.5f * frames;
			return (p_.Life > 0.0f);
		}

		// 本編 UpdatePlayerEffect の移植（Life はフレーム単位、64 → 0）
		bool UpdateAura(TitleParticle& p_, void const* context_) {
			auto const& ctx{ *static_cast<UpdateContext const*>(context_) };
			float const frames{ ctx.Frames };

			p_.Translate.x += p_.Velocity.x * frames;
			p_.Translate.z += p_.Velocity.z * frames;
			p_.RenderData.RGBA.w *= std::pow(0.95f, frames);
			p_.Scale.x *= std::pow(0.93f, frames);
			p_.Scale.y *= std::pow(0.93f, frames);
			p_.Rotate.z += p_.Velocity.z * 0.01f * frames;

			p_.Life -= frames;
			return (p_.Life > 0.0f);
		}

		bool UpdateEmber(TitleParticle& p_, void const* context_) {
			auto const& ctx{ *static_cast<UpdateContext const*>(context_) };
			auto const& prm{ *static_cast<EmberEmitter const*>(ctx.Params) };
			float const dt{ ctx.DeltaTime };

			// 乱流（横方向のランダムな加速）と減衰
			p_.Velocity.x += RandSigned() * prm.Turbulence * dt;
			p_.Velocity.z += RandSigned() * prm.Turbulence * dt;
			float const drag{ std::exp(-prm.Drag * dt) };
			p_.Velocity.x *= drag;
			p_.Velocity.z *= drag;

			// 軸まわりの旋回（位置そのものを回す）
			float const rx{ p_.Translate.x - ctx.Center.x };
			float const rz{ p_.Translate.z - ctx.Center.z };
			float const swirlAngle{ prm.Swirl * dt };
			float const s{ std::sin(swirlAngle) };
			float const c{ std::cos(swirlAngle) };
			p_.Translate.x = ctx.Center.x + (rx * c - rz * s) + p_.Velocity.x * dt;
			p_.Translate.z = ctx.Center.z + (rx * s + rz * c) + p_.Velocity.z * dt;
			p_.Translate.y += p_.Velocity.y * dt;

			p_.Life -= dt;
			float const age{ Saturate(1.0f - p_.Life / p_.MaxLife) };

			// 色：熱い → 冷める、フェード：出だしは素早く、消え際はゆっくり
			Lumina::Float3 const color{ Lerp3(p_.ColorA, p_.ColorB, age) };
			float const alpha{ p_.BaseAlpha * Saturate(age / 0.08f) * std::pow(1.0f - age, 1.5f) };
			p_.RenderData.RGBA = { color.x, color.y, color.z, alpha };

			// 画面上の進行方向（旋回の接線速度を含む）に沿って縦長に伸ばす
			Lumina::Float3 const velocity{
				p_.Velocity.x - rz * prm.Swirl,
				p_.Velocity.y,
				p_.Velocity.z + rx * prm.Swirl,
			};
			float const sx{ Dot3(velocity, ctx.CameraRight) };
			float const sy{ Dot3(velocity, ctx.CameraUp) };
			float const screenSpeed{ std::sqrt(sx * sx + sy * sy) };
			if (screenSpeed > 1e-4f) {
				// ローカル y 軸（筋の向き）を (sx, sy) に合わせる
				p_.Rotate.z = std::atan2(-sx, sy);
			}
			float const stretch{
				1.0f + (std::max(prm.Stretch, 1.0f) - 1.0f) * Saturate(screenSpeed / std::max(prm.RiseSpeed, 0.01f))
			};
			float const size{ p_.BaseSize * (1.0f - 0.5f * age) };
			p_.Scale.x = size;
			p_.Scale.y = size * stretch;

			return (p_.Life > 0.0f);
		}

		bool UpdateCircleLight(TitleParticle& p_, void const* context_) {
			auto const& ctx{ *static_cast<UpdateContext const*>(context_) };
			auto const& prm{ *static_cast<CircleLightEmitter const*>(ctx.Params) };
			float const dt{ ctx.DeltaTime };

			p_.Life -= dt;
			float const elapsed{ p_.MaxLife - p_.Life };
			float const age{ Saturate(1.0f - p_.Life / p_.MaxLife) };

			// ゆっくり昇りながら横に揺れる（揺れの向きは粒ごと）
			float const swaySpeed{ std::cos(elapsed * 2.0f + p_.Seed) * prm.Sway };
			p_.Translate.x += std::cos(p_.Seed) * swaySpeed * dt;
			p_.Translate.z += std::sin(p_.Seed) * swaySpeed * dt;
			p_.Translate.y += p_.Velocity.y * dt;

			Lumina::Float3 const color{ Lerp3(p_.ColorA, p_.ColorB, age) };
			float const alpha{ p_.BaseAlpha * Saturate(age / 0.15f) * Saturate((1.0f - age) / 0.4f) };
			p_.RenderData.RGBA = { color.x, color.y, color.z, alpha };

			// ワールドの上方向が画面上でどちらを向くかに合わせて回す（カメラが傾いても縦の筋になる）
			float const sx{ ctx.CameraRight.y };
			float const sy{ ctx.CameraUp.y };
			p_.Rotate.z = std::atan2(-sx, sy);
			p_.Scale.x = p_.BaseSize;
			p_.Scale.y = p_.BaseSize * std::max(prm.Stretch, 1.0f);

			return (p_.Life > 0.0f);
		}

		// 動かない。寿命の中央で一番明るく（sin の山）、少しずつ大きくなる
		bool UpdateWeaponGlow(TitleParticle& p_, void const* context_) {
			auto const& ctx{ *static_cast<UpdateContext const*>(context_) };
			auto const& prm{ *static_cast<WeaponGlowEmitter const*>(ctx.Params) };

			p_.Life -= ctx.DeltaTime;
			float const age{ Saturate(1.0f - p_.Life / p_.MaxLife) };

			p_.RenderData.RGBA.w = p_.BaseAlpha * std::sin(Pi * age);
			float const size{ p_.BaseSize * (1.0f + prm.Grow * age) };
			p_.Scale.x = size;
			p_.Scale.y = size;

			return (p_.Life > 0.0f);
		}

		// 何もしない（毎フレーム Clear してから 1 枚だけ置き直すので、寿命で消す必要がない）
		bool UpdateWeaponFlare(TitleParticle&, void const*) {
			return true;
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Initialize								//
	//////	//////	//////	//////	//////	//////

	void Title::InitializeParticles(
		Lumina::DX12::Context const& dxContext_,
		Lumina::AssetManager& assetMngr_
	) {
		auto const& device{ dxContext_.Device() };

		// * Root signature：本編と同じ "Common RS"
		{
			auto config{ Lumina::Utils::LoadFromFile<nlohmann::json>("Assets/Configs/ParticleSystem.json") };
			RS_Particle_.Initialize(
				device,
				Lumina::DX12::LoadRootSignatureSetup(config.at("Common RS")),
				"Title Particle RS"
			);
		}

		// * Shaders：VS は本編のまま、PS はタイトル用（手動の深度比較なし）
		dxContext_.Compile(
			VS_Particle_,
			L"Assets/Shaders/BasicParticle.VS.hlsl",
			L"vs_6_6",
			L"main",
			"Title.Particle.VS"
		);
		dxContext_.Compile(
			PS_Particle_,
			L"Assets/Title/TitleParticle.PS.hlsl",
			L"ps_6_6",
			L"main",
			"Title.Particle.PS"
		);

		// * PSO：本編と同じ加算（SrcAlpha, One）＋ 深度テストのみ
		{
			Lumina::DX12::BlendState blendState{};
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_ONE },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_SRC_ALPHA },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			Lumina::DX12::GraphicsPSO::InputLayout inputLayout{};
			inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);
			inputLayout.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);

			PSO_Particle_.Initialize(
				device,
				RS_Particle_,
				VS_Particle_,
				PS_Particle_,
				blendState,
				Lumina::DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_NONE },
					.FrontCounterClockwise{ false },
					.DepthClipEnable{ true },
				},
				Lumina::DX12::DepthStencilState{
					.DepthEnable{ true },
					.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ZERO },
					.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
					.StencilEnable{ false },
				},
				inputLayout,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, },
				Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
				"Title Particle PSO"
			);
		}

		// * Texture：本編と同じ Particles.png（InGame とは別名で読み込む）
		//   RS の Textures は 32 枠・Textures2 は 8 枠なので、32 枠確保して両方に同じテーブルを渡す
		{
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "Title.Particles", "Assets/Img/Particles.png" },
				}
			);
			GlobalTable_ParticleTextures_ = dxContext_.GlobalDescriptorHeap().Allocate(32U);
			device->CopyDescriptorsSimple(
				1U,
				GlobalTable_ParticleTextures_.CPUHandle(0U),
				assetMngr_.Graphics().CPUHandle(texIDs.at(0U)),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
		}

		// * Scene constants（ParticleSystem::Render がローカルの CBV をコピーしていく）
		UB_ParticleViewToWorld_.Initialize(device, 256LLU, "Title Particle ViewToWorld");
		UB_ParticleVP_.Initialize(device, 256LLU, "Title Particle VP");
		LocalHeap_Particle_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2U, false);
		Lumina::DX12::CBV::Create(device, LocalHeap_Particle_.CPUHandle(0U), UB_ParticleViewToWorld_);
		Lumina::DX12::CBV::Create(device, LocalHeap_Particle_.CPUHandle(1U), UB_ParticleVP_);

		// * Systems
		Sparkles_ = std::make_unique<ParticleSystem<TitleParticle>>();
		Sparkles_->Initialize(dxContext_, 256U);
		Aura_ = std::make_unique<ParticleSystem<TitleParticle>>();
		Aura_->Initialize(dxContext_, 512U);
		Embers_ = std::make_unique<ParticleSystem<TitleParticle>>();
		Embers_->Initialize(dxContext_, 1024U);
		CircleLights_ = std::make_unique<ParticleSystem<TitleParticle>>();
		CircleLights_->Initialize(dxContext_, 1024U);
		WeaponGlow_ = std::make_unique<ParticleSystem<TitleParticle>>();
		WeaponGlow_->Initialize(dxContext_, 512U);
		WeaponFlare_ = std::make_unique<ParticleSystem<TitleParticle>>();
		WeaponFlare_->Initialize(dxContext_, 4U);
	}

	//////	//////	//////	//////	//////	//////
	//	Update									//
	//////	//////	//////	//////	//////	//////

	void Title::UpdateParticles(
		Lumina::DX12::CommandList const& cmdList_,
		float deltaTime_,
		Lumina::Mat4 const& view_,
		Lumina::Mat4 const& viewProj_
	) {
		Lumina::Mat4 const viewToWorld{ view_.Inv() };
		UB_ParticleViewToWorld_.Store(&viewToWorld, sizeof(Lumina::Mat4), 0LLU);
		UB_ParticleVP_.Store(&viewProj_, sizeof(Lumina::Mat4), 0LLU);

		auto const& center{ Character_.Position };
		float const frames{ deltaTime_ * 60.0f };

		UpdateContext ctx{
			.DeltaTime{ deltaTime_ },
			.Frames{ frames },
			// 行ベクトル規約なので、ViewToWorld の 0 行目 = カメラの右、1 行目 = カメラの上
			.CameraRight{ viewToWorld[0][0], viewToWorld[0][1], viewToWorld[0][2] },
			.CameraUp{ viewToWorld[1][0], viewToWorld[1][1], viewToWorld[1][2] },
			.Center{ center },
			.Params{ nullptr },
		};

		//----	------	------	------	------	----//
		// Sparkles（本編 Ambient sparkles の発生処理と同じ式）

		SparkleTimeFactor_ += 0.75f * frames;
		if (SparkleParams_.Enabled) {
			auto const& prm{ SparkleParams_ };
			int const count{ SpawnCount(prm.Rate, deltaTime_, SparkleAccum_) };
			for (int i{ 0 }; i < count; ++i) {
				float const radius{ Rand01() * prm.AreaRadius };
				float const theta{ Rand01() * TwoPi };

				TitleParticle p{};
				p.Translate = {
					center.x + radius * std::cos(theta),
					center.y + Rand01() * prm.HeightMax,
					center.z + radius * std::sin(theta),
				};
				p.Scale = { 1.0f, 1.0f, 1.0f };
				p.Life = 60.0f;
				p.BaseSize = prm.Size;
				p.BaseAlpha = prm.Alpha;

				auto const rgb{
					Lumina::Utils::Color::Convert(
						Lumina::Utils::Color::HSV{ Rand01() * 45.0f, Rand01() * 0.5f + 0.5f, 0.75f }
					)
				};
				auto const rgb_Gaming{
					Lumina::Utils::Color::Convert(
						Lumina::Utils::Color::HSV{
							std::fmod(Rand01() * 45.0f + SparkleTimeFactor_ + theta * RadToDeg, 360.0f),
							Rand01() * 0.3f + 0.5f,
							0.95f
						}
					)
				};
				p.RenderData.RGBA = {
					rgb.R * (0.7f + rgb_Gaming.R * 0.3f),
					rgb.G * (0.7f + rgb_Gaming.G * 0.3f),
					rgb.B * (0.7f + rgb_Gaming.B * 0.3f),
					0.0f
				};
				p.RenderData.DiffuseID = 0U;
				p.RenderData.DiffuseAtlasID = 5U;
				Sparkles_->Emit(std::move(p));
			}
		}
		ctx.Params = &SparkleParams_;
		Sparkles_->Update(cmdList_, viewToWorld, UpdateSparkle, &ctx);

		//----	------	------	------	------	----//
		// Aura（本編 Player effects の発生処理と同じ式。手元の 2 粒は省略）

		AuraTimeFactor_ += 0.5f * frames;
		if (AuraParams_.Enabled) {
			auto const& prm{ AuraParams_ };
			float const t{ AuraTimeFactor_ };

			auto const rgb_Gaming{
				Lumina::Utils::Color::Convert(
					Lumina::Utils::Color::HSV{
						std::fmod(Rand01() * 60.0f + t + t * 0.1f * RadToDeg, 360.0f),
						Rand01() * 0.3f + 0.5f,
						0.95f
					}
				)
			};
			Lumina::Float3 const fixedColor{ SRGBToLinear(prm.Color) };

			int const count{ SpawnCount(prm.Rate, deltaTime_, AuraAccum_) };
			for (int n{ 0 }; n < count; ++n) {
				float const i{ static_cast<float>(AuraSpawnIndex_) };
				AuraSpawnIndex_ ^= 1;

				TitleParticle p{};
				p.Translate = {
					std::cos(t * 0.3f + i * 3.6f) * prm.OrbitRadius,
					std::sin(t * 0.4f * i) * prm.OrbitHeight,
					std::sin(t * 0.5f - i * 1.2f) * prm.OrbitRadius,
				};
				p.Velocity.x = p.Translate.z * (-0.05f);
				p.Velocity.z = p.Translate.x * (-0.05f);

				p.Translate.x += center.x;
				p.Translate.y += center.y + prm.HeightOffset;
				p.Translate.z += center.z;

				p.Scale = { prm.Size, prm.Size, 1.0f };
				p.Rotate.z = Rand01() * TwoPi;
				p.Life = 64.0f;

				if (prm.HueCycle) {
					p.RenderData.RGBA = {
						rgb_Gaming.R + Rand01() * 0.1f,
						rgb_Gaming.G + Rand01() * 0.1f,
						rgb_Gaming.B,
						prm.Alpha
					};
				}
				else {
					p.RenderData.RGBA = { fixedColor.x, fixedColor.y, fixedColor.z, prm.Alpha };
				}
				p.RenderData.DiffuseID = 0U;
				p.RenderData.DiffuseAtlasID = (Rand01() < 0.75f) ? 3U : 4U;
				Aura_->Emit(std::move(p));
			}
		}
		ctx.Params = &AuraParams_;
		Aura_->Update(cmdList_, viewToWorld, UpdateAura, &ctx);

		//----	------	------	------	------	----//
		// Embers

		if (EmberParams_.Enabled) {
			auto const& prm{ EmberParams_ };
			Lumina::Float3 const colorHot{ SRGBToLinear(prm.ColorHot) };
			Lumina::Float3 const colorCool{ SRGBToLinear(prm.ColorCool) };
			float const lifeMin{ std::max(std::min(prm.LifeMin, prm.LifeMax), 0.05f) };
			float const lifeMax{ std::max(prm.LifeMax, lifeMin) };

			int const count{ SpawnCount(prm.Rate, deltaTime_, EmberAccum_) };
			for (int i{ 0 }; i < count; ++i) {
				float const angle{ Rand01() * TwoPi };
				float const radius{ std::max(prm.SpawnRadius + RandSigned() * prm.SpawnRadiusJitter, 0.0f) };
				float const life{ lifeMin + (lifeMax - lifeMin) * Rand01() };

				TitleParticle p{};
				p.Translate = {
					center.x + radius * std::cos(angle),
					center.y + Rand01() * prm.SpawnHeight,
					center.z + radius * std::sin(angle),
				};
				p.Velocity = { 0.0f, std::max(prm.RiseSpeed + RandSigned() * prm.RiseJitter, 0.1f), 0.0f };
				p.Life = life;
				p.MaxLife = life;
				p.BaseSize = prm.Size * (0.6f + 0.8f * Rand01());
				p.BaseAlpha = prm.Alpha;
				p.ColorA = colorHot;
				p.ColorB = colorCool;
				p.Scale = { p.BaseSize, p.BaseSize, 1.0f };
				p.RenderData.RGBA = { colorHot.x, colorHot.y, colorHot.z, 0.0f };
				p.RenderData.DiffuseID = 0U;
				p.RenderData.DiffuseAtlasID = ClampAtlas(prm.Atlas);
				Embers_->Emit(std::move(p));
			}
		}
		ctx.Params = &EmberParams_;
		Embers_->Update(cmdList_, viewToWorld, UpdateEmber, &ctx);

		//----	------	------	------	------	----//
		// CircleLights：有効なレイヤーを円周の長さで重み付けして選び、リング上から出す

		if (CircleLightParams_.Enabled) {
			auto const& prm{ CircleLightParams_ };
			float const lifeMin{ std::max(std::min(prm.LifeMin, prm.LifeMax), 0.05f) };
			float const lifeMax{ std::max(prm.LifeMax, lifeMin) };

			float totalWeight{ 0.0f };
			for (auto const& layer : MagicCircleRenderLayers_) {
				if (layer.Enabled) { totalWeight += std::max(layer.Radius, 0.0f); }
			}

			int const count{ SpawnCount(prm.Rate, deltaTime_, CircleLightAccum_) };
			for (int i{ 0 }; (i < count) && (totalWeight > 0.0f); ++i) {
				// レイヤーを選ぶ
				float pick{ Rand01() * totalWeight };
				Lumina::MagicCircle::Layer const* layer{ nullptr };
				for (auto const& candidate : MagicCircleRenderLayers_) {
					if (!candidate.Enabled) { continue; }
					layer = &candidate;
					pick -= std::max(candidate.Radius, 0.0f);
					if (pick <= 0.0f) { break; }
				}
				if (layer == nullptr) { break; }

				// リングを選ぶ（どちらのリングも無効ならほぼ外周）
				using MC = Lumina::MagicCircle;
				float rings[2]{};
				int num_Rings{ 0 };
				if ((layer->Elements & MC::Element_Ring0) != 0U) { rings[num_Rings++] = layer->Ring0; }
				if ((layer->Elements & MC::Element_Ring1) != 0U) { rings[num_Rings++] = layer->Ring1; }
				float const ring{
					(num_Rings == 0) ? 0.98f :
					rings[std::min(static_cast<int>(Rand01() * static_cast<float>(num_Rings)), num_Rings - 1)]
				};

				float const angle{ Rand01() * TwoPi };

				// 描き込み途中なら、描かれている範囲からだけ出す（MagicCircle.hlsl と同じ角度の取り方）
				if (prm.RespectReveal && layer->Reveal < 0.999f) {
					float const layerAngle{ angle - (layer->Phase + layer->RotateSpeed * Time_) };
					float const t{ (Pi * 0.5f - layerAngle) / TwoPi };
					if (t - std::floor(t) > layer->Reveal) { continue; }
				}

				float const radius{ layer->Radius * ring };
				float const life{ lifeMin + (lifeMax - lifeMin) * Rand01() };
				Lumina::Float3 const baseColor{ SRGBToLinear(layer->Color) };

				TitleParticle p{};
				p.Translate = {
					center.x + radius * std::cos(angle),
					center.y + layer->Height,
					center.z + radius * std::sin(angle),
				};
				p.Velocity = { 0.0f, std::max(prm.RiseSpeed + RandSigned() * prm.RiseJitter, 0.05f), 0.0f };
				p.Life = life;
				p.MaxLife = life;
				p.Seed = Rand01() * TwoPi;
				p.BaseSize = prm.Size * (0.7f + 0.6f * Rand01());
				p.BaseAlpha = prm.Alpha * std::clamp(layer->Intensity, 0.0f, 2.0f);
				p.ColorA = Lerp3(baseColor, Lumina::Float3{ 1.0f, 1.0f, 1.0f }, Saturate(prm.Whiten));
				p.ColorB = baseColor;
				p.Scale = { p.BaseSize, p.BaseSize * prm.Stretch, 1.0f };
				p.RenderData.RGBA = { p.ColorA.x, p.ColorA.y, p.ColorA.z, 0.0f };
				p.RenderData.DiffuseID = 0U;
				p.RenderData.DiffuseAtlasID = ClampAtlas(prm.Atlas);
				CircleLights_->Emit(std::move(p));
			}
		}
		ctx.Params = &CircleLightParams_;
		CircleLights_->Update(cmdList_, viewToWorld, UpdateCircleLight, &ctx);

		//----	------	------	------	------	----//
		// WeaponGlow：武器の軸上のランダムな点＋軸に垂直な方向へ散らす（Character_.Advance の後なのでこのフレームの姿勢）
		// Outro 中は、調整値のコピーに Outro の強め方を掛けて使う（元の調整値は書き換えない）

		float const glowLevel{ OutroWeaponGlowLevel() };	// 0..1（Outro 外は 0）
		auto const& outroGlow{ Outro_.WeaponGlow };
		WeaponGlowEmitter glowParams{ WeaponGlowParams_ };
		glowParams.Rate *= std::lerp(1.0f, outroGlow.RateScale, glowLevel);
		glowParams.SpanBegin = std::lerp(glowParams.SpanBegin, outroGlow.SpanBegin, glowLevel);
		glowParams.SizeMin *= std::lerp(1.0f, outroGlow.SizeScale, glowLevel);
		glowParams.SizeMax *= std::lerp(1.0f, outroGlow.SizeScale, glowLevel);
		glowParams.Alpha *= std::lerp(1.0f, outroGlow.AlphaScale, glowLevel);
		float const whiteHot{ Saturate(outroGlow.WhiteHot) * glowLevel };

		Lumina::Float3 weaponBase{};
		Lumina::Float3 weaponTip{};
		float weaponRadius{ 0.0f };
		if (glowParams.Enabled && Character_.WeaponAxis(weaponBase, weaponTip, weaponRadius)) {
			auto const& prm{ glowParams };
			float const lifeMin{ std::max(std::min(prm.LifeMin, prm.LifeMax), 0.05f) };
			float const lifeMax{ std::max(prm.LifeMax, lifeMin) };
			Lumina::Float3 const fixedColor{ SRGBToLinear(prm.Color) };

			int const count{ SpawnCount(prm.Rate, deltaTime_, WeaponGlowAccum_) };
			for (int i{ 0 }; i < count; ++i) {
				float const s{ prm.SpanBegin + (prm.SpanEnd - prm.SpanBegin) * Rand01() };

				// 散らす方向：ランダムな向きから軸方向の成分を除く（軸に垂直な円盤の中）
				Lumina::Float3 const axis{
					weaponTip.x - weaponBase.x,
					weaponTip.y - weaponBase.y,
					weaponTip.z - weaponBase.z,
				};
				float const axisLengthSq{ std::max(Dot3(axis, axis), 1e-8f) };
				Lumina::Float3 jitter{ RandSigned(), RandSigned(), RandSigned() };
				float const along{ Dot3(jitter, axis) / axisLengthSq };
				jitter = { jitter.x - axis.x * along, jitter.y - axis.y * along, jitter.z - axis.z * along };
				float const jitterScale{ weaponRadius * prm.RadialJitter };

				float const life{ lifeMin + (lifeMax - lifeMin) * Rand01() };

				TitleParticle p{};
				p.Translate = {
					weaponBase.x + axis.x * s + jitter.x * jitterScale,
					weaponBase.y + axis.y * s + jitter.y * jitterScale,
					weaponBase.z + axis.z * s + jitter.z * jitterScale,
				};
				p.Life = life;
				p.MaxLife = life;
				p.BaseSize = prm.SizeMin + (prm.SizeMax - prm.SizeMin) * Rand01();
				p.BaseAlpha = prm.Alpha;
				p.Rotate.z = Rand01() * TwoPi;
				p.Scale = { p.BaseSize, p.BaseSize, 1.0f };

				Lumina::Float3 color{ fixedColor };
				if (prm.HueFlow) {
					float hue{ std::fmod(prm.HueBase + prm.HueRange * s + prm.HueSpeed * Time_, 360.0f) };
					if (hue < 0.0f) { hue += 360.0f; }
					auto const rgb{
						Lumina::Utils::Color::Convert(
							Lumina::Utils::Color::HSV{ hue, Saturate(prm.Saturation), std::max(prm.Value, 0.0f) }
						)
					};
					color = SRGBToLinear(Lumina::Float3{ rgb.R, rgb.G, rgb.B });
				}
				color = Lerp3(color, Lumina::Float3{ 1.0f, 1.0f, 1.0f }, whiteHot);	// Outro：白熱寄りに
				p.RenderData.RGBA = { color.x, color.y, color.z, 0.0f };
				p.RenderData.DiffuseID = 0U;
				p.RenderData.DiffuseAtlasID = ClampAtlas(prm.Atlas);
				WeaponGlow_->Emit(std::move(p));
			}
		}
		ctx.Params = &glowParams;
		WeaponGlow_->Update(cmdList_, viewToWorld, UpdateWeaponGlow, &ctx);

		//----	------	------	------	------	----//
		// WeaponFlare：Outro 中だけ、武器の先端に四芒星を 1 枚（明滅・回転）

		WeaponFlare_->Clear();
		float const flare{ OutroWeaponFlareStrength() };
		if (flare > 0.0f && Character_.WeaponAxis(weaponBase, weaponTip, weaponRadius)) {
			auto const& style{ Outro_.WeaponGlow };

			// カメラの方へ少し寄せる（ViewToWorld の 3 行目 = カメラの位置）
			Lumina::Float3 toEye{
				viewToWorld[3][0] - weaponTip.x,
				viewToWorld[3][1] - weaponTip.y,
				viewToWorld[3][2] - weaponTip.z,
			};
			float const distance{ std::sqrt(Dot3(toEye, toEye)) };
			float const offset{ (distance > 1e-4f) ? (std::min(style.FlareCameraOffset, distance * 0.5f) / distance) : 0.0f };

			// 明滅：周波数の違う 2 つの波を混ぜて、規則的に見えすぎないように
			float const phase{ TwoPi * style.FlareFlickerFrequency * OutroTime_ };
			float const wave{ 0.5f + 0.25f * std::sin(phase) + 0.25f * std::sin(phase * 1.73f + 0.9f) };
			float const flicker{ 1.0f - Saturate(style.FlareFlickerDepth) * wave };

			float const size{ std::max(style.FlareSize, 0.0f) * (0.6f + 0.4f * flare) };
			Lumina::Float3 const color{ SRGBToLinear(style.FlareColor) };

			TitleParticle p{};
			p.Translate = {
				weaponTip.x + toEye.x * offset,
				weaponTip.y + toEye.y * offset,
				weaponTip.z + toEye.z * offset,
			};
			p.Life = 1.0f;
			p.MaxLife = 1.0f;
			p.Scale = { size, size, 1.0f };
			p.Rotate.z = style.FlareSpin * OutroTime_;
			p.RenderData.RGBA = { color.x, color.y, color.z, std::max(style.FlareAlpha, 0.0f) * flare * flicker };
			p.RenderData.DiffuseID = 0U;
			p.RenderData.DiffuseAtlasID = ClampAtlas(style.FlareAtlas);
			WeaponFlare_->Emit(std::move(p));
		}
		WeaponFlare_->Update(cmdList_, viewToWorld, UpdateWeaponFlare, &ctx);
	}

	//////	//////	//////	//////	//////	//////
	//	Render									//
	//////	//////	//////	//////	//////	//////

	void Title::RenderParticles(Lumina::DX12::CommandList const& cmdList_) {
		ParticleSystem<TitleParticle>* const systems[]{
			Sparkles_.get(),
			Aura_.get(),
			Embers_.get(),
			CircleLights_.get(),
			WeaponGlow_.get(),
			WeaponFlare_.get(),
		};
		for (auto* const system : systems) {
			system->Render(
				cmdList_,
				RS_Particle_,
				PSO_Particle_,
				LocalHeap_Particle_.CPUHandle(0U),	// GlobalData（ViewToWorld）
				LocalHeap_Particle_.CPUHandle(1U),	// VP
				GlobalTable_ParticleTextures_,
				GlobalTable_ParticleTextures_		// Textures2 は PS で使わない
			);
		}
	}

	//////	//////	//////	//////	//////	//////
	//	ImGui									//
	//////	//////	//////	//////	//////	//////

	void Title::EditParticles() {
		#if defined(_DEBUG)
		ImGui::Begin("Particles");

		if (ImGui::Button("Clear All")) {
			Sparkles_->Clear();
			Aura_->Clear();
			Embers_->Clear();
			CircleLights_->Clear();
			WeaponGlow_->Clear();
		}
		ImGui::TextDisabled("Atlas (Particles.png): 0 dot / 1 streak / 2 star / 3 soft star / 4 sparkle / 5 small sparkle / 6 smoke / 7 circle");

		if (ImGui::CollapsingHeader("Sparkles (in-game AmbientSparkles)")) {
			ImGui::PushID("Sparkles");
			auto& prm{ SparkleParams_ };
			ImGui::Checkbox("Enabled", &prm.Enabled);
			ImGui::SliderFloat("Rate (/s)", &prm.Rate, 0.0f, 300.0f);
			ImGui::SliderFloat("AreaRadius", &prm.AreaRadius, 0.0f, 40.0f);
			ImGui::SliderFloat("HeightMax", &prm.HeightMax, 0.0f, 10.0f);
			ImGui::SliderFloat("Size", &prm.Size, 0.0f, 5.0f);
			ImGui::SliderFloat("Alpha", &prm.Alpha, 0.0f, 1.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Aura (in-game PlayerEffects)")) {
			ImGui::PushID("Aura");
			auto& prm{ AuraParams_ };
			ImGui::Checkbox("Enabled", &prm.Enabled);
			ImGui::SliderFloat("Rate (/s)", &prm.Rate, 0.0f, 480.0f);
			ImGui::SliderFloat("OrbitRadius", &prm.OrbitRadius, 0.0f, 8.0f);
			ImGui::SliderFloat("OrbitHeight", &prm.OrbitHeight, 0.0f, 8.0f);
			ImGui::SliderFloat("HeightOffset", &prm.HeightOffset, -2.0f, 10.0f);
			ImGui::SliderFloat("Size", &prm.Size, 0.0f, 3.0f);
			ImGui::SliderFloat("Alpha", &prm.Alpha, 0.0f, 1.0f);
			ImGui::Checkbox("HueCycle (in-game color)", &prm.HueCycle);
			ImGui::BeginDisabled(prm.HueCycle);
			ImGui::ColorEdit3("Color", &prm.Color.x);
			ImGui::EndDisabled();
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Embers", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID("Embers");
			auto& prm{ EmberParams_ };
			ImGui::Checkbox("Enabled", &prm.Enabled);
			ImGui::SliderFloat("Rate (/s)", &prm.Rate, 0.0f, 400.0f);
			ImGui::SliderFloat("LifeMin", &prm.LifeMin, 0.05f, 6.0f);
			ImGui::SliderFloat("LifeMax", &prm.LifeMax, 0.05f, 6.0f);
			ImGui::SeparatorText("Spawn");
			ImGui::SliderFloat("SpawnRadius", &prm.SpawnRadius, 0.0f, 10.0f);
			ImGui::SliderFloat("SpawnRadiusJitter", &prm.SpawnRadiusJitter, 0.0f, 5.0f);
			ImGui::SliderFloat("SpawnHeight", &prm.SpawnHeight, 0.0f, 5.0f);
			ImGui::SeparatorText("Motion");
			ImGui::SliderFloat("RiseSpeed", &prm.RiseSpeed, 0.0f, 10.0f);
			ImGui::SliderFloat("RiseJitter", &prm.RiseJitter, 0.0f, 5.0f);
			ImGui::SliderFloat("Swirl (rad/s)", &prm.Swirl, -4.0f, 4.0f);
			ImGui::SliderFloat("Turbulence", &prm.Turbulence, 0.0f, 20.0f);
			ImGui::SliderFloat("Drag", &prm.Drag, 0.0f, 10.0f);
			ImGui::SeparatorText("Look");
			ImGui::SliderFloat("Size", &prm.Size, 0.0f, 1.0f);
			ImGui::SliderFloat("Stretch", &prm.Stretch, 1.0f, 12.0f);
			ImGui::SliderFloat("Alpha", &prm.Alpha, 0.0f, 2.0f);
			ImGui::ColorEdit3("ColorHot", &prm.ColorHot.x);
			ImGui::ColorEdit3("ColorCool", &prm.ColorCool.x);
			ImGui::SliderInt("Atlas", &prm.Atlas, 0, 7);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Circle Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID("CircleLights");
			auto& prm{ CircleLightParams_ };
			ImGui::Checkbox("Enabled", &prm.Enabled);
			ImGui::SameLine();
			ImGui::Checkbox("RespectReveal", &prm.RespectReveal);
			ImGui::SliderFloat("Rate (/s)", &prm.Rate, 0.0f, 400.0f);
			ImGui::SliderFloat("LifeMin", &prm.LifeMin, 0.05f, 8.0f);
			ImGui::SliderFloat("LifeMax", &prm.LifeMax, 0.05f, 8.0f);
			ImGui::SeparatorText("Motion");
			ImGui::SliderFloat("RiseSpeed", &prm.RiseSpeed, 0.0f, 6.0f);
			ImGui::SliderFloat("RiseJitter", &prm.RiseJitter, 0.0f, 3.0f);
			ImGui::SliderFloat("Sway", &prm.Sway, 0.0f, 2.0f);
			ImGui::SeparatorText("Look");
			ImGui::SliderFloat("Size", &prm.Size, 0.0f, 1.0f);
			ImGui::SliderFloat("Stretch", &prm.Stretch, 1.0f, 20.0f);
			ImGui::SliderFloat("Alpha", &prm.Alpha, 0.0f, 2.0f);
			ImGui::SliderFloat("Whiten", &prm.Whiten, 0.0f, 1.0f);
			ImGui::SliderInt("Atlas", &prm.Atlas, 0, 7);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Weapon Glow", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID("WeaponGlow");
			auto& prm{ WeaponGlowParams_ };
			ImGui::Checkbox("Enabled", &prm.Enabled);
			ImGui::SliderFloat("Rate (/s)", &prm.Rate, 0.0f, 400.0f);
			ImGui::SliderFloat("LifeMin", &prm.LifeMin, 0.05f, 4.0f);
			ImGui::SliderFloat("LifeMax", &prm.LifeMax, 0.05f, 4.0f);
			ImGui::SeparatorText("Spawn (along the weapon axis)");
			ImGui::SliderFloat("SpanBegin", &prm.SpanBegin, 0.0f, 1.0f);
			ImGui::SliderFloat("SpanEnd", &prm.SpanEnd, 0.0f, 1.0f);
			ImGui::SliderFloat("RadialJitter", &prm.RadialJitter, 0.0f, 3.0f);
			ImGui::SeparatorText("Look");
			ImGui::SliderFloat("SizeMin", &prm.SizeMin, 0.0f, 4.0f);
			ImGui::SliderFloat("SizeMax", &prm.SizeMax, 0.0f, 4.0f);
			ImGui::SliderFloat("Grow", &prm.Grow, -1.0f, 3.0f);
			ImGui::SliderFloat("Alpha", &prm.Alpha, 0.0f, 2.0f);
			ImGui::SliderInt("Atlas", &prm.Atlas, 0, 7);
			ImGui::SeparatorText("Color");
			ImGui::Checkbox("HueFlow (temporary iridescence)", &prm.HueFlow);
			ImGui::BeginDisabled(prm.HueFlow);
			ImGui::ColorEdit3("Color", &prm.Color.x);
			ImGui::EndDisabled();
			ImGui::BeginDisabled(!prm.HueFlow);
			ImGui::SliderFloat("HueBase (deg)", &prm.HueBase, 0.0f, 360.0f);
			ImGui::SliderFloat("HueRange (deg)", &prm.HueRange, -360.0f, 360.0f);
			ImGui::SliderFloat("HueSpeed (deg/s)", &prm.HueSpeed, -360.0f, 360.0f);
			ImGui::SliderFloat("Saturation", &prm.Saturation, 0.0f, 1.0f);
			ImGui::SliderFloat("Value", &prm.Value, 0.0f, 2.0f);
			ImGui::EndDisabled();
			ImGui::PopID();
		}

		ImGui::End();
		#endif
	}
}
