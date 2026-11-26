//////	//////	//////	//////	//////	//////
//	Boss - ボス（体＋脳）					//
//////	//////	//////	//////	//////	//////
// 旧 Boss_Kinoko（Boss.Kinoko.ixx / .cpp）の作り直し。体はプレイヤーと同じ Game.Fighter、脳は Game.BossBrain（ビヘイビアツリー）。
// 古い技（波濤・円形の弾）は外した。攻撃は通常攻撃のコンボ（プレイヤーと同じ ATK1〜3 のクリップとサイドカー）と、
// スキル 2 つ（Game.BossSkills の溜め斬り・茨の波。Tool の枠 0 / 1 に挿してある。2026-11-11）。
//
// Fighter を継承しているので、位置・HP・コライダなどの問い合わせはプレイヤーと同じ名前で使える
// （InGame は今のところ Boss_Kinoko_ の名前のまま持っている。InGame を見直すときに名前を変える）。
// Initialize / Update / SetRewindPose は、ボス用に名前を隠している（中で Fighter の同名の関数を呼ぶ）。
//
// 1 フレームの流れ（Update）
//   1. 自分の先行入力を 1 フレーム進める
//   2. 脳を回す（操作を乗っ取っている・AI を止めている・動かせないときは、ツリーを止める）
//   3. 体を Update（Intent・先行入力・アニメの状態・Cue）
//   4. 茨の列を進める（当たりの処理と見た目は InGame）
//
// Debug
//   FreezeAI   … 脳を止めてその場に立たせる（吹き飛ばしは動く）。当たり判定の確認用
//   NoKnockBack … プレイヤーの攻撃で押されず、のけぞらない（ダメージは入る）。既定 off（プレイヤーと同じく、殴られたらのけぞる。
//                 2026-11-10 に on から変えた。プレイヤーだけ硬直するのはズルいため）
//   Possess    … キーボード／パッドでボスを動かす（InGame がプレイヤーの入力を回してくる。プレイヤーは動かなくなる）

export module Game.Boss;

//****	******	******	******	******	****//

import <cmath>;
import <cstddef>;
import <cstdint>;
import <optional>;
import <span>;
import <string>;
import <string_view>;
import <utility>;

import Lumina.Math;
import Lumina.Input;

import Game.CollisionLayer;
import Game.Fighter;
import Game.BossBrain;
export import Game.BossSkills;		// Waves() / スキルの調整値を InGame から使うため

//////	//////	//////	//////	//////	//////

export namespace Game {
	// ボスの体の設定（旧 Boss_Kinoko の値に寄せた。旋回の速さなどは新しく決めた仮の値）
	auto BossSetup() -> FighterSetup;

	// 1 フレームぶんの外の状態（InGame が作る）
	struct BossUpdate {
		Fighter const* Target{ nullptr };					// 狙う相手（プレイヤー）
		bool ControlEnabled{ true };						// false: 何も始めない（IN_BATTLE 以外、ゲームオーバーの演出中）
		Lumina::Input::ActionMask WindowAllow{ 0U };		// ボスのクリップで開いている Cancel 窓
		bool ClipFinished{ false };
		bool CanAttack{ true };								// false: エディタのプレビュー中
		std::optional<FighterIntent> ManualIntent{};		// Debug.Possess のとき、脳の代わりに使う
		Lumina::Input::InputBuffer* ManualBuffer{ nullptr };
		std::span<std::uint32_t const> Cues{};				// ボスのクリップで前のステップに鳴った Cue（BossAnimEvents_.FiredCues()）
	};

	class Boss : public Fighter {
	public:
		struct DebugSwitches {
			bool FreezeAI{ true };
			bool NoKnockBack{ false };
			bool Possess{ false };
		};
		DebugSwitches Debug{};

		Boss();
		Boss(Boss const&) = delete;				// スキルが自分のメンバ（調整値・茨の列）を指しているので、写さない
		Boss& operator=(Boss const&) = delete;

		void Initialize(Lumina::Vec3 const& initPos_);
		void Update(BossUpdate const& update_);

		// 巻き戻し（ゲームオーバーのあと、再開の前）。Update を止めている間に呼ぶので、見た目もすぐ合わせる
		void SetRewindPose(float x_, float z_, float angle_) noexcept;

		auto Tuning() noexcept -> BossTuning& { return Tuning_; }
		auto Tuning() const noexcept -> BossTuning const& { return Tuning_; }
		auto Brain() const noexcept -> BossBrain const& { return Brain_; }
		auto Buffer() const noexcept -> Lumina::Input::InputBuffer const& { return Buffer_; }

		// スキル（枠は BossSkillSlot）。茨の列は Update の中で進む。当たりの処理（無敵・ReceiveHit・MarkHit）と見た目は InGame
		auto Waves() noexcept -> ThornWaves& { return Waves_; }
		auto Waves() const noexcept -> ThornWaves const& { return Waves_; }
		auto SlashTuning() noexcept -> ChargeSlashTuning& { return SlashTuning_; }
		auto WaveTuning() noexcept -> ThornWaveTuning& { return WaveTuning_; }
		auto SlashSkill() const noexcept -> ChargeSlash const& { return Slash_; }
		auto WaveSkill() const noexcept -> ThornWave const& { return Wave_; }

		// ImGui「Boss AI」（Debug のみ。Release では何もしない）
		void DrawEditor();

		// 調整値（体の FighterSetup の調整する値と BossTuning）の保存・読み込み。InGame の Initialize で 1 回読む。
		// 戦闘をやり直すたび（Initialize）には読まない（保存していない調整が消えないように）
		static constexpr char const* ConfigPath{ "Assets/Configs/Boss.json" };
		auto Save() -> bool;
		auto Load() -> bool;
		auto SettingsStatus() const noexcept -> std::string const& { return Status_; }

	private:
		BossBrain Brain_{};
		BossTuning Tuning_{};
		Lumina::Input::InputBuffer Buffer_{ 10U };
		std::string Status_{};

		ChargeSlashTuning SlashTuning_{};
		ThornWaveTuning WaveTuning_{};
		ThornWaves Waves_{};
		ChargeSlash Slash_{ &SlashTuning_ };
		ThornWave Wave_{ &WaveTuning_, &Waves_ };
	};
}
