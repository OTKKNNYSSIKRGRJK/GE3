export module Game.Scene;

import <memory>;

import Lumina;

namespace Game::Scene {
	export class Base {
		Base() = default;
		virtual ~Base() = default;
	};

	export int Flag = 0;
}