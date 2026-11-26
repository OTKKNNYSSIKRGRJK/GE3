module Lumina.FirePillar;

#undef max
#undef min

namespace Lumina {
	namespace {
		auto SRGBToLinear(float c_) -> float {
			return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
		}
		auto SRGBToLinear(Float3 const& c_) -> Float3 {
			return Float3{ SRGBToLinear(c_.x), SRGBToLinear(c_.y), SRGBToLinear(c_.z) };
		}
	}

	void FlamePillar::Renderer::Initialize(
		DX12::Context const& dxContext_,
		AssetManager& assetMngr_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_
	) {
		auto const& device{ dxContext_.Device() };

		//----	------	------	------	------	----//
		// Texture
		//----	------	------	------	------	----//

		{
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "FirePillar.Noise", "Assets/Title/FirePillarNoise.png" },
				}
			);
			SRV_Texture_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			device->CopyDescriptorsSimple(
				1U,
				SRV_Texture_.CPUHandle(0U),
				assetMngr_.Graphics().CPUHandle(texIDs.at(0U)),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
		}

		//----	------	------	------	------	----//
		// Root signature / Shaders
		//----	------	------	------	------	----//

		{
			auto settings{ Utils::LoadFromFile<nlohmann::json>("FirePillar.json", "Assets/Title") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device, rsSetup, "FirePillar RS");
		}

		dxContext_.Compile(
			VS_,
			L"Assets/Title/FirePillar.hlsl",
			L"vs_6_6",
			L"VSMain",
			"FirePillar.VS"
		);
		dxContext_.Compile(
			PS_,
			L"Assets/Title/FirePillar.hlsl",
			L"ps_6_6",
			L"PSMain",
			"FirePillar.PS"
		);

		//----	------	------	------	------	----//
		// PSO
		//----	------	------	------	------	----//

		{
			// * Premultiplied alpha
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
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ dsvFormat_ != DXGI_FORMAT_UNKNOWN },
				.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ZERO },
				.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
			};

			// * The cylinder vertices are generated within the VS.
			DX12::GraphicsPipelineState::InputLayout inputLayout{};

			std::vector<DXGI_FORMAT> const rtvFormats{ rtvFormat_ };

			auto const createPSO{
				[&](DX12::GraphicsPSO& pso_, D3D12_CULL_MODE cullMode_, std::string_view name_) {
					DX12::RasterizerState rasterizerState{
						.FillMode{ D3D12_FILL_MODE_SOLID },
						.CullMode{ cullMode_ },
						.FrontCounterClockwise{ false },
						.DepthClipEnable{ true },
					};
					pso_.Initialize(
						device,
						RS_,
						VS_,
						PS_,
						blendState,
						rasterizerState,
						depthStencilState,
						inputLayout,
						D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
						rtvFormats,
						dsvFormat_,
						name_
					);
				}
			};

			createPSO(PSO_BackFaces_, D3D12_CULL_MODE_FRONT, "FirePillar PSO (BackFaces)");
			createPSO(PSO_FrontFaces_, D3D12_CULL_MODE_BACK, "FirePillar PSO (FrontFaces)");
		}

		//----	------	------	------	------	----//
		// Constant buffer
		// インスタンスごとに256バイト境界のスロットを持ち、Root CBVで指す
		//----	------	------	------	------	----//

		UB_Constants_.Initialize(device, ConstantsStride * MaxInstances, "FirePillar Constants");
	}

	void FlamePillar::Renderer::StoreConstants(
		uint32_t slot_,
		Instance const& instance_,
		Mat4 const& viewProj_,
		Vec3 const& cameraPos_
	) {
		Description const& desc{ *(instance_.Desc) };

		ShaderConstants const shaderConstants{
			.World{
				Mat4::SRT(
					Vec3{ 1.0f, 1.0f, 1.0f },
					Vec3{ 0.0f, 0.0f, 0.0f },
					Vec3{ instance_.Position.x, instance_.Position.y, instance_.Position.z }
				)
			},
			.ViewProj{ viewProj_ },

			.CameraPos{ cameraPos_.x, cameraPos_.y, cameraPos_.z },
			.Time{ instance_.Age },

			.Radius{ desc.Radius },
			.Height{ desc.Height },
			.TopScale{ desc.TopScale },
			.ShapeCurve{ desc.ShapeCurve },

			.WobbleAmp{ desc.WobbleAmp },
			.WobbleFreq{ desc.WobbleFreq },
			.WobbleWave{ desc.WobbleWave },
			.Twist{ desc.Twist },

			// * `NoiseScaleU` is rounded so that seamlessness is guaranteed.
			.NoiseScaleU{ std::max(1.0f, std::round(desc.Layer.ScaleU)) },
			.NoiseScaleV{ desc.Layer.ScaleV },
			.NoiseSpeed{ desc.Layer.ScrollSpeed },
			.WarpAmount{ desc.Layer.WarpAmount },

			.WarpScaleU{ std::max(1.0f, std::round(desc.WarpLayer.ScaleU)) },
			.WarpScaleV{ desc.WarpLayer.ScaleV },
			.WarpSpeed{ desc.WarpLayer.ScrollSpeed },
			.Seed{ static_cast<float>(instance_.Seed) },

			.CutBottom{ desc.CutBottom },
			.CutTop{ desc.CutTop },
			.CutCurve{ desc.CutCurve },
			.Softness{ std::max(desc.Softness, 0.001f) },

			.FadeBottom{ std::max(desc.FadeBottom, 0.001f) },
			.FadeTopStart{ std::min(desc.FadeTopStart, 0.999f) },
			.RimPower{ desc.RimPower },
			.BackDarken{ desc.BackDarken },

			.Intensity{ desc.Intensity * instance_.IntensityScale },
			.Opacity{ desc.Opacity },
			.Additive{ desc.Additive },
			.TextureColor{ desc.TextureColor },

			.RampLow{ SRGBToLinear(desc.Ramp.Low) },
			.HeatScale{ desc.HeatScale },
			.RampMid{ SRGBToLinear(desc.Ramp.Mid) },
			.HeatHeight{ desc.HeatHeight },
			.RampHigh{ SRGBToLinear(desc.Ramp.High) },
			.RampMidPos{ std::clamp(desc.Ramp.MidPosition, 0.001f, 0.999f) },

			.NumSegmentsU{ SegmentsU },
			.NumSegmentsV{ SegmentsV },
			.Padding0{ 0U },
			.Padding1{ 0U },
		};

		UB_Constants_.Store(
			&shaderConstants,
			sizeof(ShaderConstants),
			ConstantsStride * slot_
		);
	}

	// * 複数層を重ねるときは外側->内側の順で

	void FlamePillar::Renderer::Render(
		DX12::CommandList const& cmdList_,
		std::span<Instance const> instances_,
		Mat4 const& viewProj_,
		Vec3 const& cameraPos_
	) {
		uint32_t const num_Instances{
			std::min(static_cast<uint32_t>(instances_.size()), MaxInstances)
		};
		if (num_Instances == 0U) { return; }

		for (uint32_t idx{ 0U }; idx < num_Instances; ++idx) {
			StoreConstants(idx, instances_[idx], viewProj_, cameraPos_);
		}

		uint32_t const num_Vertices{ SegmentsU * SegmentsV * 6U };
		D3D12_GPU_VIRTUAL_ADDRESS const cbBase{ UB_Constants_->GetGPUVirtualAddress() };

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetGraphicsRootDescriptorTable(1U, SRV_Texture_.GPUHandle(0U));
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// * 奥（外側 -> 内側）
		cmdList_->SetPipelineState(PSO_BackFaces_.Get());
		for (int idx{ 0 }; idx < static_cast<int>(num_Instances); ++idx) {
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbBase + ConstantsStride * idx);
			cmdList_->DrawInstanced(num_Vertices, 1U, 0U, 0U);
		}

		// * 手前（内側 -> 外側）
		cmdList_->SetPipelineState(PSO_FrontFaces_.Get());
		for (int idx{ static_cast<int>(num_Instances) - 1 }; idx >= 0; --idx) {
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbBase + ConstantsStride * idx);
			cmdList_->DrawInstanced(num_Vertices, 1U, 0U, 0U);
		}
	}
}
