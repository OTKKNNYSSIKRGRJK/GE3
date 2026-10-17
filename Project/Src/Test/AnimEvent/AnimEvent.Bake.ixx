export module Lumina.AnimEvent : Bake;

//****	******	******	******	******	****//

import <cstddef>;
import <cstdint>;
import <optional>;
import <span>;
import <string>;
import <utility>;
import <vector>;

import : Data;

//////	//////	//////	//////	//////	//////

// 焼き込み：編集用のソース（フレーム番号）→ 実行時データ（秒・ソート済み）（講義 前編 4.2）
//
// ソースは JSON の構造をほぼそのまま写したもの。JSON からソースへの変換（ペイロードのバイト列化を含む）は Adapter が行う。
// ここではファイルも JSON も扱わない（コアは標準ライブラリのみ）。
//
// 焼き込みで行うこと
// - フレーム番号を秒に変換する
// - 瞬間イベントは Time 昇順、窓は Begin 昇順に並べ替える（同じ値なら元の順を保つ）。並べ替え後の添字が窓ID
// - ペイロードを 1 本のバイト列に詰める（各型の alignof に合わせる）
// - カーブを等間隔（1 / FrameRate）に焼き直す
// - 検証する。エラーが 1 つでもあれば nullopt を返す。検証は途中で止めず、見つかった問題はすべて issues_ に積む
//
// 焼き込みでは「やらない」こと
// - ループ終端ちょうどのイベントを 0 へ移すこと。講義（前編 1.3）は焼き込みで行う案だったが、
//   GE3 ではループするかどうかを再生する側が決めるので、焼き込みの時点では分からない。Player が実行時に寄せる。
// - 「同じスロットのヒットボックスが重なっている」のような、ペイロードの中身に依存する検証（後編 8.8）。
//   コアはペイロードの意味を知らないので、必要ならゲーム側の型ごとの検証として足す。

export namespace Lumina::AnimEvent {
	// ペイロード 1 件ぶんのバイト列。Adapter が型ごとに JSON から作る
	struct PayloadBytes {
		std::vector<std::byte> Bytes{};
		std::size_t Align{ 1U };	// 2 の冪で、alignof(std::max_align_t) 以下
	};

	struct InstantSource {
		std::string TypeName{};		// JSON の "type"。型IDは TypeIdOf(TypeName)
		int Frame{ 0 };
		float MinWeight{ 0.0f };
		EventCategory Category{ EventCategory::Presentation };
		PayloadBytes Payload{};
	};

	struct WindowSource {
		std::string TypeName{};
		int BeginFrame{ 0 };		// [BeginFrame, EndFrame)
		int EndFrame{ 0 };
		float MinWeight{ 0.0f };
		EventCategory Category{ EventCategory::Gameplay };
		PayloadBytes Payload{};
	};

	struct CurveSource {
		std::string Name{};								// カーブIDは TypeIdOf(Name)
		std::vector<std::pair<float, float>> Keys{};	// (フレーム, 値)。キーの間は線形補間、範囲外は端の値
	};

	struct ClipEventSource {
		std::string ClipName{};		// 診断メッセージ用
		float FrameRate{ 30.0f };	// イベントを置いたときのフレームレート（glTF には入っていないのでサイドカーに明記する）
		std::vector<InstantSource> Instants{};
		std::vector<WindowSource> Windows{};
		std::vector<CurveSource> Curves{};
	};

	enum class BakeSeverity : std::uint8_t { Warning, Error };

	struct BakeIssue {
		BakeSeverity Severity{ BakeSeverity::Error };
		std::string Message{};
	};

	auto HasError(std::span<BakeIssue const> issues_) noexcept -> bool;

	// clipLength_ はエンジン側のクリップの長さ（秒）。GE3 なら MyAnimation::DurationInSeconds（Adapter::ClipLength）。
	// 例外は使わない（ホットリロードで壊れた JSON を読んでも、ゲームを止めずに issues を表示できるように）。
	auto Bake(ClipEventSource const& src_, float clipLength_, std::vector<BakeIssue>& issues_) -> std::optional<ClipEvents>;
}
