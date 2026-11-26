//////	//////	//////	//////	//////	//////
//	Game.CollisionLayer						//
//////	//////	//////	//////	//////	//////
// Collider::Layer / Mask に入れるビット。組み合わせの意味はゲーム側で決める。

export module Game.CollisionLayer;

import <cstdint>;

export namespace Game::CollisionLayer {
	inline constexpr uint32_t PlayerBody{ 1U << 0U };
	inline constexpr uint32_t PlayerWeapon{ 1U << 1U };
	inline constexpr uint32_t BossBody{ 1U << 2U };
	inline constexpr uint32_t BossAttack{ 1U << 3U };		// ボスの攻撃判定（未使用。弾などを移すとき用）
}
