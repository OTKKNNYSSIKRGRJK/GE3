//////	//////	//////	//////	//////	//////
//	Fighter - 戦うキャラの体				//
//////	//////	//////	//////	//////	//////
// プレイヤーと敵で共通の「体」。移動・ダッシュ・ジャンプ・通常攻撃（コンボ）・ガード・のけぞり・アビリティ（スキル／アイテム）を持つ。
// 何をするかは「脳」が決めて、毎フレーム FighterFrame で渡す。
//   プレイヤーの脳 = 入力（Game.PlayerInput）。InGame がカメラ基準の入力をワールド基準の Intent に直して渡す。
//   敵の脳       = ビヘイビアツリー（Lumina.BT）。リーフが Intent を書き、先行入力に Push する（まだ作っていない）。
// 以前の Game::Player の中身をここへ移した（Game.Player は Fighter の別名として残してある）。
//
// 脳から体へ渡すものは 2 種類（寿命が違うので分けてある）
//   Intent（FighterIntent） … 毎フレーム上書きする「今こうしたい」。移動・向き・ガードの押しっぱなし。積まない。
//   先行入力（InputBuffer）   … 押した瞬間の行動（Attack / Dodge / Jump / Tool / UseItem）。寿命付きで積み、体が移れるときに取り出す。
//                              Tool / UseItem には「どの枠か」を引数（BufferedInput::Arg）で付ける。
// 「今それを受け付けてよいか」は体が自分の状態で決める（脳は要求するだけ。プレイヤーの同時押しと同じ扱い）。
//
// 1 フレームの中の優先順（Update）
//   1. 動けない（入力ロック・のけぞり・中央への戻り）なら、使用中のアビリティを止める。コンボもやめる
//   2. 使用中のアビリティを進める（終わったら外す）
//   3. コンボ（Game.PlayerCombo の Controller）。何もしていなければ Attack で 1 段目。攻撃中は Cancel 窓の許す行動を取り出す
//   4. 何もしていなければ: アビリティ（Tool / UseItem）→ ダッシュ（Dodge）→ ジャンプ（Jump）→ 移動・ガード
//   ※ 何もしていないときに Attack と Jump が両方積まれていたら、Attack が先（コンボが先に取り出す）。
//
// アビリティ（スキル・アイテム）
//   体の外で作って枠に挿す（SetAbility。体は持ち主ではない）。使える条件・資源（クールダウン・残数）はアビリティ側に持つ。
//   アイテムは「押した瞬間に効果」: Start で効果を出して false を返せば、体の状態は変わらない（動きも止まらない）。
//   続くもの（スキル）は Start で true を返し、次のフレームから Update が false を返すまで UsingAbility になる。
//   のけぞり・ロック・戦闘のやり直しで途中で止まったら Halt が呼ばれる。
//   使用中に再生したいクリップは Clip() で返す（InGame がアニメを選ぶときに見る）。
//   アニメーションイベントの瞬間イベント（サイドカーの "Cue"）は FighterFrame::Cues で届く（溜めから踏み込む・弾を出す、などの時刻をデータで決める）。
//   SuperArmor() が true の間は、殴られても押されず止まらない（ダメージは入る）。
//
// ガード（Intent::GuardHeld）: 何もしていない地上でだけ構える。構えている間は向きを変えずに歩く（速さ × GuardMoveScale）。
//
// 攻撃を受ける（ReceiveHit。当てた側の判定は InGame が行い、結果に応じた演出も InGame が出す）
//   回避の無敵（ダッシュの始めから DodgeInvincibleStart〜+Frames）… Dodged。何も起きない
//     ※ InGame は無敵の間 TryHit を呼ばない（呼ぶと「この窓ではもう当たった」になり、無敵が明けた後も当たらなくなるため）
//   ガード中で、攻撃が前から来た（向きとの角度が GuardHalfAngle 以内）
//     押してから DeflectFrames 以内 … Deflected（弾き）。ダメージ・押しなし。InGame が当てた側に Deflected() を呼ぶ（のけぞる）
//     それ以外                     … Guarded。ダメージ × GuardDamageScale、押す距離 × GuardKnockbackScale、
//                                     GuardStunFrames の間は動けない（ガードの姿勢のまま。Action = Guard）
//     弾きの連打対策: 前に押してから DeflectRepressLockout フレーム以内に押し直したら、その押下には弾きの猶予を付けない
//   それ以外 … Hit。今までどおり（ダメージそのまま、のけぞり）
//
// まだやっていないこと
//   体幹（ガードで溜まり、満ちると崩れる）。今はガードで削りダメージを受けるだけ。
//   回避の押しっぱなしでダッシュ（セキロの操作）。今は押すたびに 1 回のステップ。
//   攻撃中の旋回（追尾）。サイドカーの TurnRate のカーブを体へ渡す口はまだ無い。

