export module Lumina.Math.Random;

//****	******	******	******	******	****//

import <random>;

import <memory>;

//////	//////	//////	//////	//////	//////

#define INLINE_NAMESPACE_NUMERICS_BEGIN		inline namespace Numerics {
#define INLINE_NAMESPACE_NUMERICS_END		}

//****	******	******	******	******	****//

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

//****	******	******	******	******	****//

namespace Lumina {
	INLINE_NAMESPACE_NUMERICS_BEGIN

	export class Random {
		class Engine {
			friend Random;

		public:
			inline auto operator()() -> std::mt19937::result_type { return MT19937_(); }

		private:
			std::mt19937 MT19937_{ std::random_device{}() };
		};

	public:
		static inline Engine& Generator() {
			static UniPtr<Engine> engine{ new Engine{} };
			return *engine;
		}
	};
	
	INLINE_NAMESPACE_NUMERICS_END
}