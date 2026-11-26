export module Game.Scene;

import <memory>;

import Lumina;

namespace Game::Scene {
	export class Base {
		Base() = default;
		virtual ~Base() = default;
	};

	#if !defined(NDEBUG)
	export int Flag = 1;
	#else
	export int Flag = 0;
	#endif

	// true の間、タイトルで ESC を押してもウィンドウを閉じない（main.cpp が見る）。
	// タイトルで設定の画面を開いている間と、そこを ESC で閉じたあと ESC を離すまで（2026-11-11）
	export bool BlockEscapeQuit = false;
}