export module Game.Fighter;

//****	******	******	******	******	****//

import <algorithm>;
import <array>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <optional>;
import <span>;
import <string_view>;

import Lumina.Math;
import Lumina.Phys.Collider;
import Lumina.Input;

import Game.CollisionLayer;
import Game.PlayerInput;
import Game.PlayerCombo;

//////	//////	//////	//////	//////	//////

export namespace Game {
	class Fighter;

	//==== 脳から体へ ====//

	// 毎フレーム上書きする「今こうしたい」
	struct FighterIntent {
		Lumina::Vec2 Move{};							// ワールドの XZ（x = X、y = Z）。長さ 0..1（1 を超えたら 1 に収める）
		std::optional<Lumina::Vec2> Face{};				// 向きたい方向（ワールドの XZ、長さは問わない）。あれば移動では向きを変えない（横歩き）
		bool GuardHeld{ false };						// ガードを押しっぱなし
	};

	// 1 フレームぶんの入力（脳が作る）と、アニメ側の状態（InGame が作る）
	struct FighterFrame {
		FighterIntent Intent{};
		Lumina::Input::InputBuffer* Buffer{ nullptr };	// 脳の先行入力。null なら押した瞬間の行動は無し
		Lumina::Input::ActionMask WindowAllow{ 0U };	// 今のクリップで開いている Cancel 窓の Allow の和
		bool ClipFinished{ false };						// 今のクリップが終端に着いた（非ループ）
		bool ControlEnabled{ true };					// false: 何も始めない（STARTUP、ゲームオーバー）。重力・吹き飛ばしは進む
		bool CanAttack{ true };							// false: コンボとアビリティを止める（エディタのプレビュー中）。移動はできる
		std::span<std::uint32_t const> Cues{};			// 前のステップで今のクリップに鳴った Cue（瞬間イベント）の名前の ID（CueIdOf）。アビリティが見る
	};

	// Cue の名前の ID。Lumina::AnimEvent::TypeIdOf と同じ規則（FNV-1a 32bit）。PlayerAnimEvents::FiredCues() はこの値を返す
	constexpr auto CueIdOf(std::string_view name_) noexcept -> std::uint32_t {
		std::uint32_t h{ 2166136261U };
		for (char const ch : name_) {
			h ^= static_cast<std::uint8_t>(ch);
			h *= 16777619U;
		}
		return h;
	}

	// 今している行動（BT が「出せたか・終わったか」を見るため）。上ほど優先して報告する
	enum class FighterAction : std::uint8_t {
		Locked,			// 入力ロック中（ゲームオーバー）
		Stagger,		// 吹き飛ばされている・中央へ戻っている
		Ability,		// スキルの使用中
		Attack,			// コンボの途中
		Dash,
		Guard,
		Move,
		Idle,
	};

	//==== 攻撃を受ける ====//

	struct HitRequest {
		float Damage{};						// そのまま受けたときのダメージ
		float Knockback{};					// サイドカーの knockback（power。押す距離 = power × 受けた側の KnockbackScale）
		Lumina::Vec2 PushDir{};				// 押す向き = 当てた側 → 受けた側（ワールドの XZ。長さは問わない）
	};

	enum class HitResult : std::uint8_t {
		Hit,			// 当たった
		Guarded,		// ガードした（削りダメージと、少しのガード硬直）
		Deflected,		// 弾いた（何も受けない。当てた側をのけぞらせる）
		Dodged,			// 回避の無敵で当たらなかった
	};

	//==== アビリティ（スキル・アイテム）====//

	enum class AbilityGroup : std::uint8_t {
		Tool,		// Act::Tool で使う（スキル）
		Item,		// Act::UseItem で使う（アイテム）
		Count,
	};
	inline constexpr std::size_t MaxAbilitySlots{ 4U };

	struct AbilityContext {
		Fighter& Self;
		FighterFrame const& Frame;
	};

