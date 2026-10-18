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

export module Game.AnimEventDemo;

//****	******	******	******	******	****//

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cmath>;
import <cstddef>;
import <cstdint>;
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

//////	//////	//////	//////	//////	//////

export namespace Game::AnimEventDemo {
	// 武器の当たり判定の窓のペイロード
	struct HitboxPayload {
		static constexpr std::string_view TypeName{ "Hitbox" };
		std::uint32_t Slot{ 0U };		// "slot": "RightBlade" → TypeIdOf。今は武器が 1 本なので使っていない
		float Damage{ 0.0f };			// "damage"
		float Knockback{ 0.0f };		// "knockback"（Boss_Kinoko::KnockedBack の power）
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

		// 武器が targetId_ に触れたときに呼ぶ。今開いている窓でまだ当たっていなければ、その窓のペイロードを返す。
		// 同じ窓の間に 2 回目以降は nullopt。プレビュー中はダメージを入れないので常に nullopt
		auto TryHit(std::uint32_t targetId_) -> std::optional<HitboxPayload>;

		// エディタ（ImGui）。Release では何もしない。編集の反映は次の Update の頭で行う
		void DrawEditor();

		// true: アニメーションイベントの窓で当たり判定する／false: 旧方式（攻撃中ずっと有効、ボスがノックバック中は当てない）。
		// 時刻とポーズはどちらでもこのクラスが持つ（比べやすいように、違うのは当たり判定だけにしている）
		bool UseAnimEvent{ true };

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
