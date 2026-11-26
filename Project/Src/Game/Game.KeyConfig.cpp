module;

#if defined(_WIN32)
// 記号キー（VK_OEM_*）の表示名を、今のキーボード配列の文字から作るため（MapVirtualKeyW）
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

module Game.KeyConfig;

// ここで import しているヘッダユニットは、いずれも Game.KeyConfig.ixx で import 済み
import <array>;
import <cstddef>;
import <cstdint>;
import <fstream>;
import <optional>;
import <string>;
import <string_view>;

import nlohmann.json;

import Lumina.Input;

namespace {
	namespace In = Lumina::Input;
	using JSON = nlohmann::json;
	using Game::KeyConfig::Bindings;
	using Game::KeyConfig::NumSlots;
	using Game::KeyConfig::Slot;

	// 仮想キーコード（GE3 の KEY と同じ値）
	constexpr std::uint16_t VkBack{ 0x08U }, VkTab{ 0x09U }, VkReturn{ 0x0DU }, VkPause{ 0x13U }, VkCapital{ 0x14U }, VkEscape{ 0x1BU };
	constexpr std::uint16_t VkSpace{ 0x20U }, VkPrior{ 0x21U }, VkNext{ 0x22U }, VkEnd{ 0x23U }, VkHome{ 0x24U };
	constexpr std::uint16_t VkLeft{ 0x25U }, VkUp{ 0x26U }, VkRight{ 0x27U }, VkDown{ 0x28U }, VkInsert{ 0x2DU }, VkDelete{ 0x2EU };
	constexpr std::uint16_t VkNumpad0{ 0x60U }, VkMultiply{ 0x6AU }, VkAdd{ 0x6BU }, VkSubtract{ 0x6DU }, VkDecimal{ 0x6EU }, VkDivide{ 0x6FU };
	constexpr std::uint16_t VkF1{ 0x70U }, VkF2{ 0x71U }, VkF12{ 0x7BU };
	constexpr std::uint16_t VkLShift{ 0xA0U }, VkRShift{ 0xA1U }, VkLControl{ 0xA2U }, VkRControl{ 0xA3U }, VkLMenu{ 0xA4U }, VkRMenu{ 0xA5U };
	constexpr std::uint16_t VkQ{ 0x51U };

	constexpr auto Key(std::uint16_t vk_) noexcept -> In::Source { return In::FromKey(vk_); }

	auto Same(In::Source const& a_, In::Source const& b_) noexcept -> bool {
		return a_.Kind == b_.Kind && a_.Code == b_.Code;
	}

	auto KindName(In::SourceKind k_) noexcept -> char const* {
		switch (k_) {
			case In::SourceKind::Key: return "Key";
			case In::SourceKind::MouseButton: return "Mouse";
			default: return "None";
		}
	}

	// 共有の表
	struct Shared {
		Bindings Table{};
		std::uint32_t Version{ 0U };
		bool Loaded{ false };
	};
	auto State() -> Shared& {
		static Shared s{};
		if (!s.Loaded) {
			s.Table = Game::KeyConfig::LoadFrom(Game::KeyConfig::ConfigPath);
			s.Version = 1U;
			s.Loaded = true;
		}
		return s;
	}
}

namespace Game::KeyConfig {
	auto Defaults() noexcept -> Bindings {
		Bindings b{};
		b[static_cast<std::size_t>(Slot::MoveForward)] = Key(0x57U);	// W
		b[static_cast<std::size_t>(Slot::MoveBack)] = Key(0x53U);		// S
		b[static_cast<std::size_t>(Slot::MoveLeft)] = Key(0x41U);		// A
		b[static_cast<std::size_t>(Slot::MoveRight)] = Key(0x44U);		// D
		b[static_cast<std::size_t>(Slot::Attack)] = In::FromMouse(In::MouseButton::Left);
		b[static_cast<std::size_t>(Slot::Guard)] = In::FromMouse(In::MouseButton::Right);
		b[static_cast<std::size_t>(Slot::Dodge)] = Key(VkLShift);
		b[static_cast<std::size_t>(Slot::Jump)] = Key(VkSpace);
		return b;
	}

