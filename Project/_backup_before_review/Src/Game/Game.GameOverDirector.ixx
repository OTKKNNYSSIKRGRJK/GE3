//////	//////	//////	//////	//////	//////
//	GameOverDirector						//
//////	//////	//////	//////	//////	//////
// ゲームオーバー演出の「ひとつの時計」。致命打（LOSE への切替）の瞬間に Start() すると、次のものが同じ時刻から動く：
//   * 色抜け（Game.GameOverGrade：グラデーションマップ・ビネット・UI のフェード）
//   * カメラワーク（Game.GameOverCamera：キーフレーム）
//   * 倒れるモーション（Dying.gltf のクリップ。このクラスがポーズの時刻を決め、InGame が骨に反映する）
//   * 粉々になって消える演出（Game.GameOverShatter：ディゾルブ＋粒子。時刻はここの経過秒を渡す）
//   * ラベル（「Game Over」と「Press Space to Continue」の画像）のフェードイン
//   * 再開できるようになる時刻
// どれも「Start からの秒」で並べてあるので、編集画面で同じ時間軸に並べて合わせられる。
//
// 時刻の進め方：固定の dt（InGame の Update から Update(dt)。ポーズ中は進まない）。色抜けとカメラは自分の時計も持っていて、
//   Start() / Reset() / Update(dt) を同時に呼ぶので、ずれない。
// スローモーション（序盤）：演出の時計（倒れるモーション・カメラ・色抜け・粉々の時刻）の進みを、Start から少しの間だけ遅くする。
//   TimeScale() が、その瞬間の進む速さ（1 = 通常）。InGame の Update が色抜けとカメラの dt にも同じ倍率を掛ける。
//   ボスや弾などの世界の動きは遅くならない（このクラスが持つのは演出の時計だけ）。
//   このクラスの Elapsed() や各 Delay は、遅くなった時計の上の秒（実時間は、スローの分だけ長くなる）。
// 巻き戻し（再開の前）：BeginRewind() で、演出の時計を今の秒から 0 へ、Look.RewindDuration（実時間の秒）かけて戻す。
//   進みは p = u^RewindPower（u = 経過 / RewindDuration。ゆっくり始まって加速）、時計 = 始まりの秒 × (1 - p)。
//   色抜け・カメラ・倒れるモーション・粉々（ディゾルブ）・ラベルは、どれも時計の関数なので、そのまま逆に再生される。
//   プレイヤーとボスを初期配置へ戻すのは InGame の Lose()（RewindProgress() = p を使う）。終わったら IsRewindFinished() が true になるので、InGame が再開する。
// エディタ「GameOver Director」（Debug のみ）：全トラックを 1 枚のタイムラインに並べて表示する（読み取り専用）。
//   タイムラインをクリック／ドラッグするか、スライダーを動かすと、色抜け・カメラ・倒れるモーション・結果表示がその時刻の見た目で止まる
//   （スクラブ）。個々の値は「GameOver Grade」「GameOver Camera」と、このウィンドウの値で編集する。
// 倒れるモーション：Dying.gltf のクリップから 1 本を使う。アニメーションは骨の名前で引くので、
//   「プレイヤーの骨格の骨を実際に動かす数（driven）」で選ぶ：
//     1. Look.ClipName と同じ名前のクリップがあり、1 本以上の骨を動かすなら、それ。
//     2. 無ければ（Blender で出し直すとクリップ名が変わる。…Retarget.009 → .010 など）、動かす骨が一番多いクリップ。
//   骨の名前が違うクリップ（Mixamo の Hips / Spine… など）は 0 本なので選ばれない。Neki と同じ骨格で名前だけ違う（Root, Spine1, LeftHip…）
//   クリップは、Initialize で 2 つの骨格の階層を頭から順に突き合わせて、骨の名前を Neki の名前に付け替えてから数える。
//   エディタの Clip の欄で別のクリップに切り替えられる（動かす骨の数が出る。0 のクリップはポーズが変わらない）。
//   クリップは 5.5 秒（30fps）で、立ったまま後ろへよろめく（約 1.7 秒まで）→ 崩れ落ちる（約 3.4 秒で床）→ 横たわったまま。
//   再生速度は DyingRate（1 = 実時間）。最後まで来たら最後のポーズのまま止まる。
// 調整値は Assets/Configs/GameOverDirector.json に保存し、Initialize で読み込む（Release でも読む。無いキーは初期値のまま）
//
// 勝利演出（2026-11-17）：同じクラスの 2 台目を、別の設定ファイル・ラベル画像・エディタ窓（Paths）で使う。
//   倒れるのはボス（Initialize に渡す骨格をボスのものに。プレイヤーと同じ骨格）、カメラは 2 台目の Rig、粉々は 2 台目の Shatter、色抜けは共用。
//   続行は巻き戻しの代わりに暗転（BeginExit → ExitAlpha が 0 → 1。IsExitFinished で呼び出し側がタイトルへ）。暗転は巻き戻しと同じく、どちらの台でも使える

