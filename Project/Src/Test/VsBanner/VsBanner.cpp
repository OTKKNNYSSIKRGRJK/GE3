module Lumina.VsBanner;

// ここで import しているヘッダユニットは、いずれも VsBanner.ixx で import 済み
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <memory>;
import <optional>;
import <random>;
import <string>;
import <vector>;

import nlohmann.json;

import Lumina.Utils.Data;
import Lumina.DX12.Aux.View;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace Lumina::VsBanner {
	using JSON = nlohmann::json;

	namespace {
		constexpr float Pi{ 3.14159265358979f };

		auto Saturate(float x_) -> float { return std::clamp(x_, 0.0f, 1.0f); }
		auto Lerp(float a_, float b_, float t_) -> float { return a_ + (b_ - a_) * t_; }

		// ease-out-back（overshoot_ = 0 なら ease-out cubic と同じ）
		auto EaseOutBack(float x_, float overshoot_) -> float {
			float const c1{ std::max(overshoot_, 0.0f) };
			float const c3{ c1 + 1.0f };
			float const u{ x_ - 1.0f };
			return 1.0f + c3 * u * u * u + c1 * u * u;
		}
		auto EaseInCubic(float x_) -> float { return x_ * x_ * x_; }

		auto SRGBToLinear(float c_) -> float {
			return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
		}
		auto SRGBToLinear(Float3 const& c_) -> Float3 {
			return { SRGBToLinear(c_.x), SRGBToLinear(c_.y), SRGBToLinear(c_.z) };
		}

		// 左手系・行ベクトル（Title / InGame の LookAt と同じ）
		auto LookAt(Vec3 const& src_, Vec3 const& dst_, Vec3 const& up_) -> Mat4 {
			Vec3 const forward{ Vec3{ dst_ - src_ }.Unit() };
			Vec3 const right{ Vec3::Cross(up_, forward).Unit() };
			Vec3 const up{ Vec3::Cross(forward, right) };
			return {
				right.x, up.x, forward.x, 0.0f,
				right.y, up.y, forward.y, 0.0f,
				right.z, up.z, forward.z, 0.0f,
				-Vec3::Dot(src_, right),
				-Vec3::Dot(src_, up),
				-Vec3::Dot(src_, forward),
				1.0f,
			};
		}
	}

	//====	======	======	======	======	====//
	//	配置の計算								//
	//====	======	======	======	======	====//

	auto TotalTime(Timing const& timing_) -> float {
		return std::max(timing_.Delay, 0.0f) + std::max(timing_.InTime, 0.0f)
			+ std::max(timing_.HoldTime, 0.0f) + std::max(timing_.OutTime, 0.0f);
	}

	auto ScheduleLetter(Settings const& settings_, uint32_t index_) -> LetterSchedule {
		Timing const& timing{ settings_.Time };
		LetterSettings const& cfg{ settings_.Letters };
		float const settle{ std::max(timing.Delay, 0.0f) + std::max(timing.InTime, 0.0f) };	// バナーが止まる時刻
		float const bannerOut{ settle + std::max(timing.HoldTime, 0.0f) };						// バナーが抜け始める時刻
		float const stagger{ (index_ == 0U) ? 0.0f : cfg.Stagger };

		LetterSchedule s{};
		s.InBegin = settle + cfg.InDelay + stagger;
		s.Land = s.InBegin + std::max(cfg.InTime, 0.0f);
		s.OutBegin = std::max(bannerOut - cfg.OutLead + stagger, s.Land);	// 着く前には抜け始めない
		s.OutEnd = s.OutBegin + std::max(cfg.OutTime, 0.0f);
		return s;
	}

	auto TotalTime(Settings const& settings_) -> float {
		float total{ TotalTime(settings_.Time) };
		if (settings_.Letters.Enabled) {
			for (uint32_t idx{ 0U }; idx < 2U; ++idx) { total = std::max(total, ScheduleLetter(settings_, idx).OutEnd); }
		}
		return total;
	}

	auto Evaluate(Settings const& settings_, float time_, Float2 screenSize_) -> Pose {
		Layout const& layout{ settings_.Placement };
		Timing const& timing{ settings_.Time };

		float const delay{ std::max(timing.Delay, 0.0f) };
		float const inTime{ std::max(timing.InTime, 0.0f) };
		float const hold{ std::max(timing.HoldTime, 0.0f) };
		float const outTime{ std::max(timing.OutTime, 0.0f) };

		float const halfLength{ std::max(layout.Length, 0.0f) * 0.5f };
		float const halfThickness{ std::max(layout.Thickness, 0.0f) * 0.5f };

		float const angle{ layout.AngleDeg * (Pi / 180.0f) };
		Float2 const u{ std::cos(angle), std::sin(angle) };		// 帯の向き
		Float2 const n{ -u.y, u.x };								// 法線（A は中心線の上側へ Offset だけ離す）

		// 画面外の位置: バナーの点を u へ射影した値は「中心の射影 ± 半分の長さ」（n 方向の成分は u への射影が 0）。
		// 画面の射影の範囲は ±screenProj。だから中心の射影が ±(screenProj + 半分の長さ) を越えれば、バナー全体が画面外
		float const screenProj{ screenSize_.x * 0.5f * std::abs(u.x) + screenSize_.y * 0.5f * std::abs(u.y) };
		float const outside{ screenProj + halfLength + 2.0f };		// 2 = アンチエイリアスのにじみ分

		float const holdBegin{ layout.Shift };
		float const holdEnd{ layout.Shift + timing.Drift * hold };
		float const exitAt{ timing.PassThrough ? outside : -outside };

		// A の中心の、帯の向きの位置
		float along{ -outside };
		float const t{ time_ - delay };
		if (t <= 0.0f) {
			along = -outside;
		}
		else if (t < inTime) {
			along = Lerp(-outside, holdBegin, EaseOutBack(t / inTime, timing.Overshoot));
		}
		else if (t < inTime + hold) {
			along = Lerp(holdBegin, holdEnd, (hold > 0.0f) ? ((t - inTime) / hold) : 1.0f);
		}
		else if (t < inTime + hold + outTime) {
			along = Lerp(holdEnd, exitAt, EaseInCubic((t - inTime - hold) / outTime));
		}
		else {
			along = exitAt;
		}

		Pose pose{};
		pose.Boss.Center = { n.x * layout.Offset + u.x * along, n.y * layout.Offset + u.y * along };
		pose.Boss.Axis = u;
		pose.Boss.HalfSize = { halfLength, halfThickness };
		pose.Boss.HoldCenter = { n.x * layout.Offset + u.x * holdBegin, n.y * layout.Offset + u.y * holdBegin };

		// B = A を画面中心まわりに 180° 回したもの（(x, y) → (-x, -y)。向きも反転）
		pose.Player.Center = { -pose.Boss.Center.x, -pose.Boss.Center.y };
		pose.Player.Axis = { -u.x, -u.y };
		pose.Player.HalfSize = pose.Boss.HalfSize;
		pose.Player.HoldCenter = { -pose.Boss.HoldCenter.x, -pose.Boss.HoldCenter.y };

		// 終わりの判定は Presenter と同じ TotalTime で行う（区間ごとの足し算とは丸めが違い、最後の 1 コマだけ食い違うことがあるため）
		pose.Visible = (t > 0.0f) && (time_ < TotalTime(timing)) && (halfLength > 0.0f) && (halfThickness > 0.0f);
		return pose;
	}

	namespace {
		// center_ から dir_（単位ベクトル）へ動かして、半分の大きさ halfSize_ の長方形（回さない）が画面の外に出きる距離。
		// 長方形を dir_ へ射影した範囲は「中心の射影 ± (hx·|dx| + hy·|dy|)」、画面は ±(W/2·|dx| + H/2·|dy|)
		auto DistanceToLeaveScreen(Float2 center_, Float2 halfSize_, Float2 dir_, Float2 screenSize_, float margin_) -> float {
			float const screenProj{ screenSize_.x * 0.5f * std::abs(dir_.x) + screenSize_.y * 0.5f * std::abs(dir_.y) };
			float const rectProj{ halfSize_.x * std::abs(dir_.x) + halfSize_.y * std::abs(dir_.y) };
			float const centerProj{ center_.x * dir_.x + center_.y * dir_.y };
			return std::max(screenProj + rectProj + margin_ - centerProj, 0.0f);
		}
	}

	auto EvaluateLetters(
		Settings const& settings_, float time_, Float2 screenSize_,
		std::array<Float2, 2> const& halfSizes_, float margin_
	) -> std::array<LetterPiece, 2> {
		LetterSettings const& cfg{ settings_.Letters };
		std::array<LetterPiece, 2> pieces{};
		if (!cfg.Enabled) { return pieces; }

		float const enterAngle{ cfg.EnterFromDeg * (Pi / 180.0f) };
		float const exitAngle{ cfg.ExitToDeg * (Pi / 180.0f) };
		for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
			// S は V を画面中心まわりに 180° 回した位置・向き（文字そのものは回さない）
			float const sign{ (idx == 0U) ? 1.0f : -1.0f };
			Float2 const hold{ cfg.HoldOffset.x * sign, cfg.HoldOffset.y * sign };
			Float2 const enterDir{ std::cos(enterAngle) * sign, std::sin(enterAngle) * sign };	// 止まる位置から見た「来る方」
			Float2 const exitDir{ std::cos(exitAngle) * sign, std::sin(exitAngle) * sign };
			Float2 const half{ std::max(halfSizes_[idx].x, 0.0f), std::max(halfSizes_[idx].y, 0.0f) };

			float const enterDist{ DistanceToLeaveScreen(hold, half, enterDir, screenSize_, margin_) };
			float const exitDist{ DistanceToLeaveScreen(hold, half, exitDir, screenSize_, margin_) };
			Float2 const from{ hold.x + enterDir.x * enterDist, hold.y + enterDir.y * enterDist };
			Float2 const to{ hold.x + exitDir.x * exitDist, hold.y + exitDir.y * exitDist };

			LetterSchedule const s{ ScheduleLetter(settings_, idx) };
			LetterPiece& piece{ pieces[idx] };
			piece.HalfSize = half;
			piece.Visible = (time_ > s.InBegin) && (time_ < s.OutEnd) && (half.x > 0.0f) && (half.y > 0.0f);

			if (time_ < s.Land) {
				// 入り（ease-out-back。行き過ぎは止まる位置を越えて来た方の反対側へ出る）
				float const inTime{ s.Land - s.InBegin };
				float const k{ (inTime > 0.0f) ? EaseOutBack(Saturate((time_ - s.InBegin) / inTime), cfg.Overshoot) : 1.0f };
				piece.Center = { Lerp(from.x, hold.x, k), Lerp(from.y, hold.y, k) };
			}
			else if (time_ < s.OutBegin) {
				piece.Center = hold;
			}
			else {
				float const outTime{ s.OutEnd - s.OutBegin };
				float const k{ (outTime > 0.0f) ? EaseInCubic(Saturate((time_ - s.OutBegin) / outTime)) : 1.0f };
				piece.Center = { Lerp(hold.x, to.x, k), Lerp(hold.y, to.y, k) };
			}

			// 着地の光: 着いた瞬間に最大、(1 − 経過 / FlashTime)^2 で消える
			if (cfg.FlashTime > 0.0f && time_ >= s.Land && time_ < s.Land + cfg.FlashTime) {
				float const r{ 1.0f - (time_ - s.Land) / cfg.FlashTime };
				piece.Flash = std::max(cfg.FlashIntensity, 0.0f) * r * r;
			}
		}
		return pieces;
	}

	//====	======	======	======	======	====//
	//	描画									//
	//====	======	======	======	======	====//

	void Renderer::Initialize(
		DX12::Context const& dxContext_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_
	) {
		auto const& device{ dxContext_.Device() };

		{
			auto settings{ Utils::LoadFromFile<nlohmann::json>("VsBanner.json", "Assets/Effects") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device, rsSetup, "VsBanner RS");
		}

		dxContext_.Compile(VS_, L"Assets/Effects/VsBanner.hlsl", L"vs_6_6", L"VSMain", "VsBanner.VS");
		dxContext_.Compile(PS_, L"Assets/Effects/VsBanner.hlsl", L"ps_6_6", L"PSMain", "VsBanner.PS");

		// PSO（プリマルチプライド α・深度なし・両面）
		{
			DX12::BlendState blendState{};
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_ONE },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ONE },
				.DestBlendAlpha{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			// * クアッドは VS で SV_VertexID から生成
			DX12::GraphicsPipelineState::InputLayout inputLayout{};
			std::vector<DXGI_FORMAT> const rtvFormats{ rtvFormat_ };
			PSO_.Initialize(
				device,
				RS_,
				VS_,
				PS_,
				blendState,
				DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_NONE },
				},
				DX12::DepthStencilState{
					.DepthEnable{ false },
					.StencilEnable{ false },
				},
				inputLayout,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				rtvFormats,
				dsvFormat_,
				"VsBanner PSO"
			);
		}

		UB_Constants_.Initialize(device, ConstantsStride * NumSlots, "VsBanner Constants");
	}

	void Renderer::Render(
		DX12::CommandList const& cmdList_,
		Pose const& pose_,
		Layout const& layout_,
		Look const& look_,
		std::array<Content, 2> const& contents_,
		ShapeInput const& shape_,
		Decoration const& decoration_,
		Float2 screenSize_
	) {
		if (!pose_.Visible) { return; }
		if (!(screenSize_.x > 0.0f) || !(screenSize_.y > 0.0f)) { return; }

		float const edgeWidth{ std::max(look_.EdgeWidth, 0.0f) };
		float const cornerRadius{ std::clamp(layout_.CornerRadius, 0.0f, std::min(pose_.Boss.HalfSize.x, pose_.Boss.HalfSize.y)) };
		float const opacity{ Saturate(look_.Opacity) };

		std::array<Banner const*, NumBannerSlots> const banners{ &pose_.Boss, &pose_.Player };

		// 背景とグロー
		BackgroundArt const fallbackArt{ .Enabled{ false } };
		GlowSettings const fallbackGlow{ .Enabled{ false } };
		BackgroundArt const& art{ (decoration_.Background != nullptr) ? *decoration_.Background : fallbackArt };
		GlowSettings const& glow{ (decoration_.Glow != nullptr) ? *decoration_.Glow : fallbackGlow };
		float glowWidth{ glow.Enabled ? std::max(glow.Width, 0.0f) : 0.0f };
		if (decoration_.RangePx > 0.0f) { glowWidth = std::min(glowWidth, decoration_.RangePx * 0.95f); }	// SDF が張り付く所でグローが一定にならないように
		std::array<CosinePalette const*, NumBannerSlots> const palettes{ &art.Boss, &art.Player };
		std::array<Float3, NumBannerSlots> const colors{ SRGBToLinear(look_.ColorBoss), SRGBToLinear(look_.ColorPlayer) };

		for (uint32_t idx{ 0U }; idx < NumBannerSlots; ++idx) {
			Banner const& b{ *banners[idx] };
			Content const& content{ contents_[idx] };
			// 中身の RT の中心は「帯の中心 − HoldCenter」= 帯が止まったとき画面の中心。帯と一緒に滑るが回さない
			bool const hasContent{ content.Enabled && (content.Size.x > 0.0f) && (content.Size.y > 0.0f) };
			ShaderConstants const constants{
				.Center{ b.Center },
				.Axis{ b.Axis },
				.HalfSize{ b.HalfSize },
				.InvHalfScreen{ 2.0f / screenSize_.x, 2.0f / screenSize_.y },
				.Color{ colors[idx] },
				.Opacity{ opacity },
				.EdgeColor{ SRGBToLinear(look_.EdgeColor) },
				.EdgeWidth{ edgeWidth },
				.HasContent{ hasContent ? 1.0f : 0.0f },
				.DebugView{ static_cast<float>(look_.DebugView) },
				.CornerRadius{ cornerRadius },
				.Margin{ 2.0f + glowWidth },			// アンチエイリアスのにじみ分 ＋ グローの幅だけクアッドを広げる（px）
				.ContentOffset{ -b.HoldCenter.x, -b.HoldCenter.y },
				.ContentSize{ content.Size },
				.ContentOpacity{ Saturate(look_.ContentOpacity) },
				.ContentFeather{ std::max(look_.ContentFeather, 0.0f) },
				.HasShape{ shape_.Enabled ? 1.0f : 0.0f },
				.ShapeRange{ std::max(shape_.Range, 1.0e-3f) },
				.ShapeTexels{ std::max(shape_.Texels.x, 1.0f), std::max(shape_.Texels.y, 1.0f) },
				.ShapeUseOpacity{ shape_.UseOpacity ? 1.0f : 0.0f },
				.EdgeUseOpacity{ shape_.EdgeUsesOpacity ? 1.0f : 0.0f },
				.PaletteBias{ palettes[idx]->Bias },
				.ArtTime{ decoration_.Time },
				.PaletteAmplitude{ palettes[idx]->Amplitude },
				.ArtScale{ std::max(art.Scale, 1.0f) },
				.PaletteFrequency{ palettes[idx]->Frequency },
				.ArtEnabled{ art.Enabled ? 1.0f : 0.0f },
				.PalettePhase{ palettes[idx]->Phase },
				.GlowWidth{ glowWidth },
				.GlowColor{ SRGBToLinear(glow.Color) },
				.GlowIntensity{ std::max(glow.Intensity, 0.0f) },
				.ArtOrigin{ art.Origin },
				.GlowFalloff{ std::max(glow.Falloff, 0.01f) },
				.ArtOutputIsGamma{ art.OutputIsGamma ? 1.0f : 0.0f },
			};
			UB_Constants_.Store(&constants, sizeof(ShaderConstants), ConstantsStride * idx);
		}

		D3D12_GPU_VIRTUAL_ADDRESS const cbBase{ UB_Constants_->GetGPUVirtualAddress() };

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (uint32_t idx{ 0U }; idx < NumBannerSlots; ++idx) {
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbBase + ConstantsStride * idx);
			cmdList_->SetGraphicsRootDescriptorTable(1U, contents_[idx].SRV);
			cmdList_->SetGraphicsRootDescriptorTable(2U, shape_.SRV);
			cmdList_->DrawInstanced(6U, 1U, 0U, 0U);
		}
	}

	void Renderer::RenderLetters(
		DX12::CommandList const& cmdList_,
		std::array<LetterPiece, 2> const& pieces_,
		LetterSettings const& settings_,
		std::array<LetterShape, 2> const& shapes_,
		int debugView_,
		Float2 screenSize_
	) {
		if (!(screenSize_.x > 0.0f) || !(screenSize_.y > 0.0f)) { return; }
		if (!pieces_[0].Visible && !pieces_[1].Visible) { return; }

		Float3 const color{ SRGBToLinear(settings_.Color) };
		Float3 const edgeColor{ SRGBToLinear(settings_.EdgeColor) };
		Float3 const flashColor{ SRGBToLinear(settings_.FlashColor) };
		GlowSettings const& glow{ settings_.Glow };
		auto const mix{
			[](Float3 const& a_, Float3 const& b_, float t_) -> Float3 {
				return { Lerp(a_.x, b_.x, t_), Lerp(a_.y, b_.y, t_), Lerp(a_.z, b_.z, t_) };
			}
		};

		for (uint32_t idx{ 0U }; idx < NumLetterSlots; ++idx) {
			LetterPiece const& piece{ pieces_[idx] };
			LetterShape const& shape{ shapes_[idx] };
			// グローは SDF の距離の範囲（px 換算）までに抑える
			float glowWidth{ glow.Enabled ? std::max(glow.Width, 0.0f) : 0.0f };
			if (shape.Enabled) {
				float const rangePx{ shape.Range * (2.0f * piece.HalfSize.x) / std::max(shape.Texels.x, 1.0f) };
				glowWidth = std::min(glowWidth, rangePx * 0.95f);
			}
			float const flash{ Saturate(piece.Flash) };

			ShaderConstants const constants{
				.Center{ piece.Center },
				.Axis{ 1.0f, 0.0f },					// 回さない（画像の右 = 画面の右、上 = 画面の上）
				.HalfSize{ piece.HalfSize },
				.InvHalfScreen{ 2.0f / screenSize_.x, 2.0f / screenSize_.y },
				.Color{ mix(color, flashColor, flash) },
				.Opacity{ Saturate(settings_.Opacity) },
				.EdgeColor{ mix(edgeColor, flashColor, flash) },
				.EdgeWidth{ std::max(settings_.EdgeWidth, 0.0f) },
				.HasContent{ 0.0f },
				.DebugView{ static_cast<float>(debugView_) },
				.CornerRadius{ 0.0f },
				.Margin{ 2.0f + glowWidth },
				.ContentOffset{ 0.0f, 0.0f },
				.ContentSize{ 1.0f, 1.0f },
				.ContentOpacity{ 0.0f },
				.ContentFeather{ 0.0f },
				.HasShape{ shape.Enabled ? 1.0f : 0.0f },
				.ShapeRange{ std::max(shape.Range, 1.0e-3f) },
				.ShapeTexels{ std::max(shape.Texels.x, 1.0f), std::max(shape.Texels.y, 1.0f) },
				.ShapeUseOpacity{ 1.0f },
				.EdgeUseOpacity{ 1.0f },
				.PaletteBias{ 0.0f, 0.0f, 0.0f },
				.ArtTime{ 0.0f },
				.PaletteAmplitude{ 0.0f, 0.0f, 0.0f },
				.ArtScale{ 1.0f },
				.PaletteFrequency{ 0.0f, 0.0f, 0.0f },
				.ArtEnabled{ 0.0f },					// ベタ塗り
				.PalettePhase{ 0.0f, 0.0f, 0.0f },
				.GlowWidth{ glowWidth },
				.GlowColor{ SRGBToLinear(glow.Color) },
				.GlowIntensity{ std::max(glow.Intensity, 0.0f) * (1.0f + piece.Flash) },
				.ArtOrigin{ 0.0f, 0.0f },
				.GlowFalloff{ std::max(glow.Falloff, 0.01f) },
				.ArtOutputIsGamma{ 0.0f },
			};
			UB_Constants_.Store(&constants, sizeof(ShaderConstants), ConstantsStride * (NumBannerSlots + idx));
		}

		D3D12_GPU_VIRTUAL_ADDRESS const cbBase{ UB_Constants_->GetGPUVirtualAddress() };

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (uint32_t idx{ 0U }; idx < NumLetterSlots; ++idx) {
			if (!pieces_[idx].Visible) { continue; }
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbBase + ConstantsStride * (NumBannerSlots + idx));
			// 中身は使わない（HasContent = 0）が、テーブルは設定しておく（形の SRV をそのまま置く）
			cmdList_->SetGraphicsRootDescriptorTable(1U, shapes_[idx].SRV);
			cmdList_->SetGraphicsRootDescriptorTable(2U, shapes_[idx].SRV);
			cmdList_->DrawInstanced(6U, 1U, 0U, 0U);
		}
	}

	//====	======	======	======	======	====//
	//	再生									//
	//====	======	======	======	======	====//

	void Presenter::Initialize(
		DX12::Context const& dxContext_,
		AssetManager& assetMngr_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_,
		Float2 screenSize_
	) {
		ScreenSize_ = screenSize_;
		Context_ = &dxContext_;
		ShapeSRV_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
		LetterSRVs_ = dxContext_.GlobalDescriptorHeap().Allocate(2U);
		Renderer_.Initialize(dxContext_, rtvFormat_, dsvFormat_);

		// 中身: キャラ 2 体と、それぞれの Canvas（SRGB ＋ D24S8。ShowcaseCharacter の既定のフォーマット）
		auto const& device{ dxContext_.Device() };
		PortraitWidth_ = static_cast<uint32_t>(std::max(ScreenSize_.x, 1.0f)) + PortraitMargin * 2U;
		PortraitHeight_ = static_cast<uint32_t>(std::max(ScreenSize_.y, 1.0f)) + PortraitMargin * 2U;
		PortraitSRVs_ = dxContext_.GlobalDescriptorHeap().Allocate(2U);
		for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
			Characters_[idx].Initialize(dxContext_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);

			DX12::Canvas& canvas{ PortraitCanvases_[idx] };
			canvas.AllocateTextures(1U, true);
			// α = 0 でクリアする（キャラのいる所だけ α = 1 になり、バナー側で帯の色と混ぜる）
			canvas.RenderTexture(0U).Initialize(
				device, PortraitWidth_, PortraitHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				{ 0.0f, 0.0f, 0.0f, 0.0f }, (idx == 0U) ? "VsBanner.Portrait.Boss" : "VsBanner.Portrait.Player"
			);
			canvas.DepthTexture().Initialize(device, PortraitWidth_, PortraitHeight_, "VsBanner.Portrait.Depth");
			canvas.TransitionResourceStates(device, dxContext_.DirectQueue());	// RT・深度とも PIXEL_SHADER_RESOURCE になる
			canvas.CreateViews(device);
			canvas.Viewport(0U) = D3D12_VIEWPORT{
				.TopLeftX{ 0.0f },
				.TopLeftY{ 0.0f },
				.Width{ static_cast<float>(PortraitWidth_) },
				.Height{ static_cast<float>(PortraitHeight_) },
				.MinDepth{ 0.0f },
				.MaxDepth{ 1.0f },
			};
			canvas.ScissorRect(0U) = D3D12_RECT{
				.left{ 0 },
				.top{ 0 },
				.right{ static_cast<LONG>(PortraitWidth_) },
				.bottom{ static_cast<LONG>(PortraitHeight_) },
			};
			DX12::SRV<void>::Create(device, PortraitSRVs_.CPUHandle(idx), canvas.RenderTexture(0U));
		}

		// 背景のパーティクル（本編・Title と同じ ParticleSystem。RS は "Common RS"、VS は BasicParticle.VS、PS は Title の深度比較なし版）
		{
			auto config{ Utils::LoadFromFile<nlohmann::json>("Assets/Configs/ParticleSystem.json") };
			RS_Particle_.Initialize(device, DX12::LoadRootSignatureSetup(config.at("Common RS")), "VsBanner Particle RS");
		}
		dxContext_.Compile(VS_Particle_, L"Assets/Shaders/BasicParticle.VS.hlsl", L"vs_6_6", L"main", "VsBanner.Particle.VS");
		dxContext_.Compile(PS_Particle_, L"Assets/Title/TitleParticle.PS.hlsl", L"ps_6_6", L"main", "VsBanner.Particle.PS");
		{
			// 加算（色は SrcAlpha, One。α はそのまま = キャラのいない所は α = 0 のまま。バナー側はプリマルチプライドで合成する）・深度なし
			DX12::BlendState blendState{};
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_ONE },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ZERO },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
			DX12::GraphicsPSO::InputLayout inputLayout{};
			inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);
			inputLayout.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
			PSO_Particle_.Initialize(
				device,
				RS_Particle_,
				VS_Particle_,
				PS_Particle_,
				blendState,
				DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_NONE },
				},
				DX12::DepthStencilState{
					.DepthEnable{ false },
					.StencilEnable{ false },
				},
				inputLayout,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, },
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				"VsBanner Particle PSO"
			);
		}
		{
			// Particles.png（InGame・Title とは別名で読み込む）。RS の Textures は 32 枠・Textures2 は 8 枠なので、32 枠確保して両方に渡す
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(texIDs, { { "VsBanner.Particles", "Assets/Img/Particles.png" } });
			GlobalTable_ParticleTextures_ = dxContext_.GlobalDescriptorHeap().Allocate(32U);
			device->CopyDescriptorsSimple(
				1U,
				GlobalTable_ParticleTextures_.CPUHandle(0U),
				assetMngr_.Graphics().CPUHandle(texIDs.at(0U)),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
		}
		LocalHeap_Particle_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4U, false);
		for (uint32_t side{ 0U }; side < 2U; ++side) {
			UB_ParticleViewToWorld_[side].Initialize(device, 256LLU, "VsBanner Particle ViewToWorld");
			UB_ParticleVP_[side].Initialize(device, 256LLU, "VsBanner Particle VP");
			DX12::CBV::Create(device, LocalHeap_Particle_.CPUHandle(side * 2U + 0U), UB_ParticleViewToWorld_[side]);
			DX12::CBV::Create(device, LocalHeap_Particle_.CPUHandle(side * 2U + 1U), UB_ParticleVP_[side]);
			for (auto& system : ParticleSystems_[side]) {
				system = std::make_unique<ParticleSystem<BannerParticle>>();
				system->Initialize(dxContext_, MaxParticlesPerGroup);
			}
		}

		// ファイルが無ければ初期値のまま。読めたときは Load の中で形も作る
		if (!Load()) {
			LoadShape();
			LoadLetters();
		}
		AdvancePortraits(0.0f);
		ResetParticles();
	}

	void Presenter::Play() {
		State_ = State::Playing;
		Time_ = 0.0f;
		ResetParticles();		// 毎回撒き直す（年齢もばらすので、最初から流れている状態で入ってくる）
	}

	void Presenter::Reset() {
		State_ = State::Idle;
		Time_ = 0.0f;
	}

	void Presenter::Update(DX12::CommandList const& cmdList_, float dt_) {
		ApplyShapeAspect();
		// 中身のアニメは、見えている間（再生中・プレビュー中）だけ進める
		if (Preview_ || State_ == State::Playing) {
			ArtTime_ += std::max(dt_, 0.0f) * Config.Background.Speed;
			AdvancePortraits(std::max(dt_, 0.0f));
			UpdateParticles(cmdList_, std::max(dt_, 0.0f));
		}

		if (State_ != State::Playing || Preview_) { return; }
		Time_ += std::max(dt_, 0.0f);
		if (Time_ >= TotalTime(Config)) { State_ = State::Finished; }
	}

	auto Presenter::Progress() const -> float {
		if (State_ == State::Finished) { return 1.0f; }
		float const total{ TotalTime(Config) };
		return (total > 0.0f) ? Saturate(Time_ / total) : 1.0f;
	}

	auto Presenter::IsVisibleNow() const -> bool {
		if (Preview_) { return Evaluate(Config, PreviewTime_, ScreenSize_).Visible; }
		if (State_ != State::Playing) { return false; }
		return Evaluate(Config, Time_, ScreenSize_).Visible;
	}

	void Presenter::AdvancePortraits(float dt_) {
		std::array<Portrait const*, 2> const portraits{ &Config.Boss, &Config.Player };
		for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
			Portrait const& p{ *portraits[idx] };
			ShowcaseCharacter& c{ Characters_[idx] };
			c.Play(p.Animation, true);			// 同じ名前なら時間はそのまま。無い名前なら前のクリップのまま
			c.AnimationSpeed = p.AnimationSpeed;
			c.Advance(dt_);
		}
	}

	void Presenter::RenderPortraits(DX12::CommandList const& cmdList_) {
		ApplyShapeAspect();
		if (!IsVisibleNow()) { return; }

		// HoldCenter（帯が止まったときの中心）は時刻によらない。キャラの画面上の位置 = HoldCenter ＋ ContentOffset（B は −ContentOffset）
		Pose const pose{ Evaluate(Config, Preview_ ? PreviewTime_ : Time_, ScreenSize_) };
		std::array<Float2, 2> const characterOnScreen{
			Float2{ pose.Boss.HoldCenter.x + Config.Placement.ContentOffset.x, pose.Boss.HoldCenter.y + Config.Placement.ContentOffset.y },
			Float2{ pose.Player.HoldCenter.x - Config.Placement.ContentOffset.x, pose.Player.HoldCenter.y - Config.Placement.ContentOffset.y },
		};
		float const rtWidth{ static_cast<float>(PortraitWidth_) };
		float const rtHeight{ static_cast<float>(PortraitHeight_) };
		bool const drawParticles{ Config.Particles.Enabled };

		// パーティクル（カメラと描画データは Update で用意済み）
		auto const renderParticles{
			[&](uint32_t side_, bool front_) {
				auto const& groups{ Config.Particles.Groups };
				for (std::size_t g{ 0U }; g < groups.size() && g < MaxParticleGroups; ++g) {
					if (!groups[g].Enabled || groups[g].Front != front_ || ParticleCounts_[side_][g] <= 0) { continue; }
					ParticleSystems_[side_][g]->Render(
						cmdList_,
						RS_Particle_,
						PSO_Particle_,
						LocalHeap_Particle_.CPUHandle(side_ * 2U + 0U),	// GlobalData（ViewToWorld）
						LocalHeap_Particle_.CPUHandle(side_ * 2U + 1U),	// VP
						GlobalTable_ParticleTextures_,
						GlobalTable_ParticleTextures_					// Textures2 は PS で使わない
					);
				}
			}
		};

		std::array<Portrait const*, 2> const portraits{ &Config.Boss, &Config.Player };
		for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
			Portrait const& p{ *portraits[idx] };
			if (!p.Enabled && !drawParticles) { continue; }

			DX12::Canvas& canvas{ PortraitCanvases_[idx] };

			D3D12_RESOURCE_BARRIER barriers[2]{};
			uint32_t num_Barriers{ 0U };
			barriers[num_Barriers++] = DX12::Barrier::Transition(
				canvas.RenderTexture(0U),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			if (!IsPortraitDepthWritable_[idx]) {
				// 深度は SRV として読まないので、最初の 1 回だけ遷移してそのまま（Title の Canvas と同じ）
				barriers[num_Barriers++] = DX12::Barrier::Transition(
					canvas.DepthTexture(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_DEPTH_WRITE
				);
				IsPortraitDepthWritable_[idx] = true;
			}
			cmdList_->ResourceBarrier(num_Barriers, barriers);

			D3D12_CPU_DESCRIPTOR_HANDLE const rtv{ canvas.RTV(0U) };
			D3D12_CPU_DESCRIPTOR_HANDLE const dsv{ canvas.DSV() };
			float const clearColor[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
			cmdList_->OMSetRenderTargets(1U, &rtv, false, &dsv);
			cmdList_->RSSetViewports(1U, &canvas.Viewport(0U));
			cmdList_->RSSetScissorRects(1U, &canvas.ScissorRect(0U));
			cmdList_->ClearRenderTargetView(rtv, clearColor, 0U, nullptr);
			cmdList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0U, 0U, nullptr);

			// 1. 奥・中の層（キャラの後ろ）
			if (drawParticles) { renderParticles(idx, false); }

			// 2. キャラ
			if (p.Enabled) {
				// スタジオ: キャラは原点。カメラは注視点のまわりを回る（Title の BuildCamera と同じ置き方）
				ShowcaseCharacter& c{ Characters_[idx] };
				c.Position = { 0.0f, 0.0f, 0.0f };
				c.Yaw = p.Yaw;
				c.ShowWeapon = p.ShowWeapon;
				c.BodyMaterial.BaseColor = p.BodyColor;

				Vec3 const target{ 0.0f, p.TargetY, 0.0f };
				Vec3 const eye{
					target.x + std::sin(p.CameraYaw) * std::cos(p.CameraPitch) * p.CameraDistance,
					target.y + std::sin(p.CameraPitch) * p.CameraDistance,
					target.z - std::cos(p.CameraYaw) * std::cos(p.CameraPitch) * p.CameraDistance,
				};
				Mat4 const view{ LookAt(eye, target, Vec3{ 0.0f, 1.0f, 0.0f }) };

				// FovY は画面の高さ（720 px）に対する画角。RT はそれより高いので、同じ px あたりの大きさになるよう広げる
				float const tanHalf{ std::tan(std::clamp(p.FovY, 0.05f, 3.0f) * 0.5f) * (rtHeight / FovReferenceHeight) };
				Mat4 const proj{ Mat4::PerspectiveFOV(2.0f * std::atan(tanHalf), rtWidth / rtHeight, 0.1f, 200.0f) };

				// レンズシフト: 注視点が RT の中心から characterOnScreen[idx] px ずれた所に写るよう、クリップ座標の x, y に w × シフトを足す
				// （行ベクトルなので 4 行目の x, y 列に入れる。画角・遠近感は変わらない）
				float const shiftX{ characterOnScreen[idx].x * 2.0f / rtWidth };
				float const shiftY{ characterOnScreen[idx].y * 2.0f / rtHeight };
				Mat4 const lensShift{
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					shiftX, shiftY, 0.0f, 1.0f,
				};
				Mat4 const viewProj{ view * proj * lensShift };

				ShowcaseLighting lighting{ p.Lighting };
				lighting.FireBase = { 0.0f, 0.0f, 0.0f };		// 照り返しの軸はキャラの足元
				c.Render(cmdList_, viewProj, eye, lighting);
			}

			// 3. 手前の層（キャラの前）
			if (drawParticles) { renderParticles(idx, true); }

			D3D12_RESOURCE_BARRIER const toSRV{
				DX12::Barrier::Transition(
					canvas.RenderTexture(0U),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				)
			};
			cmdList_->ResourceBarrier(1U, &toSRV);
		}
	}

	//====	======	======	======	======	====//
	//	背景のパーティクル						//
	//====	======	======	======	======	====//
	// パーティクル用カメラ: +Z を向く透視カメラ。ワールドの x = 帯の向き、y = 帯の法線、z = カメラからの距離。
	//   帯の傾き（B は 180° 足した向き）は射影の後に画面上で回す（RT のピクセルの比率で回すので歪まない）。
	//   焦点距離 f（px）= 360 / tan(FovY / 2)。距離 z の粒は 1 単位 = f / z px に写る。
	// 視差: 帯の定位置からのずれを d（px、帯の向き）として、カメラを x に −d·P·ParallaxDepth / f 動かし、
	//   射影の後に画面を帯の向きへ −d·P px ずらす（レンズシフト）。RT は帯と一緒に動くので、画面上の動きは
	//   d·(1 − P + P·ParallaxDepth / z)。P = 1 なら ParallaxDepth の粒は帯と一緒、遠い粒ほど遅れ、近い粒は帯より速い。
	// 粒は「帯の通る範囲」（RT 上で帯の向きに ±（RT の対角線の半分 ＋ 余白）、法線の向きに Offset ±（太さの半分 ＋ 余白））を
	//   カメラ基準で周期的に折り返す。帯は RT と一緒に動くので、RT 上の帯の位置は時刻によらない。
	//   範囲は距離 z に比例して広げるので、画面上の密度は距離によらない

	namespace {
		constexpr float ParticleMarginPx{ 48.0f };

		struct ParticleUpdateContext {
			float DeltaTime;
			float Pan;					// カメラの x（ワールド）
			float Focal;				// 焦点距離（px）
			float HalfX;				// 折り返しの範囲（RT 上の px、帯の向き）
			float CenterY;				// 帯の中心線（RT 上の px、法線の向き）
			float HalfY;
			Float3 Color;				// リニア（グループの色。毎フレーム反映する）
			uint32_t Atlas;
			ParticleGroup const* Group;
			std::mt19937* Rng;
		};

		auto Wrap(float v_, float h_) -> float {
			return (h_ > 0.0f) ? (v_ - 2.0f * h_ * std::floor((v_ + h_) / (2.0f * h_))) : 0.0f;
		}

		void RespawnParticle(BannerParticle& p_, ParticleUpdateContext const& ctx_, bool randomAge_) {
			ParticleGroup const& g{ *ctx_.Group };
			std::uniform_real_distribution<float> unit{ 0.0f, 1.0f };
			auto const range{ [&](float a_, float b_) { return a_ + (b_ - a_) * unit(*ctx_.Rng); } };

			float const z{ std::max(range(g.DepthMin, g.DepthMax), 0.05f) };
			float const toWorld{ z / ctx_.Focal };		// RT 上の 1 px = この距離でのワールドの長さ
			p_.Translate = {
				ctx_.Pan + range(-1.0f, 1.0f) * ctx_.HalfX * toWorld,
				(ctx_.CenterY + range(-1.0f, 1.0f) * ctx_.HalfY) * toWorld,
				z,
			};
			p_.Velocity = { range(g.SpeedMin, g.SpeedMax), range(-g.CrossSpeed, g.CrossSpeed), 0.0f };
			p_.BaseSize = std::max(range(g.SizeMin, g.SizeMax), 0.001f);
			p_.Scale = { p_.BaseSize, p_.BaseSize, 1.0f };
			p_.Rotate = { 0.0f, 0.0f, range(0.0f, 6.2831853f) };
			p_.Spin = range(-1.0f, 1.0f) * g.Spin;
			p_.MaxLife = std::max(range(g.LifeMin, g.LifeMax), 0.05f);
			p_.Life = randomAge_ ? (p_.MaxLife * range(0.05f, 1.0f)) : p_.MaxLife;
			p_.BaseAlpha = std::max(g.Alpha, 0.0f);
			p_.RenderData.RGBA = { ctx_.Color.x, ctx_.Color.y, ctx_.Color.z, 0.0f };
			p_.RenderData.DiffuseID = 0U;
			p_.RenderData.DiffuseAtlasID = ctx_.Atlas;
		}

		// ParticleSystem に関数ポインタで渡す。寿命が来た粒はその場で生まれ直させる（数を一定に保つ）
		bool UpdateBannerParticle(BannerParticle& p_, void const* context_) {
			auto const& ctx{ *static_cast<ParticleUpdateContext const*>(context_) };
			ParticleGroup const& g{ *ctx.Group };
			float const dt{ ctx.DeltaTime };

			p_.Life -= dt;
			if (p_.Life <= 0.0f) { RespawnParticle(p_, ctx, false); }

			p_.Translate.x += p_.Velocity.x * dt;
			p_.Translate.y += p_.Velocity.y * dt;

			// 形: 速さに応じて流れる向きへ伸ばす（ローカルの y 軸を速度の向きに合わせる。Title の Embers と同じ）
			float const speed{ std::sqrt(p_.Velocity.x * p_.Velocity.x + p_.Velocity.y * p_.Velocity.y) };
			if (g.Stretch > 0.0f && speed > 1.0e-4f) {
				p_.Rotate.z = std::atan2(-p_.Velocity.x, p_.Velocity.y);
				p_.Scale = { p_.BaseSize, p_.BaseSize + speed * g.Stretch, 1.0f };
			}
			else {
				p_.Rotate.z += p_.Spin * dt;
				p_.Scale = { p_.BaseSize, p_.BaseSize, 1.0f };
			}

			// カメラ基準で折り返す（範囲は距離に比例。粒の大きさの分だけ余分に取って、範囲の端で消えて見えないように）
			float const z{ std::max(p_.Translate.z, 0.05f) };
			float const toWorld{ z / ctx.Focal };
			float const pad{ 0.5f * std::max(p_.Scale.x, p_.Scale.y) };
			float const centerY{ ctx.CenterY * toWorld };
			p_.Translate.x = ctx.Pan + Wrap(p_.Translate.x - ctx.Pan, ctx.HalfX * toWorld + pad);
			p_.Translate.y = centerY + Wrap(p_.Translate.y - centerY, ctx.HalfY * toWorld + pad);

			// フェード（始め 0.25 秒・終わり 0.35 秒）。色とセルはグループの設定を毎フレーム反映する
			float const age{ p_.MaxLife - p_.Life };
			float const fade{ std::clamp(std::min(age / 0.25f, p_.Life / 0.35f), 0.0f, 1.0f) };
			p_.RenderData.RGBA = { ctx.Color.x, ctx.Color.y, ctx.Color.z, std::max(g.Alpha, 0.0f) * fade };
			p_.RenderData.DiffuseAtlasID = ctx.Atlas;
			return true;
		}
	}

	auto Presenter::MakeParticleCamera(Banner const& banner_, Mat4& viewProj_, Mat4& viewToWorld_) const -> ParticleCamera {
		float const rtWidth{ static_cast<float>(std::max(PortraitWidth_, 1U)) };
		float const rtHeight{ static_cast<float>(std::max(PortraitHeight_, 1U)) };
		BackgroundParticles const& bp{ Config.Particles };

		float const focal{ (FovReferenceHeight * 0.5f) / std::tan(std::clamp(bp.FovY, 0.05f, 3.0f) * 0.5f) };
		float const displacement{
			(banner_.Center.x - banner_.HoldCenter.x) * banner_.Axis.x + (banner_.Center.y - banner_.HoldCenter.y) * banner_.Axis.y
		};
		float const shiftPx{ -displacement * std::clamp(bp.Parallax, 0.0f, 1.0f) };
		float const pan{ shiftPx * std::max(bp.ParallaxDepth, 0.01f) / focal };

		// 射影（行ベクトル）: x' = c·x − s·y、y' = s·x + c·y を px（× f / z）にしてから NDC へ。
		// 3 行目（z の行）に w = z とレンズシフトを入れる。深度は使わない（PSO で深度テストなし）が、0..1 に収めておく
		float const c{ banner_.Axis.x };
		float const s{ banner_.Axis.y };
		float const kx{ 2.0f * focal / rtWidth };
		float const ky{ 2.0f * focal / rtHeight };
		float const shiftX{ shiftPx * c * 2.0f / rtWidth };
		float const shiftY{ shiftPx * s * 2.0f / rtHeight };
		constexpr float nearZ{ 0.1f };
		constexpr float farZ{ 1000.0f };
		float const a{ farZ / (farZ - nearZ) };
		float const b{ -nearZ * farZ / (farZ - nearZ) };

		Mat4 const view{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			-pan, 0.0f, 0.0f, 1.0f,
		};
		Mat4 const proj{
			c * kx, s * ky, 0.0f, 0.0f,
			-s * kx, c * ky, 0.0f, 0.0f,
			shiftX, shiftY, a, 1.0f,
			0.0f, 0.0f, b, 0.0f,
		};
		viewProj_ = view * proj;
		viewToWorld_ = Mat4{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			pan, 0.0f, 0.0f, 1.0f,
		};
		return { .Pan{ pan }, .Focal{ focal } };
	}

	void Presenter::ResetParticles() {
		ParticlesNeedReset_ = true;
	}

	void Presenter::UpdateParticles(DX12::CommandList const& cmdList_, float dt_) {
		if (ParticlesNeedReset_) {
			for (auto& side : ParticleSystems_) {
				for (auto& system : side) { system->Clear(); }
			}
			for (auto& side : ParticleCounts_) { side.fill(0); }
			ParticlesNeedReset_ = false;
		}

		Pose const pose{ Evaluate(Config, Preview_ ? PreviewTime_ : Time_, ScreenSize_) };
		std::array<Banner const*, 2> const banners{ &pose.Boss, &pose.Player };
		auto const& groups{ Config.Particles.Groups };

		float const rtWidth{ static_cast<float>(PortraitWidth_) };
		float const rtHeight{ static_cast<float>(PortraitHeight_) };
		float const halfX{ 0.5f * std::sqrt(rtWidth * rtWidth + rtHeight * rtHeight) + ParticleMarginPx };
		float const halfY{ 0.5f * std::max(Config.Placement.Thickness, 0.0f) + ParticleMarginPx };

		for (uint32_t side{ 0U }; side < 2U; ++side) {
			Mat4 viewProj{};
			Mat4 viewToWorld{};
			ParticleCamera const camera{ MakeParticleCamera(*banners[side], viewProj, viewToWorld) };
			UB_ParticleVP_[side].Store(&viewProj, sizeof(Mat4), 0LLU);
			UB_ParticleViewToWorld_[side].Store(&viewToWorld, sizeof(Mat4), 0LLU);

			for (uint32_t g{ 0U }; g < MaxParticleGroups; ++g) {
				auto& system{ *ParticleSystems_[side][g] };
				int& count{ ParticleCounts_[side][g] };

				if (g >= groups.size()) {
					if (count != 0) { system.Clear(); count = 0; }
					continue;
				}
				ParticleGroup const& group{ groups[g] };
				ParticleUpdateContext const ctx{
					.DeltaTime{ dt_ },
					.Pan{ camera.Pan },
					.Focal{ camera.Focal },
					.HalfX{ halfX },
					.CenterY{ Config.Placement.Offset },	// B も同じ値（B のワールドの y は B の法線の向き）
					.HalfY{ halfY },
					.Color{ SRGBToLinear(group.Color) },
					.Atlas{ static_cast<uint32_t>(std::clamp(group.Atlas, 0, 7)) },
					.Group{ &group },
					.Rng{ &Random_ },
				};

				// 数が変わったら撒き直す（年齢をばらして、最初から流れている状態にする）
				int const target{ std::clamp(group.Count, 0, static_cast<int>(MaxParticlesPerGroup)) };
				if (count != target) {
					system.Clear();
					for (int i{ 0 }; i < target; ++i) {
						BannerParticle p{};
						RespawnParticle(p, ctx, true);
						system.Emit(std::move(p));
					}
					count = target;
				}
				if (count > 0 && group.Enabled) {
					system.Update(cmdList_, viewToWorld, UpdateBannerParticle, &ctx);
				}
			}
		}
	}

	void Presenter::Render(DX12::CommandList const& cmdList_) {
		ApplyShapeAspect();
		float time{ Time_ };
		if (Preview_) { time = PreviewTime_; }
		else if (State_ != State::Playing) { return; }

		Pose const pose{ Evaluate(Config, time, ScreenSize_) };
		Float2 const size{ static_cast<float>(PortraitWidth_), static_cast<float>(PortraitHeight_) };
		std::array<Content, 2> const contents{
			Content{ .SRV{ PortraitSRVs_.GPUHandle(0U) }, .Size{ size }, .Enabled{ Config.Boss.Enabled || Config.Particles.Enabled } },
			Content{ .SRV{ PortraitSRVs_.GPUHandle(1U) }, .Size{ size }, .Enabled{ Config.Player.Enabled || Config.Particles.Enabled } },
		};
		ShapeInput const shape{
			.SRV{ ShapeSRV_.GPUHandle(0U) },
			.Texels{ ShapeTexels_ },
			.Range{ ShapeBakedRange_ },
			.Enabled{ HasShape_ },
			.UseOpacity{ Config.Shape.UseOpacity },
			.EdgeUsesOpacity{ Config.Shape.EdgeUsesOpacity },
		};
		Decoration const decoration{
			.Background{ &Config.Background },
			.Glow{ &Config.Glow },
			.Time{ ArtTime_ },
			// SDF の距離の範囲（px）: Range テクセル × 1 テクセルの px（帯の長さ / テクスチャの幅）
			.RangePx{ HasShape_ ? (ShapeBakedRange_ * std::max(Config.Placement.Length, 0.0f) / std::max(ShapeTexels_.x, 1.0f)) : 0.0f },
		};
		Renderer_.Render(cmdList_, pose, Config.Placement, Config.Appearance, contents, shape, decoration, ScreenSize_);

		// 文字はバナーの上に描く
		std::array<LetterPiece, 2> const pieces{ EvaluateLetters(Config, time, ScreenSize_, LetterHalfSizes(), LetterGlowMargin()) };
		std::array<LetterShape, 2> letterShapes{};
		for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
			letterShapes[idx] = LetterShape{
				.SRV{ LetterSRVs_.GPUHandle(idx) },
				.Texels{ LetterTexels_[idx] },
				.Range{ LetterRange_[idx] },
				.Enabled{ HasLetter_[idx] },
			};
		}
		Renderer_.RenderLetters(cmdList_, pieces, Config.Letters, letterShapes, Config.Appearance.DebugView, ScreenSize_);
	}

	//====	======	======	======	======	====//
	//	バナーの形（画像 → SDF）				//
	//====	======	======	======	======	====//

	void Presenter::LoadShape() {
		if (Context_ == nullptr) { return; }
		auto const& device{ Context_->Device() };
		ShapeSettings const& cfg{ Config.Shape };

		HasShape_ = false;
		ShapeAspect_ = 0.0f;

		std::string error{};
		std::optional<SDF::Baked> baked{};
		if (!cfg.Image.empty()) {
			SDF::BakeOptions const options{
				.Width{ static_cast<uint32_t>(std::clamp(cfg.Resolution, 16, 8192)) },
				.Height{ 0U },
				.Range{ std::max(cfg.Range, 0.5f) },
			};
			SDF::ImageOptions const image{
				.Source{ SDF::Channel::Alpha },
				.Threshold{ std::clamp(cfg.Threshold, 0.001f, 0.999f) },
				.Invert{ false },
			};
			baked = SDF::BakeFromImage(cfg.Image, image, options, error);
		}

		std::unique_ptr<DX12::DefaultTexture2D> texture{};
		if (baked) {
			// R = 距離、G = 元の α（濃さ）
			std::vector<uint8_t> const texels{ SDF::EncodeUNorm8x2(baked->Distance, baked->Range, baked->Coverage) };
			texture = SDF::CreateTexture(
				*Context_, SDF::TexelFormat::R8G8, baked->Distance.Width, baked->Distance.Height, texels, "VsBanner.Shape.SDF"
			);
			if (!texture) { error = "SDF のテクスチャを作れませんでした"; }
		}

		if (texture) {
			ShapeTexture_ = std::move(texture);
			ShapeTexels_ = { static_cast<float>(baked->Distance.Width), static_cast<float>(baked->Distance.Height) };
			ShapeBakedRange_ = baked->Range;
			ShapeAspect_ = static_cast<float>(baked->SourceHeight) / static_cast<float>(std::max(baked->SourceWidth, 1U));
			HasShape_ = true;
			ShapeStatus_ =
				"Loaded: " + std::to_string(baked->SourceWidth) + "x" + std::to_string(baked->SourceHeight) +
				" -> SDF " + std::to_string(baked->Distance.Width) + "x" + std::to_string(baked->Distance.Height);
		}
		else {
			// テーブルは毎回設定するので、使わないときも有効な SRV を置いておく（1×1、値は使われない）
			uint8_t const outside[2]{ 0U, 255U };
			ShapeTexture_ = SDF::CreateTexture(*Context_, SDF::TexelFormat::R8G8, 1U, 1U, outside, "VsBanner.Shape.Fallback");
			ShapeTexels_ = { 1.0f, 1.0f };
			ShapeBakedRange_ = 1.0f;
			ShapeStatus_ = cfg.Image.empty() ? std::string{ "No image (rounded rectangle)." } : ("Failed: " + error);
		}
		DX12::SRV<void>::Create(device, ShapeSRV_.CPUHandle(0U), *ShapeTexture_);
		ApplyShapeAspect();
	}

	void Presenter::ApplyShapeAspect() {
		if (HasShape_ && Config.Shape.LockAspect && ShapeAspect_ > 0.0f) {
			Config.Placement.Thickness = std::max(Config.Placement.Length, 0.0f) * ShapeAspect_;
		}
	}

	void Presenter::LoadLetters() {
		if (Context_ == nullptr) { return; }
		auto const& device{ Context_->Device() };
		LetterSettings const& cfg{ Config.Letters };
		std::array<std::string const*, 2> const images{ &cfg.ImageV, &cfg.ImageS };
		std::array<char const*, 2> const names{ "V", "S" };

		LetterStatus_.clear();
		for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
			HasLetter_[idx] = false;
			LetterAspect_[idx] = 1.0f;

			std::string error{};
			std::optional<SDF::Baked> baked{};
			if (!images[idx]->empty()) {
				SDF::BakeOptions const options{
					.Width{ static_cast<uint32_t>(std::clamp(cfg.Resolution, 16, 4096)) },
					.Height{ 0U },
					.Range{ std::max(cfg.Range, 0.5f) },
				};
				SDF::ImageOptions const image{
					.Source{ SDF::Channel::Alpha },
					.Threshold{ std::clamp(cfg.Threshold, 0.001f, 0.999f) },
					.Invert{ false },
				};
				baked = SDF::BakeFromImage(*images[idx], image, options, error);
			}
			else {
				error = "no image";
			}

			std::unique_ptr<DX12::DefaultTexture2D> texture{};
			std::string const debugName{ std::string{ "VsBanner.Letter." } + names[idx] };
			if (baked) {
				std::vector<uint8_t> const texels{ SDF::EncodeUNorm8x2(baked->Distance, baked->Range, baked->Coverage) };
				texture = SDF::CreateTexture(
					*Context_, SDF::TexelFormat::R8G8, baked->Distance.Width, baked->Distance.Height, texels, debugName.c_str()
				);
				if (!texture) { error = "cannot create texture"; }
			}

			if (texture) {
				LetterTextures_[idx] = std::move(texture);
				LetterTexels_[idx] = { static_cast<float>(baked->Distance.Width), static_cast<float>(baked->Distance.Height) };
				LetterRange_[idx] = baked->Range;
				LetterAspect_[idx] = static_cast<float>(baked->SourceWidth) / static_cast<float>(std::max(baked->SourceHeight, 1U));
				HasLetter_[idx] = true;
				LetterStatus_ += std::string{ names[idx] } + ": " + std::to_string(baked->SourceWidth) + "x" + std::to_string(baked->SourceHeight) +
					" -> SDF " + std::to_string(baked->Distance.Width) + "x" + std::to_string(baked->Distance.Height) + "\n";
			}
			else {
				// テーブルは毎回設定するので、使わないときも有効な SRV を置いておく（形が無いので長方形で描かれる）
				uint8_t const outside[2]{ 0U, 255U };
				LetterTextures_[idx] = SDF::CreateTexture(*Context_, SDF::TexelFormat::R8G8, 1U, 1U, outside, (debugName + ".Fallback").c_str());
				LetterTexels_[idx] = { 1.0f, 1.0f };
				LetterRange_[idx] = 1.0f;
				LetterStatus_ += std::string{ names[idx] } + ": failed (" + error + "), drawn as a box\n";
			}
			DX12::SRV<void>::Create(device, LetterSRVs_.CPUHandle(idx), *LetterTextures_[idx]);
		}
	}

	auto Presenter::LetterHalfSizes() const -> std::array<Float2, 2> {
		float const halfHeight{ std::max(Config.Letters.Height, 0.0f) * 0.5f };
		return {
			Float2{ halfHeight * LetterAspect_[0], halfHeight },
			Float2{ halfHeight * LetterAspect_[1], halfHeight },
		};
	}

	auto Presenter::LetterGlowMargin() const -> float {
		GlowSettings const& glow{ Config.Letters.Glow };
		return 2.0f + (glow.Enabled ? std::max(glow.Width, 0.0f) : 0.0f);
	}

	//====	======	======	======	======	====//
	//	保存・読み込み							//
	//====	======	======	======	======	====//

	namespace {
		auto ToJSON(Float3 const& v_) -> JSON { return JSON::array({ v_.x, v_.y, v_.z }); }

		void Read(JSON const& obj_, char const* key_, float& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = itr->get<float>(); }
		}
		void Read(JSON const& obj_, char const* key_, int& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number_integer()) { out_ = itr->get<int>(); }
		}
		void Read(JSON const& obj_, char const* key_, bool& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_boolean()) { out_ = itr->get<bool>(); }
		}
		void Read(JSON const& obj_, char const* key_, Float3& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr == obj_.end() || !itr->is_array() || itr->size() != 3U) { return; }
			for (auto const& e : *itr) { if (!e.is_number()) { return; } }
			out_ = { (*itr)[0].get<float>(), (*itr)[1].get<float>(), (*itr)[2].get<float>() };
		}
		void Read(JSON const& obj_, char const* key_, Float2& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr == obj_.end() || !itr->is_array() || itr->size() != 2U) { return; }
			for (auto const& e : *itr) { if (!e.is_number()) { return; } }
			out_ = { (*itr)[0].get<float>(), (*itr)[1].get<float>() };
		}
		void Read(JSON const& obj_, char const* key_, std::string& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_string()) { out_ = itr->get<std::string>(); }
		}
		auto ToJSON(Float2 const& v_) -> JSON { return JSON::array({ v_.x, v_.y }); }

		void ReadPortrait(JSON const& j_, Portrait& p_) {
			Read(j_, "Enabled", p_.Enabled);
			Read(j_, "ShowWeapon", p_.ShowWeapon);
			Read(j_, "Animation", p_.Animation);
			Read(j_, "AnimationSpeed", p_.AnimationSpeed);
			Read(j_, "Yaw", p_.Yaw);
			Read(j_, "CameraYaw", p_.CameraYaw);
			Read(j_, "CameraPitch", p_.CameraPitch);
			Read(j_, "CameraDistance", p_.CameraDistance);
			Read(j_, "TargetY", p_.TargetY);
			Read(j_, "FovY", p_.FovY);
			Read(j_, "BodyColor", p_.BodyColor);
			auto const itr{ j_.find("Lighting") };
			if (itr == j_.end() || !itr->is_object()) { return; }
			JSON const& l{ *itr };
			ShowcaseLighting& s{ p_.Lighting };
			Read(l, "FireColor", s.FireColor);
			Read(l, "FireIntensity", s.FireIntensity);
			Read(l, "FireHeight", s.FireHeight);
			Read(l, "UnderWeight", s.UnderWeight);
			Read(l, "SkyColor", s.SkyColor);
			Read(l, "GroundColor", s.GroundColor);
			Read(l, "AmbientIntensity", s.AmbientIntensity);
			Read(l, "RimColor", s.RimColor);
			Read(l, "RimIntensity", s.RimIntensity);
			Read(l, "RimThreshold", s.RimThreshold);
			Read(l, "RimSoftness", s.RimSoftness);
			Read(l, "CelThreshold", s.CelThreshold);
			Read(l, "CelSoftness", s.CelSoftness);
			Read(l, "ShadowLevel", s.ShadowLevel);
		}

		auto WritePortrait(Portrait const& p_) -> JSON {
			ShowcaseLighting const& s{ p_.Lighting };
			return JSON{
				{ "Enabled", p_.Enabled },
				{ "ShowWeapon", p_.ShowWeapon },
				{ "Animation", p_.Animation },
				{ "AnimationSpeed", p_.AnimationSpeed },
				{ "Yaw", p_.Yaw },
				{ "CameraYaw", p_.CameraYaw },
				{ "CameraPitch", p_.CameraPitch },
				{ "CameraDistance", p_.CameraDistance },
				{ "TargetY", p_.TargetY },
				{ "FovY", p_.FovY },
				{ "BodyColor", ToJSON(p_.BodyColor) },
				{ "Lighting", JSON{
					{ "FireColor", ToJSON(s.FireColor) },
					{ "FireIntensity", s.FireIntensity },
					{ "FireHeight", s.FireHeight },
					{ "UnderWeight", s.UnderWeight },
					{ "SkyColor", ToJSON(s.SkyColor) },
					{ "GroundColor", ToJSON(s.GroundColor) },
					{ "AmbientIntensity", s.AmbientIntensity },
					{ "RimColor", ToJSON(s.RimColor) },
					{ "RimIntensity", s.RimIntensity },
					{ "RimThreshold", s.RimThreshold },
					{ "RimSoftness", s.RimSoftness },
					{ "CelThreshold", s.CelThreshold },
					{ "CelSoftness", s.CelSoftness },
					{ "ShadowLevel", s.ShadowLevel },
				} },
			};
		}

		void ReadGroup(JSON const& j_, ParticleGroup& g_) {
			Read(j_, "Name", g_.Name);
			Read(j_, "Enabled", g_.Enabled);
			Read(j_, "Front", g_.Front);
			Read(j_, "Count", g_.Count);
			Read(j_, "DepthMin", g_.DepthMin);
			Read(j_, "DepthMax", g_.DepthMax);
			Read(j_, "SizeMin", g_.SizeMin);
			Read(j_, "SizeMax", g_.SizeMax);
			Read(j_, "SpeedMin", g_.SpeedMin);
			Read(j_, "SpeedMax", g_.SpeedMax);
			Read(j_, "CrossSpeed", g_.CrossSpeed);
			Read(j_, "Stretch", g_.Stretch);
			Read(j_, "Spin", g_.Spin);
			Read(j_, "Atlas", g_.Atlas);
			Read(j_, "Color", g_.Color);
			Read(j_, "Alpha", g_.Alpha);
			Read(j_, "LifeMin", g_.LifeMin);
			Read(j_, "LifeMax", g_.LifeMax);
		}

		auto WriteGroup(ParticleGroup const& g_) -> JSON {
			return JSON{
				{ "Name", g_.Name },
				{ "Enabled", g_.Enabled },
				{ "Front", g_.Front },
				{ "Count", g_.Count },
				{ "DepthMin", g_.DepthMin },
				{ "DepthMax", g_.DepthMax },
				{ "SizeMin", g_.SizeMin },
				{ "SizeMax", g_.SizeMax },
				{ "SpeedMin", g_.SpeedMin },
				{ "SpeedMax", g_.SpeedMax },
				{ "CrossSpeed", g_.CrossSpeed },
				{ "Stretch", g_.Stretch },
				{ "Spin", g_.Spin },
				{ "Atlas", g_.Atlas },
				{ "Color", ToJSON(g_.Color) },
				{ "Alpha", g_.Alpha },
				{ "LifeMin", g_.LifeMin },
				{ "LifeMax", g_.LifeMax },
			};
		}

		void ReadPalette(JSON const& j_, CosinePalette& p_) {
			Read(j_, "Bias", p_.Bias);
			Read(j_, "Amplitude", p_.Amplitude);
			Read(j_, "Frequency", p_.Frequency);
			Read(j_, "Phase", p_.Phase);
		}
		auto WritePalette(CosinePalette const& p_) -> JSON {
			return JSON{
				{ "Bias", ToJSON(p_.Bias) },
				{ "Amplitude", ToJSON(p_.Amplitude) },
				{ "Frequency", ToJSON(p_.Frequency) },
				{ "Phase", ToJSON(p_.Phase) },
			};
		}

		void ReadGlow(JSON const& j_, GlowSettings& g_) {
			Read(j_, "Enabled", g_.Enabled);
			Read(j_, "Color", g_.Color);
			Read(j_, "Intensity", g_.Intensity);
			Read(j_, "Width", g_.Width);
			Read(j_, "Falloff", g_.Falloff);
		}
		auto WriteGlow(GlowSettings const& g_) -> JSON {
			return JSON{
				{ "Enabled", g_.Enabled },
				{ "Color", ToJSON(g_.Color) },
				{ "Intensity", g_.Intensity },
				{ "Width", g_.Width },
				{ "Falloff", g_.Falloff },
			};
		}

		auto Section(JSON const& root_, char const* key_) -> JSON const* {
			auto const itr{ root_.find(key_) };
			return (itr != root_.end() && itr->is_object()) ? &*itr : nullptr;
		}
	}

	auto Presenter::Load() -> bool {
		std::ifstream ifs{ ConfigPath };
		if (!ifs.is_open()) { Status_ = "No config file (using defaults)."; return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { Status_ = "Config is not valid JSON."; return false; }

		// あるキーだけ上書きする
		if (JSON const* j{ Section(root, "Layout") }) {
			Layout& s{ Config.Placement };
			Read(*j, "AngleDeg", s.AngleDeg);
			Read(*j, "Thickness", s.Thickness);
			Read(*j, "Length", s.Length);
			Read(*j, "Offset", s.Offset);
			Read(*j, "Shift", s.Shift);
			Read(*j, "CornerRadius", s.CornerRadius);
			Read(*j, "ContentOffset", s.ContentOffset);
		}
		if (JSON const* j{ Section(root, "Timing") }) {
			Timing& s{ Config.Time };
			Read(*j, "Delay", s.Delay);
			Read(*j, "InTime", s.InTime);
			Read(*j, "HoldTime", s.HoldTime);
			Read(*j, "OutTime", s.OutTime);
			Read(*j, "Overshoot", s.Overshoot);
			Read(*j, "Drift", s.Drift);
			Read(*j, "PassThrough", s.PassThrough);
		}
		if (JSON const* j{ Section(root, "Look") }) {
			Look& s{ Config.Appearance };
			Read(*j, "ColorBoss", s.ColorBoss);
			Read(*j, "ColorPlayer", s.ColorPlayer);
			Read(*j, "EdgeColor", s.EdgeColor);
			Read(*j, "EdgeWidth", s.EdgeWidth);
			Read(*j, "Opacity", s.Opacity);
			Read(*j, "ContentOpacity", s.ContentOpacity);
			Read(*j, "ContentFeather", s.ContentFeather);
		}
		if (JSON const* j{ Section(root, "Portraits") }) {
			if (JSON const* b{ Section(*j, "Boss") }) { ReadPortrait(*b, Config.Boss); }
			if (JSON const* p{ Section(*j, "Player") }) { ReadPortrait(*p, Config.Player); }
		}
		if (JSON const* j{ Section(root, "Particles") }) {
			Read(*j, "Enabled", Config.Particles.Enabled);
			Read(*j, "FovY", Config.Particles.FovY);
			Read(*j, "Parallax", Config.Particles.Parallax);
			Read(*j, "ParallaxDepth", Config.Particles.ParallaxDepth);
			// グループはキーがあれば丸ごと置き換える（各要素は初期値の ParticleGroup に上書き）
			auto const itr{ j->find("Groups") };
			if (itr != j->end() && itr->is_array()) {
				std::vector<ParticleGroup> groups{};
				for (JSON const& e : *itr) {
					if (!e.is_object()) { continue; }
					ReadGroup(e, groups.emplace_back());
				}
				if (groups.size() > MaxParticleGroups) { groups.resize(MaxParticleGroups); }
				Config.Particles.Groups = std::move(groups);
			}
		}
		if (JSON const* j{ Section(root, "Shape") }) {
			Read(*j, "Image", Config.Shape.Image);
			Read(*j, "LockAspect", Config.Shape.LockAspect);
			Read(*j, "Resolution", Config.Shape.Resolution);
			Read(*j, "Range", Config.Shape.Range);
			Read(*j, "Threshold", Config.Shape.Threshold);
			Read(*j, "UseOpacity", Config.Shape.UseOpacity);
			Read(*j, "EdgeUsesOpacity", Config.Shape.EdgeUsesOpacity);
		}
		if (JSON const* j{ Section(root, "Background") }) {
			BackgroundArt& b{ Config.Background };
			Read(*j, "Enabled", b.Enabled);
			Read(*j, "Speed", b.Speed);
			Read(*j, "Scale", b.Scale);
			Read(*j, "Origin", b.Origin);
			Read(*j, "OutputIsGamma", b.OutputIsGamma);
			if (JSON const* q{ Section(*j, "Boss") }) { ReadPalette(*q, b.Boss); }
			if (JSON const* q{ Section(*j, "Player") }) { ReadPalette(*q, b.Player); }
		}
		if (JSON const* j{ Section(root, "Glow") }) {
			GlowSettings& g{ Config.Glow };
			Read(*j, "Enabled", g.Enabled);
			Read(*j, "Color", g.Color);
			Read(*j, "Intensity", g.Intensity);
			Read(*j, "Width", g.Width);
			Read(*j, "Falloff", g.Falloff);
		}
		if (JSON const* j{ Section(root, "Letters") }) {
			LetterSettings& v{ Config.Letters };
			Read(*j, "Enabled", v.Enabled);
			Read(*j, "ImageV", v.ImageV);
			Read(*j, "ImageS", v.ImageS);
			Read(*j, "Resolution", v.Resolution);
			Read(*j, "Range", v.Range);
			Read(*j, "Threshold", v.Threshold);
			Read(*j, "Height", v.Height);
			Read(*j, "HoldOffset", v.HoldOffset);
			Read(*j, "EnterFromDeg", v.EnterFromDeg);
			Read(*j, "ExitToDeg", v.ExitToDeg);
			Read(*j, "InDelay", v.InDelay);
			Read(*j, "InTime", v.InTime);
			Read(*j, "Overshoot", v.Overshoot);
			Read(*j, "Stagger", v.Stagger);
			Read(*j, "OutLead", v.OutLead);
			Read(*j, "OutTime", v.OutTime);
			Read(*j, "FlashTime", v.FlashTime);
			Read(*j, "FlashIntensity", v.FlashIntensity);
			Read(*j, "FlashColor", v.FlashColor);
			Read(*j, "Color", v.Color);
			Read(*j, "EdgeColor", v.EdgeColor);
			Read(*j, "EdgeWidth", v.EdgeWidth);
			Read(*j, "Opacity", v.Opacity);
			if (JSON const* g{ Section(*j, "Glow") }) { ReadGlow(*g, v.Glow); }
		}
		ResetParticles();
		LoadShape();
		LoadLetters();
		Status_ = "Loaded.";
		return true;
	}

	auto Presenter::Save() -> bool {
		Layout const& l{ Config.Placement };
		Timing const& t{ Config.Time };
		Look const& k{ Config.Appearance };

		JSON root = JSON::object();
		root["Layout"] = JSON{
			{ "AngleDeg", l.AngleDeg },
			{ "Thickness", l.Thickness },
			{ "Length", l.Length },
			{ "Offset", l.Offset },
			{ "Shift", l.Shift },
			{ "CornerRadius", l.CornerRadius },
			{ "ContentOffset", ToJSON(l.ContentOffset) },
		};
		root["Timing"] = JSON{
			{ "Delay", t.Delay },
			{ "InTime", t.InTime },
			{ "HoldTime", t.HoldTime },
			{ "OutTime", t.OutTime },
			{ "Overshoot", t.Overshoot },
			{ "Drift", t.Drift },
			{ "PassThrough", t.PassThrough },
		};
		// DebugView は保存しない（確認用の一時的な表示なので）
		root["Look"] = JSON{
			{ "ColorBoss", ToJSON(k.ColorBoss) },
			{ "ColorPlayer", ToJSON(k.ColorPlayer) },
			{ "EdgeColor", ToJSON(k.EdgeColor) },
			{ "EdgeWidth", k.EdgeWidth },
			{ "Opacity", k.Opacity },
			{ "ContentOpacity", k.ContentOpacity },
			{ "ContentFeather", k.ContentFeather },
		};
		JSON groups = JSON::array();
		for (ParticleGroup const& g : Config.Particles.Groups) { groups.push_back(WriteGroup(g)); }
		root["Particles"] = JSON{
			{ "Enabled", Config.Particles.Enabled },
			{ "FovY", Config.Particles.FovY },
			{ "Parallax", Config.Particles.Parallax },
			{ "ParallaxDepth", Config.Particles.ParallaxDepth },
			{ "Groups", groups },
		};
		root["Shape"] = JSON{
			{ "Image", Config.Shape.Image },
			{ "LockAspect", Config.Shape.LockAspect },
			{ "Resolution", Config.Shape.Resolution },
			{ "Range", Config.Shape.Range },
			{ "Threshold", Config.Shape.Threshold },
			{ "UseOpacity", Config.Shape.UseOpacity },
			{ "EdgeUsesOpacity", Config.Shape.EdgeUsesOpacity },
		};
		root["Background"] = JSON{
			{ "Enabled", Config.Background.Enabled },
			{ "Speed", Config.Background.Speed },
			{ "Scale", Config.Background.Scale },
			{ "Origin", ToJSON(Config.Background.Origin) },
			{ "OutputIsGamma", Config.Background.OutputIsGamma },
			{ "Boss", WritePalette(Config.Background.Boss) },
			{ "Player", WritePalette(Config.Background.Player) },
		};
		root["Glow"] = JSON{
			{ "Enabled", Config.Glow.Enabled },
			{ "Color", ToJSON(Config.Glow.Color) },
			{ "Intensity", Config.Glow.Intensity },
			{ "Width", Config.Glow.Width },
			{ "Falloff", Config.Glow.Falloff },
		};
		LetterSettings const& v{ Config.Letters };
		root["Letters"] = JSON{
			{ "Enabled", v.Enabled },
			{ "ImageV", v.ImageV },
			{ "ImageS", v.ImageS },
			{ "Resolution", v.Resolution },
			{ "Range", v.Range },
			{ "Threshold", v.Threshold },
			{ "Height", v.Height },
			{ "HoldOffset", ToJSON(v.HoldOffset) },
			{ "EnterFromDeg", v.EnterFromDeg },
			{ "ExitToDeg", v.ExitToDeg },
			{ "InDelay", v.InDelay },
			{ "InTime", v.InTime },
			{ "Overshoot", v.Overshoot },
			{ "Stagger", v.Stagger },
			{ "OutLead", v.OutLead },
			{ "OutTime", v.OutTime },
			{ "FlashTime", v.FlashTime },
			{ "FlashIntensity", v.FlashIntensity },
			{ "FlashColor", ToJSON(v.FlashColor) },
			{ "Color", ToJSON(v.Color) },
			{ "EdgeColor", ToJSON(v.EdgeColor) },
			{ "EdgeWidth", v.EdgeWidth },
			{ "Opacity", v.Opacity },
			{ "Glow", WriteGlow(v.Glow) },
		};
		root["Portraits"] = JSON{
			{ "Boss", WritePortrait(Config.Boss) },
			{ "Player", WritePortrait(Config.Player) },
		};

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

	auto Presenter::DrawEditor() -> bool {
		bool replay{ false };
#if defined(_DEBUG)
		if (!ImGui::Begin("VS Banner")) {
			ImGui::End();
			return false;
		}

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { Config = Settings{}; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		ImGui::SeparatorText("Playback");
		float const total{ TotalTime(Config) };
		char const* const stateName{
			(State_ == State::Playing) ? "Playing" : (State_ == State::Finished) ? "Finished" : "Idle"
		};
		ImGui::Text("%s  %.2f / %.2f s", stateName, Time_, total);
		if (ImGui::Button("Replay (back to STARTUP)")) { replay = true; }
		ImGui::Checkbox("Preview (freeze time)", &Preview_);
		if (Preview_) {
			ImGui::SliderFloat("Preview time", &PreviewTime_, 0.0f, total, "%.3f s");
			ImGui::TextDisabled("While previewing, STARTUP does not advance.");
		}

		ImGui::SeparatorText("Layout (px, 1280x720)");
		Layout& l{ Config.Placement };
		ImGui::DragFloat("Angle (deg)", &l.AngleDeg, 0.1f, -45.0f, 45.0f, "%.1f");
		ImGui::DragFloat("Thickness", &l.Thickness, 1.0f, 0.0f, 720.0f, "%.0f");
		ImGui::DragFloat("Length", &l.Length, 1.0f, 0.0f, 4000.0f, "%.0f");
		ImGui::DragFloat("Offset", &l.Offset, 1.0f, -720.0f, 720.0f, "%.0f");
		ImGui::DragFloat("Shift", &l.Shift, 1.0f, -2000.0f, 2000.0f, "%.0f");
		ImGui::DragFloat("Corner radius", &l.CornerRadius, 0.5f, 0.0f, 360.0f, "%.1f");
		ImGui::TextDisabled("Gap between banners: %.0f px", 2.0f * l.Offset - l.Thickness);
		ImGui::DragFloat2("Character offset (boss)", &l.ContentOffset.x, 1.0f, -2000.0f, 2000.0f, "%.0f");
		ImGui::TextDisabled("Player uses (-x, -y). Portrait RT %ux%u px (screen + %u px each side).", PortraitWidth_, PortraitHeight_, PortraitMargin);

		ImGui::SeparatorText("Timing (s)");
		Timing& t{ Config.Time };
		ImGui::DragFloat("Delay", &t.Delay, 0.005f, 0.0f, 5.0f, "%.3f");
		ImGui::DragFloat("In", &t.InTime, 0.005f, 0.0f, 5.0f, "%.3f");
		ImGui::DragFloat("Hold", &t.HoldTime, 0.005f, 0.0f, 10.0f, "%.3f");
		ImGui::DragFloat("Out", &t.OutTime, 0.005f, 0.0f, 5.0f, "%.3f");
		ImGui::DragFloat("Overshoot", &t.Overshoot, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::DragFloat("Drift (px/s)", &t.Drift, 0.5f, -500.0f, 500.0f, "%.1f");
		ImGui::Checkbox("Pass through (off = go back)", &t.PassThrough);

		ImGui::SeparatorText("Look");
		Look& k{ Config.Appearance };
		ImGui::ColorEdit3("Boss (sRGB)", &k.ColorBoss.x);
		ImGui::ColorEdit3("Player (sRGB)", &k.ColorPlayer.x);
		ImGui::ColorEdit3("Edge (sRGB)", &k.EdgeColor.x);
		ImGui::DragFloat("Edge width", &k.EdgeWidth, 0.1f, 0.0f, 64.0f, "%.1f");
		ImGui::SliderFloat("Opacity", &k.Opacity, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Content opacity", &k.ContentOpacity, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat("Content feather", &k.ContentFeather, 0.5f, 0.0f, static_cast<float>(PortraitMargin), "%.0f px");
		ImGui::Combo("Debug view", &k.DebugView, "Normal\0Content UV\0Distance\0");

		DrawShapeEditor();
		DrawDecorationEditor();
		DrawLetterEditor();
		DrawParticleEditor();
		DrawPortraitEditor("Portrait: Boss", Config.Boss, 0U);
		DrawPortraitEditor("Portrait: Player", Config.Player, 1U);
		if (ImGui::Button("Player = mirror of Boss (camera / yaw)")) {
			Portrait& b{ Config.Boss };
			Portrait& p{ Config.Player };
			p.Yaw = -b.Yaw;
			p.CameraYaw = -b.CameraYaw;
			p.CameraPitch = b.CameraPitch;
			p.CameraDistance = b.CameraDistance;
			p.TargetY = b.TargetY;
			p.FovY = b.FovY;
		}

		ImGui::End();
#endif
		return replay;
	}

	void Presenter::DrawShapeEditor() {
#if defined(_DEBUG)
		if (!ImGui::CollapsingHeader("Shape (image -> SDF)")) { return; }
		ImGui::PushID("Shape");

		ShapeSettings& cfg{ Config.Shape };
		char path[260]{};
		cfg.Image.copy(path, sizeof(path) - 1U);
		if (ImGui::InputText("Image (empty = rounded rect)", path, sizeof(path))) { cfg.Image = path; }
		ImGui::Checkbox("Lock thickness to image aspect", &cfg.LockAspect);
		ImGui::SliderInt("SDF width (texels)", &cfg.Resolution, 64, 4096);
		ImGui::DragFloat("SDF range (texels)", &cfg.Range, 0.1f, 0.5f, 64.0f, "%.1f");
		ImGui::SliderFloat("Outline threshold (alpha)", &cfg.Threshold, 0.001f, 0.999f, "%.3f");
		if (ImGui::Button("Reload shape")) { LoadShape(); }
		ImGui::SameLine();
		ImGui::TextDisabled("(Image / width / range / threshold apply on reload)");
		ImGui::Checkbox("Use image alpha as opacity", &cfg.UseOpacity);
		ImGui::SameLine();
		ImGui::Checkbox("Fade outline with opacity", &cfg.EdgeUsesOpacity);
		ImGui::TextUnformatted(ShapeStatus_.c_str());
		if (HasShape_) {
			float const texelPx{ std::max(Config.Placement.Length, 0.0f) / std::max(ShapeTexels_.x, 1.0f) };
			ImGui::TextDisabled(
				"1 texel = %.2f px, distance range = %.1f px. Right edge of the image leads; top = banner normal.",
				texelPx, ShapeBakedRange_ * texelPx
			);
			if (cfg.LockAspect) { ImGui::TextDisabled("Thickness follows Length x %.4f.", ShapeAspect_); }
		}
		ImGui::TextDisabled("Debug view > Distance shows the contours.");

		ImGui::PopID();
#endif
	}

	void Presenter::DrawDecorationEditor() {
#if defined(_DEBUG)
		if (ImGui::CollapsingHeader("Glow (outside the shape)")) {
			ImGui::PushID("Glow");
			GlowSettings& g{ Config.Glow };
			ImGui::Checkbox("Enabled", &g.Enabled);
			ImGui::ColorEdit3("Color (sRGB)", &g.Color.x);
			ImGui::DragFloat("Intensity", &g.Intensity, 0.01f, 0.0f, 10.0f, "%.2f");
			ImGui::DragFloat("Width (px)", &g.Width, 0.1f, 0.0f, 200.0f, "%.1f");
			ImGui::DragFloat("Falloff", &g.Falloff, 0.01f, 0.01f, 8.0f, "%.2f");
			if (HasShape_) {
				float const rangePx{ ShapeBakedRange_ * std::max(Config.Placement.Length, 0.0f) / std::max(ShapeTexels_.x, 1.0f) };
				if (g.Width > rangePx * 0.95f) {
					ImGui::TextColored(ImVec4{ 1.0f, 0.6f, 0.3f, 1.0f }, "Clamped to %.1f px (SDF range). Raise Shape > SDF range and reload.", rangePx * 0.95f);
				}
			}
			ImGui::PopID();
		}
		if (ImGui::CollapsingHeader("Background (shader art)")) {
			ImGui::PushID("Background");
			BackgroundArt& b{ Config.Background };
			ImGui::Checkbox("Enabled (off = flat color)", &b.Enabled);
			ImGui::DragFloat("Speed", &b.Speed, 0.01f, -10.0f, 10.0f, "%.2f");
			ImGui::DragFloat("Scale (px per unit)", &b.Scale, 0.5f, 1.0f, 8192.0f, "%.1f");
			ImGui::DragFloat2("Center offset (px)", &b.Origin.x, 1.0f, -4096.0f, 4096.0f, "%.0f");
			ImGui::Checkbox("Output is gamma (saturate + to linear)", &b.OutputIsGamma);
			ImGui::Text("Time %.2f", ArtTime_);
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset time")) { ArtTime_ = 0.0f; }
			auto const editPalette{
				[](char const* label_, CosinePalette& p_) {
					ImGui::PushID(label_);
					ImGui::SeparatorText(label_);
					ImGui::DragFloat3("Bias", &p_.Bias.x, 0.005f, -2.0f, 2.0f, "%.3f");
					ImGui::DragFloat3("Amplitude", &p_.Amplitude.x, 0.005f, -2.0f, 2.0f, "%.3f");
					ImGui::DragFloat3("Frequency", &p_.Frequency.x, 0.005f, -8.0f, 8.0f, "%.3f");
					ImGui::DragFloat3("Phase", &p_.Phase.x, 0.005f, -2.0f, 2.0f, "%.3f");
					ImGui::PopID();
				}
			};
			editPalette("Palette: Boss", b.Boss);
			editPalette("Palette: Player", b.Player);
			ImGui::TextDisabled("color(t) = Bias + Amplitude * cos(2pi * (Frequency * t + Phase))");
			ImGui::PopID();
		}
#endif
	}

	void Presenter::DrawLetterEditor() {
#if defined(_DEBUG)
		if (!ImGui::CollapsingHeader("Letters (V / S)")) { return; }
		ImGui::PushID("Letters");
		LetterSettings& v{ Config.Letters };

		ImGui::Checkbox("Enabled", &v.Enabled);

		ImGui::SeparatorText("Images (-> SDF)");
		auto const editPath{
			[](char const* label_, std::string& path_) {
				char buffer[260]{};
				path_.copy(buffer, sizeof(buffer) - 1U);
				if (ImGui::InputText(label_, buffer, sizeof(buffer))) { path_ = buffer; }
			}
		};
		editPath("Image V", v.ImageV);
		editPath("Image S", v.ImageS);
		ImGui::SliderInt("SDF width (texels)", &v.Resolution, 32, 2048);
		ImGui::DragFloat("SDF range (texels)", &v.Range, 0.1f, 0.5f, 64.0f, "%.1f");
		ImGui::SliderFloat("Outline threshold (alpha)", &v.Threshold, 0.001f, 0.999f, "%.3f");
		if (ImGui::Button("Reload letters")) { LoadLetters(); }
		ImGui::SameLine();
		ImGui::TextDisabled("(images / width / range / threshold apply on reload)");
		ImGui::TextUnformatted(LetterStatus_.c_str());

		ImGui::SeparatorText("Placement (px, V; S is point-symmetric)");
		ImGui::DragFloat("Height", &v.Height, 1.0f, 0.0f, 720.0f, "%.0f");
		ImGui::DragFloat2("Hold offset (V)", &v.HoldOffset.x, 1.0f, -1280.0f, 1280.0f, "%.0f");
		ImGui::DragFloat("Enter from (deg)", &v.EnterFromDeg, 1.0f, -360.0f, 360.0f, "%.0f");
		ImGui::SameLine();
		ImGui::TextDisabled("180 = left");
		ImGui::DragFloat("Exit to (deg)", &v.ExitToDeg, 1.0f, -360.0f, 360.0f, "%.0f");
		ImGui::SameLine();
		ImGui::TextDisabled("90 = up");
		{
			std::array<Float2, 2> const half{ LetterHalfSizes() };
			ImGui::TextDisabled("V %.0fx%.0f px, S %.0fx%.0f px", half[0].x * 2.0f, half[0].y * 2.0f, half[1].x * 2.0f, half[1].y * 2.0f);
		}

		ImGui::SeparatorText("Timing (s)");
		ImGui::DragFloat("In delay (after banners stop)", &v.InDelay, 0.005f, -3.0f, 3.0f, "%.3f");
		ImGui::DragFloat("In", &v.InTime, 0.005f, 0.0f, 3.0f, "%.3f");
		ImGui::DragFloat("Overshoot", &v.Overshoot, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::DragFloat("Stagger (S after V)", &v.Stagger, 0.005f, -1.0f, 1.0f, "%.3f");
		ImGui::DragFloat("Out lead (before banners leave)", &v.OutLead, 0.005f, -3.0f, 3.0f, "%.3f");
		ImGui::DragFloat("Out", &v.OutTime, 0.005f, 0.0f, 3.0f, "%.3f");
		for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
			LetterSchedule const s{ ScheduleLetter(Config, idx) };
			ImGui::TextDisabled(
				"%s: in %.2f-%.2f  hold  out %.2f-%.2f", (idx == 0U) ? "V" : "S", s.InBegin, s.Land, s.OutBegin, s.OutEnd
			);
		}
		ImGui::TextDisabled("Banners: stop %.2f, leave %.2f, end %.2f",
			std::max(Config.Time.Delay, 0.0f) + std::max(Config.Time.InTime, 0.0f),
			std::max(Config.Time.Delay, 0.0f) + std::max(Config.Time.InTime, 0.0f) + std::max(Config.Time.HoldTime, 0.0f),
			TotalTime(Config.Time)
		);

		ImGui::SeparatorText("Flash on landing");
		ImGui::DragFloat("Flash time", &v.FlashTime, 0.005f, 0.0f, 2.0f, "%.3f s");
		ImGui::DragFloat("Flash intensity", &v.FlashIntensity, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::ColorEdit3("Flash color (sRGB)", &v.FlashColor.x);

		ImGui::SeparatorText("Look");
		ImGui::ColorEdit3("Fill (sRGB)", &v.Color.x);
		ImGui::ColorEdit3("Edge (sRGB)", &v.EdgeColor.x);
		ImGui::DragFloat("Edge width", &v.EdgeWidth, 0.1f, 0.0f, 64.0f, "%.1f");
		ImGui::SliderFloat("Opacity", &v.Opacity, 0.0f, 1.0f, "%.2f");

		ImGui::SeparatorText("Glow");
		GlowSettings& g{ v.Glow };
		ImGui::Checkbox("Glow enabled", &g.Enabled);
		ImGui::ColorEdit3("Glow color (sRGB)", &g.Color.x);
		ImGui::DragFloat("Glow intensity", &g.Intensity, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Glow width (px)", &g.Width, 0.1f, 0.0f, 200.0f, "%.1f");
		ImGui::DragFloat("Glow falloff", &g.Falloff, 0.01f, 0.01f, 8.0f, "%.2f");
		{
			// 文字ごとの SDF の距離の範囲（px）。小さい方で切られる
			std::array<Float2, 2> const half{ LetterHalfSizes() };
			float rangePx{ 1.0e9f };
			for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
				if (!HasLetter_[idx]) { continue; }
				rangePx = std::min(rangePx, LetterRange_[idx] * (2.0f * half[idx].x) / std::max(LetterTexels_[idx].x, 1.0f));
			}
			if (rangePx < 1.0e8f && g.Width > rangePx * 0.95f) {
				ImGui::TextColored(ImVec4{ 1.0f, 0.6f, 0.3f, 1.0f }, "Clamped to %.1f px (SDF range). Raise SDF range and reload.", rangePx * 0.95f);
			}
		}

		ImGui::PopID();
#endif
	}

	void Presenter::DrawParticleEditor() {
#if defined(_DEBUG)
		if (!ImGui::CollapsingHeader("Background particles")) { return; }
		ImGui::PushID("Particles");

		BackgroundParticles& bp{ Config.Particles };
		ImGui::Checkbox("Enabled", &bp.Enabled);
		ImGui::SameLine();
		if (ImGui::Button("Respawn")) { ResetParticles(); }
		ImGui::SliderAngle("Camera FovY (720 px)", &bp.FovY, 5.0f, 150.0f);
		ImGui::SliderFloat("Parallax", &bp.Parallax, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat("Parallax depth", &bp.ParallaxDepth, 0.1f, 0.01f, 1000.0f, "%.2f");
		ImGui::TextDisabled("Particles at 'Parallax depth' move with the banner; farther ones lag behind.");
		ImGui::TextDisabled("Size / color / atlas changes apply as particles respawn (or press Respawn).");

		int removeAt{ -1 };
		for (std::size_t g{ 0U }; g < bp.Groups.size(); ++g) {
			ParticleGroup& group{ bp.Groups[g] };
			ImGui::PushID(static_cast<int>(g));
			bool const open{ ImGui::TreeNode("##group", "%s%s", group.Name.c_str(), group.Front ? "  (front)" : "") };
			if (open) {
				ImGui::Checkbox("Enabled", &group.Enabled);
				ImGui::SameLine();
				ImGui::Checkbox("In front of character", &group.Front);
				ImGui::SliderInt("Count (per banner)", &group.Count, 0, static_cast<int>(MaxParticlesPerGroup));
				ImGui::DragFloatRange2("Depth", &group.DepthMin, &group.DepthMax, 0.1f, 0.1f, 1000.0f, "%.1f");
				ImGui::DragFloatRange2("Size", &group.SizeMin, &group.SizeMax, 0.01f, 0.001f, 100.0f, "%.2f");
				ImGui::DragFloatRange2("Speed (/s)", &group.SpeedMin, &group.SpeedMax, 0.05f, -500.0f, 500.0f, "%.2f");
				ImGui::DragFloat("Cross speed (/s)", &group.CrossSpeed, 0.01f, 0.0f, 100.0f, "%.2f");
				ImGui::DragFloat("Stretch (s)", &group.Stretch, 0.001f, 0.0f, 2.0f, "%.3f");
				ImGui::DragFloat("Spin (rad/s)", &group.Spin, 0.01f, 0.0f, 50.0f, "%.2f");
				ImGui::SliderInt("Atlas", &group.Atlas, 0, 7);
				ImGui::SameLine();
				ImGui::TextDisabled("(0 dot 1 streak 2 star 3 soft star 4 4pt 5 small 4pt 6 smoke 7 circle)");
				ImGui::ColorEdit3("Color (sRGB)", &group.Color.x);
				ImGui::DragFloat("Alpha", &group.Alpha, 0.01f, 0.0f, 10.0f, "%.2f");
				ImGui::DragFloatRange2("Life (s)", &group.LifeMin, &group.LifeMax, 0.01f, 0.05f, 20.0f, "%.2f");
				if (ImGui::Button("Remove this layer")) { removeAt = static_cast<int>(g); }
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if (removeAt >= 0) {
			bp.Groups.erase(bp.Groups.begin() + removeAt);
			ResetParticles();
		}
		if (bp.Groups.size() < MaxParticleGroups) {
			if (ImGui::Button("Add layer")) { bp.Groups.emplace_back(); }
		}
		else {
			ImGui::TextDisabled("Max %u layers.", MaxParticleGroups);
		}

		ImGui::PopID();
#endif
	}

	void Presenter::DrawPortraitEditor(
		[[maybe_unused]] char const* label_,
		[[maybe_unused]] Portrait& portrait_,
		[[maybe_unused]] uint32_t index_
	) {
#if defined(_DEBUG)
		if (!ImGui::CollapsingHeader(label_)) { return; }
		ImGui::PushID(label_);

		Portrait& p{ portrait_ };
		ImGui::Checkbox("Enabled", &p.Enabled);
		ImGui::SameLine();
		ImGui::Checkbox("Show weapon", &p.ShowWeapon);

		// クリップ（ShowcaseCharacter が読み込んだもの）
		ShowcaseCharacter const& c{ Characters_[index_] };
		if (ImGui::BeginCombo("Animation", p.Animation.c_str())) {
			for (std::string const& name : c.AnimationNames()) {
				bool const selected{ name == p.Animation };
				if (ImGui::Selectable(name.c_str(), selected)) { p.Animation = name; }
				if (selected) { ImGui::SetItemDefaultFocus(); }
			}
			ImGui::EndCombo();
		}
		if (c.CurrentAnimationName() != p.Animation) {
			ImGui::TextDisabled("(playing \"%s\" - not found or not advanced yet)", c.CurrentAnimationName().c_str());
		}
		ImGui::DragFloat("Animation speed", &p.AnimationSpeed, 0.01f, 0.0f, 8.0f, "%.2f");

		ImGui::SeparatorText("Studio");
		ImGui::SliderAngle("Character yaw", &p.Yaw, -180.0f, 180.0f);
		ImGui::SliderAngle("Camera yaw", &p.CameraYaw, -180.0f, 180.0f);
		ImGui::SliderAngle("Camera pitch", &p.CameraPitch, -80.0f, 80.0f);
		ImGui::DragFloat("Camera distance", &p.CameraDistance, 0.05f, 0.5f, 100.0f, "%.2f");
		ImGui::DragFloat("Target Y", &p.TargetY, 0.01f, -5.0f, 15.0f, "%.2f");
		ImGui::SliderAngle("FovY (720 px)", &p.FovY, 5.0f, 120.0f);
		ImGui::ColorEdit3("Body (sRGB)", &p.BodyColor.x);

		ImGui::SeparatorText("Lighting");
		ShowcaseLighting& s{ p.Lighting };
		ImGui::ColorEdit3("Fire color", &s.FireColor.x);
		ImGui::DragFloat("Fire intensity", &s.FireIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Fire height", &s.FireHeight, 0.01f, 0.01f, 20.0f, "%.2f");
		ImGui::DragFloat("Under weight", &s.UnderWeight, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::ColorEdit3("Sky color", &s.SkyColor.x);
		ImGui::ColorEdit3("Ground color", &s.GroundColor.x);
		ImGui::DragFloat("Ambient intensity", &s.AmbientIntensity, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::ColorEdit3("Rim color", &s.RimColor.x);
		ImGui::DragFloat("Rim intensity", &s.RimIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Rim threshold", &s.RimThreshold, 0.005f, 0.0f, 1.0f, "%.3f");
		ImGui::DragFloat("Rim softness", &s.RimSoftness, 0.001f, 0.001f, 0.5f, "%.3f");
		ImGui::DragFloat("Cel threshold", &s.CelThreshold, 0.005f, -1.0f, 1.0f, "%.3f");
		ImGui::DragFloat("Cel softness", &s.CelSoftness, 0.001f, 0.001f, 1.0f, "%.3f");
		ImGui::DragFloat("Shadow level", &s.ShadowLevel, 0.005f, 0.0f, 1.0f, "%.3f");

		ImGui::PopID();
#endif
	}
}
