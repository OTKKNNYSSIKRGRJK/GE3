export module Lumina.Container.List;

//****	******	******	******	******	****//

import <memory>;
import <cassert>;

namespace Lumina {
	export template<typename T>
	class List {
	protected:
		T* Elements_{ nullptr };
		bool* Table_IsActive_{ nullptr };
		int* Table_Prev_{ nullptr };
		int* Table_Next_{ nullptr };

		uint32_t Capacity_{ 32U };
		uint32_t Size_{ 0U };

		int32_t Active_First_{};
		int32_t Active_Last_{};
		int32_t Inactive_First_{};
		int32_t Inactive_Last_{};

		void Initialize_Implementation(uint32_t capacity_) {
			if (capacity_ > 0U) { Capacity_ = capacity_; }

			assert(Elements_ == nullptr);
			Elements_ = new T[Capacity_];

			assert(Table_IsActive_ == nullptr);
			Table_IsActive_ = new bool[Capacity_];

			assert(Table_Prev_ == nullptr);
			Table_Prev_ = new int[Capacity_];

			assert(Table_Next_ == nullptr);
			Table_Next_ = new int[Capacity_];

			std::memset(Table_IsActive_, 0, sizeof(bool) * Capacity_);
			for (uint32_t i{ 0U }; i < Capacity_; ++i) {
				Table_Prev_[i] = i - 1;
				Table_Next_[i] = i + 1;
			}
			Table_Next_[Capacity_ - 1] = -1;

			Active_First_ = -1;
			Active_Last_ = -1;
			Inactive_First_ = 0;
			Inactive_Last_ = Capacity_ - 1;

			Size_ = 0U;
		}

		void Delete_Implementation(int index) {
			if (index > -1 && index < static_cast<int>(Capacity_) && Table_IsActive_[index] == 1) {
				int prev{ Table_Prev_[index] };
				int next{ Table_Next_[index] };

				if (index != Active_First_) { Table_Next_[prev] = Table_Next_[index]; }
				else { Active_First_ = Table_Next_[Active_First_]; }
				if (index != Active_Last_) { Table_Prev_[next] = Table_Prev_[index]; }
				else { Active_Last_ = Table_Prev_[Active_Last_]; }

				if (Inactive_First_ == -1) {
					Inactive_First_ = index;
					Table_Prev_[index] = -1;
				}
				else {
					Table_Next_[Inactive_Last_] = index;
					Table_Prev_[index] = Inactive_Last_;
				}
				Table_Next_[index] = -1;
				Inactive_Last_ = index;

				Table_IsActive_[index] = 0;

				--Size_;
			}
		}

	public:
		class Iterator;

	public:
		using ElementType = typename T;

	public:
		void Initialize(uint32_t capacity_) {
			Initialize_Implementation(capacity_);
		}

	public:
		constexpr List() noexcept = default;
		inline List(uint32_t capacity_) {
			Initialize_Implementation(capacity_);
		}

		virtual ~List() noexcept {
			if (Elements_ != nullptr) {
				delete[] Elements_;
				Elements_ = nullptr;
			}

			if (Table_IsActive_ != nullptr) {
				delete[] Table_IsActive_;
				Table_IsActive_ = nullptr;
			}

			if (Table_Prev_ != nullptr) {
				delete[] Table_Prev_;
				Table_Prev_ = nullptr;
			}

			if (Table_Next_ != nullptr) {
				delete[] Table_Next_;
				Table_Next_ = nullptr;
			}
		}

	public:
		// Returns an unused entry.
		[[nodiscard]] T& New() {
			assert(!IsFull());

			int i{ Inactive_First_ };
			
			if (Active_First_ == -1) {
				Active_First_ = i;
			}
			else {
				Table_Prev_[i] = Active_Last_;
				Table_Next_[Active_Last_] = i;
			}
			Active_Last_ = i;
			Inactive_First_ = Table_Next_[Inactive_First_];
			if (Inactive_First_ == -1) { Inactive_Last_ = -1; }
			Table_Next_[i] = -1;

			Table_IsActive_[i] = 1;

			++Size_;

			return Elements_[i];
		}

		void Delete(Iterator& it_) { Delete_Implementation(it_.Index_Current); }

		void Clear() {
			std::memset(Table_IsActive_, 0, sizeof(bool) * Capacity_);
			for (uint32_t i{ 0U }; i < Capacity_; ++i) {
				Table_Prev_[i] = i - 1;
				Table_Next_[i] = i + 1;
			}
			Table_Next_[Capacity_ - 1] = -1;

			Active_First_ = -1;
			Active_Last_ = -1;
			Inactive_First_ = 0;
			Inactive_Last_ = Capacity_ - 1;

			Size_ = 0U;
		}

		constexpr uint32_t Size() const noexcept { return Size_; }
		constexpr uint32_t Capacity() const noexcept { return Capacity_; }
		constexpr bool IsFull() const noexcept { return (Inactive_First_ == -1); }

		constexpr T const* Data() const noexcept { return Elements_; }

		class Iterator {
			friend List;

		private:
			List<T> const* Iteratee_{ nullptr };
			int Index_Current{ -1 };
			int Index_Next{ -1 };

		public:
			explicit Iterator(List<T> const& iteratee_) : Iteratee_{ &iteratee_ } {}
			~Iterator() = default;

			constexpr void Begin() {
				Index_Current = Iteratee_->Active_First_;
				Index_Next = Iteratee_->Table_Next_[Index_Current];
			}
			constexpr bool End() const { return (Index_Current == -1); }
			constexpr void Next() {
				Index_Current = Index_Next;
				Index_Next = (Index_Next == -1) ? (-1) : (Iteratee_->Table_Next_[Index_Next]);
			}

			inline T& operator*() {
				assert(Index_Current != -1);
				return Iteratee_->Elements_[Index_Current];
			};

			constexpr int Index() const noexcept { return Index_Current; }
		};
	};
}