//////	//////	//////	//////	//////	//////
//	AnimEvent demo - player hitbox timing	//
//////	//////	//////	//////	//////	//////
// アニメーションイベント（Lumina.AnimEvent）を InGame のプレイヤーにつなぐテスト。今回は当たり判定のタイミングだけ。
//
// やること
// - プレイヤーのアニメの時刻を持つ（InGame の animTimer_ の代わり。4 倍速・ループ・終端で止まる、は今までと同じ）
// - Hitbox の窓が開いている間だけ武器のコライダを有効にする（状態は IsOpen で問い合わせる）
// - 窓が 1 回開くあいだ、同じ相手には 1 回しか当てない（Begin でヒット済みの相手を空にする）
// - ダメージとノックバックはペイロードから取る
// - 調整用のエディタ（Debug のみ）：タイムライン、プレビュー（クリップを選んでスクラブ・コマ送り）、窓のフレーム編集、保存
//
// 1 フレームの順序（InGame の今の構成に合わせている）
//   フレーム N の終わり: Update（Step → 時刻 t_N、窓の開閉） → CurrentPose で t_N のポーズを作る（描画もこのポーズ）
//   フレーム N+1 の途中: コライダの更新（ボーン行列は t_N のもの） → IsHitboxOpen（これも t_N の状態） → Detect → TryHit
//   ポーズと窓の状態が同じ時刻 t_N を指すので、窓の中のポーズはどれも 1 回ずつ判定される（判定は描画から 1 フレーム遅れる）
//
// サイドカー JSON の形は Lumina.AnimEvent.Adapter の冒頭を参照。Hitbox のキー: "slot"（名前）, "damage", "knockback"
//
// 演出の窓（VfxLoop、2026-10-19 追加）
// - 窓の開閉を「トラック」として持つだけ（開いてからの秒・閉じてからの秒・閉じた理由）。見た目（エンベロープ・位置・描画）はエフェクト側
//   （Game.AnimEventVfx）が VfxTracks() を読んで決める。タイミング（いつ）と見た目（どう出すか）を分けるため
// - Reloaded で閉じた窓が同じ Step で開き直されたら、同じトラックとして引き継ぐ（編集のたびに立ち上がりからやり直さないように）
// - プレビュー中は窓の開閉をそのまま見せる（Preview = true。閉じたら即座に消す）。スクラブのたびに再生し直すので、
//   エンベロープを掛けると、止めた時刻の光り方を確認できなくなるため
// - VfxLoop のキー: "effect"（名前、必須）, "socket"（名前、省略時 "WeaponTip"）, "scale"（省略時 1）
//
// 合図の瞬間イベント（Cue、2026-11-11 追加。スキル用）
// - "instants": [ { "type": "Cue", "frame": 19, "name": "Lunge" } ]。判定系なので持ち主のインスタンスだけ、ライブ再生のときだけ
// - Update で鳴った Cue の名前の ID（TypeIdOf(name) = Game::CueIdOf(name)）を FiredCues() で返す。次の Update まで残る
//   （体はそれを次のフレームの FighterFrame::Cues で受け取る。Cancel の窓と同じく 1 フレーム遅れ）
//
// 行動の受付と再生速度の窓（2026-11-06 追加。コンボ用。判定系なので持ち主のインスタンスだけ）
// - Cancel:   開いている間、"allow" に書いた行動へ移れる（["Attack"] でコンボの次の段、["Dodge", "Jump", "Move"] で振りから抜ける）。
//             名前は Game.PlayerInput の行動名。"allow" が無い・配列でない・知らない名前は読み込みエラー。状態は CancelAllowMask() で問い合わせる
// - PlayRate: 開いている間、再生速度に "rate"（0 より大きい、必須）を掛ける。振り抜きの後半を遅くして、次の段の受付を長くする。
//             重なったら一番小さい値。窓の状態は前のステップのものなので、1 フレーム遅れて効く。クリップを再生し直したフレームには掛けない

export module Game.AnimEventDemo;

