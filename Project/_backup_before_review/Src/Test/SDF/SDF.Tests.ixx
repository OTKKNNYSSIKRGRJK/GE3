//////	//////	//////	//////	//////	//////
//	SDF tests								//
//////	//////	//////	//////	//////	//////
// Lumina.SDF の単体テスト（標準ライブラリだけ）。失敗した項目を返すだけで、表示は呼び出し側で行う。
// 呼び出し例（Debug のみ）:
//   for (auto const& f : Lumina::SDF::Tests::RunAll()) {
//       OutputDebugStringA(std::format("[SDF.Tests] {}: {}\n", f.Test, f.Detail).c_str());
//   }

export module Lumina.SDF.Tests;

// SDF.cpp / SDF.Tests.cpp で使うヘッダユニット（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cmath>;
import <cstdint>;
import <random>;
import <span>;
import <string>;
import <vector>;

import Lumina.SDF;

export namespace Lumina::SDF::Tests {
	struct Failure {
		std::string Test;
		std::string Detail;
	};

	auto RunAll() -> std::vector<Failure>;
}
