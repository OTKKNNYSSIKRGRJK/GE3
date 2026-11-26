//////	//////	//////	//////	//////	//////
//	CombatEvents - 戦闘の出来事の通知			//
//////	//////	//////	//////	//////	//////
// 昔のイベントシステム（Src/Test/Event/EventSystem.h、Innominatus::EventManager）の上に載せた薄い口。
// 当たり判定の結果（命中・ガード・弾き・回避）や壁への接触を InGame が Publish し、演出側（Game.CombatVfx・Game.Battlefield）が
// Subscribe で受け取る。InGame は「何が起きたか」だけを積み、どんな演出を出すかは知らない。
//
// - Publish は積むだけ（EventManager::Queue_Event）。配るのは Dispatch（InGame の Update の終わりに 1 回）。
//   演出の更新の直前に配るので、同じフレームのうちに演出が始まる。
// - EventSystem.h はこのモジュールの実装（.cpp）だけが #include する。インターフェースには std::function などを出さず、
//   受け取り側は「文脈ポインタ＋関数ポインタ」で登録する（ヘッダのインクルードとヘッダユニットの import を同じ翻訳単位で混ぜないため）。
// - 登録の解除は無い（元の EventManager::Remove_EventListener が未実装）。InGame の寿命 = アプリの寿命なので今は困らない。
//   同じ相手を 2 回 Subscribe すると 2 回届くので、受け取り側は Initialize で 1 回だけ登録すること。

export module Game.CombatEvents;

//****	******	******	******	******	****//

import <cstdint>;

import Lumina.Math.Numerics;

//////	//////	//////	//////	//////	//////

export namespace Game::CombatEvents {
	enum class Side : std::uint8_t {
		Player,
		Boss,
	};

	// Game::HitResult と同じ並び（InGame は static_cast で渡す）
	enum class Outcome : std::uint8_t {
		Hit,
		Guarded,
		Deflected,
		Dodged,
	};

	enum class Source : std::uint8_t {
		Weapon,		// 武器のコライダ（通常攻撃・溜め斬り）
		Thorn,		// 茨の波の列
	};

	// 攻撃が相手に届いた（結果つき）
	struct Hit {
		Side Victim{ Side::Player };		// 受けた側
		Outcome Result{ Outcome::Hit };
		Source From{ Source::Weapon };
		Lumina::Float3 Point{};				// 接触点（ワールド）
		Lumina::Float2 Dir{};				// 当てた側 → 受けた側（XZ、長さは問わない）
		float Damage{ 0.0f };				// 受けた側に入る前のダメージ（演出の強さの目安）
	};

	// 体が闘技場の端（見えない壁）に触れた
	struct WallTouch {
		Side Who{ Side::Player };
		std::uint32_t Wall{ 0U };			// 0: -Z / 1: +Z / 2: -X / 3: +X
		Lumina::Float3 Point{};				// 触れた位置（ワールド。高さは体の中ほど）
		float Strength{ 1.0f };				// 速さから決めた強さ（目安 0..3）
	};

	void Publish(Hit const& e_);
	void Publish(WallTouch const& e_);

	using HitHandler = void(*)(void* context_, Hit const& e_);
	using WallTouchHandler = void(*)(void* context_, WallTouch const& e_);

	void Subscribe(void* context_, HitHandler handler_);
	void Subscribe(void* context_, WallTouchHandler handler_);

	// 積んだ出来事を、登録した順に配る
	void Dispatch();
}