export module Game.GameOverDirector;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <map>;
import <memory>;
import <optional>;
import <string>;
import <utility>;
import <vector>;

import Lumina;
import Lumina.CG3D.Struct;
import Game.GameOverGrade;
import Game.GameOverCamera;
import Game.GameOverShatter;

export namespace Game::GameOverDirector {
	struct Style {
		// 倒れるモーション
		std::string ClipName{};				// 空なら自動（動かす骨が一番多いクリップ）。エディタで選ぶとここに入って保存される
		float DyingRate{ 1.0f };			// クリップの秒 / 実時間の秒
		float DyingStartOffset{ 0.0f };		// クリップの頭を飛ばす秒（立ったままの部分を詰めたいとき）

		// 「Game Over」ラベルのフェードイン
		float ResultDelay{ 2.8f };
		float ResultFadeDuration{ 0.8f };

		// この時刻から再開（SPACE）できる
		float RestartDelay{ 3.5f };

		// 「Press Space to Continue」ラベルのフェードイン
		float PromptDelay{ 3.2f };
		float PromptFadeDuration{ 0.6f };

		// ラベルの位置（画面 1280x720 の中心 x = 640、y はここ）と、大きさ（画像の px に掛ける倍率）
		float GameOverY{ 290.0f };
		float GameOverScale{ 0.5f };
		float PromptY{ 430.0f };
		float PromptScale{ 0.5f };

		// スローモーション（序盤）。実時間の秒で、Hold の間は SlowScale 倍の速さ、そのあと Recover かけて 1 倍へ戻す
		float SlowScale{ 0.25f };		// 1 = スローなし
		float SlowHold{ 0.5f };
		float SlowRecover{ 0.6f };

		// 巻き戻し（SPACE で再開するとき）。実時間の秒で、演出を逆に再生して初期配置へ戻してから再開する
		float RewindDuration{ 1.6f };	// 0 なら巻き戻さない（すぐ再開）
		float RewindPower{ 2.0f };		// 1 = 一定の速さ、大きいほど「ゆっくり始まって加速」
		float RewindLabelFade{ 0.25f };	// 巻き戻しが始まったら、ラベルをこの秒数（実時間）でフェードアウトする（時計の戻りを待たない）

		// 巻き戻し中の回転ぼかし（画面の中心 or キャラのまわりに回す）。振れ幅は巻き戻しの頭で SpinIn の割合かけて立ち上がり、終わりの SpinOut の割合で消える
		float SpinAngleDeg{ 30.0f };	// 振れ幅（度）。負にすると逆回り。0 でなし
		float SpinFocus{ 0.0f };		// 回転の中心：0 = 画面の中心、1 = キャラの位置
		float SpinIn{ 0.25f };			// 巻き戻しの長さに対する割合
		float SpinOut{ 0.25f };