//****	******	******	******	******	****//

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <cstring>;
import <format>;
import <fstream>;
import <memory>;
import <optional>;
import <span>;
import <string>;
import <string_view>;
import <unordered_map>;
import <utility>;
import <vector>;

import nlohmann.json;

import Lumina.AnimEvent;
import Lumina.AnimEvent.Adapter;
import Lumina.CG3D.Struct;

import Game.PlayerInput;

//////	//////	//////	//////	//////	//////

export namespace Game::AnimEventDemo {
	// 武器の当たり判定の窓のペイロード
	struct HitboxPayload {
		static constexpr std::string_view TypeName{ "Hitbox" };
		std::uint32_t Slot{ 0U };		// "slot": "RightBlade" → TypeIdOf。今は武器が 1 本なので使っていない
		float Damage{ 0.0f };			// "damage"
		float Knockback{ 0.0f };		// "knockback"（Fighter::KnockedBack の power）
	};

	// 演出の窓のペイロード。窓が開いている間エフェクトを出す。
	// 見た目の値（色・長さ・立ち上がり秒など）はエフェクト側の定義に持たせ、サイドカーには「どれを・どこに」だけを書く
	struct VfxPayload {
		static constexpr std::string_view TypeName{ "VfxLoop" };
		std::uint32_t Effect{ 0U };		// "effect": "TipRays" → TypeIdOf
		std::uint32_t Socket{ 0U };		// "socket": "WeaponTip" → TypeIdOf
		float Scale{ 1.0f };			// "scale"：強さの倍率
	};

	// 行動の受付の窓のペイロード（"allow": 行動名の配列）
	struct CancelPayload {
		static constexpr std::string_view TypeName{ "Cancel" };
		std::uint64_t Allow{ 0U };		// Lumina::Input::ActionMask（bit = Game::PlayerInput::Act の値）
	};

	// 再生速度の窓のペイロード（"rate": 倍率）
	struct PlayRatePayload {
		static constexpr std::string_view TypeName{ "PlayRate" };
		float Rate{ 1.0f };
	};

	// 合図の瞬間イベントのペイロード（"name": 合図の名前）。スキルが「ここで踏み込む・ここで撃つ」をデータで決めるため
	struct CuePayload {
		static constexpr std::string_view TypeName{ "Cue" };
		std::uint32_t Name{ 0U };		// "name": "Lunge" → TypeIdOf
	};

	// VfxLoop の窓 1 つ分（開いているもの、または閉じてから VfxLinger 秒以内のもの）
	struct VfxTrack {
		VfxPayload Payload{};
		float OpenAge{ 0.0f };			// 窓が開いてからの秒（実時間。閉じたら止まる）
		float ClosedAge{ -1.0f };		// 閉じてからの秒。開いている間は負
		Lumina::AnimEvent::EndReason Reason{ Lumina::AnimEvent::EndReason::None };	// 閉じた理由（Completed / Interrupted）
		bool Preview{ false };			// プレビュー中に開いたもの（エンベロープを掛けず、開いている間は最大で見せる）
		std::uint32_t Serial{ 0U };		// 開いた順の通し番号（模様のずらしなどに使う）

		// 以下は PlayerAnimEvents の内部用
		Lumina::AnimEvent::InstanceId Source{ Lumina::AnimEvent::InvalidInstance };
		std::uint16_t Window{ 0U };
		bool PendingReload{ false };	// Reloaded で閉じた直後（同じ Step の Begin で引き継がれなければ Interrupted 扱いで閉じる）
	};

	// クリップ名（animDatabase_ のキー）とサイドカーのパス
	struct ClipBinding {
		std::string Name{};
		std::string SidecarPath{};		// ファイルが無くてもよい（エディタの Create で作れる）
	};

	class PlayerAnimEvents {
	public:
		using ClipMap = std::unordered_map<std::string, Lumina::CG3D::MyAnimation>;

		struct Pose {
			Lumina::CG3D::MyAnimation const* Clip{ nullptr };
			float Time{ 0.0f };
		};

