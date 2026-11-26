//////	//////	//////	//////	//////	//////
//	Input (prototype) - core				//
//////	//////	//////	//////	//////	//////
// 入力の抽象化のコア。キーやボタン（物理的な入力）を、ゲームの「行動」（Move / Attack / Guard ...）に対応づける。
//
// 方針
// - コアは標準ライブラリしか import しない。OS やエンジンの入力（GE3 の RawInput など）は Lumina.Input.Adapter が RawFrame に詰める。
//   名前空間 Lumina::Input は名前を借りているだけで、Lumina の他モジュールには依存しない（移植のとき丸ごと持っていける）。
// - コアは「行動」の中身を知らない。行動はゲーム側が enum で定義し、その値（0..63）を ActionId として渡す。
// - 時間の単位はフレーム（= Update を呼んだ回数）。押していた長さも、先行入力の寿命もフレームで数える。
// - コアはポーリング型。1 フレームのあいだに押して離した入力（1 フレーム未満の連打）は拾えない（RawFrame がその瞬間の状態だから）。
//
// パーティション
//   :Map    生の入力（RawFrame）、行動の定義とバインド（InputMap）、行動ごとの状態とエッジ（InputState）
//   :Buffer 先行入力（InputBuffer）。押した行動を寿命付きで覚えておき、受付が開いた瞬間に取り出す

export module Lumina.Input;

export import : Map;
export import : Buffer;
