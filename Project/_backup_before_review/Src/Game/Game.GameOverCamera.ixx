//////	//////	//////	//////	//////	//////
//	GameOverCamera							//
//////	//////	//////	//////	//////	//////
// ゲームオーバーのカメラワーク：キーフレームでカメラの各変数を決め、キーの間をイージングでつなぐ。
//
// 姿勢の表し方：注視点（プレイヤーの位置 ＋ 高さ TargetHeight）のまわりを回る球座標。
//   Distance     … 注視点からカメラまでの距離
//   TargetHeight … 注視点をプレイヤーの足元から上へずらす量（胸 ≒ 3.5、顔 ≒ 5 くらい。メッシュの背丈は約 5.7）。
//                  高く狙うほどプレイヤーは画面の下に寄る
//   Pitch        … 俯角（度）。正でカメラが注視点より上（見下ろし）
//   Yaw          … 回り込み（度）。演出を始めた瞬間のカメラの向きからのずれ。変えると注視点のまわりを横へ回る
//   Roll         … 画面の傾き（度）。符号は見た目で確認する
//   Fov          … 垂直画角（度）
//   Focus        … 注視点を「プレイヤー（0）」から「もう一人（1。SetOther で渡す点。勝利演出ではボス）」へ寄せる量。キーの間で補間する
// Style::YawFromOther が true のとき、Yaw の 0 は「もう一人 → プレイヤー」の向きの延長（プレイヤーの背中側から、もう一人を見る位置）。
//   先頭のキーの Yaw は、取り込んだカメラのその向きからのずれになる（Start の瞬間の位置で決める）
// 最初のキー（Time = 0）は、演出を始めた瞬間の「いまのカメラ」を自動で取り込む（値の欄は使わない）。
// そのため、つなぎ目が出ない。Yaw は 0、Roll は 0、TargetHeight は 0（足元）から始まる。
// イージングは「そのキーから次のキーへ」の区間に掛かる。Hold は次のキーの時刻まで値を保って、そこで切り替わる。
// 最後のキーを過ぎたら、最後の姿勢のまま止まる（Reset するまで）。
//
// 差し込み：UpdatePlayerCamera の直後に Apply を毎フレーム呼ぶ。演出中だけ View / VP / FOV を上書きする
//           （TrackingCamera::Position は触らない）。演出が終わったら、次の Apply で FOV を元に戻す。
// 時間：Start() からの秒。固定の dt（InGame の Update から Update(dt)。ポーズ中は進まない）
// 調整：ImGui「GameOver Camera」（Debug のみ）。タイムライン（クリックでキー追加・ドラッグで時刻・右クリックで削除）、
//       選択中のキーの値、真上と真横から見たカメラの軌道、スクラブ。Assets/Configs/GameOverCamera.json に保存する

export module Game.GameOverCamera;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <string>;
import <vector>;

import Lumina;
import Game.PlayerCamera;

export namespace Game::GameOverCamera {
	inline constexpr uint32_t MaxKeys{ 16U };
	inline constexpr uint32_t MinKeys{ 2U };

	enum class Ease : int {
		Hold,			// 次のキーの時刻まで保って、そこで切り替わる
		Linear,
		InQuad,
		OutQuad,
		InOutQuad,
		InCubic,
		OutCubic,
		InOutCubic,
		InOutSine,
		OutQuart,
		SmootherStep,
		Count,
	};

	struct Key {
		float Time{ 0.0f };				// Start からの秒（先頭は 0 固定）
		float Distance{ 43.0f };
		float TargetHeight{ 0.0f };
		float PitchDeg{ 20.0f };
		float YawDeg{ 0.0f };			// 演出開始時のカメラの向きからのずれ
		float RollDeg{ 0.0f };
		float FovDeg{ 25.8f };
		Ease EaseToNext{ Ease::Linear };
		float Focus{ 0.0f };			// 注視点：0 = プレイヤー（camera_.Target）、1 = SetOther の点
	};

	struct Style {
		// カメラシェーク：SetShake で渡された強さ（0..1。InGame はチャンネルスプリットの強さ = 包絡 × 鼓動）に比例して、
		// カメラを画面の縦横へ ShakeAmount（ワールドの長さ）、画面の傾きを ShakeRollDeg（度）だけ揺らす。揺れ方は ShakeFrequency（Hz）の細かい波
		// （演出の時計の上の秒なので、スローの間は揺れもゆっくりになる）。ShakeAmount と ShakeRollDeg が両方 0 ならなし
		float ShakeAmount{ 0.15f };
		float ShakeRollDeg{ 0.5f };
		float ShakeFrequency{ 22.0f };

		// true なら Yaw の基準を「もう一人 → プレイヤー」の向きにする（SetOther を呼んでいなければ false と同じ）
		bool YawFromOther{ false };