	class Ability {
	public:
		virtual ~Ability() = default;

		virtual auto Name() const noexcept -> std::string_view = 0;

		// 今使えるか（クールダウン・残数・地上でのみ、など）。false なら先行入力に残したまま（寿命で消える）
		virtual auto CanStart([[maybe_unused]] Fighter const& self_) const -> bool { return true; }

		// 使い始める。資源を払い、押した瞬間の効果はここで出す。
		// 使用中の状態を続けるなら true（次のフレームから Update）、その場で終わるなら false（アイテム）
		virtual auto Start(AbilityContext& ctx_) -> bool = 0;

		// 使用中、毎フレーム（Start したフレームには呼ばない）。続けるなら true。既定はクリップの終わりまで
		virtual auto Update(AbilityContext& ctx_) -> bool { return !ctx_.Frame.ClipFinished; }

		// 途中で止められた（のけぞり・ロック・戦闘のやり直し）
		virtual void Halt([[maybe_unused]] Fighter& self_) {}

		// 使用中に再生したいクリップの名前（InGame の animDatabase_ の名前）。空なら体の状態どおり。
		// 途中で名前を変えれば、次のクリップが頭から再生される（溜め → 振り、など）
		virtual auto Clip() const noexcept -> std::string_view { return {}; }
		virtual auto ClipLoop() const noexcept -> bool { return false; }

		// true の間は殴られても押されず、のけぞらない（ダメージは入る。アビリティも止まらない）。ガード・弾かれは別
		virtual auto SuperArmor() const noexcept -> bool { return false; }
	};

	//==== 体の設定 ====//

	struct FighterSetup {
		// コリジョンの所属（既定はプレイヤー）
		std::uint32_t BodyLayer{ CollisionLayer::PlayerBody };
		std::uint32_t BodyMask{ CollisionLayer::BossBody | CollisionLayer::BossAttack };
		std::uint32_t WeaponLayer{ CollisionLayer::PlayerWeapon };
		std::uint32_t WeaponMask{ CollisionLayer::BossBody };

		// 体のコライダ（やられ判定）。メッシュ空間（Neki のバインドポーズ）の中心と半分の大きさ。ワールドでは ModelScale × 2 倍になる
		Lumina::Float3 BodyCenter{ 0.0f, 1.33f, -0.05f };
		Lumina::Float3 BodyHalf{ 0.4f, 1.51f, 0.27f };
		float PushRadius{ 1.0f };				// 体どうしが重ならないように押し分けるときの半径（ワールド、XZ の円）

		float MaxHP{ 300.0f };					// 以前は 150（ボスの攻撃で数回で死んでいた）。ImGui「Player」で変えられる
		float InitialAngle{ 1.5707963f };		// Initialize の向き（ラジアン。前方 = (cos, sin)）

		float MoveSpeed{ 0.25f };				// 倒しきったときの 1 フレームの移動量
		float TurnRate{ 0.0f };					// 1 フレームに回れる角度（ラジアン）。0 以下ならすぐ向く（プレイヤー）
		float GuardMoveScale{ 1.0f };			// ガード中の移動の速さの倍率

		// ガード（ReceiveHit）
		float GuardHalfAngle{ 1.2217305f };		// 前から ±この角度（ラジアン、既定 70°）の攻撃だけ防げる
		float GuardDamageScale{ 0.2f };			// ガードしたときのダメージの倍率（削り。体幹を作るまでの代わり）
		float GuardKnockbackScale{ 0.5f };		// ガードしたときの押される距離の倍率
		std::int32_t GuardStunFrames{ 8 };		// ガードしたときに動けないフレーム数（ガードの姿勢のまま）
		std::int32_t DeflectFrames{ 10 };		// ガードを押してから、このフレーム数の間に受けた攻撃は弾く（0 で弾きなし）
		std::int32_t DeflectRepressLockout{ 20 };	// 前に押してからこのフレーム数以内の押し直しには、弾きの猶予を付けない（連打対策）
		// 自分の攻撃を弾かれたとき（Deflected）
		float DeflectedKnockback{ 4.0f };		// power（押す距離 = power × KnockbackScale）
		std::int32_t DeflectedStunFrames{ 30 };	// のけぞるフレーム数

