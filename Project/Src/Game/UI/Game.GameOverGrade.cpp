module Game.GameOverGrade;

// ここで import しているヘッダユニットは、いずれも Game.GameOverGrade.ixx で import 済み
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <string>;
import <d3d12.h>;

import Lumina;
import nlohmann.json;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

#undef max
#undef min

namespace Game::GameOverGrade {
	namespace {
		constexpr uint32_t ScreenWidth{ 1280U };
		constexpr uint32_t ScreenHeight{ 720U };

		// GameOverGrade.hlsl の cbuffer Constants と同じ並び（32 の倍数に揃える）
		struct GradeConstants {
			float Stops[MaxStops][4];		// xyz = 色（sRGB）、w = 位置

			float GradientAmount;
			float BlackPoint;
			float WhitePoint;
			float Gamma;

			uint32_t NumStops;
			float VignetteAmount;
			float VignetteRadius;
			float VignetteSoftness;

			Lumina::Float3 VignetteColor;
			float VignetteRoundness;

			float Aspect;
			float SplitPosition;
			float ViewLuma;
			float DitherAmount;

			float Padding[8];
		};
		static_assert(sizeof(GradeConstants) % 32U == 0U);

		auto Saturate(float x_) -> float { return std::clamp(x_, 0.0f, 1.0f); }

		auto SmootherStep(float x_) -> float {
			float const x{ Saturate(x_) };
			return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
		}

		// Delay 秒後から Duration 秒かけて 0 → 1（なめらかに）
		auto Progress(float elapsed_, float delay_, float duration_) -> float {
			if (duration_ <= 1.0e-4f) { return (elapsed_ >= delay_) ? 1.0f : 0.0f; }
			return SmootherStep((elapsed_ - delay_) / duration_);
		}

		// 位置の昇順に並べる（先頭から count_ 個）
		void SortStops(std::array<Stop, MaxStops>& stops_, uint32_t count_) {
			std::stable_sort(
				stops_.begin(),
				stops_.begin() + count_,
				[](Stop const& a_, Stop const& b_) { return a_.Position < b_.Position; }
			);
		}

		using JSON = nlohmann::json;

		// キーがあって数値なら上書きする（無い・型が違うときは今の値のまま）
		void Read(JSON const& obj_, char const* key_, float& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr != obj_.end() && itr->is_number()) { out_ = itr->get<float>(); }
		}
		void Read(JSON const& obj_, char const* key_, Lumina::Float3& out_) {
			auto const itr{ obj_.find(key_) };
			if (itr == obj_.end() || !itr->is_array() || itr->size() != 3U) { return; }
			for (std::size_t idx{ 0U }; idx < 3U; ++idx) {
				if (!(*itr)[idx].is_number()) { return; }
			}
			out_ = Lumina::Float3{ (*itr)[0].get<float>(), (*itr)[1].get<float>(), (*itr)[2].get<float>() };
		}

		// 無いセクションは空のオブジェクトとして扱う（中のキーは全部「無い」になる）
		auto Section(JSON const& root_, char const* key_) -> JSON const& {
			static JSON const empty = JSON::object();
			auto const itr{ root_.find(key_) };
			return (itr != root_.end() && itr->is_object()) ? *itr : empty;
		}

#if defined(_DEBUG)
		//////	//////	//////	//////	//////	//////
		//	Editor helpers							//
		//////	//////	//////	//////	//////	//////

		auto ToU32(Lumina::Float3 const& c_) -> ImU32 {
			auto const channel{ [](float v_) -> ImU32 { return static_cast<ImU32>(Saturate(v_) * 255.0f + 0.5f); } };
			return 0xFF000000U | (channel(c_.z) << 16U) | (channel(c_.y) << 8U) | channel(c_.x);
		}

		// ストップ列から色（sRGB）を引く（シェーダの EvaluateGradient と同じ）
		auto EvaluateStops(Style const& style_, float t_) -> Lumina::Float3 {
			if (t_ <= style_.Stops[0].Position) { return style_.Stops[0].Color; }
			for (uint32_t idx{ 1U }; idx < style_.NumStops; ++idx) {
				Stop const& a{ style_.Stops[idx - 1U] };
				Stop const& b{ style_.Stops[idx] };
				if (t_ <= b.Position) {
					float const f{ Saturate((t_ - a.Position) / std::max(b.Position - a.Position, 1.0e-5f)) };
					return Lumina::Float3{
						a.Color.x + (b.Color.x - a.Color.x) * f,
						a.Color.y + (b.Color.y - a.Color.y) * f,
						a.Color.z + (b.Color.z - a.Color.z) * f,
					};
				}
			}
			return style_.Stops[style_.NumStops - 1U].Color;
		}

