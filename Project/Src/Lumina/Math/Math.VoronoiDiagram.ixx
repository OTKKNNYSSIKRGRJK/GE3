export module Lumina.Math.VoronoiDiagram;

import <vector>;
import <queue>;

import Lumina.Math.Numerics;

//////	//////	//////	//////	//////	//////

namespace Lumina {
	#define INLINE_NAMESPACE_MATH_BEGIN		inline namespace Math {
	#define INLINE_NAMESPACE_MATH_END		}
}

//****	******	******	******	******	****//

namespace Lumina {
	INLINE_NAMESPACE_MATH_BEGIN

	class VoronoiDiagram {
	private:
		struct Event {
			enum { SITE, CIRCLE, } Type;
			Numerics::Float2 Position;

			Event() {
				Type = SITE;
			}

			// Compares in ascending order the x-coordinates
			// and then the y-coordinates if the x-coordinates are the same.
			struct PositionComparator {
				constexpr bool operator()(
					const Event& lhs_, const Event& rhs_
				) const noexcept {
					return
						(
							(lhs_.Position.x < rhs_.Position.x) ||
							(
								(lhs_.Position.x == rhs_.Position.x) &&
								(lhs_.Position.y < lhs_.Position.y)
							)
						);
				}
			};
		};

	private:
		std::priority_queue<Event, std::vector<Event>, Event::PositionComparator> EventQueue_;
	};

	INLINE_NAMESPACE_MATH_END
}