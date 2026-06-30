module CG3Eval2;

import <cstdint>;
import <algorithm>;
import <vector>;
import <cassert>;

import Lumina.AssetManager;

import Lumina.Utils.Data;
import Lumina.Utils.Data.Mesh;

import Lumina.Math;

import Lumina.Skybox;
import Lumina.SimpleFX;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

namespace {
	using namespace Lumina;

	Mat4 LocalToWorld_{};
	Mat4 Transpose_WorldToLocal_{};
	Mat4 ScreenToWorld_{};
	Vec3 ModelScale_{ 3.0f, 3.0f, 3.0f };
	Vec3 ModelRotate_{ 0.0f, 0.0f, 0.0f };
	Vec3 ModelTranslate_{ 0.0f, 0.0f, 0.0f };
	float ModelShininess_{ 32.0f };
	int IsUsingBlinnPhong_{ 1 };

	Vec3 LookAtSrc_{ -20.0f, 5.0f, -15.0f };
	Vec3 LookAtDst_{ 0.0f, 0.0f, 0.0f };

	std::vector<uint32_t> ActivePtLightList_{};

	Mat4 Inv_Viewport{
		1.0f / 640.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f / 360.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 1.0f,
	};

	struct Vertex {
		Float3 LocalPos;
		Float2 TexCoord;
		float TexID;
		Float3 Normal;
	};

	struct DissolveConstants {
		uint32_t MaskTextureID;
		float Threshold;
	} DissolveC;

	template<uint32_t Div1 = 12U, uint32_t Div2 = 24U>
		requires(Div1 > 0U && Div2 > 0U)
	void CreateSphere(
		std::vector<Vertex>& vertices_,
		std::vector<uint32_t>& indices_,
		float radius_,
		Lumina::Float3 const& center_ = { 0.0f, 0.0f, 0.0f }
	) {
		vertices_.clear();
		indices_.clear();

		Lumina::Math::PerlinNoise noise{ 1.0f, 4U, 0.5f };

		constexpr float inv_Div1{ 1.0f / static_cast<float>(Div1) };
		constexpr float deltaTheta{ 1.0f * std::numbers::pi_v<float> *inv_Div1 };
		constexpr float inv_Div2{ 1.0f / static_cast<float>(Div2) };
		constexpr float deltaPhi{ 2.0f * std::numbers::pi_v<float> *inv_Div2 };

		float cosThetas[Div1 + 1]{};
		float sinThetas[Div1 + 1]{};
		float cosPhis[Div2 + 1]{};
		float sinPhis[Div2 + 1]{};
		for (int i{ 0 }; i <= Div1; ++i) {
			float const theta{ i * deltaTheta - 0.5f * std::numbers::pi_v<float> };
			cosThetas[i] = std::cos(theta);
			sinThetas[i] = std::sin(theta);
		}
		for (int i{ 0 }; i <= Div2; ++i) {
			float const phi{ i * deltaPhi };
			cosPhis[i] = std::cos(phi);
			sinPhis[i] = std::sin(phi);
		}

		for (int i{ 0 }; i <= Div1; ++i) {
			for (int j{ 0 }; j <= Div2; ++j) {
				auto& vert = vertices_.emplace_back();
				{
					vert.LocalPos.x = cosThetas[i] * cosPhis[j] * radius_ + center_.x;
					vert.LocalPos.z = cosThetas[i] * sinPhis[j] * radius_ + center_.z;
					vert.LocalPos.y = sinThetas[i] * radius_ + center_.y;

					vert.Normal.x = cosThetas[i] * cosPhis[j];
					vert.Normal.z = cosThetas[i] * sinPhis[j];
					vert.Normal.y = sinThetas[i];

					vert.TexCoord.x = j * inv_Div2;
					vert.TexCoord.y = 1.0f - i * inv_Div1;

					vert.TexID = noise(i * 3.5f, j * 3.5f, (i + j) * 3.5f);
				}
			}
		}

		for (int i{ 0 }; i < Div1; ++i) {
			for (int j{ 0 }; j < Div2; ++j) {
				indices_.emplace_back(i * (Div2 + 1) + j);
				indices_.emplace_back((i + 1) * (Div2 + 1) + j);
				indices_.emplace_back((i + 1) * (Div2 + 1) + (j + 1));
				indices_.emplace_back(i * (Div2 + 1) + j);
				indices_.emplace_back((i + 1) * (Div2 + 1) + (j + 1));
				indices_.emplace_back(i * (Div2 + 1) + (j + 1));
			}
		}
	}

	Lumina::Mat4 LookAt(
		Lumina::Vec3 const& src_,
		Lumina::Vec3 const& dst_,
		Lumina::Vec3 const& up_
	) {
		Lumina::Vec3 const forward{ Lumina::Vec3{ dst_ - src_ }.Unit() };
		Lumina::Vec3 const right{ Lumina::Vec3::Cross(up_, forward).Unit() };
		Lumina::Vec3 const up{ Lumina::Vec3::Cross(forward, right) };

		return {
			right.x,
			up.x,
			forward.x,
			0.0f,

			right.y,
			up.y,
			forward.y,
			0.0f,

			right.z,
			up.z,
			forward.z,
			0.0f,

			-Lumina::Vec3::Dot(src_, right),
			-Lumina::Vec3::Dot(src_, up),
			-Lumina::Vec3::Dot(src_, forward),
			1.0f,
		};
	}
}