		// 暗転（勝利演出の続行。SPACE でタイトルへ戻るとき）。実時間の秒。ExitFadeDuration かけて黒へ、そのあと ExitHold 秒は黒のまま
		float ExitFadeDuration{ 1.0f };
		float ExitHold{ 0.3f };
	};

	// 設定ファイル・ラベル画像・エディタ窓の名前（2 台目を別の名前で動かすため）
	struct Paths {
		char const* ConfigPath{ "Assets/Configs/GameOverDirector.json" };
		char const* LabelImage{ "Assets/UI/Label.GameOver.png" };		// 結果のラベル（白文字・透明背景）
		char const* PromptImage{ "Assets/UI/Label.PressSpace.png" };	// 続行の案内
		char const* EditorTitle{ "GameOver Director" };
	};

	// エディタのプレビュー（Debug のみ）
	enum class Preview : int {
		Off,		// ゲームの進行に任せる
		Scrub,		// 時刻をエディタで決める（全トラックがその時刻で止まる）
	};

	class Director {
	public:
		// clips_ は LoadAnimationFile("Dying.gltf", ...) の結果。使う 1 本を取り出して持つ（clips_ は空になることがある）。
		// clipSkeleton_ は Dying.gltf の骨格（CreateSkeleton(Import("Dying.gltf").Root)）、playerSkeleton_ はプレイヤーの骨格。
		//   クリップの骨の名前を playerSkeleton_ の名前に付け替えるのに使う（階層が合わなければ付け替えず、状況をエディタに出す）
		// grade_ / camera_ は呼び出し側が持ち続けること（覚えるだけ）
		void Initialize(
			std::vector<Lumina::CG3D::MyAnimation>& clips_,
			Lumina::CG3D::Skeleton const& clipSkeleton_,
			Lumina::CG3D::Skeleton const& playerSkeleton_,
			Game::GameOverGrade::Effect& grade_,
			Game::GameOverCamera::Rig& camera_,
			Game::GameOverShatter::Effect& shatter_,
			Paths const& paths_ = Paths{}
		);

		void Start();						// 全トラックを頭から（色抜けとカメラの Start も呼ぶ）
		void Reset();						// 演出なし（色抜けとカメラの Reset も呼ぶ）
		void Update(float deltaTime_);		// 秒。Start していなければ何もしない

		auto IsActive() const -> bool { return Started_ || PreviewMode_ != Preview::Off; }
		auto IsRunning() const -> bool { return Started_ && PreviewMode_ == Preview::Off; }	// ゲームの進行で時間が進んでいる（スクラブ中は false）
		auto Elapsed() const -> float;		// 演出の経過秒（プレビューを含む）
		auto TotalTime() const -> float;	// 全トラックが出そろう秒

		// 倒れるモーション。HasDyingPose が true の間、プレイヤーのポーズはこのクリップのこの時刻にする
		auto HasDyingPose() const -> bool { return !Clips_.empty() && IsActive(); }
		auto DyingClip() const -> Lumina::CG3D::MyAnimation const* { return Clips_.empty() ? nullptr : &Clips_[ClipIndex_]; }
		auto DyingTime() const -> float;	// クリップの秒（終端で止まる）

		auto ResultAlpha() const -> float;	// 「Game Over」ラベルの不透明度。演出中でなければ 1
		auto PromptAlpha() const -> float;	// 「Press Space to Continue」ラベルの不透明度。演出中でなければ 1
		auto LabelSize() const -> Lumina::Float2 { return Lumina::Float2{ LabelImageSize_.x * Look.GameOverScale, LabelImageSize_.y * Look.GameOverScale }; }	// 「Game Over」の表示の大きさ（px）
		auto PromptSize() const -> Lumina::Float2 { return Lumina::Float2{ PromptImageSize_.x * Look.PromptScale, PromptImageSize_.y * Look.PromptScale }; }
		auto TimeScale() const -> float;	// 演出の時計の進む速さ（スロー中は 1 未満。演出が進んでいなければ 1）。dt に掛けて使う
		auto SlowEndTime() const -> float;	// スローが終わる、演出の時計の秒（スローなしなら 0）
		auto CanRestart() const -> bool;	// 再開できるか。演出中でなければ true

