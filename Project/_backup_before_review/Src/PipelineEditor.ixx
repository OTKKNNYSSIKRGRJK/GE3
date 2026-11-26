module;

#include <d3d12.h>

export module PipelineEditor;

import <type_traits>;

import <utility>;
import <initializer_list>;

import <vector>;
import <set>;
import <unordered_set>;

import <string>;
import <format>;

import Lumina.DX12;

import Lumina.Utils.ImGui;

export class PipelineEditor {
public:
	void Update() {
		if (ImGui::BeginTabItem("Pipeline State")) {
			ImGui::EndTabItem();
		}
	}
};