	auto Current() -> Bindings const& { return State().Table; }
	auto Version() -> std::uint32_t { return State().Version; }

	auto Apply(Bindings const& b_) -> bool {
		Shared& s{ State() };
		s.Table = b_;
		++s.Version;
		return SaveTo(ConfigPath, b_);
	}

	auto SlotLabel(Slot s_) noexcept -> std::string_view {
		switch (s_) {
			case Slot::MoveForward: return "前へ移動";
			case Slot::MoveBack: return "後ろへ移動";
			case Slot::MoveLeft: return "左へ移動";
			case Slot::MoveRight: return "右へ移動";
			case Slot::Attack: return "攻撃";
			case Slot::Guard: return "ガード（押した瞬間で弾き）";
			case Slot::Dodge: return "回避";
			case Slot::Jump: return "ジャンプ（空中でもう一度で二段）";
			default: return "?";
		}
	}

	auto SlotKey(Slot s_) noexcept -> std::string_view {
		switch (s_) {
			case Slot::MoveForward: return "MoveForward";
			case Slot::MoveBack: return "MoveBack";
			case Slot::MoveLeft: return "MoveLeft";
			case Slot::MoveRight: return "MoveRight";
			case Slot::Attack: return "Attack";
			case Slot::Guard: return "Guard";
			case Slot::Dodge: return "Dodge";
			case Slot::Jump: return "Jump";
			default: return "?";
		}
	}