		// clips_ は呼び出し側が持ち続けること（要素のアドレスを覚える。unordered_map の要素は再ハッシュでは動かない）。
		// rate_ は再生速度（GE3 は 4 倍速）
		void Initialize(ClipMap const& clips_, std::span<ClipBinding const> bindings_, float rate_);

		// 毎フレーム 1 回。ゲームが選んだクリップ（名前が変わったら最初から再生する）とループの有無を渡す。
		// プレビュー中はゲームの選んだクリップを無視して、エディタで選んだクリップを再生する
		void Update(std::string const& clipName_, bool loop_, float dt_);

		// ポーズを作るクリップと時刻（プレビュー中はプレビューの方）
		auto CurrentPose() const -> Pose;

		// Hitbox の窓が開いているか（武器のコライダの Enabled にそのまま使う）
		auto IsHitboxOpen() const -> bool;

		// 開いている Cancel の窓の Allow の和（コンボの次の段・振りから抜ける行動）。Update の後の状態
		auto CancelAllowMask() const -> std::uint64_t;

		// 開いている PlayRate の窓の倍率（一番小さい値。無ければ 1）
		auto PlayRateScale() const -> float;

		// ゲームが再生しているクリップ（非ループ）が終端に着いて止まっているか
		auto IsLiveFinished() const -> bool;

		// 前の Update で鳴った Cue の名前の ID（ライブ再生のみ）
		auto FiredCues() const noexcept -> std::span<std::uint32_t const> { return FiredCues_; }

		// エディタのプレビュー中か（プレビュー中はゲームのクリップを再生していないので、コンボは止める）
		auto IsPreviewing() const noexcept -> bool { return Mode_ == Mode::Preview; }

		// 次の Update で、ゲームが選んでいるクリップを頭から再生し直す（名前が同じでも。コンボの段が変わったときなど）
		void RestartLive() noexcept { RestartLive_ = true; }

		// 武器が targetId_ に触れたときに呼ぶ。今開いている窓でまだ当たっていなければ、その窓のペイロードを返す。
		// 同じ窓の間に 2 回目以降は nullopt。プレビュー中はダメージを入れないので常に nullopt
		auto TryHit(std::uint32_t targetId_) -> std::optional<HitboxPayload>;

		// VfxLoop の窓（開いているもの＋閉じてから消えるまでのもの）。Update の後に読む
		auto VfxTracks() const -> std::span<VfxTrack const> { return VfxTracks_; }

		// 閉じてからこの秒数を過ぎたトラックは捨てる（エフェクトの消える時間はこれより短くすること）
		static constexpr float VfxLinger{ 2.0f };

		// エディタ（ImGui）。Release では何もしない。編集の反映は次の Update の頭で行う
		void DrawEditor();

		// true: アニメーションイベントの窓で当たり判定する／false: 旧方式（攻撃中ずっと有効、ボスがノックバック中は当てない）。
		// 時刻とポーズはどちらでもこのクラスが持つ（比べやすいように、違うのは当たり判定だけにしている）
		bool UseAnimEvent{ true };

		// エディタのウィンドウ名（2 つ目を作るときは変えること。ImGui は名前でウィンドウを区別する）
		std::string EditorTitle{ "AnimEvent Hitbox" };

	public:
		PlayerAnimEvents() = default;
		PlayerAnimEvents(PlayerAnimEvents const&) = delete;
		auto operator=(PlayerAnimEvents const&) -> PlayerAnimEvents& = delete;
		~PlayerAnimEvents();

	private:
		using JSON = Lumina::AnimEvent::Adapter::JSON;

		struct ClipSlot {
			std::string Name{};
			Lumina::CG3D::MyAnimation const* Clip{ nullptr };
			std::string SidecarPath{};
			JSON Doc{};						// 編集中のサイドカー。null ならサイドカーなし
			std::unique_ptr<Lumina::AnimEvent::ClipEvents> Events{};	// 再生に渡すもの。Initialize 後は常に非 null
			std::vector<Lumina::AnimEvent::BakeIssue> LoadIssues{};	// ファイルの読み込みの問題（壊れた JSON など）
			std::vector<Lumina::AnimEvent::BakeIssue> Issues{};		// 最後の焼き込みの問題（LoadIssues を含む）
			bool Dirty{ false };			// 未保存の変更がある
			bool NeedsRebuild{ false };		// 次の Update の頭で焼き直す（Rebind はステップの境目で行うため）
		};

