//////	//////	//////	//////	//////	//////
//	Input (prototype) - unit tests			//
//////	//////	//////	//////	//////	//////
// コアの単体テスト。テストフレームワークは使わず、標準ライブラリだけで書く（コアと同じ制約）。
// 失敗した項目を返すだけで、出力や停止はしない。表示は呼び出し側で行う。
//
// 呼び出し側のサンプル（main.cpp などで、Debug のときだけ 1 回）:
//   #if defined(_DEBUG)
//   import Lumina.Input.Tests;
//   ...
//   for (auto const& f : Lumina::Input::Tests::RunAll()) {
//       OutputDebugStringA(std::format("[Input.Tests] {}: {}\n", f.Test, f.Detail).c_str());
//   }
//   #endif

export module Lumina.Input.Tests;

//****	******	******	******	******	****//

import <string>;
import <vector>;

//////	//////	//////	//////	//////	//////

export namespace Lumina::Input::Tests {
	struct Failure {
		std::string Test;		// テスト名
		std::string Detail;		// 何が食い違ったか
	};

	// 全テストを実行し、失敗した項目を返す（空なら全部通った）
	auto RunAll() -> std::vector<Failure>;
}
