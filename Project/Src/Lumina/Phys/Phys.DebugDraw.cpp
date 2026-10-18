module Lumina.Phys.DebugDraw;

import <cmath>;
import <numbers>;
import <algorithm>;
import <variant>;

import nlohmann.json;

import Lumina.Utils.Data;

#undef min
#undef max

namespace Lumina::Phys {
	void DebugRenderer::Initialize(
		DX12::Context const& dxContext_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_
	) {
		auto const& device{ dxContext_.Device() };

		{
			auto settings{ Utils::LoadFromFile<nlohmann::json>("PhysDebugLine.json", "Assets/Configs") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device, rsSetup, "PhysDebugLine RS");
		}

		dxContext_.Compile(VS_, L"Assets/Shaders/PhysDebugLine.hlsl", L"vs_6_6", L"VSMain", "PhysDebugLine.VS");
		dxContext_.Compile(PS_, L"Assets/Shaders/PhysDebugLine.hlsl", L"ps_6_6", L"PSMain", "PhysDebugLine.PS");

		{
			DX12::BlendState blendState{};
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ZERO },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			DX12::RasterizerState rasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_NONE },
				.FrontCounterClockwise{ false },
				.DepthClipEnable{ true },
			};

			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ dsvFormat_ != DXGI_FORMAT_UNKNOWN },
				.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ZERO },
				.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
			};

			DX12::GraphicsPipelineState::InputLayout inputLayout{};

			std::vector<DXGI_FORMAT> const rtvFormats{ rtvFormat_ };

			PSO_.Initialize(
				device,
				RS_,
				VS_,
				PS_,
				blendState,
				rasterizerState,
				depthStencilState,
				inputLayout,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				rtvFormats,
				dsvFormat_,
				"PhysDebugLine PSO"
			);
		}

		UB_Vertices_.Initialize(device, sizeof(LineVertex) * MaxLines * 2U, "PhysDebugLine Vertices");
		UB_Constants_.Initialize(device, 256U, "PhysDebugLine Constants");

		Vertices_.reserve(1024U);
	}

	//////	//////	//////	//////	//////	//////
	//	線を積む								//
	//////	//////	//////	//////	//////	//////

	void DebugRenderer::Line(Float3 const& p0_, Float3 const& p1_, Float4 const& color_) {
		if (Vertices_.size() + 2U > static_cast<size_t>(MaxLines) * 2U) { return; }
		Vertices_.push_back(LineVertex{ p0_, 0.0f, color_ });
		Vertices_.push_back(LineVertex{ p1_, 0.0f, color_ });
	}

	void DebugRenderer::Cross(Float3 const& p_, float size_, Float4 const& color_) {
		for (int k{ 0 }; k < 3; ++k) {
			Float3 const d{ F3::Mul(F3::Axis(k), size_) };
			Line(F3::Sub(p_, d), F3::Add(p_, d), color_);
		}
	}

	void DebugRenderer::Circle(
		Float3 const& center_,
		Float3 const& axisU_,
		Float3 const& axisV_,
		float radius_,
		Float4 const& color_
	) {
		constexpr float step{ std::numbers::pi_v<float> * 2.0f / static_cast<float>(CircleSegments) };
		Float3 prev{ F3::MulAdd(center_, axisU_, radius_) };
		for (uint32_t i{ 1U }; i <= CircleSegments; ++i) {
			float const theta{ step * static_cast<float>(i) };
			Float3 curr{ center_ };
			curr = F3::MulAdd(curr, axisU_, std::cos(theta) * radius_);
			curr = F3::MulAdd(curr, axisV_, std::sin(theta) * radius_);
			Line(prev, curr, color_);
			prev = curr;
		}
	}

	void DebugRenderer::Draw(Sphere const& sphere_, Float4 const& color_) {
		Float3 const x{ 1.0f, 0.0f, 0.0f };
		Float3 const y{ 0.0f, 1.0f, 0.0f };
		Float3 const z{ 0.0f, 0.0f, 1.0f };
		Circle(sphere_.Center, x, y, sphere_.Radius, color_);
		Circle(sphere_.Center, y, z, sphere_.Radius, color_);
		Circle(sphere_.Center, z, x, sphere_.Radius, color_);
	}

	void DebugRenderer::Draw(AABB const& aabb_, Float4 const& color_) {
		Draw(ToOBB(aabb_), color_);
	}

	void DebugRenderer::Draw(OBB const& obb_, Float4 const& color_) {
		// 頂点番号のビット 0/1/2 = x/y/z の符号。1 ビットだけ違う 2 頂点を結ぶと辺になる
		for (uint32_t i{ 0U }; i < 8U; ++i) {
			for (uint32_t bit{ 1U }; bit < 8U; bit <<= 1U) {
				if (i & bit) { continue; }
				Line(Corner(obb_, i), Corner(obb_, i | bit), color_);
			}
		}
	}

	void DebugRenderer::Draw(Shape const& shape_, Float4 const& color_) {
		std::visit([this, &color_](auto const& s) { Draw(s, color_); }, shape_);
	}

	void DebugRenderer::Draw(Ray const& ray_, Float4 const& color_) {
		Line(ray_.Origin, ray_.At(ray_.MaxT), color_);
	}

	void DebugRenderer::Draw(CollisionWorld const& world_) {
		// 重なっているコライダの ID を集める
		HitIDs_.clear();
		for (ContactPair const& pair : world_.Contacts()) {
			HitIDs_.push_back(pair.A->ID());
			HitIDs_.push_back(pair.B->ID());
		}
		std::sort(HitIDs_.begin(), HitIDs_.end());

		for (Collider const* collider : world_.Colliders()) {
			Float4 const* color{ &ColorIdle };
			if (!collider->Enabled) {
				color = &ColorDisabled;
			}
			else if (std::binary_search(HitIDs_.begin(), HitIDs_.end(), collider->ID())) {
				color = &ColorHit;
			}
			Draw(collider->World(), *color);

			if (ShowBounds && collider->Enabled) {
				Draw(collider->Bounds(), ColorBounds);
			}
		}

		if (ShowContacts) {
			for (ContactPair const& pair : world_.Contacts()) {
				float const size{ 0.15f };
				Cross(pair.Info.Point, size, ColorContact);
				// 法線（A → B）。長さはめり込み量 + 少し
				Line(
					pair.Info.Point,
					F3::MulAdd(pair.Info.Point, pair.Info.Normal, pair.Info.Depth + 0.5f),
					ColorContact
				);
			}
		}
	}

	//////	//////	//////	//////	//////	//////
	//	描画									//
	//////	//////	//////	//////	//////	//////

	void DebugRenderer::Render(DX12::CommandList const& cmdList_, Mat4 const& viewProj_) {
		if (Vertices_.empty()) { return; }

		UB_Vertices_.Store(Vertices_.data(), sizeof(LineVertex) * Vertices_.size(), 0LLU);
		UB_Constants_.Store(&viewProj_, sizeof(Mat4), 0LLU);

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		cmdList_->SetGraphicsRootConstantBufferView(0U, UB_Constants_->GetGPUVirtualAddress());
		cmdList_->SetGraphicsRootShaderResourceView(1U, UB_Vertices_->GetGPUVirtualAddress());
		cmdList_->DrawInstanced(static_cast<uint32_t>(Vertices_.size()), 1U, 0U, 0U);

		Vertices_.clear();
	}
}