		struct OpenHitbox {
			Lumina::AnimEvent::InstanceId Source{ Lumina::AnimEvent::InvalidInstance };
			std::uint16_t Window{ 0U };
			HitboxPayload Payload{};
			std::vector<std::uint32_t> HitTargets{};	// この窓で当たった相手
		};

		enum class Mode : std::uint8_t { Live, Preview };

		Lumina::AnimEvent::Adapter::PayloadRegistry Registry_{};
		Lumina::AnimEvent::AnimEventPlayer Player_{};
		std::vector<Lumina::AnimEvent::AnimEvent> Events_{};
		std::vector<std::unique_ptr<ClipSlot>> Slots_{};		// ClipSlot のアドレスは動かさない
		std::vector<OpenHitbox> OpenHitboxes_{};
		std::vector<VfxTrack> VfxTracks_{};
		std::vector<std::uint32_t> FiredCues_{};
		std::uint32_t NextVfxSerial_{ 0U };
		float Rate_{ 1.0f };
		std::uint64_t StepCount_{ 0U };

		// インスタンス → クリップ（ログの表示用。直近のものだけ）
		std::vector<std::pair<Lumina::AnimEvent::InstanceId, ClipSlot const*>> InstanceClips_{};

		// ゲームが再生しているクリップ
		ClipSlot* Live_{ nullptr };
		Lumina::AnimEvent::InstanceId LiveId_{ Lumina::AnimEvent::InvalidInstance };
		bool RestartLive_{ false };

		// プレビュー（エディタで選んだクリップ）
		Mode Mode_{ Mode::Live };
		std::size_t PreviewIndex_{ 0U };
		float PreviewTime_{ 0.0f };
		bool PreviewPlaying_{ false };
		bool PreviewLoop_{ true };
		bool PreviewFullRate_{ true };		// false なら等速（1 倍）で見る
		Lumina::AnimEvent::InstanceId PreviewId_{ Lumina::AnimEvent::InvalidInstance };
		bool RestartPreview_{ false };

		// ログ（ライブ再生のイベントとヒット）
		std::vector<std::string> Log_{};
		bool LogEnabled_{ true };

	private:
		auto Find(std::string_view name_) -> ClipSlot*;
		auto Shown() -> ClipSlot*;		// エディタに出すクリップ（ライブなら再生中、プレビューなら選んだもの）
		auto Play(ClipSlot& slot_, float startTime_, float rate_, bool loop_) -> Lumina::AnimEvent::InstanceId;
		void LoadSidecar(ClipSlot& slot_);
		auto SaveSidecar(ClipSlot& slot_) -> bool;
		void CreateSidecar(ClipSlot& slot_);
		void Rebuild(ClipSlot& slot_);
		void HandleEvents();
		void HandleVfx(Lumina::AnimEvent::AnimEvent const& e_);
		void AgeVfxTracks(float dt_);
		void FinishVfxReloads();
		void PushLog(std::string text_);
		auto SlotOf(Lumina::AnimEvent::InstanceId id_) const -> ClipSlot const*;
		auto ClipNameOf(Lumina::AnimEvent::InstanceId id_) const -> std::string_view;
		auto TypeNameOf(Lumina::AnimEvent::EventTypeId type_) const -> std::string_view;
		static auto FrameRateOf(ClipSlot const& slot_) -> float;
		static auto ReportIssues(ClipSlot const& slot_) -> void;

		// エディタ（Game.AnimEventDemo.Editor.cpp）
		void DrawTimeline(ClipSlot& slot_, float time_);
		void DrawSidecarEditor(ClipSlot& slot_, int currentFrame_);
	};
}