		// idx_ のストップを t_ へ動かし、並べ替え後の添字を返す（追い越せる）
		auto MoveStop(Style& style_, int idx_, float t_) -> int {
			auto const at{ [&style_](int i_) -> Stop& { return style_.Stops[static_cast<std::size_t>(i_)]; } };
			int const num{ static_cast<int>(style_.NumStops) };
			at(idx_).Position = Saturate(t_);
			while (idx_ > 0 && at(idx_ - 1).Position > at(idx_).Position) {
				std::swap(at(idx_ - 1), at(idx_));
				--idx_;
			}
			while (idx_ < num - 1 && at(idx_ + 1).Position < at(idx_).Position) {
				std::swap(at(idx_ + 1), at(idx_));
				++idx_;
			}
			return idx_;
		}

		// t_ の位置に、その位置の今の色でストップを足す。足したストップの添字を返す（満杯なら -1）
		auto AddStop(Style& style_, float t_) -> int {
			if (style_.NumStops >= MaxStops) { return -1; }
			Stop const added{ Saturate(t_), EvaluateStops(style_, Saturate(t_)) };
			style_.Stops[style_.NumStops] = added;
			++style_.NumStops;
			return MoveStop(style_, static_cast<int>(style_.NumStops) - 1, added.Position);
		}

		void RemoveStop(Style& style_, int idx_) {
			if (style_.NumStops <= MinStops) { return; }
			for (uint32_t i{ static_cast<uint32_t>(idx_) }; i + 1U < style_.NumStops; ++i) {
				style_.Stops[i] = style_.Stops[i + 1U];
			}
			--style_.NumStops;
			style_.Stops[style_.NumStops] = Stop{};
		}

		struct Preset {
			char const* Name;
			uint32_t NumStops;
			std::array<Stop, 4> Stops;
		};
		Preset const Presets[]{
			{ "Mono", 2U, { Stop{ 0.0f, { 0.0f, 0.0f, 0.0f } }, Stop{ 1.0f, { 1.0f, 1.0f, 1.0f } } } },
			{ "Sepia", 3U, { Stop{ 0.0f, { 0.10f, 0.06f, 0.03f } }, Stop{ 0.55f, { 0.62f, 0.45f, 0.28f } }, Stop{ 1.0f, { 1.0f, 0.93f, 0.78f } } } },
			{ "Blue-black > Cream", 3U, { Stop{ 0.0f, { 0.03f, 0.05f, 0.12f } }, Stop{ 0.5f, { 0.45f, 0.48f, 0.55f } }, Stop{ 1.0f, { 0.97f, 0.93f, 0.82f } } } },
			{ "Red-black", 3U, { Stop{ 0.0f, { 0.02f, 0.0f, 0.0f } }, Stop{ 0.55f, { 0.55f, 0.04f, 0.05f } }, Stop{ 1.0f, { 1.0f, 0.85f, 0.8f } } } },
			{ "Cyanotype", 3U, { Stop{ 0.0f, { 0.02f, 0.08f, 0.17f } }, Stop{ 0.55f, { 0.15f, 0.45f, 0.65f } }, Stop{ 1.0f, { 0.93f, 0.97f, 1.0f } } } },
		};

