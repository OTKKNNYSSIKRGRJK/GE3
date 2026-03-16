export module Lumina.Container.Bitset;

//****	******	******	******	******	****//

import <memory>;
import <cassert>;
import <array>;

//////	//////	//////	//////	//////	//////

namespace Lumina {
	export template<uint32_t N>
	class Bitset {
	public:
		constexpr bool operator[](uint32_t pos_) const;
		constexpr const uint8_t* operator()() const noexcept;

	public:
		constexpr Bitset<N>& Set(uint32_t pos_, bool val_ = true);
		constexpr Bitset<N>& Set(const Bitset<N>& bitset_);
		Bitset<N>& SetAllZero() {
			Data_.fill(0);
			return (*this);
		}

	private:
		constexpr uint32_t DataLength() const noexcept;

		constexpr uint32_t Index(uint32_t pos_) const noexcept;
		constexpr uint32_t Target(uint32_t pos_) const noexcept;

	public:
		Bitset();
		~Bitset() noexcept;

	private:
		std::array<uint8_t, N> Data_;
	};

	template<uint32_t N>
	constexpr bool Bitset<N>::operator[](uint32_t pos_) const {
		return (Data_[Index(pos_)] & Target(pos_));
	}

	template<uint32_t N>
	constexpr const uint8_t* Bitset<N>::operator()() const noexcept {
		return Data_.data();
	}

	template<uint32_t N>
	constexpr Bitset<N>& Bitset<N>::Set(uint32_t pos_, bool val_) {
		auto idx{ Index(pos_) };
		auto target{ Target(pos_) };
		Data_[idx] &= (0xFFU - target);
		Data_[idx] |= val_ * target;

		return *this;
	}

	template<uint32_t N>
	constexpr Bitset<N>& Bitset<N>::Set(const Bitset<N>& bitset_) {
		Data_ = bitset_.Data_;
		return *this;
	}

	template<uint32_t N>
	constexpr uint32_t Bitset<N>::DataLength() const noexcept { return (N + 0x07U) >> 3U; }

	template<uint32_t N>
	constexpr uint32_t Bitset<N>::Index(uint32_t pos_) const noexcept { return pos_ >> 3U; }

	template<uint32_t N>
	constexpr uint32_t Bitset<N>::Target(uint32_t pos_) const noexcept { return 1U << (pos_ & 0x07U); }

	template<uint32_t N>
	Bitset<N>::Bitset() {
		static_assert(N > 0U);
	}

	template<uint32_t N>
	Bitset<N>::~Bitset() noexcept {}
}