module Lumina.ShowcaseCharacter;

import <cmath>;
import <algorithm>;

import nlohmann.json;

import Lumina.Utils.Data;
import Lumina.Utils.Data.Mesh;

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
		auto Scale3(Float3 const& c_, float s_) -> Float3 {
			return Float3{ c_.x * s_, c_.y * s_, c_.z * s_ };
		}

		// 起動時に読み込むアニメ（タイトル向けに待機・溜め系を中心に）
		struct AnimationFile {
			char const* Name;
			char const* FileName;
		};
		constexpr AnimationFile AnimationFiles[]{
			{ "Idle", "Idle.gltf" },
			{ "Idle_Drawn_H", "Idle_Drawn_H.gltf" },
			{ "Idle_Drawn_Open_H", "Idle_Drawn_Open_H.gltf" },
			{ "IdleHoldingUmbrella", "IdleHoldingUmbrella.gltf" },
			{ "Guard", "guard.gltf" },
			{ "ReverseCharge", "ReverseCharge.gltf" },
			{ "Swinging", "Swinging.gltf" },
			{ "Rotate_H", "Rotate_H.gltf" },
		};
	}

	void ShowcaseCharacter::Initialize(
		DX12::Context const& dxContext_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_
	) {
		auto const& device{ dxContext_.Device() };

		//----	------	------	------	------	----//
		// Skinned model（InGame と同じ Neki）
		//----	------	------	------	------	----//

		Skinned_ = std::make_unique<ShowcaseDetail::SkinnedResources>();
		{
			auto& skinned{ *Skinned_ };
			skinned.Collection = CG3D::Import("Neki.gltf", "Assets/Neki");
			auto const& mesh{ skinned.Collection.Meshes[0] };

			uint64_t const sizeVB{ sizeof(CG3D::Mesh::Vertex) * mesh.Vertices.size() };
			skinned.VertexBuffer.Initialize(device, sizeVB, "Showcase.Skinned.VB");
			skinned.VertexBuffer.Store(mesh.Vertices.data(), sizeVB, 0LLU);
			skinned.VBV = DX12::VBV::Create<CG3D::Mesh::Vertex>(skinned.VertexBuffer);

			uint64_t const sizeIB{ sizeof(uint32_t) * mesh.Indices.size() };
			skinned.IndexBuffer.Initialize(device, sizeIB, "Showcase.Skinned.IB");
			skinned.IndexBuffer.Store(mesh.Indices.data(), sizeIB, 0LLU);
			skinned.IBV = DX12::IBV::Create(skinned.IndexBuffer);
			skinned.NumIndices = static_cast<uint32_t>(mesh.Indices.size());

			skinned.Skeleton = CG3D::CreateSkeleton(skinned.Collection.Root);
			CG3D::CreateSkinCluster(
				skinned.SkinCluster,
				device,
				dxContext_.GlobalDescriptorHeap(),
				skinned.Skeleton,
				mesh
			);

			// InGame と同じ右手ボーン
			auto const itr{ skinned.Skeleton.IDX_Joint.find("Bone.024") };
			skinned.HasRightHand = (itr != skinned.Skeleton.IDX_Joint.end());
			skinned.IDX_RightHand = skinned.HasRightHand ? itr->second : 0U;

			for (auto const& file : AnimationFiles) {
				auto animations{ CG3D::LoadAnimationFile(file.FileName, "Assets/Neki") };
				if (animations.empty()) { continue; }
				skinned.Animations.emplace(file.Name, std::move(animations[0]));
				AnimationNames_.emplace_back(file.Name);
			}
		}

		//----	------	------	------	------	----//
		// Weapon（InGame と同じ Kinoko.obj を引き伸ばしたもの）
		//----	------	------	------	------	----//

		Weapon_ = std::make_unique<ShowcaseDetail::StaticResources>();
		{
			auto const meshes{
				Utils::Mesh::Load(
					Utils::LoadFromFile<Utils::WavefrontOBJ>("Kinoko.obj", "Assets/Kinoko")
				)
			};

			auto& weapon{ *Weapon_ };

			// インデックス参照の頂点をスキンメッシュと同じ { Pos, UV, Normal } に展開する
			std::vector<CG3D::Mesh::Vertex> vertices{};
			for (auto const& mesh : meshes) {
				for (auto const& vert : mesh.Vertices) {
					auto& dst{ vertices.emplace_back() };
					dst.Position = mesh.Positions[vert.Index_Position];
					dst.TexCoord = mesh.TexCoords.empty() ? Float2{ 0.0f, 0.0f } : mesh.TexCoords[vert.Index_TexCoord];
					dst.Normal = mesh.Normals.empty() ? Float3{ 0.0f, 1.0f, 0.0f } : mesh.Normals[vert.Index_Normal];
				}
			}

			// 軸（Y）の範囲と太さ
			if (!vertices.empty()) {
				weapon.AxisMinY = vertices.front().Position.y;
				weapon.AxisMaxY = vertices.front().Position.y;
				for (auto const& vert : vertices) {
					weapon.AxisMinY = std::min(weapon.AxisMinY, vert.Position.y);
					weapon.AxisMaxY = std::max(weapon.AxisMaxY, vert.Position.y);
					weapon.AxisRadius = std::max(
						weapon.AxisRadius,
						std::sqrt(vert.Position.x * vert.Position.x + vert.Position.z * vert.Position.z)
					);
				}
			}

			uint64_t const sizeVB{ sizeof(CG3D::Mesh::Vertex) * vertices.size() };
			weapon.VertexBuffer.Initialize(device, sizeVB, "Showcase.Weapon.VB");
			weapon.VertexBuffer.Store(vertices.data(), sizeVB, 0LLU);
			weapon.VBV = DX12::VBV::Create<CG3D::Mesh::Vertex>(weapon.VertexBuffer);
			weapon.NumVertices = static_cast<uint32_t>(vertices.size());
		}

		//----	------	------	------	------	----//
		// Root signature / Shaders
		//----	------	------	------	------	----//

		{
			auto settings{ Utils::LoadFromFile<nlohmann::json>("ShowcaseCharacter.json", "Assets/Title") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device, rsSetup, "ShowcaseCharacter RS");
		}

		dxContext_.Compile(
			VS_Skinned_,
			L"Assets/Shaders/MeshSkinning.VS.hlsl",
			L"vs_6_6",
			L"main",
			"ShowcaseCharacter.Skinned.VS"
		);
		dxContext_.Compile(
			VS_Static_,
			L"Assets/Title/ShowcaseCharacter.hlsl",
			L"vs_6_6",
			L"VSStatic",
			"ShowcaseCharacter.Static.VS"
		);
		dxContext_.Compile(
			PS_,
			L"Assets/Title/ShowcaseCharacter.hlsl",
			L"ps_6_6",
			L"PSMain",
			"ShowcaseCharacter.PS"
		);

		//----	------	------	------	------	----//
		// PSO（不透明・深度書き込みあり）
		//----	------	------	------	------	----//

		{
			DX12::BlendState blendState{};
			blendState.RenderTarget[0].BlendEnable = false;
			blendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			DX12::RasterizerState rasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_BACK },
				.FrontCounterClockwise{ false },
				.DepthClipEnable{ true },
			};

			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ dsvFormat_ != DXGI_FORMAT_UNKNOWN },
				.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
				.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
				.StencilEnable{ false },
			};

			std::vector<DXGI_FORMAT> const rtvFormats{ rtvFormat_ };

			// * スキンメッシュ：InGame と同じ入力レイアウト（スロット 1 にインフルエンス）
			DX12::GraphicsPSO::InputLayout inputLayout_Skinned{};
			inputLayout_Skinned.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			inputLayout_Skinned.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
			inputLayout_Skinned.Append("NORMAL", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			inputLayout_Skinned.Append("WEIGHT", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT, 1U);
			inputLayout_Skinned.Append("PALETTE", 0U, DXGI_FORMAT_R32G32B32A32_SINT, 1U);

			PSO_Skinned_.Initialize(
				device,
				RS_,
				VS_Skinned_,
				PS_,
				blendState,
				rasterizerState,
				depthStencilState,
				inputLayout_Skinned,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				rtvFormats,
				dsvFormat_,
				"ShowcaseCharacter PSO (Skinned)"
			);

			// * 静的メッシュ：スロット 0 のみ
			DX12::GraphicsPSO::InputLayout inputLayout_Static{};
			inputLayout_Static.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			inputLayout_Static.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
			inputLayout_Static.Append("NORMAL", 0U, DXGI_FORMAT_R32G32B32_FLOAT);

			PSO_Static_.Initialize(
				device,
				RS_,
				VS_Static_,
				PS_,
				blendState,
				rasterizerState,
				depthStencilState,
				inputLayout_Static,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				rtvFormats,
				dsvFormat_,
				"ShowcaseCharacter PSO (Static)"
			);
		}

		//----	------	------	------	------	----//
		// Constant buffers（スロットごとに 256 バイト境界、Root CBV で指す）
		//----	------	------	------	------	----//

		UB_Transforms_.Initialize(device, ConstantsStride * NumSlots, "ShowcaseCharacter Transforms");
		UB_Shading_.Initialize(device, ConstantsStride * NumSlots, "ShowcaseCharacter Shading");

		//----	------	------	------	------	----//

		if (!AnimationNames_.empty()) {
			Play(AnimationNames_.front(), true);
			Advance(0.0f);
		}
	}

	void ShowcaseCharacter::Play(std::string_view name_, bool isLoop_) {
		IsLoop_ = isLoop_;
		if (CurrentAnimationName_ == name_) { return; }

		auto const itr{ Skinned_->Animations.find(std::string{ name_ }) };
		if (itr == Skinned_->Animations.end()) { return; }

		CurrentAnimation_ = &(itr->second);
		CurrentAnimationName_ = itr->first;
		AnimationTime_ = 0.0f;
	}

	void ShowcaseCharacter::Advance(float deltaTime_) {
		if (CurrentAnimation_ == nullptr) { return; }

		AnimationTime_ += deltaTime_ * AnimationSpeed;

		float const duration{ CurrentAnimation_->DurationInSeconds };
		if (duration > 0.0f) {
			if (IsLoop_) {
				AnimationTime_ = std::fmod(AnimationTime_, duration);
				if (AnimationTime_ < 0.0f) { AnimationTime_ += duration; }
			}
			else {
				AnimationTime_ = std::clamp(AnimationTime_, 0.0f, duration);
			}
		}

		CG3D::Update(
			Skinned_->SkinCluster,
			Skinned_->Skeleton,
			*CurrentAnimation_,
			AnimationTime_
		);
	}

	void ShowcaseCharacter::StoreTransforms(uint32_t slot_, Mat4 const& world_, Mat4 const& viewProj_) {
		TransformConstants constants{};
		constants.WVP = world_ * viewProj_;
		constants.World = world_;
		Mat4 const inv_World{ world_.Inv() };
		Mat4::Transpose(constants.TR_INV_World, inv_World);

		UB_Transforms_.Store(&constants, sizeof(TransformConstants), ConstantsStride * slot_);
	}

	void ShowcaseCharacter::StoreShading(
		uint32_t slot_,
		ShowcaseMaterial const& material_,
		Vec3 const& cameraPos_,
		ShowcaseLighting const& lighting_
	) {
		ShadingConstants const constants{
			.BaseColor{ SRGBToLinear(material_.BaseColor) },
			.CelThreshold{ lighting_.CelThreshold },
			.EmissiveColor{ Scale3(SRGBToLinear(material_.EmissiveColor), material_.EmissiveIntensity) },
			.CelSoftness{ std::max(lighting_.CelSoftness, 0.001f) },
			.FireBase{ lighting_.FireBase },
			.FireHeight{ std::max(lighting_.FireHeight, 0.01f) },
			.FireColor{ Scale3(SRGBToLinear(lighting_.FireColor), lighting_.FireIntensity) },
			.UnderWeight{ lighting_.UnderWeight },
			.SkyColor{ Scale3(SRGBToLinear(lighting_.SkyColor), lighting_.AmbientIntensity) },
			.ShadowLevel{ lighting_.ShadowLevel },
			.GroundColor{ Scale3(SRGBToLinear(lighting_.GroundColor), lighting_.AmbientIntensity) },
			.RimThreshold{ lighting_.RimThreshold },
			.RimColor{ Scale3(SRGBToLinear(lighting_.RimColor), lighting_.RimIntensity) },
			.RimSoftness{ std::max(lighting_.RimSoftness, 0.001f) },
			.CameraPos{ cameraPos_.x, cameraPos_.y, cameraPos_.z },
			.Padding0{ 0.0f },
		};

		UB_Shading_.Store(&constants, sizeof(ShadingConstants), ConstantsStride * slot_);
	}

	Mat4 ShowcaseCharacter::MeshWorld() const {
		return Mat4::SRT(
			Vec3{ Scale, Scale, Scale },
			Vec3{ 0.0f, Yaw, 0.0f },
			Vec3{ Position.x, Position.y, Position.z }
		);
	}

	bool ShowcaseCharacter::WeaponWorld(Mat4 const& meshWorld_, Mat4& weaponWorld_) const {
		auto const& skinned{ *Skinned_ };
		auto const& weapon{ *Weapon_ };
		if (!ShowWeapon || !skinned.HasRightHand || weapon.NumVertices == 0U) { return false; }

		// InGame の Tag_HoldWeapon と同じ：ボーンの SkeletonSpace × メッシュのワールド
		Mat4 const handWorld{ skinned.Skeleton.ARR_Joint[skinned.IDX_RightHand].SkeletonSpace * meshWorld_ };
		weaponWorld_ =
			Mat4::SRT(
				Vec3{ WeaponScale.x, WeaponScale.y, WeaponScale.z },
				Vec3{ WeaponRotate.x, WeaponRotate.y, WeaponRotate.z },
				Vec3{ WeaponOffset.x, WeaponOffset.y, WeaponOffset.z }
			) * handWorld;
		return true;
	}

	bool ShowcaseCharacter::WeaponAxis(Float3& base_, Float3& tip_, float& radius_) const {
		Mat4 weaponWorld{};
		if (!WeaponWorld(MeshWorld(), weaponWorld)) { return false; }

		auto const& weapon{ *Weapon_ };
		Vec4 const base{ Vec4{ 0.0f, weapon.AxisMinY, 0.0f, 1.0f } * weaponWorld };
		Vec4 const tip{ Vec4{ 0.0f, weapon.AxisMaxY, 0.0f, 1.0f } * weaponWorld };
		Vec4 const side{ Vec4{ weapon.AxisRadius, weapon.AxisMinY, 0.0f, 1.0f } * weaponWorld };

		base_ = Float3{ base.x, base.y, base.z };
		tip_ = Float3{ tip.x, tip.y, tip.z };
		float const dx{ side.x - base.x };
		float const dy{ side.y - base.y };
		float const dz{ side.z - base.z };
		radius_ = std::sqrt(dx * dx + dy * dy + dz * dz);
		return true;
	}

	void ShowcaseCharacter::Render(
		DX12::CommandList const& cmdList_,
		Mat4 const& viewProj_,
		Vec3 const& cameraPos_,
		ShowcaseLighting const& lighting_
	) {
		auto const& skinned{ *Skinned_ };
		auto const& weapon{ *Weapon_ };

		Mat4 const meshWorld{ MeshWorld() };
		StoreTransforms(0U, meshWorld, viewProj_);
		StoreShading(0U, BodyMaterial, cameraPos_, lighting_);

		Mat4 weaponWorld{};
		bool const drawWeapon{ WeaponWorld(meshWorld, weaponWorld) };
		if (drawWeapon) {
			StoreTransforms(1U, weaponWorld, viewProj_);
			StoreShading(1U, WeaponMaterial, cameraPos_, lighting_);
		}

		D3D12_GPU_VIRTUAL_ADDRESS const cbTransforms{ UB_Transforms_->GetGPUVirtualAddress() };
		D3D12_GPU_VIRTUAL_ADDRESS const cbShading{ UB_Shading_->GetGPUVirtualAddress() };

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// * Body
		{
			cmdList_->SetPipelineState(PSO_Skinned_.Get());
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbTransforms);
			cmdList_->SetGraphicsRootDescriptorTable(1U, skinned.SkinCluster.PaletteSRVHandle.second);
			cmdList_->SetGraphicsRootConstantBufferView(2U, cbShading);

			D3D12_VERTEX_BUFFER_VIEW const vbvs[2]{
				skinned.VBV,
				skinned.SkinCluster.InfluenceBufferView,
			};
			cmdList_->IASetVertexBuffers(0U, 2U, vbvs);
			cmdList_->IASetIndexBuffer(&skinned.IBV);
			cmdList_->DrawIndexedInstanced(skinned.NumIndices, 1U, 0U, 0, 0U);
		}

		// * Weapon
		if (drawWeapon) {
			cmdList_->SetPipelineState(PSO_Static_.Get());
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbTransforms + ConstantsStride);
			cmdList_->SetGraphicsRootConstantBufferView(2U, cbShading + ConstantsStride);
			cmdList_->IASetVertexBuffers(0U, 1U, &weapon.VBV);
			cmdList_->DrawInstanced(weapon.NumVertices, 1U, 0U, 0U);
		}
	}
}
