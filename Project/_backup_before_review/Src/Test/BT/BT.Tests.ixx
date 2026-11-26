//////	//////	//////	//////	//////	//////
//	BT (prototype) - unit tests				//
//////	//////	//////	//////	//////	//////
// Lumina.BT の単体テスト。標準ライブラリだけで書き、失敗した項目を返すだけ（表示は呼び出し側）。
//
// 呼び出し側のサンプル（Debug のときだけ 1 回）:
//   #if defined(_DEBUG)
//   import Lumina.BT.Tests;
//   for (auto const& f : Lumina::BT::Tests::RunAll()) {
//       OutputDebugStringA(std::format("[BT.Tests] {}: {}\n", f.Test, f.Detail).c_str());
//   }
//   #endif

export module Lumina.BT.Tests;

//****	******	******	******	******	****//

import <string>;
import <vector>;

//////	//////	//////	//////	//////	//////

export namespace Lumina::BT::Tests {
	struct Failure {
		std::string Test;
		std::string Detail;
	};

	auto RunAll() -> std::vector<Failure>;
}