namespace CG3Eval2 {
	namespace {
		struct GradientControl {
			int Index;
			Float4 Color;
		};

		struct RadialBlurParams {
			Float2 ScreenSpaceCenter;
			Float2 RCP_Size;
			float BlurWidth_R;
			float BlurWidth_G;
			float BlurWidth_B;
			uint32_t NUM_Samples;
			float RCP_NUM_Samples;
			float Time;
		} radialBlurParams{};
	}

	void Scene::Update() {
		#if defined(_DEBUG)

		ImGui::Begin("CG3");

		bool srtChanged = 0;
		srtChanged |= ImGui::DragFloat3("Model.Scale", ModelScale_(), 0.01f);
		srtChanged |= ImGui::DragFloat3("Model.Rotate", ModelRotate_(), 0.01f);
		srtChanged |= ImGui::DragFloat3("Model.Translate", ModelTranslate_(), 0.01f);
		if (srtChanged) {
			LocalToWorld_ = Mat4::SRT(ModelScale_, ModelRotate_, ModelTranslate_);
			Mat4::Transpose(Transpose_WorldToLocal_, LocalToWorld_.Inv());
		}

		ImGui::DragFloat("Model.Shininess", &ModelShininess_, 0.01f, 0.0f);
		if (ModelShininess_ < 0.25f) { ModelShininess_ = 0.25f; }

		static Lumina::Vec3 fxTranslate{ (LookAtSrc_ + LookAtDst_) * 0.5f };
		ImGui::DragFloat3("FX Translate", fxTranslate(), 0.01f);

		ImGui::Separator();

		bool cameraChanged = 0;
		cameraChanged |= ImGui::DragFloat3("LookAt.Src", LookAtSrc_(), 0.1f);
		cameraChanged |= ImGui::DragFloat3("LookAt.Dst", LookAtDst_(), 0.1f);
		if (cameraChanged) {
			WorldToView_ = LookAt(LookAtSrc_, LookAtDst_, { 0.0f, 1.0f, 0.0f });
			Constant_Scene_.WorldToProjective = WorldToView_ * ViewToProjective_;
			ScreenToWorld_ = Inv_Viewport * Constant_Scene_.WorldToProjective.Inv();
		}

		ImGui::Separator();

		ImGui::Checkbox("IsUsingBlinnPhong", reinterpret_cast<bool*>(&IsUsingBlinnPhong_));

		ImGui::Separator();

		static int selectedDirLight = 0;
		if (ImGui::Button("Add Directional Light")) {
			if (!List_DirectionalLight_.IsFull()) {
				auto& light = List_DirectionalLight_.New();
				{
					light.Direction = { 1.0f, 0.0f, 0.0f };
					light.DiffuseRGB = { 1.0f, 1.0f, 1.0f };
					light.DiffuseIntensity = 0.25f;
					light.SpecularRGB = { 1.0f, 1.0f, 1.0f };
					light.SpecularIntensity = 2.0f;
				}
				selectedDirLight = List_DirectionalLight_.Size() - 1;
			}
		}
		ImGui::SliderInt("Directional Light", &selectedDirLight, 0, List_DirectionalLight_.Size() - 1);
		auto& dirLight = List_DirectionalLight_.At(selectedDirLight);
		if (ImGui::DragFloat3("Direction##DirLight", &dirLight.Direction.x, 0.01f)) {
			Lumina::Vec3 tmp{ &dirLight.Direction.x };
			tmp = tmp.Unit();
			std::memcpy(&dirLight.Direction, &tmp, sizeof(Lumina::Float3));
		};
		ImGui::ColorEdit3("Diffuse Color##DirLight", &dirLight.DiffuseRGB.x);
		ImGui::DragFloat("Diffuse Intensity##DirLight", &dirLight.DiffuseIntensity, 0.01f, 0.0f);
		if (dirLight.DiffuseIntensity < 0.0f) { dirLight.DiffuseIntensity = 0.0f; }
		ImGui::ColorEdit3("Specular Color##DirLight", &dirLight.SpecularRGB.x);
		ImGui::DragFloat("Specular Intensity##DirLight", &dirLight.SpecularIntensity, 0.01f, 0.0f);
		if (dirLight.SpecularIntensity < 0.0f) { dirLight.SpecularIntensity = 0.0f; }

		static int selectedPointLight = 0;
		if (ImGui::Button("Add Point Light")) {
			ActivePtLightList_.emplace_back(List_PointLight_.Size());
			if (!List_PointLight_.IsFull()) {
				auto& light = List_PointLight_.New();
				{
					light.WorldPosition = { 0.0f, 1.0f, -4.0f };
					light.DiffuseRGB = { 1.0f, 0.25f, 0.0f };
					light.DiffuseIntensity = 3.0f;
					light.SpecularRGB = { 1.0f, 0.25f, 0.0f };
					light.SpecularIntensity = 5.0f;
				}
				auto& lightSphere = List_Matrix_World_LightSphere_.New();
				{
					float const r{ Lumina::LightSphereRadius(1024.0f, 2.0f, 1.0f, 1.0f, 0.5f) };
					lightSphere = {
						r, 0.0f, 0.0f, 0.0f,
						0.0f, r, 0.0f, 0.0f,
						0.0f, 0.0f, r, 0.0f,
						-5.0f, 0.0f, 0.0f, 1.0f,
					};
				}
				selectedPointLight = List_PointLight_.Size() - 1;
			}
		}
		ImGui::SliderInt("Point Light", &selectedPointLight, 0, List_PointLight_.Size() - 1);
		auto& ptLight = List_PointLight_.At(selectedPointLight);
		auto& ptLightSphere = List_Matrix_World_LightSphere_.At(selectedPointLight);
		if (ImGui::DragFloat3("WorldPos##PtLight", &ptLight.WorldPosition.x, 0.01f)) {
			ptLightSphere[3][0] = ptLight.WorldPosition.x;
			ptLightSphere[3][1] = ptLight.WorldPosition.y;
			ptLightSphere[3][2] = ptLight.WorldPosition.z;
		};
		ImGui::ColorEdit3("Diffuse Color##PtLight", &ptLight.DiffuseRGB.x);
		if (ImGui::DragFloat("Diffuse Intensity##PtLight", &ptLight.DiffuseIntensity, 0.01f, 0.0f)) {
			if (ptLight.DiffuseIntensity < 0.0f) { ptLight.DiffuseIntensity = 0.0f; }
			float intensity{ std::max<float>(ptLight.DiffuseIntensity, ptLight.SpecularIntensity) };
			float const r{ Lumina::LightSphereRadius(1024.0f, intensity, 1.0f, 1.0f, 0.5f) };
			ptLightSphere[0][0] = r;
			ptLightSphere[1][1] = r;
			ptLightSphere[2][2] = r;
		}
		ImGui::ColorEdit3("Specular Color##PtLight", &ptLight.SpecularRGB.x);
		if (ImGui::DragFloat("Specular Intensity##PtLight", &ptLight.SpecularIntensity, 0.01f, 0.0f)) {
			if (ptLight.DiffuseIntensity < 0.0f) { ptLight.DiffuseIntensity = 0.0f; }
			float intensity{ std::max<float>(ptLight.DiffuseIntensity, ptLight.SpecularIntensity) };
			float const r{ Lumina::LightSphereRadius(1024.0f, intensity, 1.0f, 1.0f, 0.5f) };
			ptLightSphere[0][0] = r;
			ptLightSphere[1][1] = r;
			ptLightSphere[2][2] = r;
		}

		ImGui::End();

		ImGui::Begin("Vignetting");
		auto& vignettingParams{ Vignetting_->ConstantsData() };
		ImGui::DragFloat("Scale", &vignettingParams.Scale, 0.01f);
		ImGui::DragFloat("Power", &vignettingParams.Power, 0.01f);
		Vignetting_->Update();
		ImGui::End();

		//ImGui::Begin("G-Buffers");
		//ImGui::Image(GlobalTable_Graphics_.GPUHandle(0U + 64U + 96U).ptr, { 480.0f, 270.0f });
		//ImGui::Image(GlobalTable_Graphics_.GPUHandle(1U + 64U + 96U).ptr, { 480.0f, 270.0f });
		//ImGui::Image(GlobalTable_Graphics_.GPUHandle(3U + 64U + 96U).ptr, { 640.0f, 360.0f });
		//ImGui::End();

		//ImGui::Begin("Gradient");
		//auto& gradients{ GradientMapping_->Gradients() };
		//static std::vector<GradientControl> gradientControls{
		//	{ 0, Float4{ 0.0f, 0.0f, 0.0f, 1.0f } },
		//	{ 64, Float4{ 0.25f, 0.25f, 0.25f, 1.0f } },
		//	{ 128, Float4{ 0.5f, 0.5f, 0.5f, 1.0f } },
		//	{ 192, Float4{ 0.75f, 0.75f, 0.75f, 1.0f } },
		//	{ 255, Float4{ 1.0f, 1.0f, 1.0f, 1.0f } },
		//};
		//for (auto& gradientControl : gradientControls) {
		//	ImGui::PushID(&gradientControl);
		//	//ImGui::DragInt("Index", &gradientControl.Index, 0.5f, 0, 255, "%d");
		//	ImGui::ColorEdit3("Color", &gradientControl.Color.x);
		//	ImGui::PopID();
		//}
		///*std::stable_sort(
		//	gradientControls.begin(),
		//	gradientControls.end(),
		//	[] (GradientControl const& lhs_, GradientControl const& rhs_)
		//		-> bool { return (lhs_.Index < rhs_.Index); }
		//);*/
		//for (size_t i = 0; i < gradientControls.size() - 1; ++i) {
		//	auto const& a = gradientControls[i];
		//	auto const& b = gradientControls[i + 1];
		//	if (a.Index == b.Index) { continue; }
		//	float const c{ 1.0f / static_cast<float>(b.Index - a.Index) };
		//	for (size_t j = a.Index; j < b.Index; ++j) {
		//		float const t = c * (j - a.Index);
		//		gradients[j] = Float4{
		//			a.Color.x * (1.0f - t) + b.Color.x * t,
		//			a.Color.y * (1.0f - t) + b.Color.y * t,
		//			a.Color.z * (1.0f - t) + b.Color.z * t,
		//			a.Color.w * (1.0f - t) + b.Color.w * t,
		//		};
		//	}
		//}
		//gradients[255] = gradientControls[gradientControls.size() - 1].Color;
		//GradientMapping_->Update();
		//ImGui::End();

		#endif

		Lighting_.Update(
			List_DirectionalLight_,
			List_PointLight_,
			List_Matrix_World_LightSphere_,
			ActivePtLightList_
		);

		SimpleFX_->Update({ fxTranslate.x, fxTranslate.y, fxTranslate.z });

		static float dissolveT{ 0.0f };
		DissolveC.Threshold = std::abs(std::sin(dissolveT));
		dissolveT += 0.01f;
		UB_DissolveConstants_.Store(&DissolveC, sizeof(DissolveConstants), 0LLU);
	}