		// 先頭（Time = 0）の値の欄は使わない（実カメラを取り込む）。おすすめの初期値：
		//   0 → 1.8 秒：ぐっと寄って、俯瞰を強め、胸〜顔へ注視点を上げ、少し回り込み、傾ける（減速しながら）
		//   1.8 → 4.0 秒：ほとんど止まる手前で、ゆっくり寄り続ける（完全には止めない）
		std::vector<Key> Keys{
			Key{ 0.0f, 43.0f, 0.0f, 20.0f, 0.0f, 0.0f, 25.8f, Ease::OutQuad },
			Key{ 1.8f, 16.0f, 3.6f, 29.0f, 13.0f, 3.5f, 23.0f, Ease::Linear },
			Key{ 4.0f, 12.5f, 4.1f, 31.0f, 22.0f, 5.0f, 22.0f, Ease::Linear },
		};
	};

	// 1 時刻の姿勢（Yaw は開始時の向きからのずれ）
	struct Pose {
		float Distance;
		float TargetHeight;
		float PitchDeg;
		float YawDeg;
		float RollDeg;
		float FovDeg;
		float Focus;
	};

	// エディタのプレビュー（Debug のみ）
	enum class Preview : int {
		Off,		// ゲームの進行に任せる
		Scrub,		// 時間をスライダーで動かす（いつでも、いまのカメラを先頭として取り込み直す）
	};

	class Rig {
	public:
		// 設定ファイルを読む。configPath_ / editorTitle_ を渡すと、別の設定ファイル・別のエディタ窓で動く（勝利演出用の 2 台目など）
		void Initialize(char const* configPath_ = nullptr, char const* editorTitle_ = nullptr);

		void Start();						// 演出を頭から始める（始まった瞬間のカメラを取り込む）
		void Reset();						// 演出なし（元のカメラ）に戻す
		void Update(float deltaTime_);		// 秒。Start していなければ何もしない

		auto IsActive() const -> bool { return Started_ || PreviewMode_ != Preview::Off; }
		auto TotalTime() const -> float;	// 最後のキーの時刻

		// UpdatePlayerCamera の直後に毎フレーム呼ぶ。演出中だけカメラを上書きする
		void Apply(Game::TrackingCamera& camera_);

		void DrawEditor();					// ImGui「GameOver Camera」（Debug のみ）

		// 外の時計（Game.GameOverDirector）から時刻を指定して止める／解除する。解除すると自分の時計に戻る
		void ScrubTo(float seconds_) { PreviewMode_ = Preview::Scrub; ScrubTime_ = seconds_; }
		void ClearScrub() { PreviewMode_ = Preview::Off; }
		void SetTime(float seconds_) { if (Started_) { Time_ = seconds_; } }	// 巻き戻し用：自分の時計を直接決める（Start していなければ何もしない）

		// シェークの強さ（0..1）。毎フレーム、Apply の前に呼ぶ（呼ばなければ前の値のまま）
		void SetShake(float intensity_) { Shake_ = intensity_; }

		// 「もう一人」の足元の位置（Focus と YawFromOther が使う）。毎フレーム、Apply の前に呼ぶ
		void SetOther(Lumina::Float3 const& other_) { Other_ = other_; HasOther_ = true; }

		auto Load() -> bool;				// Look を設定ファイルから読む（あるキーだけ上書き）
		auto Save() -> bool;

	public:
		Style Look{};
		char const* ConfigPath{ "Assets/Configs/GameOverCamera.json" };
		char const* EditorTitle{ "GameOver Camera" };

	private:
		auto Elapsed() const -> float;
		auto StartPose() const -> Pose;		// 取り込んだ「いまのカメラ」の姿勢（Yaw / Roll / TargetHeight = 0）
		void Capture(Game::TrackingCamera& camera_);

	private:
		bool Started_{ false };
		float Time_{ 0.0f };

		// 取り込んだ開始時のカメラ（演出中は固定）。プレイヤーの位置は毎フレーム camera_.Target から取る
		bool Captured_{ false };
		float StartDistance_{ 43.0f };
		float StartPitchDeg_{ 20.0f };
		float StartYawRad_{ 0.0f };			// ワールドでの向き（Z+ から X+ へ測る）
		float StartFovDeg_{ 25.8f };
		float BaseYawRad_{ 0.0f };			// Yaw の 0 の向き（ワールド）。既定は StartYawRad_、YawFromOther なら「もう一人 → プレイヤー」
		float StartYawOffsetDeg_{ 0.0f };	// 先頭のキーの Yaw（BaseYawRad_ からのずれ）
		Lumina::Float3 Other_{ 0.0f, 0.0f, 0.0f };
		bool HasOther_{ false };

		bool IsFovModified_{ false };		// camera_ の FOV を変えたままか（元に戻す必要があるか）
		float BaseFovRad_{ 0.45f };			// 変える前の FOV
		float Shake_{ 0.0f };					// シェークの強さ（0..1）

		// エディタの状態（保存しない）
		Preview PreviewMode_{ Preview::Off };
		float ScrubTime_{ 0.0f };
		int Selected_{ 0 };
		int DraggingKey_{ -1 };
		float TimelineRange_{ 4.5f };		// タイムラインの右端の秒（ドラッグ中は変えない）
		std::string Status_{};
	};
}