	auto SourceName(In::Source src_) -> std::string {
		if (src_.Kind == In::SourceKind::MouseButton) {
			switch (static_cast<In::MouseButton>(src_.Code)) {
				case In::MouseButton::Left: return "左クリック";
				case In::MouseButton::Right: return "右クリック";
				case In::MouseButton::Middle: return "中クリック";
				default: return "マウス " + std::to_string(src_.Code);
			}
		}
		if (src_.Kind != In::SourceKind::Key) { return "（未設定）"; }

		std::uint16_t const vk{ src_.Code };
		if ((vk >= 0x30U && vk <= 0x39U) || (vk >= 0x41U && vk <= 0x5AU)) { return std::string(1U, static_cast<char>(vk)); }
		if (vk >= VkF1 && vk <= VkF12) { return "F" + std::to_string(vk - VkF1 + 1U); }
		if (vk >= VkNumpad0 && vk <= VkNumpad0 + 9U) { return "テンキー" + std::to_string(vk - VkNumpad0); }
		switch (vk) {
			case VkBack: return "BackSpace";
			case VkTab: return "Tab";
			case VkReturn: return "Enter";
			case VkPause: return "Pause";
			case VkCapital: return "CapsLock";
			case VkEscape: return "ESC";
			case VkSpace: return "Space";
			case VkPrior: return "PageUp";
			case VkNext: return "PageDown";
			case VkEnd: return "End";
			case VkHome: return "Home";
			case VkLeft: return "←";
			case VkUp: return "↑";
			case VkRight: return "→";
			case VkDown: return "↓";
			case VkInsert: return "Insert";
			case VkDelete: return "Delete";
			case VkMultiply: return "テンキー*";
			case VkAdd: return "テンキー+";
			case VkSubtract: return "テンキー-";
			case VkDecimal: return "テンキー.";
			case VkDivide: return "テンキー/";
			case VkLShift: return "左Shift";
			case VkRShift: return "右Shift";
			case VkLControl: return "左Ctrl";
			case VkRControl: return "右Ctrl";
			case VkLMenu: return "左Alt";
			case VkRMenu: return "右Alt";
			default: break;
		}
#if defined(_WIN32)
		// 記号キーなど：今のキーボード配列で出る文字（日本語配列なら @ や : など）
		UINT const ch{ ::MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR) & 0x7FFFU };
		if (ch >= 0x21U && ch <= 0x7EU) { return std::string(1U, static_cast<char>(ch)); }
#endif
		static constexpr char hex[]{ "0123456789ABCDEF" };
		std::string s{ "キー 0x" };
		s += hex[(vk >> 4U) & 0xFU];
		s += hex[vk & 0xFU];
		return s;
	}

	auto IsReserved(In::Source src_) noexcept -> bool {
		if (src_.Kind == In::SourceKind::MouseButton) {
			// GE3 の RawInput が記録するのは左・右だけ
			return !(src_.Code == static_cast<std::uint16_t>(In::MouseButton::Left) || src_.Code == static_cast<std::uint16_t>(In::MouseButton::Right));
		}
		if (src_.Kind != In::SourceKind::Key) { return true; }
		std::uint16_t const vk{ src_.Code };
		if (vk == 0U || vk > 0xFEU) { return true; }
		if (vk <= 0x06U) { return true; }											// マウスボタンの仮想キー
		if (vk == 0x10U || vk == 0x11U || vk == 0x12U) { return true; }		// 左右を区別しない Shift / Ctrl / Alt（RawInput は左右に分けて記録する）
		return vk == VkEscape || vk == VkF1 || vk == VkF2 || vk == VkQ;
	}

	auto Assign(Bindings& b_, Slot slot_, In::Source src_) -> std::optional<Slot> {
		std::size_t const i{ static_cast<std::size_t>(slot_) };
		if (i >= NumSlots || IsReserved(src_)) { return std::nullopt; }
		std::optional<Slot> swapped{};
		for (std::size_t j = 0U; j < NumSlots; ++j) {
			if (j != i && Same(b_[j], src_)) {
				b_[j] = b_[i];
				swapped = static_cast<Slot>(j);
			}
		}
		b_[i] = src_;
		return swapped;
	}

	auto SaveTo(char const* path_, Bindings const& b_) -> bool {
		JSON keys = JSON::object();
		for (std::size_t i = 0U; i < NumSlots; ++i) {
			keys[std::string{ SlotKey(static_cast<Slot>(i)) }] = JSON{
				{ "Kind", KindName(b_[i].Kind) },
				{ "Code", b_[i].Code },
			};
		}
		JSON const root{ { "Keys", keys } };
		std::ofstream ofs{ path_, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { return false; }
		ofs << root.dump(2) << '\n';
		return ofs.good();
	}

	auto LoadFrom(char const* path_) -> Bindings {
		Bindings b{ Defaults() };
		std::ifstream ifs{ path_ };
		if (!ifs.is_open()) { return b; }
		JSON const root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { return b; }
		auto const keys{ root.find("Keys") };
		if (keys == root.end() || !keys->is_object()) { return b; }

		for (std::size_t i = 0U; i < NumSlots; ++i) {
			auto const it{ keys->find(std::string{ SlotKey(static_cast<Slot>(i)) }) };
			if (it == keys->end() || !it->is_object()) { continue; }
			auto const kind{ it->find("Kind") };
			auto const code{ it->find("Code") };
			if (kind == it->end() || !kind->is_string() || code == it->end() || !code->is_number_unsigned()) { continue; }
			std::string const k{ kind->get<std::string>() };
			std::uint32_t const c{ code->get<std::uint32_t>() };
			if (c > 0xFFFFU) { continue; }
			In::Source src{};
			if (k == "Key") { src = In::FromKey(static_cast<std::uint16_t>(c)); }
			else if (k == "Mouse") { src = In::Source{ In::SourceKind::MouseButton, static_cast<std::uint16_t>(c) }; }
			else { continue; }
			if (IsReserved(src)) { continue; }
			Assign(b, static_cast<Slot>(i), src);		// 同じキーが 2 か所に書かれていても、入れ替えで重ならないようにする
		}
		return b;
	}
}
