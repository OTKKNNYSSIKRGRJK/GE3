//////	//////	//////	//////	//////	//////
//	AnimEvent (prototype) - unit tests		//
//////	//////	//////	//////	//////	//////
// コアの単体テスト。テストフレームワークは使わず、標準ライブラリだけで書く（コアと同じ制約）。
// 失敗した項目を返すだけで、出力や停止はしない。表示は呼び出し側で行う。
//
// 呼び出し側のサンプル（main.cpp などで、Debug のときだけ 1 回）:
//   #if defined(_DEBUG)
//   import Lumina.AnimEvent.Tests;
//   ...
//   for (auto const& f : Lumina::AnimEvent::Tests::RunAll()) {
//       OutputDebugStringA(std::format("[AnimEvent.Tests] {}: {}\n", f.Test, f.Detail).c_str());
//   }
//   #endif

export module Lumina.AnimEvent.Tests;

//****	******	******	******	******	****//

import <string>;
import <vector>;

//////	//////	//////	//////	//////	//////

export namespace Lumina::AnimEvent::Tests {
	struct Failure {
		std::string Test;		// テスト名
		std::string Detail;		// 何が食い違ったか
	};

	// 全テストを実行し、失敗した項目を返す（空なら全部通った）
	auto RunAll() -> std::vector<Failure>;
}
