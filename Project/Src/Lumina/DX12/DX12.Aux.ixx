module;

#include<d3d12.h>

#include<External/nlohmann.JSON/single_include/nlohmann/json.hpp>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12.Aux;

export import Lumina.DX12.Aux.View;

//****	******	******	******	******	****//

import <cstdint>;
import <type_traits>;

import <memory>;
import <utility>;
import <any>;

import <vector>;
import <unordered_map>;

import <string>;

import Lumina.DX12;

import Lumina.Utils.Data;
import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;

	using JSON = NLohmannJSON;
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	LoadRootSignature						//
//	LoadRootSignatureSetup					//
//	LoadGraphicsPipelineState				//
//	LoadRasterizerState						//
//	LoadInputLayout							//
//////	//////	//////	//////	//////	//////

export namespace Lumina::DX12 {
	void LoadRootSignature(
		RootSignature& rs_,
		const GraphicsDevice& device_,
		const JSON& dict_RSSetup_,
		std::string_view debugName_
	);
	RootSignature::Setup LoadRootSignatureSetup(
		const JSON& dict_RSSetup_
	);

	void LoadGraphicsPipelineState(
		GraphicsPipelineState& graphicsPSO_,
		const GraphicsDevice& device_,
		const RootSignature& rootSignature_,
		const Shader& vertexShader_,
		const Shader& pixelShader_,
		const JSON& dict_PSOSetup_,
		std::string_view debugName_
	);
	RasterizerState LoadRasterizerState(
		const JSON& dict_PSOSetup_
	);
	GraphicsPipelineState::InputLayout LoadInputLayout(
		const JSON& dict_PSOSetup_
	);
}

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