		// グラデーションバー：クリックで追加・ドラッグで移動・右クリックで削除。選択中の添字を selected_ に入れる。
		// 色は sRGB のまま描く（ImGui は色を変換しないので、シェーダの結果と同じ見た目になる）
		auto GradientBar(Style& style_, int& selected_, int& dragging_) -> bool {
			constexpr float BarHeight{ 28.0f };
			constexpr float MarkerHeight{ 14.0f };
			constexpr float GrabRadius{ 8.0f };

			ImVec2 const origin{ ImGui::GetCursorScreenPos() };
			float const width{ std::max(ImGui::GetContentRegionAvail().x - 4.0f, 64.0f) };

			ImGui::InvisibleButton("##GradientBar", ImVec2{ width, BarHeight + MarkerHeight });
			bool const hovered{ ImGui::IsItemHovered() };
			bool const held{ ImGui::IsItemActive() };
			ImVec2 const mouse{ ImGui::GetIO().MousePos };
			float const mouseT{ Saturate((mouse.x - origin.x) / width) };
			int const num{ static_cast<int>(style_.NumStops) };
			bool changed{ false };

			// マウスに一番近い（GrabRadius 以内の）ストップ
			int nearest{ -1 };
			float nearestDist{ GrabRadius };
			for (int idx{ 0 }; idx < num; ++idx) {
				float const dist{ std::abs(origin.x + style_.Stops[static_cast<std::size_t>(idx)].Position * width - mouse.x) };
				if (dist <= nearestDist) {
					nearest = idx;
					nearestDist = dist;
				}
			}

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				if (nearest >= 0) {
					selected_ = nearest;
					dragging_ = nearest;
				}
				else {
					int const added{ AddStop(style_, mouseT) };
					if (added >= 0) {
						selected_ = added;
						dragging_ = added;
						changed = true;
					}
				}
			}
			else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && nearest >= 0 && style_.NumStops > MinStops) {
				RemoveStop(style_, nearest);
				selected_ = std::clamp(selected_ - ((selected_ >= nearest) ? 1 : 0), 0, static_cast<int>(style_.NumStops) - 1);
				dragging_ = -1;
				changed = true;
			}
			if (!held || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) { dragging_ = -1; }
			if (dragging_ >= 0 && dragging_ < static_cast<int>(style_.NumStops)) {
				int const moved{ MoveStop(style_, dragging_, mouseT) };
				changed = true;
				if (selected_ == dragging_) { selected_ = moved; }
				dragging_ = moved;
			}

			// 描画：バー（両端の外側は端の色のまま、間は隣のストップ同士のグラデーション）
			ImDrawList* const drawList{ ImGui::GetWindowDrawList() };
			float const top{ origin.y };
			float const bottom{ origin.y + BarHeight };
			auto const stopAt{ [&style_](int i_) -> Stop const& { return style_.Stops[static_cast<std::size_t>(i_)]; } };
			auto const xOf{ [&origin, width](float t_) -> float { return origin.x + t_ * width; } };

			drawList->AddRectFilled(ImVec2{ origin.x, top }, ImVec2{ xOf(stopAt(0).Position), bottom }, ToU32(stopAt(0).Color));
			for (int idx{ 0 }; idx + 1 < num; ++idx) {
				ImU32 const colA{ ToU32(stopAt(idx).Color) };
				ImU32 const colB{ ToU32(stopAt(idx + 1).Color) };
				drawList->AddRectFilledMultiColor(
					ImVec2{ xOf(stopAt(idx).Position), top },
					ImVec2{ xOf(stopAt(idx + 1).Position), bottom },
					colA, colB, colB, colA
				);
			}
			drawList->AddRectFilled(ImVec2{ xOf(stopAt(num - 1).Position), top }, ImVec2{ origin.x + width, bottom }, ToU32(stopAt(num - 1).Color));
			drawList->AddRect(ImVec2{ origin.x, top }, ImVec2{ origin.x + width, bottom }, 0xFF808080U);

			// 描画：マーカー（選択中は白い枠）
			for (int idx{ 0 }; idx < num; ++idx) {
				float const x{ xOf(stopAt(idx).Position) };
				ImVec2 const tip{ x, bottom + 1.0f };
				ImVec2 const left{ x - 6.0f, bottom + MarkerHeight };
				ImVec2 const right{ x + 6.0f, bottom + MarkerHeight };
				drawList->AddTriangleFilled(tip, left, right, ToU32(stopAt(idx).Color));
				drawList->AddTriangle(tip, left, right, (idx == selected_) ? 0xFFFFFFFFU : 0xFF606060U, (idx == selected_) ? 2.0f : 1.0f);
			}
			return changed;
		}