		// ダッシュ（Dodge）
		float DashSpeed{ 2.0f };				// ダッシュの初速（1 フレームの移動量）
		float DashDecay{ 0.85f };				// ダッシュ中、毎フレーム掛ける
		std::int32_t DashFrames{ 15 };
		std::int32_t DodgeInvincibleStart{ 0 };	// 無敵の始まり（ダッシュを始めたフレームを 0 として）
		std::int32_t DodgeInvincibleFrames{ 10 };	// 無敵のフレーム数（0 で無敵なし）

		float JumpInitialSpeed{ 0.25f };
		float Gravity{ 0.02f };
		std::int32_t SecondJumpDelay{ 15 };		// 1 段目から二段ジャンプできるまでのフレーム数（これを超えたら）

		// 吹き飛ばし（のけぞり）。押される距離 = power × KnockbackScale（ワールド）。速さは毎フレーム KnockbackDecay 倍に落ち、
		// HitStunFrames フレームの間は動けない（Action = Stagger）。サイドカーの knockback（power）は ATK1〜3 で 5 / 10 / 15。
		// 以前は「速さ = power × 0.2、毎フレーム 0.95 倍、0.1 を切るまで」で、ATK1 でも 20 ほど滑り、45 フレームほど動けなかった
		float KnockbackScale{ 0.15f };			// ATK1（5）で 0.75、ATK3（15）で 2.25 押される
		float KnockbackDecay{ 0.75f };
		std::int32_t HitStunFrames{ 20 };

		float ArenaMin{ -38.5f };				// X・Z をこの範囲に収める
		float ArenaMax{ 37.0f };

		float ModelScale{ 1.0f };				// 見た目の大きさ（ModelScale() の 3 成分。メッシュ・コライダ・武器はこの倍率の行列に乗る）
		float ModelBaseY{ 1.25f };				// 見た目の高さのずらし
		float FloatAmplitude{ 0.1f };			// 見た目の上下の揺れ
		float FloatAngularFrequency{ 0.1f };
		float VisualTurnBlend{ 0.15f };			// 見た目の向きが本当の向きを追う速さ（毎フレームの補間の割合）
	};

	//==== 体 ====//

	// a_ と b_ の体が重なっていたら（XZ で PushRadius の円どうし）、重なりをなくすように押し分ける。
	// aShare_ は a_ が動く割合（0..1。残りを b_ が動く）。真上に重なっているときは a_ の向きの反対へ
	void SeparateBodies(Fighter& a_, Fighter& b_, float aShare_) noexcept;

	class Fighter {
	public:
		void SetSetup(FighterSetup const& setup_) noexcept { Setup_ = setup_; }
		auto Setup() const noexcept -> FighterSetup const& { return Setup_; }

		// 位置を決めて状態を初期化する（コンボ・アビリティ・入力ロックも解く）
		void Initialize(Lumina::Vec3 const& initPos_);

		// 毎フレーム 1 回
		void Update(FighterFrame const& frame_);

		// 吹き飛ばす（dir の向きへ power × KnockbackScale だけ押し、stunFrames（負なら HitStunFrames）の間のけぞる。
		// dir が壊れていたら中央へ戻す）。ガード・弾き・無敵は見ない（それは ReceiveHit）
		void KnockedBack(float power_, Lumina::Vec2 const& dir_, std::int32_t stunFrames_ = -1);

		// false にすると KnockedBack が何もしない（押されず、のけぞらない。ダメージは入る）。デバッグ用（ボスの「No hit reaction」）
		void SetHitReactionEnabled(bool enabled_) noexcept { HitReactionEnabled_ = enabled_; }

		// 攻撃を受ける（ガード・弾き・回避の無敵を見て、ダメージとのけぞりを決める）。結果に応じた演出は呼んだ側が出す
		auto ReceiveHit(HitRequest const& hit_) -> HitResult;

		// 自分の攻撃を弾かれた（pushDir_ = 弾いた側 → 自分）。DeflectedKnockback / DeflectedStunFrames でのけぞる
		void Deflected(Lumina::Vec2 const& pushDir_);

		//==== 見た目・位置 ====//

		constexpr auto ModelScale() const noexcept -> Lumina::Vec3 { return ModelScale_; }
		constexpr auto ModelRotate() const noexcept -> Lumina::Vec3 { return ModelRotate_; }
		constexpr auto ModelTranslate() const noexcept -> Lumina::Vec3 { return ModelTranslate_; }
		constexpr auto WorldPosition() const noexcept -> Lumina::Vec3 { return Position_; }
		constexpr float Angle() const noexcept { return Angle_; }

