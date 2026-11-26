//////	//////	//////	//////	//////	//////
//	Player - プレイヤーの体（別名）			//
//////	//////	//////	//////	//////	//////
// プレイヤーの中身は Game.Fighter（プレイヤーと敵で共通の体）へ移した。
// InGame などが Game::Player の名前で使っているので、別名として残している（InGame を見直すときに Fighter へ寄せる）。
// 操作（入力 → FighterIntent）は InGame（Impl::PlayerIntent）が作る。

export module Game.Player;

export import Game.Fighter;

export namespace Game {
	using Player = Fighter;
}