#endif
	}

	//////	//////	//////	//////	//////	//////
	//	Initialize								//
	//////	//////	//////	//////	//////	//////

	void Effect::Initialize(Lumina::DX12::Context const& dxContext_) {
		auto const& device{ dxContext_.Device() };

		// * Canvas：UI まで描いた画面を受ける中間 RT（SRGB、深度なし）。初期化後は PIXEL_SHADER_RESOURCE 状態
		{
			Canvas_.AllocateTextures(1U, false);
			Canvas_.RenderTexture(0U).Initialize(
				device,
				ScreenWidth,
				ScreenHeight,
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
			);
			Canvas_.TransitionResourceStates(device, dxContext_.DirectQueue());
			Canvas_.CreateViews(device);
			Canvas_.Viewport(0U) = D3D12_VIEWPORT{
				.TopLeftX{ 0.0f },
				.TopLeftY{ 0.0f },
				.Width{ static_cast<float>(ScreenWidth) },
				.Height{ static_cast<float>(ScreenHeight) },
				.MinDepth{ 0.0f },
				.MaxDepth{ 1.0f },
			};
			Canvas_.ScissorRect(0U) = D3D12_RECT{
				.left{ 0 },
				.top{ 0 },
				.right{ static_cast<LONG>(ScreenWidth) },
				.bottom{ static_cast<LONG>(ScreenHeight) },
			};

			GlobalTable_SRV_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			Lumina::DX12::SRV<void>::Create(device, GlobalTable_SRV_.CPUHandle(0U), Canvas_.RenderTexture(0U));
		}

		// * Root signature：[0] ルート CBV（b0）/ [1] SRV テーブル（t0）＋静的サンプラ（バイリニア・Clamp）
		{
			auto settings{ Lumina::Utils::LoadFromFile<nlohmann::json>("GameOverGrade.json", "Assets/Effects") };
			RS_.Initialize(
				device,
				Lumina::DX12::LoadRootSignatureSetup(settings.at("RS")),
				"GameOverGrade RS"
			);
		}

		dxContext_.Compile(VS_, L"Assets/Effects/GameOverGrade.hlsl", L"vs_6_6", L"VSMain", "GameOverGrade.VS");
		dxContext_.Compile(PS_, L"Assets/Effects/GameOverGrade.hlsl", L"ps_6_6", L"PSMain", "GameOverGrade.PS");

		// * PSO：上書き（ブレンドなし）・深度なし。三角形は VS で SV_VertexID から生成
		{
			Lumina::DX12::BlendState blendState{};
			blendState.RenderTarget[0].BlendEnable = false;
			blendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			Lumina::DX12::GraphicsPSO::InputLayout inputLayout{};

			PSO_.Initialize(
				device,
				RS_,
				VS_,
				PS_,
				blendState,
				Lumina::DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_NONE },
				},
				Lumina::DX12::DepthStencilState{
					.DepthEnable{ false },
					.StencilEnable{ false },
				},
				inputLayout,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, },
				Lumina::DX12::GraphicsPSO::DefaultDSVFormat,
				"GameOverGrade PSO"
			);
		}

		UB_.Initialize(device, 256LLU, "GameOverGrade Constants");

		Load();
	}

	//////	//////	//////	//////	//////	//////
	//	Time									//
	//////	//////	//////	//////	//////	//////

	void Effect::Start() {
		Started_ = true;
		Time_ = 0.0f;
	}

	void Effect::Reset() {
		Started_ = false;
		Time_ = 0.0f;
	}

	void Effect::Update(float deltaTime_) {
		if (Started_) { Time_ += std::max(deltaTime_, 0.0f); }
	}

	auto Effect::TotalTime() const -> float {
		return std::max(
			Look.GradientDelay + Look.GradientDuration,
			Look.VignetteDelay + Look.VignetteDuration
		);
	}

	auto Effect::Elapsed() const -> float {
		switch (PreviewMode_) {
			case Preview::Hold:
				return TotalTime();
			case Preview::Scrub:
				return ScrubTime_;
			default:
				return Started_ ? Time_ : 0.0f;
		}
	}

	auto Effect::GradientProgress() const -> float {
		return Progress(Elapsed(), Look.GradientDelay, Look.GradientDuration);
	}

	auto Effect::VignetteProgress() const -> float {
		return Progress(Elapsed(), Look.VignetteDelay, Look.VignetteDuration);
	}

	auto Effect::UiAlpha() const -> float {
		if (!IsActive()) { return 1.0f; }
		float const duration{ std::max(Look.UiFadeDuration, 1.0e-4f) };
		return 1.0f - Saturate((Elapsed() - Look.UiFadeDelay) / duration);
	}

	//////	//////	//////	//////	//////	//////
	//	Render									//
	//////	//////	//////	//////	//////	//////

	auto Effect::BeginSource(Lumina::DX12::CommandList const& cmdList_) -> D3D12_CPU_DESCRIPTOR_HANDLE {
		Style const& style{ Look };

		GradeConstants constants{};
		for (uint32_t idx{ 0U }; idx < MaxStops; ++idx) {
			// 使わない枠は最後のストップで埋める（NumStops より先は読まれない）
			Stop const& stop{ style.Stops[std::min(idx, style.NumStops - 1U)] };
			constants.Stops[idx][0] = stop.Color.x;
			constants.Stops[idx][1] = stop.Color.y;
			constants.Stops[idx][2] = stop.Color.z;
			constants.Stops[idx][3] = stop.Position;
		}
		constants.GradientAmount = Saturate(style.GradientAmount) * GradientProgress();
		constants.BlackPoint = style.BlackPoint;
		constants.WhitePoint = style.WhitePoint;
		constants.Gamma = style.Gamma;
		constants.NumStops = std::clamp(style.NumStops, MinStops, MaxStops);
		constants.VignetteAmount = Saturate(style.VignetteStrength) * VignetteProgress();
		constants.VignetteRadius = style.VignetteRadius;
		constants.VignetteSoftness = style.VignetteSoftness;
		constants.VignetteColor = style.VignetteColor;
		constants.VignetteRoundness = Saturate(style.VignetteRoundness);
		constants.Aspect = static_cast<float>(ScreenWidth) / static_cast<float>(ScreenHeight);
		constants.SplitPosition = SplitView_ ? SplitPosition_ : -1.0f;
		constants.ViewLuma = ViewLuma_ ? 1.0f : 0.0f;
		constants.DitherAmount = 1.0f / 255.0f;
		UB_.Store(&constants, sizeof(GradeConstants), 0LLU);

		D3D12_RESOURCE_BARRIER const barrier{
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(0U),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			)
		};
		cmdList_->ResourceBarrier(1U, &barrier);

		return Canvas_.RTV(0U);
	}

	void Effect::Apply(
		Lumina::DX12::Context const& dxContext_,
		Lumina::DX12::CommandList const& cmdList_
	) {
		D3D12_RESOURCE_BARRIER const barrier{
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(0U),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			)
		};
		cmdList_->ResourceBarrier(1U, &barrier);

		D3D12_CPU_DESCRIPTOR_HANDLE const rtv{ dxContext_.SwapChain().BackBufferRTVCPUHandle() };
		cmdList_->OMSetRenderTargets(1U, &rtv, false, nullptr);
		cmdList_->RSSetViewports(1U, &Canvas_.Viewport(0U));
		cmdList_->RSSetScissorRects(1U, &Canvas_.ScissorRect(0U));

		// 全画面を上書きするのでバックバッファのクリアは要らない
		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->SetGraphicsRootConstantBufferView(0U, UB_->GetGPUVirtualAddress());
		cmdList_->SetGraphicsRootDescriptorTable(1U, GlobalTable_SRV_.GPUHandle(0U));
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList_->DrawInstanced(3U, 1U, 0U, 0U);
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

		JSON const& timeline{ Section(root, "Timeline") };
		Read(timeline, "GradientDelay", style.GradientDelay);
		Read(timeline, "GradientDuration", style.GradientDuration);
		Read(timeline, "VignetteDelay", style.VignetteDelay);
		Read(timeline, "VignetteDuration", style.VignetteDuration);
		Read(timeline, "UiFadeDelay", style.UiFadeDelay);
		Read(timeline, "UiFadeDuration", style.UiFadeDuration);

		JSON const& gradient{ Section(root, "Gradient") };
		Read(gradient, "Amount", style.GradientAmount);
		Read(gradient, "BlackPoint", style.BlackPoint);
		Read(gradient, "WhitePoint", style.WhitePoint);
		Read(gradient, "Gamma", style.Gamma);
		{
			// ストップは 2 個以上あるときだけ丸ごと置き換える
			auto const itr{ gradient.find("Stops") };
			if (itr != gradient.end() && itr->is_array()) {
				std::array<Stop, MaxStops> stops{};
				uint32_t count{ 0U };
				for (auto const& element : *itr) {
					if (count >= MaxStops) { break; }
					if (!element.is_object()) { continue; }
					Stop stop{};
					Read(element, "Position", stop.Position);
					Read(element, "Color", stop.Color);
					stop.Position = Saturate(stop.Position);
					stops[count] = stop;
					++count;
				}
				if (count >= MinStops) {
					SortStops(stops, count);
					style.Stops = stops;
					style.NumStops = count;
				}
			}
		}

		JSON const& vignette{ Section(root, "Vignette") };
		Read(vignette, "Strength", style.VignetteStrength);
		Read(vignette, "Radius", style.VignetteRadius);
		Read(vignette, "Softness", style.VignetteSoftness);
		Read(vignette, "Roundness", style.VignetteRoundness);
		Read(vignette, "Color", style.VignetteColor);

		Selected_ = std::clamp(Selected_, 0, static_cast<int>(style.NumStops) - 1);
		Status_ = "Loaded.";
		return true;
	}

	auto Effect::Save() -> bool {
		Style const& style{ Look };

		JSON stops = JSON::array();
		for (uint32_t idx{ 0U }; idx < style.NumStops; ++idx) {
			Stop const& stop{ style.Stops[idx] };
			stops.push_back(JSON{
				{ "Position", stop.Position },
				{ "Color", JSON::array({ stop.Color.x, stop.Color.y, stop.Color.z }) },
			});
		}

		JSON root = JSON::object();
		root["Timeline"] = JSON{
			{ "GradientDelay", style.GradientDelay },
			{ "GradientDuration", style.GradientDuration },
			{ "VignetteDelay", style.VignetteDelay },
			{ "VignetteDuration", style.VignetteDuration },
			{ "UiFadeDelay", style.UiFadeDelay },
			{ "UiFadeDuration", style.UiFadeDuration },
		};
		root["Gradient"] = JSON{
			{ "Amount", style.GradientAmount },
			{ "BlackPoint", style.BlackPoint },
			{ "WhitePoint", style.WhitePoint },
			{ "Gamma", style.Gamma },
			{ "Stops", stops },
		};
		root["Vignette"] = JSON{
			{ "Strength", style.VignetteStrength },
			{ "Radius", style.VignetteRadius },
			{ "Softness", style.VignetteSoftness },
			{ "Roundness", style.VignetteRoundness },
			{ "Color", JSON::array({ style.VignetteColor.x, style.VignetteColor.y, style.VignetteColor.z }) },
		};

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
		if (!ImGui::Begin("GameOver Grade")) {
			ImGui::End();
			return;
		}
		Style& style{ Look };

		if (ImGui::Button("Save")) { Save(); }
		ImGui::SameLine();
		if (ImGui::Button("Load")) { Load(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) { Look = Style{}; Selected_ = 0; Status_ = "Reset (not saved)."; }
		ImGui::TextDisabled("%s  (%s)", Status_.c_str(), ConfigPath);

		//----	Preview							----//
		ImGui::SeparatorText("Preview");
		{
			int mode{ static_cast<int>(PreviewMode_) };
			ImGui::RadioButton("Game", &mode, static_cast<int>(Preview::Off));
			ImGui::SameLine();
			ImGui::RadioButton("Hold final", &mode, static_cast<int>(Preview::Hold));
			ImGui::SameLine();
			ImGui::RadioButton("Scrub", &mode, static_cast<int>(Preview::Scrub));
			PreviewMode_ = static_cast<Preview>(mode);
		}
		if (PreviewMode_ == Preview::Scrub) {
			ImGui::SliderFloat("Time (s)", &ScrubTime_, 0.0f, TotalTime() + 0.5f);
		}
		if (ImGui::Button("Replay from start")) {
			PreviewMode_ = Preview::Off;
			Start();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			PreviewMode_ = Preview::Off;
			Reset();
		}
		ImGui::Checkbox("Split (left = original)", &SplitView_);
		if (SplitView_) { ImGui::SliderFloat("Split position", &SplitPosition_, 0.0f, 1.0f); }
		ImGui::Checkbox("Luminance only (after levels)", &ViewLuma_);
		ImGui::TextDisabled(
			"%s  elapsed %.2f / %.2f s   gradient %.2f   vignette %.2f",
			IsActive() ? "Active" : "Inactive (UI goes straight to the back buffer)",
			Elapsed(), TotalTime(), GradientProgress(), VignetteProgress()
		);

		//----	Timeline						----//
		ImGui::SeparatorText("Timeline (s since Start)");
		ImGui::SliderFloat("Gradient delay", &style.GradientDelay, 0.0f, 5.0f);
		ImGui::SliderFloat("Gradient duration", &style.GradientDuration, 0.0f, 5.0f);
		ImGui::SliderFloat("Vignette delay", &style.VignetteDelay, 0.0f, 5.0f);
		ImGui::SliderFloat("Vignette duration", &style.VignetteDuration, 0.0f, 5.0f);
		ImGui::SliderFloat("UI fade delay", &style.UiFadeDelay, 0.0f, 5.0f);
		ImGui::SliderFloat("UI fade duration", &style.UiFadeDuration, 0.0f, 5.0f);

		//----	Gradient map					----//
		ImGui::SeparatorText("Gradient map");
		ImGui::SliderFloat("Amount", &style.GradientAmount, 0.0f, 1.0f);
		ImGui::SliderFloat("Black point", &style.BlackPoint, 0.0f, 1.0f);
		ImGui::SliderFloat("White point", &style.WhitePoint, 0.0f, 1.0f);
		ImGui::SliderFloat("Gamma", &style.Gamma, 0.2f, 5.0f, "%.2f", ImGuiSliderFlags_Logarithmic);

		Selected_ = std::clamp(Selected_, 0, static_cast<int>(style.NumStops) - 1);
		GradientBar(style, Selected_, DraggingStop_);
		ImGui::TextDisabled("Click: add / Drag: move / Right-click: delete  (max %u stops)", MaxStops);

		Selected_ = std::clamp(Selected_, 0, static_cast<int>(style.NumStops) - 1);
		{
			float position{ style.Stops[static_cast<std::size_t>(Selected_)].Position };
			ImGui::Text("Stop %d / %u", Selected_ + 1, style.NumStops);
			if (ImGui::SliderFloat("Position", &position, 0.0f, 1.0f)) {
				Selected_ = MoveStop(style, Selected_, position);
			}
			Stop& selectedStop{ style.Stops[static_cast<std::size_t>(Selected_)] };
			ImGui::ColorPicker3("Color (sRGB)", &selectedStop.Color.x, ImGuiColorEditFlags_PickerHueBar);
		}

		ImGui::BeginDisabled(style.NumStops >= MaxStops);
		if (ImGui::Button("Add stop")) {
			int const added{ AddStop(style, 0.5f) };
			if (added >= 0) { Selected_ = added; }
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(style.NumStops <= MinStops);
		if (ImGui::Button("Delete stop")) {
			RemoveStop(style, Selected_);
			Selected_ = std::clamp(Selected_, 0, static_cast<int>(style.NumStops) - 1);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Reverse")) {
			std::reverse(style.Stops.begin(), style.Stops.begin() + style.NumStops);
			for (uint32_t idx{ 0U }; idx < style.NumStops; ++idx) {
				style.Stops[idx].Position = 1.0f - style.Stops[idx].Position;
			}
			Selected_ = static_cast<int>(style.NumStops) - 1 - Selected_;
		}
		ImGui::SameLine();
		if (ImGui::Button("Spread evenly")) {
			for (uint32_t idx{ 0U }; idx < style.NumStops; ++idx) {
				style.Stops[idx].Position = static_cast<float>(idx) / static_cast<float>(style.NumStops - 1U);
			}
		}

		ImGui::TextDisabled("Presets (replace all stops)");
		for (std::size_t idx{ 0U }; idx < std::size(Presets); ++idx) {
			if (idx > 0U) { ImGui::SameLine(); }
			if (ImGui::Button(Presets[idx].Name)) {
				style.Stops = std::array<Stop, MaxStops>{};
				for (uint32_t stopIdx{ 0U }; stopIdx < Presets[idx].NumStops; ++stopIdx) {
					style.Stops[stopIdx] = Presets[idx].Stops[stopIdx];
				}
				style.NumStops = Presets[idx].NumStops;
				Selected_ = 0;
			}
		}

		//----	Vignette						----//
		ImGui::SeparatorText("Vignette");
		ImGui::SliderFloat("Strength", &style.VignetteStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("Radius", &style.VignetteRadius, 0.0f, 1.0f);
		ImGui::SliderFloat("Softness", &style.VignetteSoftness, 0.0f, 1.5f);
		ImGui::SliderFloat("Roundness", &style.VignetteRoundness, 0.0f, 1.0f);
		ImGui::ColorEdit3("Vignette color (sRGB)", &style.VignetteColor.x);

		ImGui::End();
#endif
	}
}