	void Scene::Render(
		[[maybe_unused]] DX12::Context const& dxContext_,
		DX12::CommandList const& cmdList_
	) {
		UB_Constant_Model_.Store(&LocalToWorld_, sizeof(Mat4), 0LLU);
		UB_Constant_Model_.Store(&Transpose_WorldToLocal_, sizeof(Mat4), sizeof(Mat4));
		UB_Constant_Scene_.Store(&Constant_Scene_, sizeof(Constant_Scene), 0LLU);
		cmdList_->CopyBufferRegion(
			DB_Constant_Scene_.Get(),
			0LLU,
			UB_Constant_Scene_.Get(),
			0LLU,
			sizeof(Constant_Scene)
		);

		UB_LightingScene_.Store(&ScreenToWorld_, sizeof(Mat4), 0LLU);
		UB_LightingScene_.Store(&LookAtSrc_, sizeof(Float3), sizeof(Mat4));
		UB_LightingScene_.Store(&ModelShininess_, sizeof(Float3), sizeof(Mat4) + sizeof(Float3));

		static D3D12_RESOURCE_BARRIER const barriers_PreRender[]{
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(0U),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			),
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(1U),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			),
			Lumina::DX12::Barrier::Transition(
				Canvas_.DepthTexture(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_DEPTH_WRITE
			),
		};
		cmdList_->ResourceBarrier(3U, barriers_PreRender);