		// 体を直接動かす（スキルの踏み込み・押し分けなど）。見た目の位置も同じだけ動かす（範囲の収め直しは次の Update の最後で行う）
		void Translate(float dx_, float dz_) noexcept {
			Position_.x += dx_;
			Position_.z += dz_;
			ModelTranslate_.x += dx_;
			ModelTranslate_.z += dz_;
		}

		// 巻き戻し（ゲームオーバーのあと、再開の前）：水平の位置と向きを決める。次の Update で ModelTranslate / ModelRotate に反映される
		void SetRewindPose(float x_, float z_, float angle_) noexcept { Position_.x = x_; Position_.z = z_; Angle_ = angle_; }

		// 見た目（ModelTranslate / ModelRotate）を今の位置と向きにすぐ合わせる（Update を止めたまま位置を決めるとき。補間しない）
		void SnapModel() noexcept;

		// 体のコライダの形を Setup().BodyCenter / BodyHalf に合わせ直す（Initialize でも行う。SetSetup の後にすぐ効かせたいとき）
		void RefreshBodyShape() noexcept;

		// HP の上限を変える（Setup().MaxHP も変えるので、戦闘をやり直しても残る）。今の HP は割合を保つ
		void SetMaxHP(float maxHP_) noexcept;

		// 入力を受け付けない状態にする（ゲームオーバー中）。横の移動・ダッシュ・吹き飛ばしは止まり、空中なら落ちて着地する
		// （Update は呼び続けること）。Initialize で解除される
		void LockInput(bool locked_) noexcept { InputLocked_ = locked_; }
		constexpr bool IsInputLocked() const noexcept { return InputLocked_; }

		//==== 状態の問い合わせ ====//

		auto Action() const noexcept -> FighterAction;

		// 行動（コンボの段・アビリティ・ダッシュ・ジャンプ）を始めるたびに 1 増える。BT のリーフが「自分の出した行動が始まったか」を見るのに使う
		constexpr auto ActionSerial() const noexcept -> std::uint32_t { return ActionSerial_; }

		constexpr int32_t IsTriggered_Jump() const noexcept { return Trigger_Jump_; }	// このフレームでジャンプした
		constexpr int32_t IsTriggered_Dash() const noexcept { return Trigger_Dash_; }	// このフレームでダッシュした
		auto IsMoving() const noexcept -> int32_t {
			return (Velocity_.x * Velocity_.x + Velocity_.y * Velocity_.y + Velocity_.z * Velocity_.z) > 0.0f;
		}
		constexpr int32_t IsDashing() const noexcept { return (DashStatus_ == 2); }
		constexpr int32_t IsBeingKnockedBack() const noexcept { return IsBeingKnockedBack_; }
		constexpr bool IsAirborne() const noexcept { return Flag_Jump_ != 0 || Position_.y > 0.0f; }
		constexpr bool IsGuarding() const noexcept { return Guarding_; }
		constexpr bool IsGuardStunned() const noexcept { return GuardStun_ && IsBeingKnockedBack_ != 0; }	// ガードして押されている
		auto CanDeflectNow() const noexcept -> bool;		// 今攻撃を受けたら弾ける（ガード中で、押してから DeflectFrames 以内）
		auto IsInvincible() const noexcept -> bool;			// 回避の無敵の間
		// ダッシュの向きが今の向きに近い（前へのダッシュ。cos が minCos_ 以上）。アニメの選択用
		auto IsDashingForward(float minCos_ = 0.5f) const noexcept -> bool;

		// コンボ
		auto ComboStep() const noexcept -> std::int32_t { return Combo_.Step(); }			// 0 = 攻撃していない、1..MaxStep
		auto Attacking() const noexcept -> bool { return Combo_.Attacking(); }
		constexpr bool ComboStepStarted() const noexcept { return ComboStepStarted_; }	// このフレームで新しい段を始めた（クリップを頭から）
		void ResetCombo() noexcept { Combo_.Reset(); }

