module;

#include<External/DirectXTex/DirectXTex.h>
#include<External/ImGui/imgui.h>

#include<thread>

export module Lumina.PerlinNoiseTest;

import Lumina.Math.PerlinNoise;

import Lumina.DX12;

namespace Lumina {
	export class PerlinNoiseTest {
	private:
		float Frequency_{ 1.0f };
		float Redist_{ 1.0f };
		float Offset_[3]{ 0.0f, 0.0f, 0.0f };
		uint32_t Num_Octaves_{ 1U };
		float Persistance_{ 0.5f };

		int Width_{ 256 };
		int Height_{ 256 };

	private:
		void GenerateImage();
		
	public:
		void Update();

	public:
		PerlinNoiseTest();
	};

	void PerlinNoiseTest::GenerateImage() {
		Math::PerlinNoise noiseGen{
			Frequency_,
			Num_Octaves_,
			Persistance_,
			{ Offset_[0], Offset_[1], Offset_[2] }
		};

		float* pixels{ static_cast<float*>(std::malloc(Width_ * Height_ * sizeof(float) * 4U)) };

		constexpr float div{ 1.0f / 32.0f };

		for (int v = 0; v < Height_; ++v) {
			for (int u = 0; u < Width_; ++u) {
				float noise = noiseGen(
					u * div,
					v * div,
					0.0f
				);
				noise = std::pow(noise, Redist_);
				pixels[(v * Width_ + u) * 4] = noise;
				pixels[(v * Width_ + u) * 4 + 1] = noise;
				pixels[(v * Width_ + u) * 4 + 2] = noise;
				pixels[(v * Width_ + u) * 4 + 3] = 1.0f;
			}
		}

		DirectX::Image img{
			.width{ static_cast<size_t>(Width_) },
			.height{ static_cast<size_t>(Height_) },
			.format{ DXGI_FORMAT_R32G32B32A32_FLOAT },
			.rowPitch{ Width_ * sizeof(float) * 4U },
			.slicePitch{ Width_ * Height_ * sizeof(float) * 4U },
			.pixels{ reinterpret_cast<byte*>(pixels) },
		};
		DirectX::SaveToWICFile(
			img,
			DirectX::WIC_FLAGS_NONE,
			DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
			L"perlin.png"
		);

		std::free(pixels);
	}

	void PerlinNoiseTest::Update() {
		ImGui::Begin("Perlin Noise Test");
		ImGui::DragFloat("Frequency##PerlinNoiseTest", &Frequency_, 0.01f);
		ImGui::DragFloat("Amplitude##PerlinNoiseTest", &Redist_, 0.01f);
		ImGui::DragInt("Width##PerlinNoiseTest", &Width_, 1.0f, 128, 1024);
		ImGui::DragInt("Height##PerlinNoiseTest", &Height_, 1.0f, 128, 1024);
		ImGui::DragFloat3("Offset##PerlinNoiseTest", Offset_, 0.01f);
		ImGui::DragInt("Octaves##PerlinNoiseTest", reinterpret_cast<int*>(&Num_Octaves_), 0.1f, 1, 32);
		ImGui::DragFloat("Persistance##PerlinNoiseTest", &Persistance_, 0.01f, 0.0f, 0.875f);
		if (ImGui::Button("Generate Image")) {
			auto&& t = std::thread{ [this]() { GenerateImage(); } };
			t.detach();
		}
		ImGui::End();
	}

	PerlinNoiseTest::PerlinNoiseTest() {}
}