		//auto rtv{ dxContext_.SwapChain().BackBufferRTVCPUHandle() };
		//auto dsv{ dxContext_.SwapChain().DSVCPUHandle() };

		RenderPass_.Begin(cmdList_);

		cmdList_->RSSetViewports(
			Canvas_.Num_RenderTargets(),
			Canvas_.Viewports().data()
		);
		cmdList_->RSSetScissorRects(
			Canvas_.Num_RenderTargets(),
			Canvas_.ScissorRects().data()
		);

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetGraphicsRootDescriptorTable(0U, GlobalTable_Graphics_.GPUHandle(0U));
		cmdList_->SetGraphicsRootDescriptorTable(1U, GlobalTable_Graphics_.GPUHandle(96U));
		cmdList_->SetGraphicsRootDescriptorTable(2U, GlobalTable_ImageTextures_.GPUHandle(0U));
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList_->IASetVertexBuffers(0U, 1U, &VBV_Mesh_Sphere_);
		cmdList_->IASetIndexBuffer(&IBV_Mesh_Sphere_);
		cmdList_->DrawIndexedInstanced(Num_Indices_Sphere_, 1U, 0U, 0U, 0U);

		Skybox_->Render(cmdList_, GlobalTable_Graphics_);

		RenderPass_.End();

