export module Lumina.Math.Numerics;

//****	******	******	******	******	****//

import <cstdint>;

//////	//////	//////	//////	//////	//////

#define INLINE_NAMESPACE_NUMERICS_BEGIN		inline namespace Numerics {
#define INLINE_NAMESPACE_NUMERICS_END		}

//****	******	******	******	******	****//

export namespace Lumina {
	INLINE_NAMESPACE_NUMERICS_BEGIN
	
	struct Int2 { int32_t x, y; };
	struct Int3 { int32_t x, y, z; };
	struct Int4 { int32_t x, y, z, w; };

	struct Float2 { float x, y; };
	struct Float3 { float x, y, z; };
	struct Float4 { float x, y, z, w; };

	using Float4x4 = float[4][4];
	
	INLINE_NAMESPACE_NUMERICS_END
}