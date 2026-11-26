//////	//////	//////	//////	//////	//////
//	BossBrain - ボスの脳（ビヘイビアツリー）//
//////	//////	//////	//////	//////	//////
// ボスの体（Game.Fighter）を動かすビヘイビアツリー。旧 Boss.Kinoko の BT（波濤・円形の弾）は外し、近接だけで作り直した。
// スキルは Game.BossSkills（溜め斬り・茨の波）。脳は先行入力に Tool（引数 = 枠）を積むだけで、使えるか（距離・クールダウン）はスキルが決める。
// ガードの反応はまだ入れていない。
//
// 脳と体の分担
//   脳は毎 tick、ブラックボードの Intent（移動・向き）を書き、先行入力（Buffer）に Attack / Dodge を積むだけ。
//   受け付けるかどうか（コンボの窓・ダッシュ中か）は体が決める。プレイヤーの入力と同じ道を通るので、体の規則は敵にも同じく効く。
//   脳は体の Update の前に回る（読む体の状態は前のフレームの終わりのもの）。
//
// ツリー（いちど選んだ行動は終わるまで続け、終わったら選び直す。のけぞりだけは割り込む）
//   Root: ReactiveSelector
//   ├ Stagger: ReactiveSequence[ のけぞっている, Idle ]
//   └ Combat: Selector
//       ├ Close: Sequence[ 距離 < CloseRange, Pick{ Attack: Combo(1..3 段) → Recover / Retreat: Backstep → Recover / Circle: Strafe } ]
//       ├ Mid:   Sequence[ 距離 < MidRange,   Pick{ Rush: DashIn → Combo(1..2 段) → Recover / Walk: Approach(CloseRange) / Circle: Strafe
//       │                                          / Slash: UseSkill(溜め斬り) → Recover / Wave: UseSkill(茨の波) → Recover } ]
//       └ Far:   Pick{ Walk: Approach(MidRange) / Wave: UseSkill(茨の波) → Recover }
//   Pick は入ったときに重みで 1 つ選び、それを最後まで走らせる。重みは BossTuning（ImGui で変えられる）。
//   スキルの選択肢は、そのスキルが今使えないとき（距離・クールダウン）は重み 0 として扱う。
//
// 時間の単位はフレーム（固定 60fps 前提）。

export module Game.BossBrain;

//****	******	******	******	******	****//

import <array>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <memory>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

import Lumina.Math;
import Lumina.Input;
import Lumina.BT;

import Game.Fighter;

//////	//////	//////	//////	//////	//////

export namespace Game {
	// 調整値（ImGui「Boss AI」で変えられる。保存はまだしていない）
	struct BossTuning {
		float CloseRange{ 9.0f };			// これより近ければ近距離の行動（攻撃が届くくらい）
		float MidRange{ 24.0f };			// これより近ければ中距離の行動（ダッシュで詰められるくらい）
		float ApproachSpeed{ 1.0f };		// 近づくときの倒し具合（0..1）
		std::int32_t ApproachTimeout{ 240 };	// 近づき続けたら選び直す（フレーム）

		std::int32_t ComboMinCloseHits{ 1 };	// 近距離のコンボの段数（この範囲から選ぶ）
		std::int32_t ComboMaxCloseHits{ 3 };
		std::int32_t ComboMinRushHits{ 1 };		// ダッシュで詰めた後のコンボの段数
		std::int32_t ComboMaxRushHits{ 2 };
		std::int32_t RecoverMin{ 20 };			// 攻撃の後の隙（フレーム）
		std::int32_t RecoverMax{ 50 };

		std::int32_t StrafeMin{ 60 };			// 回り込む長さ（フレーム）
		std::int32_t StrafeMax{ 120 };
		float StrafeSpeed{ 0.5f };				// 回り込むときの倒し具合（距離は入ったときの距離を保つ）

		// 行動の重み（0 にすればその行動は選ばれない）
		float CloseAttackWeight{ 6.0f };
		float CloseRetreatWeight{ 1.5f };
		float CloseCircleWeight{ 1.5f };
		float MidRushWeight{ 4.0f };
		float MidWalkWeight{ 3.0f };
		float MidCircleWeight{ 2.0f };
		float MidSlashWeight{ 2.0f };			// 溜め斬り
		float MidWaveWeight{ 1.0f };			// 茨の波
		float FarWalkWeight{ 3.0f };
		float FarWaveWeight{ 2.0f };

		float DamageScale{ 0.1f };			// サイドカーのダメージに掛ける（プレイヤーの HP は 150、ATK1〜3 は 100〜300 なので）
	};

	// 脳の 1 tick ぶんのブラックボード（ボス 1 体につき 1 つ）
	struct BossBlackboard {
		// 入力（Tick の前に BossBrain が作る）
		Fighter const* Self{ nullptr };
		Fighter const* Target{ nullptr };
		BossTuning const* Tuning{ nullptr };
		Lumina::Vec2 ToTarget{ 1.0f, 0.0f };	// 自分 → 相手（XZ、長さ 1）
		float Distance{ 0.0f };					// 自分と相手の水平の距離
		std::array<bool, MaxAbilitySlots> SkillReady{};	// Tool の枠ごとに、今使えるか（CanStart）

		// 出力（リーフが書く。毎 tick 空にしてから回す）
		FighterIntent Intent{};
		Lumina::Input::InputBuffer* Buffer{ nullptr };

		// 乱数（xorshift32。再現できるように脳が持つ）
		std::uint32_t RngState{ 0x9E3779B9U };
		auto NextRandom() noexcept -> std::uint32_t;
		auto RandomRange(std::int32_t min_, std::int32_t max_) noexcept -> std::int32_t;		// [min, max]
		auto RandomUnit() noexcept -> float;											// [0, 1)

		void Press(Lumina::Input::ActionId action_, std::uint8_t arg_ = 0U) const noexcept;
	};

	class BossBrain {
	public:
		BossBrain();

		// 1 tick。Intent を空にしてからツリーを回す。target_ が無ければ何もしない（Intent は空）
		void Tick(Fighter const& self_, Fighter const* target_, BossTuning const& tuning_, Lumina::Input::InputBuffer& buffer_);

		// ツリーごと止める（体が動けない間、操作を乗っ取っている間、戦闘のやり直し）。Intent も空にする
		void Halt();

		void Seed(std::uint32_t seed_) noexcept { BB_.RngState = (seed_ != 0U) ? seed_ : 1U; }

		auto Intent() const noexcept -> FighterIntent const& { return BB_.Intent; }
		auto Blackboard() const noexcept -> BossBlackboard const& { return BB_; }
		auto Tree() const noexcept -> Lumina::BT::Tree<BossBlackboard> const& { return Tree_; }

	private:
		Lumina::BT::Tree<BossBlackboard> Tree_{};
		BossBlackboard BB_{};
	};
}
