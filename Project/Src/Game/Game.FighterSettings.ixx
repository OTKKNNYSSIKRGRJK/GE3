//////	//////	//////	//////	//////	//////
//	FighterSettings - 体の調整値の保存		//
//////	//////	//////	//////	//////	//////
// FighterSetup の調整値（ImGui で変えるもの）を JSON に書き出す・読み込む。ファイルの読み書きもここ。
//   プレイヤー: Assets/Configs/Player.json（ImGui「Player」の Save / Load。InGame の Initialize で読む）
//   ボス:       Assets/Configs/Boss.json（ImGui「Boss AI」の Save / Load。ボスの AI の調整値も一緒。Game.Boss が使う）
//
// 方針（Title の Settings.cpp・GameOverCamera と同じ）
// - 書き出しと読み込みで同じ Visit を通す（キーの書き間違いで片方だけずれるのを防ぐ）。
// - 読み込みは「ファイルにあるキーだけ上書き」。型が違うキーは無視する。後から項目を足しても古いファイルが読める。
// - 例外を使わない（壊れた JSON・ファイルが無い、は false と状態の文字列を返すだけで、値は変えない）。
// - 保存するのは調整する値だけ。コリジョンの所属・最初の向き・見た目の大きさや揺れはコードで決める（保存しない）。
// - Release でも読む（調整した値でゲームが動く）。

export module Game.FighterSettings;

//****	******	******	******	******	****//

import <cmath>;
import <cstddef>;
import <cstdint>;
import <fstream>;
import <optional>;
import <string>;
import <utility>;

import nlohmann.json;

import Lumina.Math;
import Game.Fighter;

//////	//////	//////	//////	//////	//////

export namespace Game::FighterSettings {
	using JSON = nlohmann::json;

	inline constexpr char const* PlayerConfigPath{ "Assets/Configs/Player.json" };

	// FighterSetup の調整値を j_ に書く／j_ にあるキーだけ s_ に読む
	void Write(JSON& j_, FighterSetup const& s_);
	void Read(JSON const& j_, FighterSetup& s_);

	// 読んだ設定を体に効かせる（SetSetup ＋ やられ判定の形 ＋ HP の上限。今の HP は割合を保つ）
	void Apply(Fighter& fighter_, FighterSetup const& s_);

	// ファイル。status_ には ImGui に出す短い英文が入る
	auto SaveFile(char const* path_, JSON const& root_, std::string& status_) -> bool;
	auto LoadFile(char const* path_, std::string& status_) -> std::optional<JSON>;

	// プレイヤー（{ "version": 1, "Body": {...} }）
	auto SavePlayer(Fighter const& player_, std::string& status_) -> bool;
	auto LoadPlayer(Fighter& player_, std::string& status_) -> bool;
}
