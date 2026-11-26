export module Lumina.Editor : Lexicon;

import <functional>;

import <vector>;

import <string>;
import <string_view>;

import <stdexcept>;

namespace Lumina::Editor {
	export template<typename T>
	class Lexicon {
		struct Entry {
			std::string Lexis;
			size_t HashValue_Lexis;
			T Content;
		};

	public:
		constexpr size_t Size() const noexcept { return Entries_.size(); }

	public:
		constexpr std::string_view Lexis(int idx_) const {
			return Entries_.at(idx_).Lexis;
		}
		constexpr T Content(int idx_) const {
			return Entries_.at(idx_).Content;
		}

	public:
		T Find(std::string_view lexis_) const {
			size_t hashVal{ std::hash<std::string_view>{}(lexis_) };
			for (auto const& entry : Entries_) {
				if (hashVal == entry.HashValue_Lexis) {
					return entry.Content;
				}
			}
			throw std::runtime_error{ "Entry not found!\n" };
		}
		std::string_view Find(T content_) const {
			for (auto const& entry : Entries_) {
				if (content_ == entry.Content) {
					return entry.Lexis;
				}
			}
			throw std::runtime_error{ "Entry not found!\n" };
		}

	public:
		Lexicon(std::initializer_list<std::pair<std::string, T>> initList_) {
			std::hash<std::string_view> hasher{};
			for (auto const& item : initList_) {
				Entries_.emplace_back(
					item.first,
					hasher(item.first),
					item.second
				);
			}
		}

	private:
		std::vector<Entry> Entries_{};
	};
}