module Lumina.VsBanner;

// ここで import しているヘッダユニットは、いずれも VsBanner.ixx で import 済み
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
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
		Float2 screenSize_
	) {
		if (!pose_.Visible) { return; }
		if (!(screenSize_.x > 0.0f) || !(screenSize_.y > 0.0f)) { return; }

		float const edgeWidth{ std::max(look_.EdgeWidth, 0.0f) };
		float const cornerRadius{ std::clamp(layout_.CornerRadius, 0.0f, std::min(pose_.Boss.HalfSize.x, pose_.Boss.HalfSize.y)) };
		float const opacity{ Saturate(look_.Opacity) };

		std::array<Banner const*, NumSlots> const banners{ &pose_.Boss, &pose_.Player };
		std::array<Float3, NumSlots> const colors{ SRGBToLinear(look_.ColorBoss), SRGBToLinear(look_.ColorPlayer) };

		for (uint32_t idx{ 0U }; idx < NumSlots; ++idx) {
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
				.Margin{ 2.0f },						// アンチエイリアスのにじみ分だけクアッドを広げる（px）
				.ContentOffset{ -b.HoldCenter.x, -b.HoldCenter.y },
				.ContentSize{ content.Size },
				.ContentOpacity{ Saturate(look_.ContentOpacity) },
				.ContentFeather{ std::max(look_.ContentFeather, 0.0f) },
				.Padding1{ 0.0f },
				.Padding2{ 0.0f },
				.Padding3{},
			};
			UB_Constants_.Store(&constants, sizeof(ShaderConstants), ConstantsStride * idx);
		}

		D3D12_GPU_VIRTUAL_ADDRESS const cbBase{ UB_Constants_->GetGPUVirtualAddress() };

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (uint32_t idx{ 0U }; idx < NumSlots; ++idx) {
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbBase + ConstantsStride * idx);
			cmdList_->SetGraphicsRootDescriptorTable(1U, contents_[idx].SRV);
			cmdList_->DrawInstanced(6U, 1U, 0U, 0U);
		}
	}

	//====	======	======	======	======	====//
	//	再生									//
	//====	======	======	======	======	====//

	void Presenter::Initialize(
		DX12::Context const& dxContext_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_,
		Float2 screenSize_
	) {
		ScreenSize_ = screenSize_;
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

		Load();		// ファイルが無ければ初期値のまま
		AdvancePortraits(0.0f);
	}

	void Presenter::Play() {
		State_ = State::Playing;
		Time_ = 0.0f;
	}

	void Presenter::Reset() {
		State_ = State::Idle;
		Time_ = 0.0f;
	}

	void Presenter::Update(float dt_) {
		// 中身のアニメは、見えている間（再生中・プレビュー中）だけ進める
		if (Preview_ || State_ == State::Playing) { AdvancePortraits(std::max(dt_, 0.0f)); }

		if (State_ != State::Playing || Preview_) { return; }
		Time_ += std::max(dt_, 0.0f);
		if (Time_ >= TotalTime(Config.Time)) { State_ = State::Finished; }
	}

	auto Presenter::Progress() const -> float {
		if (State_ == State::Finished) { return 1.0f; }
		float const total{ TotalTime(Config.Time) };
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
		if (!IsVisibleNow()) { return; }

		// 帯が止まったときの中心（時刻によらない）。キャラの画面上の位置 = HoldCenter ＋ ContentOffset（B は −ContentOffset）
		Pose const pose{ Evaluate(Config, 0.0f, ScreenSize_) };
		std::array<Float2, 2> const characterOnScreen{
			Float2{ pose.Boss.HoldCenter.x + Config.Placement.ContentOffset.x, pose.Boss.HoldCenter.y + Config.Placement.ContentOffset.y },
			Float2{ pose.Player.HoldCenter.x - Config.Placement.ContentOffset.x, pose.Player.HoldCenter.y - Config.Placement.ContentOffset.y },
		};
		float const rtWidth{ static_cast<float>(PortraitWidth_) };
		float const rtHeight{ static_cast<float>(PortraitHeight_) };

		std::array<Portrait const*, 2> const portraits{ &Config.Boss, &Config.Player };
		for (uint32_t idx{ 0U }; idx < 2U; ++idx) {
			Portrait const& p{ *portraits[idx] };
			if (!p.Enabled) { continue; }

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

	void Presenter::Render(DX12::CommandList const& cmdList_) {
		float time{ Time_ };
		if (Preview_) { time = PreviewTime_; }
		else if (State_ != State::Playing) { return; }

		Pose const pose{ Evaluate(Config, time, ScreenSize_) };
		Float2 const size{ static_cast<float>(PortraitWidth_), static_cast<float>(PortraitHeight_) };
		std::array<Content, 2> const contents{
			Content{ .SRV{ PortraitSRVs_.GPUHandle(0U) }, .Size{ size }, .Enabled{ Config.Boss.Enabled } },
			Content{ .SRV{ PortraitSRVs_.GPUHandle(1U) }, .Size{ size }, .Enabled{ Config.Player.Enabled } },
		};
		Renderer_.Render(cmdList_, pose, Config.Placement, Config.Appearance, contents, ScreenSize_);
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
		float const total{ TotalTime(Config.Time) };
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
