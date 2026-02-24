export module Lumina.Container.List;

//****	******	******	******	******	****//

import <memory>;
import <cassert>;

namespace Lumina {
	export template<typename T>
	class List {
	protected:
		T* Table_Element{ nullptr };
		bool* Table_IsActive{ nullptr };
		int* Table_Prev{ nullptr };
		int* Table_Next{ nullptr };

		uint32_t Capacity_{ 32U };

		int Active_First{};
		int Active_Last{};
		int Inactive_First{};
		int Inactive_Last{};

		void Initialize(uint32_t capacity_) {
			if (capacity_ > 0) { Capacity_ = capacity_; }

			assert(Table_Element == nullptr);
			Table_Element = new T[Capacity_];

			assert(Table_IsActive == nullptr);
			Table_IsActive = new bool[Capacity_];

			assert(Table_Prev == nullptr);
			Table_Prev = new int[Capacity_];

			assert(Table_Next == nullptr);
			Table_Next = new int[Capacity_];

			std::memset(Table_IsActive, 0, sizeof(bool) * Capacity_);
			for (uint32_t i{ 0U }; i < Capacity_; ++i) {
				Table_Prev[i] = i - 1;
				Table_Next[i] = i + 1;
			}
			Table_Next[Capacity_ - 1] = -1;

			Active_First = -1;
			Active_Last = -1;
			Inactive_First = 0;
			Inactive_Last = Capacity_ - 1;
		}

		void Delete_Implementation(int index) {
			if (index > -1 && index < static_cast<int>(Capacity_) && Table_IsActive[index] == 1) {
				int prev{ Table_Prev[index] };
				int next{ Table_Next[index] };

				if (index != Active_First) { Table_Next[prev] = Table_Next[index]; }
				else { Active_First = Table_Next[Active_First]; }
				if (index != Active_Last) { Table_Prev[next] = Table_Prev[index]; }
				else { Active_Last = Table_Prev[Active_Last]; }

				if (Inactive_First == -1) {
					Inactive_First = index;
					Table_Prev[index] = -1;
				}
				else {
					Table_Next[Inactive_Last] = index;
					Table_Prev[index] = Inactive_Last;
				}
				Table_Next[index] = -1;
				Inactive_Last = index;

				Table_IsActive[index] = 0;
			}
		}

	public:
		template<typename T>
		class Iterator;

		explicit List() { Initialize(32U); }
		explicit List(uint32_t capacity_) { Initialize(capacity_); }

		virtual ~List() noexcept {
			if (Table_Element != nullptr) {
				delete[] Table_Element;
				Table_Element = nullptr;
			}

			if (Table_IsActive != nullptr) {
				delete[] Table_IsActive;
				Table_IsActive = nullptr;
			}

			if (Table_Prev != nullptr) {
				delete[] Table_Prev;
				Table_Prev = nullptr;
			}

			if (Table_Next != nullptr) {
				delete[] Table_Next;
				Table_Next = nullptr;
			}
		}

		// Returns an unused entry.
		[[nodiscard]] T& NewElement() {
			assert(!IsFull());

			int i{ Inactive_First };
			
			if (Active_First == -1) {
				Active_First = i;
			}
			else {
				Table_Prev[i] = Active_Last;
				Table_Next[Active_Last] = i;
			}
			Active_Last = i;
			Inactive_First = Table_Next[Inactive_First];
			if (Inactive_First == -1) { Inactive_Last = -1; }
			Table_Next[i] = -1;

			Table_IsActive[i] = 1;

			return Table_Element[i];
		}

		void Delete(Iterator<T>& it_) { Delete_Implementation(it_.Index_Current); }

		void Clear() {
			std::memset(Table_IsActive, 0, sizeof(bool) * Capacity_);
			for (uint32_t i{ 0U }; i < Capacity_; ++i) {
				Table_Prev[i] = i - 1;
				Table_Next[i] = i + 1;
			}
			Table_Next[Capacity_ - 1] = -1;

			Active_First = -1;
			Active_Last = -1;
			Inactive_First = 0;
			Inactive_Last = Capacity_ - 1;
		}

		constexpr bool IsFull() const noexcept { return (Inactive_First == -1); }

		template<typename T>
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
				Index_Current = Iteratee_->Active_First;
				Index_Next = Iteratee_->Table_Next[Index_Current];
			}
			constexpr bool End() const { return (Index_Current == -1); }
			constexpr void Next() {
				Index_Current = Index_Next;
				Index_Next = (Index_Next == -1) ? (-1) : (Iteratee_->Table_Next[Index_Next]);
			}

			inline T& operator*() {
				assert(Index_Current != -1);
				return Iteratee_->Table_Element[Index_Current];
			};

			constexpr int Index() const noexcept { return Index_Current; }
		};
	};
}