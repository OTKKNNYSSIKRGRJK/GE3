module Lumina.Editor.DX12;

import Lumina.DX12;

import Lumina.Editor;

import : RootSignature;

namespace Lumina::Editor {
	class DirectX12::Impl {
		friend DirectX12;

	public:
		void Update();

	public:
		void Initialize();
		void Finalize() noexcept;

	private:
		Module<DX12RootSignature> Module_RS_{};
	};

	void DirectX12::Impl::Update() {
		ImGui::Begin(
			"DX12",
			nullptr,
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize
		);
		ImGui::SetWindowPos(
			ImVec2{
				ImGui::GetMainViewport()->Size.x * 0.6875f,
				0.0f
			}
		);
		ImGui::SetWindowSize(
			ImVec2{
				ImGui::GetMainViewport()->Size.x * 0.3125f,
				ImGui::GetMainViewport()->Size.y
			}
		);
		if (ImGui::BeginTabBar("DX12")) {
			Module_RS_.Update();
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	void DirectX12::Impl::Initialize() {
		Module_RS_.Initialize();
	}

	void DirectX12::Impl::Finalize() noexcept {}
}

namespace Lumina::Editor {
	void DirectX12::Update() {
		Impl_->Update();
	}

	void DirectX12::Initialize() {
		if (Impl_ == nullptr) {
			Impl_ = new Impl{};
			Impl_->Initialize();
		}
	}

	void DirectX12::Finalize() noexcept {
		if (Impl_ != nullptr) {
			Impl_->Finalize();
			delete Impl_;
			Impl_ = nullptr;
		}
	}

	DirectX12::~DirectX12() noexcept {
		Finalize();
	}
}