		// 巻き戻し。BeginRewind は演出が始まっていて（Start 済み・スクラブ中でない）巻き戻せるとき true。false なら呼び出し側がすぐ再開する
		auto BeginRewind() -> bool;
		auto IsRewinding() const -> bool { return Rewinding_; }
		auto RewindProgress() const -> float;		// 0（死んだ状態）→ 1（演出の時計が 0。初期配置）。巻き戻し中でなければ 0
		auto IsRewindFinished() const -> bool;		// 巻き戻しが終わった（時計が 0 に着いた）
		auto SpinRadians() const -> float;			// いまの回転ぼかしの振れ幅（ラジアン）。巻き戻し中でなければ 0。GradeFx_.SetSpin に渡す

		// 暗転。BeginExit は演出が始まっていて（Start 済み・スクラブ中でない）暗転できるとき true。false なら呼び出し側がすぐ次へ進む。
		// 暗転の間も演出の時計は進み続ける（カメラはゆっくり動いたまま暗くなる）
		auto BeginExit() -> bool;
		auto IsExiting() const -> bool { return Exiting_; }
		auto ExitAlpha() const -> float;			// 黒の不透明度（0..1）。暗転中でなければ 0
		auto IsExitFinished() const -> bool;		// 黒になって ExitHold 秒たった

		void DrawEditor();					// ImGui「GameOver Director」（Debug のみ）

		auto Load() -> bool;				// Look を設定ファイルから読む（あるキーだけ上書き）
		auto Save() -> bool;

	public:
		Style Look{};
		bool ForceLabels{ false };	// デバッグ用（エディタのチェック）：フェーズや時刻に関係なく、ラベルを不透明度 1 で描く。描画そのものが映るかを切り分ける
		char const* ConfigPath{ Paths{}.ConfigPath };		// Initialize の paths_ で差し替わる
		char const* LabelImage{ Paths{}.LabelImage };
		char const* PromptImage{ Paths{}.PromptImage };
		char const* EditorTitle{ Paths{}.EditorTitle };

	private:
		Game::GameOverGrade::Effect* Grade_{ nullptr };
		Game::GameOverCamera::Rig* Camera_{ nullptr };
		Game::GameOverShatter::Effect* Shatter_{ nullptr };

		std::vector<Lumina::CG3D::MyAnimation> Clips_{};	// Dying.gltf の全クリップ（骨の名前は付け替え済み）
		std::vector<uint32_t> Driven_{};					// クリップごとの、プレイヤーの骨格の骨を動かす数
		std::size_t ClipIndex_{ 0U };						// いま使っているクリップ
		std::string ClipStatus_{};

		bool Started_{ false };
		float Time_{ 0.0f };
		float RealTime_{ 0.0f };							// Start からの実時間の秒（スローの区間を決める）
		auto LabelRewindFade() const -> float;				// 巻き戻しでラベルを消す倍率（巻き戻し中でなければ 1）
		bool Rewinding_{ false };
		float RewindElapsed_{ 0.0f };						// 巻き戻しを始めてからの実時間の秒
		float RewindFromTime_{ 0.0f };						// 巻き戻しを始めたときの、演出の時計の秒
		bool Exiting_{ false };
		float ExitElapsed_{ 0.0f };							// 暗転を始めてからの実時間の秒
		Lumina::Float2 LabelImageSize_{ 1337.0f, 228.0f };	// ラベルの画像の px（Initialize で PNG のヘッダから読む。読めなければこの値）
		Lumina::Float2 PromptImageSize_{ 1062.0f, 134.0f };

		// エディタの状態（保存しない）
		Preview PreviewMode_{ Preview::Off };
		bool WasScrubbing_{ false };
		float ScrubTime_{ 0.0f };
		std::string Status_{};
	};
}