		static D3D12_RESOURCE_BARRIER const barriers_PostRender[]{
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(0U),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
			Lumina::DX12::Barrier::Transition(
				Canvas_.RenderTexture(1U),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
			Lumina::DX12::Barrier::Transition(
				Canvas_.DepthTexture(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
		};
		cmdList_->ResourceBarrier(3U, barriers_PostRender);

		Lighting_.Render(
			dxContext_.Device(),
			cmdList_,
			GlobalTable_Graphics_.GPUHandle(0U + 64U + 96U),
			Skybox_->GlobalTable().GPUHandle(0U),
			LocalHeap_CBV_.CPUHandle(0U),
			LocalHeap_CBV_.CPUHandle(2U)
		);

		auto rtv{ LocalHeap_RTV_.CPUHandle(0U) };
		auto dsv{ dxContext_.SwapChain().DSVCPUHandle() };
		cmdList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		cmdList_->OMSetRenderTargets(1U, &rtv, false, &dsv);

		Fullscreen_->Render(cmdList_, GlobalTable_Graphics_.GPUHandle(3U + 64U + 96U));

		Mat4 const viewToWorld_NoTranslate{
			WorldToView_[0][0], WorldToView_[1][0], WorldToView_[2][0], 0.0f,
			WorldToView_[0][1], WorldToView_[1][1], WorldToView_[2][1], 0.0f,
			WorldToView_[0][2], WorldToView_[1][2], WorldToView_[2][2], 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
		SimpleFX_->Render(cmdList_, LocalHeap_CBV_, viewToWorld_NoTranslate);
		SimpleFX2_->Render(cmdList_, Constant_Scene_.WorldToProjective, viewToWorld_NoTranslate);
		SimpleFX3_->Render(cmdList_, Constant_Scene_.WorldToProjective);

		static D3D12_RESOURCE_BARRIER const barriers_OSR0[]{
			Lumina::DX12::Barrier::Transition(
				OffscreenTextures_[0],
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			),
			Lumina::DX12::Barrier::Transition(
				OffscreenTextures_[0],
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			),
		};
		cmdList_->ResourceBarrier(1U, barriers_OSR0 + 0U);

		rtv = dxContext_.SwapChain().BackBufferRTVCPUHandle();
		cmdList_->OMSetRenderTargets(1U, &rtv, false, &dsv);

		static float t = 0.0f;
		radialBlurParams.BlurWidth_R = std::abs(std::sin(t * 1.0f)) * 0.1f + 0.02f;
		radialBlurParams.BlurWidth_G = std::abs(std::sin(t * 0.9f)) * 0.125f + 0.02f;
		radialBlurParams.BlurWidth_B = std::abs(std::sin(t * 0.8f)) * 0.15f + 0.02f;
		radialBlurParams.Time = t;
		t += 0.05f;
		RadialBlur_->UpdateConstant(&radialBlurParams, sizeof(RadialBlurParams));

		/*Fullscreen_->Render(
			cmdList_,
			GlobalTable_Graphics_.GPUHandle(4U + 64U + 96U),
			*RadialBlur_
		);*/

		CG5Random_->UpdateConstant(&t, sizeof(float));
		Fullscreen_->Render(
			cmdList_,
			GlobalTable_Graphics_.GPUHandle(4U + 64U + 96U),
			*CG5Random_
		);

		cmdList_->ResourceBarrier(1U, barriers_OSR0 + 1U);
	}

	template<>
	void Scene::Initialize(
		DX12::Context const& dxContext_,
		AssetManager const& assetMngr_
	) {
		auto const& device{ dxContext_.Device() };

		DB_Constant_Scene_.Initialize(device, (sizeof(Constant_Scene) + 0xFFLLU) & ~0xFFLLU);
		UB_Constant_Scene_.Initialize(device, DB_Constant_Scene_.SizeInBytes());
		WorldToView_ = LookAt(LookAtSrc_, LookAtDst_, { 0.0f, 1.0f, 0.0f });
		ViewToProjective_ = Lumina::Mat4::PerspectiveFOV(
			0.45f,
			static_cast<float>(1280.0f) / static_cast<float>(720.0f),
			0.1f,
			200.0f
		);
		Constant_Scene_.WorldToProjective = WorldToView_ * ViewToProjective_;
		UB_Constant_Scene_.Store(&Constant_Scene_, sizeof(Constant_Scene), 0LLU);

		LocalToWorld_ = Mat4::SRT(ModelScale_, ModelRotate_, ModelTranslate_);
		Mat4::Transpose(Transpose_WorldToLocal_, LocalToWorld_.Inv());
		UB_Constant_Model_.Initialize(device, 256LLU);
		UB_Constant_Model_.Store(&LocalToWorld_, sizeof(Mat4), 0LLU);
		UB_Constant_Model_.Store(&Transpose_WorldToLocal_, sizeof(Mat4), sizeof(Mat4));

		ScreenToWorld_ = Inv_Viewport * Constant_Scene_.WorldToProjective.Inv();
		UB_LightingScene_.Initialize(device, 256LLU);
		UB_LightingScene_.Store(&ScreenToWorld_, sizeof(Mat4), 0LLU);
		UB_LightingScene_.Store(&LookAtSrc_, sizeof(Float3), sizeof(Mat4));
		UB_LightingScene_.Store(&ModelShininess_, sizeof(Float3), sizeof(Mat4) + sizeof(Float3));
		UB_LightingScene_.Store(&IsUsingBlinnPhong_, sizeof(Float3), sizeof(Mat4) + sizeof(Float3) + sizeof(float));

		GlobalTable_Graphics_ = dxContext_.GlobalDescriptorHeap().Allocate(192U);

		LocalHeap_CBV_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32U, false);
		DX12::CBV::Create(device, LocalHeap_CBV_.CPUHandle(0U), DB_Constant_Scene_);
		DX12::CBV::Create(device, LocalHeap_CBV_.CPUHandle(1U), UB_Constant_Model_);
		DX12::CBV::Create(device, LocalHeap_CBV_.CPUHandle(2U), UB_LightingScene_);

		device->CopyDescriptorsSimple(
			1U,
			GlobalTable_Graphics_.CPUHandle(0U),
			LocalHeap_CBV_.CPUHandle(0U),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
		device->CopyDescriptorsSimple(
			1U,
			GlobalTable_Graphics_.CPUHandle(1U),
			LocalHeap_CBV_.CPUHandle(1U),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
		/*device->CopyDescriptorsSimple(
			1U,
			GlobalTable_Graphics_.CPUHandle(0U + 96U),
			LocalHeap_CBV_.CPUHandle(0U),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);*/

		GlobalTable_ImageTextures_ = dxContext_.GlobalDescriptorHeap().Allocate(32U);
		auto& assetMngr{ const_cast<AssetManager&>(assetMngr_) };
		std::vector<uint32_t> texIDs{};
		assetMngr.Graphics().LoadImageTextures(
			texIDs,
			{
				{ "CG3.Tex0", "Src/CG3Eval2/monsterBall.png" },
				{ "CG3.Tex1", "Src/CG3Eval2/checkerBoard.png" },
				{ "CG3.Tex2", "Assets/noise0.png" },
			}
		);
		for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
			device->CopyDescriptorsSimple(
				1U,
				GlobalTable_ImageTextures_.CPUHandle(idx),
				assetMngr.Graphics().CPUHandle(texIDs.at(idx)),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
		}

		Canvas_.AllocateTextures(2U, true);
		Canvas_.RenderTexture(0U).Initialize(device, 1280U, 720U);
		Canvas_.RenderTexture(1U).Initialize(device, 1280U, 720U, DXGI_FORMAT_R8G8B8A8_UNORM);
		Canvas_.DepthTexture().Initialize(device, 1280U, 720U);
		Canvas_.TransitionResourceStates(device, dxContext_.DirectQueue());
		Canvas_.CreateViews(device);
		Canvas_.Viewport(0U) = D3D12_VIEWPORT{
			.TopLeftX{ 0.0f },
			.TopLeftY{ 0.0f },
			.Width{ 1280.0f },
			.Height{ 720.0f },
			.MinDepth{ 0.0f },
			.MaxDepth{ 1.0f },
		};
		Canvas_.ScissorRect(0U) = D3D12_RECT{
			.left{ 0 },
			.top{ 0 },
			.right{ 1280 },
			.bottom{ 720 },
		};
		Canvas_.Viewport(1U) = Canvas_.Viewport(0U);
		Canvas_.ScissorRect(1U) = Canvas_.ScissorRect(0U);

		DX12::SRV<void>::Create(device, GlobalTable_Graphics_.CPUHandle(0U + 64U + 96U), Canvas_.RenderTexture(0U));
		DX12::SRV<void>::Create(device, GlobalTable_Graphics_.CPUHandle(1U + 64U + 96U), Canvas_.RenderTexture(1U));
		DX12::SRV<void>::Create<DXGI_FORMAT_R24_UNORM_X8_TYPELESS>(device, GlobalTable_Graphics_.CPUHandle(2U + 64U + 96U), Canvas_.DepthTexture());

		RenderPass_.Initialize(2U, true);
		RenderPass_.RenderTarget(0).BeginningEvent().ClearTarget(
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			{ 0.0f, 0.0f, 0.0f, 0.0f }
		);
		RenderPass_.RenderTarget(0).EndingEvent().Preserve();
		RenderPass_.RenderTarget(1).BeginningEvent().ClearTarget(
			DXGI_FORMAT_R8G8B8A8_UNORM,
			{ 0.0f, 0.0f, 0.0f, 0.0f }
		);
		RenderPass_.RenderTarget(1).EndingEvent().Preserve();
		RenderPass_.DepthStencil().DepthBeginningEvent().ClearTarget(
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			{ .Depth{ 1.0f }, }
		);
		RenderPass_.DepthStencil().DepthEndingEvent().Preserve();
		RenderPass_.DepthStencil().StencilBeginningEvent().NoAccess();
		RenderPass_.DepthStencil().StencilEndingEvent().NoAccess();
		for (uint32_t idx{ 0U }; idx < Canvas_.Num_RenderTargets(); ++idx) {
			RenderPass_.RenderTarget(idx).View() = Canvas_.RTV(idx);
		}
		RenderPass_.DepthStencil().View() = Canvas_.DSV();

		auto settings{ Utils::LoadFromFile<nlohmann::json>("Settings.json", "Src/CG3Eval2") };
		auto graphicsRSSetup{ DX12::LoadRootSignatureSetup(settings.at("General Graphics RS Test")) };
		RS_.Initialize(device, graphicsRSSetup, "CG3Eval2::Graphics RS");

		dxContext_.Compile(
			VertexShader_,
			L"Src/CG3Eval2/VS.hlsl",
			L"vs_6_6",
			L"main",
			"CG3Eval2::VS"
		);
		dxContext_.Compile(
			PixelShader_,
			L"Src/CG3Eval2/PS.hlsl",
			L"ps_6_6",
			L"main",
			"CG3Eval2::PS"
		);
		DX12::GraphicsPipelineState::Setup graphicsPSOSetup{};
		DX12::BlendState blendState{ .IndependentBlendEnable{ true }, };
		blendState.RenderTarget[0] = {
			.BlendEnable{ true },
			.LogicOpEnable{ false },
			.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
			.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
			.BlendOp{ D3D12_BLEND_OP_ADD },
			.SrcBlendAlpha{ D3D12_BLEND_ONE },
			.DestBlendAlpha{ D3D12_BLEND_ONE },
			.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
			.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
		};
		blendState.RenderTarget[1] = {
			.BlendEnable{ true },
			.LogicOpEnable{ false },
			.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
			.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
			.BlendOp{ D3D12_BLEND_OP_ADD },
			.SrcBlendAlpha{ D3D12_BLEND_ONE },
			.DestBlendAlpha{ D3D12_BLEND_ONE },
			.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
			.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
		};
		DX12::RasterizerState rasterizerState{
			.FillMode{ D3D12_FILL_MODE_SOLID },
			.CullMode{ D3D12_CULL_MODE_BACK },
		};
		DX12::DepthStencilState depthStencilState{
			.DepthEnable{ true },
			.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
			.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
		};
		DX12::GraphicsPipelineState::InputLayout inputLayout{};
		inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
		inputLayout.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
		inputLayout.Append("TEXID", 0U, DXGI_FORMAT_R32_FLOAT);
		inputLayout.Append("NORMAL", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
		std::vector<DXGI_FORMAT> rtvFormats{
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT_R8G8B8A8_UNORM,
		};
		graphicsPSOSetup <<
			RS_ <<
			VertexShader_ <<
			PixelShader_ <<
			blendState <<
			rasterizerState <<
			depthStencilState <<
			inputLayout <<
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE <<
			rtvFormats <<
			DX12::GraphicsPipelineState::DefaultDSVFormat;
		PSO_.Initialize(
			device,
			graphicsPSOSetup,
			"CG3Eval2::GraphicsPSO"
		);

		std::vector<Vertex> sphereVertices;
		std::vector<uint32_t> sphereIndices;
		CreateSphere<16U, 32U>(sphereVertices, sphereIndices, 0.5f, { 0.0f, 1.0f, 0.0f });
		Num_Vertices_Sphere_ = static_cast<uint32_t>(sphereVertices.size());
		Num_Indices_Sphere_ = static_cast<uint32_t>(sphereIndices.size());

		DB_VB_Mesh_Sphere_.Initialize(
			device,
			sizeof(Vertex) * Num_Vertices_Sphere_,
			"Mesh::Sphere::VB"
		);
		DB_IB_Mesh_Sphere_.Initialize(
			device,
			sizeof(uint32_t) * Num_Indices_Sphere_,
			"Mesh::Sphere::IB"
		);

		VBV_Mesh_Sphere_ = DX12::VBV::Create<Vertex>(DB_VB_Mesh_Sphere_);
		IBV_Mesh_Sphere_ = DX12::IBV::Create(DB_IB_Mesh_Sphere_);

		DX12::UploadBuffer ub_Mesh_Sphere_VB{};
		ub_Mesh_Sphere_VB.Initialize(device, DB_VB_Mesh_Sphere_.SizeInBytes());
		ub_Mesh_Sphere_VB.Store(
			sphereVertices.data(),
			sizeof(Vertex) * Num_Vertices_Sphere_,
			0LLU
		);
		DX12::UploadBuffer ub_Mesh_Sphere_IB{};
		ub_Mesh_Sphere_IB.Initialize(device, DB_IB_Mesh_Sphere_.SizeInBytes());
		ub_Mesh_Sphere_IB.Store(
			sphereIndices.data(),
			sizeof(uint32_t) * Num_Indices_Sphere_,
			0LLU
		);

		Lighting_.Initialize(dxContext_, 1280U, 720U);
		List_DirectionalLight_.Initialize(32U);
		{
			auto& light = List_DirectionalLight_.New();
			{
				light.Direction = { 1.0f, 0.0f, 0.0f };
				light.DiffuseRGB = { 1.0f, 1.0f, 1.0f };
				light.DiffuseIntensity = 0.25f;
				light.SpecularRGB = { 1.0f, 1.0f, 1.0f };
				light.SpecularIntensity = 2.0f;
			}
		}
		List_PointLight_.Initialize(128U);
		ActivePtLightList_.clear();
		ActivePtLightList_.emplace_back(List_PointLight_.Size());
		{
			auto& light = List_PointLight_.New();
			{
				light.WorldPosition = { 0.0f, 1.0f, -4.0f };
				light.DiffuseRGB = { 1.0f, 0.25f, 0.0f };
				light.DiffuseIntensity = 3.0f;
				light.SpecularRGB = { 1.0f, 0.25f, 0.0f };
				light.SpecularIntensity = 5.0f;
			}
		}
		List_Matrix_World_LightSphere_.Initialize(128U);
		{
			auto& lightSphere = List_Matrix_World_LightSphere_.New();

			float const r{ Lumina::LightSphereRadius(1024.0f, 2.0f, 1.0f, 1.0f, 0.5f) };
			lightSphere = {
				r, 0.0f, 0.0f, 0.0f,
				0.0f, r, 0.0f, 0.0f,
				0.0f, 0.0f, r, 0.0f,
				-5.0f, 0.0f, 0.0f, 1.0f,
			};
		}

		DX12::SRV<void>::Create(
			device, GlobalTable_Graphics_.CPUHandle(3U + 64U + 96U),
			Lighting_.Canvas().RenderTexture(0U)
		);

		Skybox_ = std::make_unique<Lumina::Skybox>();
		Skybox_->Initialize(dxContext_, device, assetMngr, "Assets/Skybox.dds");

		SimpleFX_ = std::make_unique<Lumina::SimpleFX>();
		SimpleFX_->Initialize(dxContext_, device, assetMngr);

		SimpleFX2_ = std::make_unique<Lumina::SimpleFX2>();
		SimpleFX2_->Initialize(dxContext_, device, assetMngr);
		SimpleFX2_->ResetRing({ 72, 2.0f, 1.0f });

		SimpleFX3_ = std::make_unique<Lumina::SimpleFX3>();
		SimpleFX3_->Initialize(dxContext_, device, assetMngr);
		SimpleFX3_->ResetCylinder({ 72, 2.0f, 5.0f, 3.0f });

		Fullscreen_ = std::make_unique<Lumina::Fullscreen>();
		Fullscreen_->Initialize(dxContext_, device);

		Grayscale_ = std::make_unique<Lumina::Grayscale>();
		Grayscale_->Initialize(dxContext_, device, *Fullscreen_);

		GradientMapping_ = std::make_unique<Lumina::GradientMapping>();
		GradientMapping_->Initialize(dxContext_, device, *Fullscreen_);

		Vignetting_ = std::make_unique<Lumina::Vignetting>();
		Vignetting_->Initialize(dxContext_, device, *Fullscreen_);

		/*float const sigma{ 2.0f };
		float const doubleSQSigma{ 2.0f * sigma * sigma };
		float const rcp_DoubleSQSigma{ 1.0f / doubleSQSigma };
		auto gaussianDist{
			[rcp_DoubleSQSigma](Vec2 const& xy_) -> float {
				float const exponent{ -Vec2::Dot(xy_, xy_) * rcp_DoubleSQSigma };
				return std::exp(exponent) * rcp_DoubleSQSigma * std::numbers::inv_pi_v<float>;
			}
		};*/

		[[maybe_unused]] auto prewittH{
			[](Vec2 const& xy_) -> float {
				if (xy_.x < 0) { return -1.0f / 6.0f; }
				if (xy_.x > 0) { return 1.0f / 6.0f; }
				return 0.0f;
			}
		};
		[[maybe_unused]] auto prewittV{
			[](Vec2 const& xy_) -> float {
				if (xy_.y < 0) { return -1.0f / 6.0f; }
				if (xy_.y > 0) { return 1.0f / 6.0f; }
				return 0.0f;
			}
		};


		Lumina::Mat4 projectiveToView{ ViewToProjective_.Inv() };

		struct FilterParams {
			Vec2 UVStepSize;
			uint32_t KernelWidth;
			uint32_t KernelHeight;
			float ProjectiveToView[16];
			float OutlineLuminanceFactor;
			float OutlineDepthFactor;
			float OutlineSaturateFactor;
			float OutlinePowerFactor;
			Float4 OutlineColor;
			float OutlineLuminanceSize;
			float OutlineDepthSize;
		} filterParams{};
		filterParams.UVStepSize = { 1.0f / 1280.0f, 1.0f / 720.0f };
		filterParams.KernelWidth = 3;
		filterParams.KernelHeight = 3;
		std::memcpy(filterParams.ProjectiveToView, &projectiveToView, sizeof(Lumina::Mat4));
		filterParams.OutlineLuminanceFactor = 1.0f;
		filterParams.OutlineDepthFactor = 2.0f;
		filterParams.OutlineSaturateFactor = 1.5f;
		filterParams.OutlinePowerFactor = 4.0f;
		filterParams.OutlineColor = { 0.5f, 0.5f, 1.0f, 0.8f };
		filterParams.OutlineLuminanceSize = 1.0f;
		filterParams.OutlineDepthSize = 3.0f;

		Filtering_ = std::make_unique<Lumina::Filtering>();
		Filtering_->Initialize(
			dxContext_,
			device,
			*Fullscreen_,
			L"Assets/CG5/Outline.PS.hlsl",
			filterParams.KernelWidth,
			filterParams.KernelHeight,
			prewittH,
			prewittV
		);

		Filtering_->UpdateConstant(&filterParams, sizeof(FilterParams));

		RadialBlur_ = std::make_unique<Lumina::RadialBlur>();
		RadialBlur_->Initialize(
			dxContext_,
			device,
			*Fullscreen_,
			L"Assets/CG5/RadialBlur.PS.hlsl"
		);

		radialBlurParams.ScreenSpaceCenter = { 640.0f, 360.0f };
		radialBlurParams.RCP_Size = { 1.0f/ 1280.0f, 1.0f/ 720.0f };
		radialBlurParams.NUM_Samples = 10U;
		radialBlurParams.RCP_NUM_Samples = 1.0f / static_cast<float>(radialBlurParams.NUM_Samples);
		RadialBlur_->UpdateConstant(&radialBlurParams, sizeof(RadialBlurParams));


		CG5Random_ = std::make_unique<Lumina::CG5Random>();
		CG5Random_->Initialize(
			dxContext_,
			device,
			*Fullscreen_,
			L"Assets/CG5/Random.PS.hlsl"
		);

		UB_DissolveConstants_.Initialize(device, 256LLU);
		DX12::CBV::Create(device, GlobalTable_Graphics_.CPUHandle(0U + 96U), UB_DissolveConstants_);

		DissolveC.MaskTextureID = 2U;

		OffscreenTextures_[0].Initialize(device, 1280U, 720U);
		//OffscreenTextures_[1].Initialize(device, 1280U, 720U);
		DX12::SRV<void>::Create(
			device,
			GlobalTable_Graphics_.CPUHandle(4U + 64U + 96U),
			OffscreenTextures_[0]
		);
		DX12::SRV<void>::Create<DXGI_FORMAT_R24_UNORM_X8_TYPELESS>(
			device,
			GlobalTable_Graphics_.CPUHandle(5U + 64U + 96U),
			Canvas_.DepthTexture()
		);
		LocalHeap_RTV_.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2U, false);
		DX12::RTV::Create(device, LocalHeap_RTV_.CPUHandle(0U), OffscreenTextures_[0]);
		//DX12::RTV::Create(device, LocalHeap_RTV_.CPUHandle(1U), OffscreenTextures_[1]);

		DX12::CommandAllocator cmdAlloc{};
		cmdAlloc.Initialize(device);
		DX12::CommandList cmdList;
		cmdList.Initialize(device, cmdAlloc);

		cmdList->CopyBufferRegion(
			DB_Constant_Scene_.Get(),
			0LLU,
			UB_Constant_Scene_.Get(),
			0LLU,
			sizeof(Constant_Scene)
		);
		cmdList->CopyResource(DB_VB_Mesh_Sphere_.Get(), ub_Mesh_Sphere_VB.Get());
		cmdList->CopyResource(DB_IB_Mesh_Sphere_.Get(), ub_Mesh_Sphere_IB.Get());

		dxContext_.DirectQueue() << cmdList;
		dxContext_.DirectQueue().CPUWait(dxContext_.DirectQueue().ExecuteBatchedCommandLists());
		cmdList.Reset(cmdAlloc);
	}
}