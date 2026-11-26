namespace Color {
	static const float Inv_60 = 1.0f / 60.0f;
	
	float3 HSVToRGB(float h_, float s_, float v_) {
		static const uint permutations_CX0[6][3] = {
			{ 0U, 1U, 2U, },
			{ 1U, 0U, 2U, },
			{ 2U, 0U, 1U, },
			{ 2U, 1U, 0U, },
			{ 1U, 2U, 0U, },
			{ 0U, 2U, 1U, },
		};
		
		h_ = fmod(h_, 360.0f);
		h_ = fmod(h_ + 360.0f, 360.0f);

		const float chroma = s_ * v_;
		const float m = v_ - chroma;
		
		const float hue_Prime = h_ * Inv_60;
		const int hueSection = ((int)floor(hue_Prime) + 6) % 6;

		const float3 CX0 = {
			chroma,
			chroma * (1.0f - abs(fmod(hue_Prime, 2.0f) - 1.0f)),
			0.0f,
		};
		
		return float3(
			CX0[permutations_CX0[hueSection][0]] + m,
			CX0[permutations_CX0[hueSection][1]] + m,
			CX0[permutations_CX0[hueSection][2]] + m
		);
	}
}