module : private;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	namespace {
		template<typename T>
		concept Numeral = (std::is_integral_v<T> || std::is_floating_point_v<T>);
		template<typename T>
		concept EligibleForJSONKey = (std::is_integral_v<T> || std::is_constructible_v<std::string_view, T>);

		template<Numeral ValueType, EligibleForJSONKey KeyType>
		ValueType GetNumber(const JSON& jsonObj_, KeyType key_) {
			return jsonObj_.at(key_).get<ValueType>();
		}
		template<EligibleForJSONKey KeyType>
		std::string_view GetString(const JSON& jsonObj_, KeyType key_) {
			return jsonObj_.at(key_).get_ref<const std::string&>();
		}
	}

	namespace {
		class Lexicon {
		private:
			template<typename T>
			using Section = std::unordered_map<std::string, T>;

			//====	======	======	======	======	====//

		public:
			template<typename T>
			static T LookUp(const JSON& jsonObj_, std::string_view key_) {
				auto ptr_Table{ Sections_.at(key_.data()) };
				std::string_view val{ GetString(jsonObj_, key_) };
				return std::any_cast<const Section<T>*>(ptr_Table)->at(val.data());
			}
			template<typename T>
			static T LookUp(const JSON& jsonObj_, std::string_view key_, uint32_t index_) {
				auto ptr_Table{ Sections_.at(key_.data()) };
				std::string_view val{ GetString(jsonObj_, index_) };
				return std::any_cast<const Section<T>*>(ptr_Table)->at(val.data());
			}

			//====	======	======	======	======	====//

		private:
			static inline const Section<D3D12_SHADER_VISIBILITY> ShaderVisibilities_{
				{ "All", D3D12_SHADER_VISIBILITY_ALL },
				{ "Vertex", D3D12_SHADER_VISIBILITY_VERTEX },
				{ "Pixel", D3D12_SHADER_VISIBILITY_PIXEL },
			};

			static inline const Section<D3D12_ROOT_PARAMETER_TYPE> RootParameterTypes_{
				{ "DescriptorTable", D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE },
				{ "32BitConstants", D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS },
				{ "CBV", D3D12_ROOT_PARAMETER_TYPE_CBV },
				{ "SRV", D3D12_ROOT_PARAMETER_TYPE_SRV },
				{ "UAV", D3D12_ROOT_PARAMETER_TYPE_UAV },
			};
			static inline const Section<D3D12_DESCRIPTOR_RANGE_TYPE> DescriptorRangeTypes_{
				{ "CBV", D3D12_DESCRIPTOR_RANGE_TYPE_CBV },
				{ "SRV", D3D12_DESCRIPTOR_RANGE_TYPE_SRV },
				{ "UAV", D3D12_DESCRIPTOR_RANGE_TYPE_UAV },
				{ "Sampler", D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER },
			};

			static inline const Section<D3D12_FILTER_REDUCTION_TYPE> FilterReductionTypes_{
				{ "Standard", D3D12_FILTER_REDUCTION_TYPE_STANDARD },
				{ "Comparison", D3D12_FILTER_REDUCTION_TYPE_COMPARISON },
				{ "Minimum", D3D12_FILTER_REDUCTION_TYPE_MINIMUM },
				{ "Maximum", D3D12_FILTER_REDUCTION_TYPE_MAXIMUM },
			};
			static inline const Section<uint32_t> FilterTypes_{
				{ "Point", 0b00 },
				{ "Bilinear", 0b01 },
			};

			static inline const Section<D3D12_TEXTURE_ADDRESS_MODE> TextureAddressMode_{
				{ "Wrap", D3D12_TEXTURE_ADDRESS_MODE_WRAP },
				{ "Clamp", D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
				{ "Border", D3D12_TEXTURE_ADDRESS_MODE_BORDER },
				{ "Mirror", D3D12_TEXTURE_ADDRESS_MODE_MIRROR },
				{ "MirrorOnce", D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE },
			};

			//----	------	------	------	------	----//

			static inline const Section<D3D12_BLEND> Blends_{
				{ "One", D3D12_BLEND_ONE },
				{ "SrcColor", D3D12_BLEND_SRC_COLOR },
				{ "InvSrcColor", D3D12_BLEND_INV_SRC_COLOR },
				{ "SrcAlpha", D3D12_BLEND_SRC_ALPHA },
				{ "InvSrcAlpha", D3D12_BLEND_INV_SRC_ALPHA },
			};
			static inline const Section<D3D12_BLEND_OP> BlendOperations_{
				{ "Src+Dst", D3D12_BLEND_OP_ADD },
			};
			static inline const Section<D3D12_LOGIC_OP> LogicOperations_{
				{ "None", D3D12_LOGIC_OP_NOOP },
			};

			static inline const Section<D3D12_FILL_MODE> FillModes_{
				{ "Wireframe", D3D12_FILL_MODE_WIREFRAME },
				{ "Solid", D3D12_FILL_MODE_SOLID },
			};
			static inline const Section<D3D12_CULL_MODE> CullModes_{
				{ "None", D3D12_CULL_MODE_NONE },		// All polygons will be drawn regardless of facing direction.
				{ "Front", D3D12_CULL_MODE_FRONT },		// Front-facing polygons will not be drawn.
				{ "Back", D3D12_CULL_MODE_BACK },		// Back-facing polygons will not be drawn.
			};

			static inline const Section<D3D12_DEPTH_WRITE_MASK> DepthWriteMasks_{
				{ "Zero", D3D12_DEPTH_WRITE_MASK_ZERO },
				{ "All", D3D12_DEPTH_WRITE_MASK_ALL },
			};
			static inline const Section<D3D12_COMPARISON_FUNC> ComparisonFuncs_{
				{ "LessEqual", D3D12_COMPARISON_FUNC_LESS_EQUAL },
			};

			static inline const Section<D3D12_PRIMITIVE_TOPOLOGY_TYPE> PrimitiveTopologyTypes_{
				{ "Point", D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT },
				{ "Line", D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE },
				{ "Triangle", D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE },
				{ "Patch", D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH },
			};

			static inline const Section<DXGI_FORMAT> InputElementFormats_{
				{ "float2", DXGI_FORMAT_R32G32_FLOAT },
				{ "float3", DXGI_FORMAT_R32G32B32_FLOAT },
				{ "float4", DXGI_FORMAT_R32G32B32A32_FLOAT },
				{ "uint", DXGI_FORMAT_R32_UINT },
				{ "uint2", DXGI_FORMAT_R32G32_UINT },
				{ "uint3", DXGI_FORMAT_R32G32B32_UINT },
			};

			//----	------	------	------	------	----//

			static inline const Section<std::any> Sections_{
				{ "ShaderVisibility", &ShaderVisibilities_ },

				{ "ParameterType", &RootParameterTypes_ },
				{ "RangeType", &DescriptorRangeTypes_ },

				{ "ReductionType", &FilterReductionTypes_ },
				{ "MinFilter", &FilterTypes_ },
				{ "MagFilter", &FilterTypes_ },
				{ "MipFilter", &FilterTypes_ },

				{ "TextureAddressU", &TextureAddressMode_ },
				{ "TextureAddressV", &TextureAddressMode_ },
				{ "TextureAddressW", &TextureAddressMode_ },

				{ "SrcBlend", &Blends_ },
				{ "DestBlend", &Blends_ },
				{ "BlendOp", &BlendOperations_ },
				{ "SrcBlendAlpha", &Blends_ },
				{ "DestBlendAlpha", &Blends_ },
				{ "BlendOpAlpha", &BlendOperations_ },
				{ "LogicOp", &LogicOperations_ },

				{ "FillMode", &FillModes_ },
				{ "CullMode", &CullModes_ },
				{ "PrimitiveTopologyType", &PrimitiveTopologyTypes_ },

				{ "DepthWriteMask", &DepthWriteMasks_ },
				{ "DepthFunc", &ComparisonFuncs_ },

				{ "InputElementFormat", &InputElementFormats_ },
			};
		};
	}

	namespace {
		D3D12_DESCRIPTOR_RANGE& operator<<(
			D3D12_DESCRIPTOR_RANGE& descriptorRange_,
			const JSON& dict_DescriptorRange_
		) {
			descriptorRange_.RangeType =
				Lexicon::LookUp<decltype(descriptorRange_.RangeType)>(
					dict_DescriptorRange_, "RangeType"
				);
			descriptorRange_.NumDescriptors = GetNumber<uint32_t>(dict_DescriptorRange_, "NumDescriptors");
			descriptorRange_.BaseShaderRegister = GetNumber<uint32_t>(dict_DescriptorRange_, "BaseShaderRegister");
			descriptorRange_.RegisterSpace = GetNumber<uint32_t>(dict_DescriptorRange_, "RegisterSpace");

			int32_t offset{ GetNumber<int32_t>(dict_DescriptorRange_, "OffsetInDescriptorsFromTableStart") };
			descriptorRange_.OffsetInDescriptorsFromTableStart = *reinterpret_cast<uint32_t*>(&offset);

			return descriptorRange_;
		}

		std::vector<D3D12_DESCRIPTOR_RANGE>& operator<<(
			std::vector<D3D12_DESCRIPTOR_RANGE>& descriptorTable_,
			const std::pair<const JSON&, const JSON&>& dictPair_RSSetup_RSParam_
		) {
			const JSON& dict_RSSetup{ dictPair_RSSetup_RSParam_.first };
			const JSON& dict_RSParam{ dictPair_RSSetup_RSParam_.second };

			std::string_view descriptorTableName{ GetString(dict_RSParam, "TableName") };
			const auto& arr_DescriptorRanges{ dict_RSSetup.at(descriptorTableName) };
			for (
				auto&& it_DescriptorRange{ arr_DescriptorRanges.cbegin() };
				it_DescriptorRange != arr_DescriptorRanges.cend();
				++it_DescriptorRange
			) {
				const auto& dict_DescriptorRange{ *it_DescriptorRange };

				auto& descriptorRange{ descriptorTable_.emplace_back() };
				descriptorRange << dict_DescriptorRange;
			}

			return descriptorTable_;
		}
	}

	//////	//////	//////	//////	//////	//////
	//	LoadRootSignature						//
	//////	//////	//////	//////	//////	//////

	void LoadRootSignature(
		RootSignature& rs_,
		const GraphicsDevice& device_,
		const JSON& dict_RSSetup_,
		std::string_view debugName_
	) {
		auto&& rsSetup{ LoadRootSignatureSetup(dict_RSSetup_) };
		rs_.Initialize(device_, rsSetup, debugName_);
	}

	//////	//////	//////	//////	//////	//////
	//	LoadRootSignatureSetup					//
	//////	//////	//////	//////	//////	//////

	RootSignature::Setup LoadRootSignatureSetup(
		const JSON& dict_RSSetup_
	) {
		RootSignature::Setup rsSetup{};
		{
			const auto& arr_RSParams{ dict_RSSetup_.at("Parameters") };
			for (const auto& dict_RSParam : arr_RSParams) {
				auto& rsParam{ rsSetup.Parameters.emplace_back() };
				{
					rsParam.ShaderVisibility =
						Lexicon::LookUp<decltype(rsParam.ShaderVisibility)>(
							dict_RSParam, "ShaderVisibility"
						);
					rsParam.ParameterType =
						Lexicon::LookUp<decltype(rsParam.ParameterType)>(
							dict_RSParam, "ParameterType"
						);
					if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
						auto& descriptorTable{ rsSetup.DescriptorTables.emplace_back() };
						descriptorTable << std::make_pair(dict_RSSetup_, dict_RSParam);
						rsParam.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{
							.NumDescriptorRanges{ static_cast<uint32_t>(descriptorTable.size()) },
							.pDescriptorRanges{ descriptorTable.data() },
						};
					}
					else if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
						rsParam.Constants = D3D12_ROOT_CONSTANTS{
							.ShaderRegister{ GetNumber<uint32_t>(dict_RSParam, "ShaderRegister") },
							.RegisterSpace{ GetNumber<uint32_t>(dict_RSParam, "RegisterSpace") },
							.Num32BitValues{ GetNumber<uint32_t>(dict_RSParam, "Num32BitValues") },
						};
					}
					// CBV, SRV, UAV
					else {
						rsParam.Descriptor = D3D12_ROOT_DESCRIPTOR{
							.ShaderRegister{ GetNumber<uint32_t>(dict_RSParam, "ShaderRegister") },
							.RegisterSpace{ GetNumber<uint32_t>(dict_RSParam, "RegisterSpace") },
						};
					}
				}
			}

			const auto& arr_StaticSamplers{ dict_RSSetup_.at("StaticSamplers") };
			for (const auto& dict_StaticSampler : arr_StaticSamplers) {
				auto& staticSampler{ rsSetup.StaticSamplers.emplace_back() };
				{
					const auto& dict_Filter{ dict_StaticSampler.at("Filter") };
					{
						auto filterReductionType{
							Lexicon::LookUp<D3D12_FILTER_REDUCTION_TYPE>(
								dict_Filter, "ReductionType"
							)
						};

						if (!!GetNumber<uint32_t>(dict_Filter, "IsAnisotropic")) {
							staticSampler.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(filterReductionType);
						}
						else {
							auto minFilter{ Lexicon::LookUp<uint32_t>(dict_Filter, "MinFilter") };
							auto magFilter{ Lexicon::LookUp<uint32_t>(dict_Filter, "MagFilter") };
							auto mipFilter{ Lexicon::LookUp<uint32_t>(dict_Filter, "MipFilter") };
							staticSampler.Filter = D3D12_ENCODE_BASIC_FILTER(
								minFilter, magFilter, mipFilter,
								filterReductionType
							);
						}
					}

					staticSampler.AddressU =
						Lexicon::LookUp<decltype(staticSampler.AddressU)>(
							dict_StaticSampler, "TextureAddressU"
						);
					staticSampler.AddressV =
						Lexicon::LookUp<decltype(staticSampler.AddressV)>(
							dict_StaticSampler, "TextureAddressV"
						);
					staticSampler.AddressW =
						Lexicon::LookUp<decltype(staticSampler.AddressW)>(
							dict_StaticSampler, "TextureAddressW"
						);

					staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

					staticSampler.MaxLOD = D3D12_FLOAT32_MAX;

					staticSampler.ShaderRegister = GetNumber<uint32_t>(dict_StaticSampler, "ShaderRegister");
					staticSampler.ShaderVisibility =
						Lexicon::LookUp<decltype(staticSampler.ShaderVisibility)>(
							dict_StaticSampler, "ShaderVisibility"
						);
				}
			}
		}
		return rsSetup;
	}

	//////	//////	//////	//////	//////	//////
	//	LoadGraphicsPipelineState				//
	//////	//////	//////	//////	//////	//////

	void LoadGraphicsPipelineState(
		GraphicsPipelineState& graphicsPSO_,
		const GraphicsDevice& device_,
		const RootSignature& rootSignature_,
		const Shader& vertexShader_,
		const Shader& pixelShader_,
		const JSON& dict_PSOSetup_,
		std::string_view debugName_
	) {
		BlendState blendState{};
		{
			blendState.AlphaToCoverageEnable = false;
			blendState.IndependentBlendEnable = false;
			blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_SRC_ALPHA },
				.DestBlendAlpha{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
		}

		RasterizerState rasterizerState{
			LoadRasterizerState(dict_PSOSetup_)
		};

		DepthStencilState depthStencilState{
			.DepthEnable{ true },
			.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
			.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
		};
		
		GraphicsPipelineState::InputLayout inputLayout{
			LoadInputLayout(dict_PSOSetup_)
		};

		graphicsPSO_.Initialize(
			device_,
			rootSignature_,
			vertexShader_,
			pixelShader_,
			blendState,
			rasterizerState,
			depthStencilState,
			inputLayout,
			Lexicon::LookUp<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(dict_PSOSetup_, "PrimitiveTopologyType"),
			GraphicsPipelineState::DefaultRTVFormats,
			GraphicsPipelineState::DefaultDSVFormat,
			debugName_
		);
	}

	//////	//////	//////	//////	//////	//////
	//	LoadRasterizerState						//
	//////	//////	//////	//////	//////	//////

	RasterizerState LoadRasterizerState(
		const JSON& dict_PSOSetup_
	) {
		RasterizerState rasterizerState{};
		const auto& dict_RasterizerState{ dict_PSOSetup_.at("RasterizerState") };
		{
			rasterizerState.FillMode =
				Lexicon::LookUp<decltype(rasterizerState.FillMode)>(
					dict_RasterizerState, "FillMode"
				);
			rasterizerState.CullMode =
				Lexicon::LookUp<decltype(rasterizerState.CullMode)>(
					dict_RasterizerState, "CullMode"
				);
		}
		return rasterizerState;
	}

	//////	//////	//////	//////	//////	//////
	//	LoadInputLayout							//
	//////	//////	//////	//////	//////	//////

	GraphicsPipelineState::InputLayout LoadInputLayout(
		const JSON& dict_PSOSetup_
	) {
		GraphicsPipelineState::InputLayout inputLayout{};
		{
			const auto& arr_InputLayout{ dict_PSOSetup_.at("InputLayout") };
			for (const auto& arr_InputElement : arr_InputLayout) {
				inputLayout.Append(
					// Semantic name
					GetString(arr_InputElement, 0U),
					// Semantic index
					GetNumber<uint32_t>(arr_InputElement, 1U),
					// Format
					Lexicon::LookUp<DXGI_FORMAT>(arr_InputElement, "InputElementFormat", 2U)
				);
			}
		}
		return inputLayout;
	}
}