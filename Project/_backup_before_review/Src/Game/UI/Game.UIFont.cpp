module Game.UIFont;

// ここで import しているヘッダユニットは、いずれも Game.UIFont.ixx で import 済み
import <cstddef>;
import <cstdint>;
import <fstream>;
import <string_view>;
import <unordered_map>;
import <vector>;

import Lumina;
import nlohmann.json;

namespace Game::UIFont {
	auto NextCodepoint(std::string_view s_, std::size_t& pos_, std::uint32_t& cp_) noexcept -> bool {
		if (pos_ >= s_.size()) { return false; }
		auto const byte{ [&](std::size_t i_) { return static_cast<std::uint32_t>(static_cast<unsigned char>(s_[i_])); } };
		std::uint32_t const b0{ byte(pos_) };
		std::size_t len{ 1U };
		std::uint32_t cp{ b0 };
		if (b0 >= 0xF0U) { len = 4U; cp = b0 & 0x07U; }
		else if (b0 >= 0xE0U) { len = 3U; cp = b0 & 0x0FU; }
		else if (b0 >= 0xC0U) { len = 2U; cp = b0 & 0x1FU; }
		else if (b0 >= 0x80U) { cp_ = 0xFFFDU; ++pos_; return true; }	// 続きのバイトから始まっている

		if (pos_ + len > s_.size()) { cp_ = 0xFFFDU; pos_ = s_.size(); return true; }
		for (std::size_t i = 1U; i < len; ++i) {
			std::uint32_t const b{ byte(pos_ + i) };
			if ((b & 0xC0U) != 0x80U) { cp_ = 0xFFFDU; pos_ += i; return true; }
			cp = (cp << 6U) | (b & 0x3FU);
		}
		pos_ += len;
		cp_ = cp;
		return true;
	}

	auto Font::Load(char const* jsonPath_) -> bool {
		using JSON = nlohmann::json;
		std::ifstream ifs{ jsonPath_ };
		if (!ifs.is_open()) { return false; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { return false; }

		auto const number{ [&root](char const* key_, float fallback_) {
			auto const it{ root.find(key_) };
			return (it != root.end() && it->is_number()) ? it->get<float>() : fallback_;
		} };
		float const size{ number("Size", 0.0f) };
		float const w{ number("Width", 0.0f) };
		float const h{ number("Height", 0.0f) };
		auto const glyphs{ root.find("Glyphs") };
		if (!(size > 0.0f) || !(w > 0.0f) || !(h > 0.0f) || glyphs == root.end() || !glyphs->is_array()) { return false; }

		Glyphs_.clear();
		for (auto const& g : *glyphs) {
			if (!g.is_array() || g.size() != 8U) { continue; }
			bool ok{ true };
			for (auto const& e : g) { ok = ok && e.is_number(); }
			if (!ok) { continue; }
			Glyphs_[g[0].get<std::uint32_t>()] = Glyph{
				g[1].get<float>(), g[2].get<float>(), g[3].get<float>(), g[4].get<float>(),
				g[5].get<float>(), g[6].get<float>(), g[7].get<float>(),
			};
		}
		Size_ = size;
		Ascent_ = number("Ascent", size);
		Descent_ = number("Descent", 0.0f);
		AtlasW_ = w;
		AtlasH_ = h;
		return true;
	}

	auto Font::LineHeight(float size_) const noexcept -> float {
		if (!(Size_ > 0.0f)) { return size_; }
		return (Ascent_ + Descent_) * size_ / Size_;
	}

	auto Font::Measure(std::string_view utf8_, float size_) const -> float {
		if (!(Size_ > 0.0f)) { return 0.0f; }
		float const s{ size_ / Size_ };
		float width{ 0.0f };
		std::size_t pos{ 0U };
		std::uint32_t cp{ 0U };
		while (NextCodepoint(utf8_, pos, cp)) {
			auto const it{ Glyphs_.find(cp) };
			if (it != Glyphs_.end()) { width += it->second.Advance * s; }
		}
		return width;
	}

	void Font::Append(
		std::vector<Lumina::Sprite>& out_,
		std::string_view utf8_,
		float x_,
		float y_,
		float size_,
		Lumina::Float4 const& rgba_,
		std::uint32_t textureId_,
		float anchorX_
	) const {
		if (!(Size_ > 0.0f) || !(size_ > 0.0f)) { return; }
		float const s{ size_ / Size_ };
		float penX{ x_ - Measure(utf8_, size_) * anchorX_ };

		std::size_t pos{ 0U };
		std::uint32_t cp{ 0U };
		while (NextCodepoint(utf8_, pos, cp)) {
			auto const it{ Glyphs_.find(cp) };
			if (it == Glyphs_.end()) {
				++Missing_;
				continue;
			}
			Glyph const& g{ it->second };
			if (g.W > 0.0f && g.H > 0.0f) {
				Lumina::Sprite& sp{ out_.emplace_back() };
				sp.AnchorPoint({ 0.0f, 0.0f });
				sp.Translate({ penX + g.XOff * s, y_ + g.YOff * s });
				sp.Scale({ g.W * s, g.H * s });
				float const u0{ g.X / AtlasW_ };
				float const v0{ g.Y / AtlasH_ };
				float const u1{ (g.X + g.W) / AtlasW_ };
				float const v1{ (g.Y + g.H) / AtlasH_ };
				sp.UV(0U, { u0, v0 });
				sp.UV(1U, { u1, v0 });
				sp.UV(2U, { u0, v1 });
				sp.UV(3U, { u1, v1 });
				sp.RGBA({ rgba_.x, rgba_.y, rgba_.z, rgba_.w });
				sp.TextureID(textureId_);
			}
			penX += g.Advance * s;
		}
	}
}