		// アビリティ
		void SetAbility(AbilityGroup group_, std::size_t slot_, Ability* ability_) noexcept;
		auto AbilityAt(AbilityGroup group_, std::size_t slot_) const noexcept -> Ability*;
		auto ActiveAbility() const noexcept -> Ability const* { return Active_; }
		constexpr bool AbilityStarted() const noexcept { return AbilityStarted_; }		// このフレームで使用中のアビリティを始めた（クリップを頭から）
		auto AbilityClip() const noexcept -> std::string_view { return Active_ ? Active_->Clip() : std::string_view{}; }
		auto AbilityClipLoop() const noexcept -> bool { return Active_ ? Active_->ClipLoop() : false; }

		// 向きを dir_ の方へ最大 maxStep_ だけ回す（0 以下ならすぐ向く）。アビリティの追尾用
		void FaceToward(Lumina::Vec2 const& dir_, float maxStep_) noexcept;

		//==== コライダ（形状はメッシュ空間で持つ。World の更新は親行列を知っている InGame が行う）====//

		Lumina::Phys::Collider& BodyCollider() noexcept { return BodyCollider_; }
		Lumina::Phys::Collider const& BodyCollider() const noexcept { return BodyCollider_; }
		Lumina::Phys::Collider& WeaponCollider() noexcept { return WeaponCollider_; }
		Lumina::Phys::Collider const& WeaponCollider() const noexcept { return WeaponCollider_; }

	private:
		void InitializeColliders();
		void HaltAbility() noexcept;
		auto TryStartAbility(FighterFrame const& frame_, std::optional<Lumina::Input::BufferedInput> request_) -> bool;
		void Dash(FighterFrame const& frame_, bool canMove_);
		void Move(FighterFrame const& frame_, bool canMove_);
		void TurnToward(float targetAngle_, float maxStep_) noexcept;
		void ReturnToCenter();

		// Vec3 は align(16) なので、メンバには Float3 を使う（C4324 を避ける。Phys.Shape と同じ方針）
		Lumina::Float3 Position_{};
		Lumina::Float3 Velocity_{};
		Lumina::Float3 ModelScale_{ 1.0f, 1.0f, 1.0f };
		Lumina::Float3 ModelRotate_{};
		Lumina::Float3 ModelTranslate_{};

		FighterSetup Setup_{};

		float Angle_{};
		float SpeedByInput_{};
		float SpeedByDash_{};
		Lumina::Vec2 DashDir_{ 1.0f, 0.0f };
		Lumina::Vec2 DashDirNow_{ 1.0f, 0.0f };	// このフレームに実際に走った向き（IsDashingForward 用）

		int32_t Trigger_Jump_{};
		int32_t Flag_Jump_{};
		int32_t SecondJumpInputInterval_{};
		float JumpSpeed_{};

		bool ForceJump_{ false };		// Cancel 窓から抜けたジャンプ・ダッシュ（このフレームだけ）
		bool ForceDash_{ false };

		int32_t Trigger_Dash_{};
		int32_t DashStatus_{ -1 };
		int32_t DashTimer_{};

		float FloatTimer_{};

		float SpeedByKnockBack_{};
		std::int32_t KnockFrames_{};		// のけぞりの残りフレーム
		Lumina::Vec2 KnockBackDir_{};
		int32_t IsBeingKnockedBack_{};
		int32_t IsReturningToCenter_{};
		bool InputLocked_{ false };
		bool HitReactionEnabled_{ true };

		bool Guarding_{ false };
		bool GuardStun_{ false };				// 今ののけぞりはガードの硬直（Guarding_ を保つ）
		bool GuardHeldPrev_{ false };			// 前のフレームの GuardHeld（押した瞬間を見るため）
		bool DeflectArmed_{ false };			// 今の押下に弾きの猶予がある（連打対策で外れることがある）
		std::int32_t SinceGuardPress_{ 1 << 20 };	// ガードを押してからのフレーム数（大きな値 = ずっと押していない）
		bool ComboStepStarted_{ false };
		bool AbilityStarted_{ false };
		std::uint32_t ActionSerial_{ 0U };

		Game::PlayerCombo::Controller Combo_{};
		Lumina::Input::InputBuffer NoInput_{};		// 脳が先行入力を渡さないときの空の入れ物

		std::array<std::array<Ability*, MaxAbilitySlots>, static_cast<std::size_t>(AbilityGroup::Count)> Slots_{};
		Ability* Active_{ nullptr };

		Lumina::Phys::Collider BodyCollider_{};
		Lumina::Phys::Collider WeaponCollider_{};

	public:
		float MaxHP_{};
		float Inv_MaxHP_{};
		float HP_{};
	};
}
