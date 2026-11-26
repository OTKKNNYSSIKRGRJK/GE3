module;

#include<cstdint>
#include<memory>
#include<vector>
#include<string>
#include<unordered_map>

export module Game.Scene.Title : Impl;

import Lumina;
import Game.KeyConfigScreen;
import Lumina.FirePillar;
import Lumina.MagicCircle;
import Lumina.ShowcaseCharacter;

namespace Game::Scene::Impl {
	// 火柱 1 本分の編集用データ。円柱ごとに Description を丸ごと持つ
	struct FirePillarEntry {
		char Name[32]{ "Pillar" };
		bool Enabled{ true };
		Lumina::FlamePillar::Description Desc{};
		Lumina::Float3 Offset{ 0.0f, 0.0f, 0.0f };	// キャラの足元からのオフセット
		int Seed{ 0 };
		float IntensityScale{ 1.0f };
	};

	//----	------	------	------	------	----//
	// パーティクル（本編の ParticleSystem<T> をそのまま使う）
	//----	------	------	------	------	----//

	// 本編の Particle に、タイトル用の追加情報を足したもの
	struct TitleParticle : public Particle {
		float MaxLife{ 1.0f };
		float BaseSize{ 1.0f };
		float BaseAlpha{ 1.0f };
		float Seed{ 0.0f };
		Lumina::Float3 ColorA{ 1.0f, 1.0f, 1.0f };	// 発生直後の色（リニア）
		Lumina::Float3 ColorB{ 1.0f, 1.0f, 1.0f };	// 消える直前の色（リニア）
	};

	// 本編の AmbientSparkles と同じ動き（Life・移動量はフレーム単位の係数を 60fps 換算で流用）
	// 色も本編と同じ式（HSV 由来の値をそのまま出力。リニア化しない）
	struct SparkleEmitter {
		bool Enabled{ true };
		float Rate{ 60.0f };			// 個/秒（本編は毎フレーム 1 回 ≒ 60）
		float AreaRadius{ 14.0f };		// 発生範囲の半径（本編は 100 の円 ∩ ±40 の正方形）
		float HeightMax{ 2.0f };
		float Size{ 1.5f };				// 本編 1.5
		float Alpha{ 0.1f };			// 本編 0.1
	};

	// 本編の PlayerEffects（プレイヤーのまわりを巡る光）と同じ動き
	struct AuraEmitter {
		bool Enabled{ true };
		float Rate{ 120.0f };			// 個/秒（本編は毎フレーム 2 個）
		float OrbitRadius{ 1.5f };		// 本編 1.5
		float OrbitHeight{ 1.0f };		// 上下の振れ幅（本編 1.0）
		float HeightOffset{ 0.0f };
		float Size{ 0.7f };				// 本編 0.7
		float Alpha{ 0.15f };			// 本編 0.15
		bool HueCycle{ true };			// 本編の「色相が回る」色。false なら Color（sRGB）
		Lumina::Float3 Color{ 1.0f, 0.6f, 0.3f };
	};

	// 火の粉：火柱の根元から旋回しながら昇り、速度方向に引き伸ばす（秒単位）
	struct EmberEmitter {
		bool Enabled{ true };
		float Rate{ 40.0f };			// 個/秒
		float LifeMin{ 1.0f };
		float LifeMax{ 2.2f };
		float SpawnRadius{ 2.5f };		// 足元からの距離
		float SpawnRadiusJitter{ 1.0f };
		float SpawnHeight{ 0.5f };		// 0 〜 この高さで発生
		float RiseSpeed{ 2.5f };		// 上昇速度（/秒）
		float RiseJitter{ 1.0f };
		float Swirl{ 0.8f };			// 軸まわりの旋回（rad/秒、負で逆回り）
		float Turbulence{ 3.0f };		// 横方向のランダムな加速
		float Drag{ 1.0f };				// 横速度の減衰（/秒）
		float Size{ 0.12f };
		float Stretch{ 4.0f };			// 画面上の進行方向への伸び（1 = 伸ばさない）
		float Alpha{ 1.0f };
		Lumina::Float3 ColorHot{ 1.00f, 0.85f, 0.50f };	// 発生直後（sRGB）
		Lumina::Float3 ColorCool{ 1.00f, 0.25f, 0.05f };	// 消える直前（sRGB）
		int Atlas{ 1 };					// Particles.png のセル番号（0..7）。1 = 縦長の筋
	};

	// 魔法陣が放つ光：有効なレイヤーのリング上から立ち昇る光の筋（秒単位）
	struct CircleLightEmitter {
		bool Enabled{ true };
		float Rate{ 30.0f };			// 個/秒（全レイヤー合計。円周の長さで振り分ける）
		float LifeMin{ 1.5f };
		float LifeMax{ 3.0f };
		float RiseSpeed{ 1.2f };
		float RiseJitter{ 0.6f };
		float Sway{ 0.3f };				// 横揺れの速さの振れ幅
		float Size{ 0.15f };
		float Stretch{ 6.0f };			// ワールドの上方向への伸び
		float Alpha{ 0.6f };
		float Whiten{ 0.3f };			// レイヤーの色をどれだけ白に寄せるか
		bool RespectReveal{ true };		// 描き込み途中のレイヤーは、描かれた所からだけ出す
		int Atlas{ 1 };
	};

	// 武器から放つ光：刀身の軸に沿って、動かない大きめの光の粒を置いていく（秒単位）
	// 置いた場所に留まるので、武器が動くと軌跡のように残る
	struct WeaponGlowEmitter {
		bool Enabled{ true };
		float Rate{ 60.0f };			// 個/秒
		float LifeMin{ 0.4f };
		float LifeMax{ 0.9f };
		float SpanBegin{ 0.15f };		// 軸の根元 0 → 先端 1 のどこからどこまでに出すか
		float SpanEnd{ 1.0f };
		float RadialJitter{ 0.6f };		// 軸からの散らばり（武器の太さに対する倍率）
		float SizeMin{ 0.6f };
		float SizeMax{ 1.4f };
		float Grow{ 0.3f };				// 寿命の間にどれだけ大きくなるか（0 = 変わらない）
		float Alpha{ 0.35f };
		bool HueFlow{ true };			// true: 色相を刀身方向と時間で流す（仮のタマムシ色）/ false: Color
		Lumina::Float3 Color{ 0.60f, 0.85f, 1.00f };	// sRGB
		float HueBase{ 160.0f };		// 度
		float HueRange{ 140.0f };		// 根元 → 先端での色相の変化（度）
		float HueSpeed{ 40.0f };		// 度/秒
		float Saturation{ 0.7f };
		float Value{ 1.0f };
		int Atlas{ 0 };					// 0 = ぼかし点
	};

	//----	------	------	------	------	----//
	// カメラ・TimeScale の往復（sway）
	//----	------	------	------	------	----//

	// 往復の端点（Hero / Overview）
	struct CameraPose {
		float Pitch{ 0.0f };
		float Distance{ 16.0f };
		float TargetY{ 3.0f };
		float FovY{ 0.8f };
	};

	// 1 チャンネルぶんの往復。重み 0 → 1 → 0 を Period 秒で 1 往復する
	struct SwayChannel {
		bool Enabled{ true };
		float Period{ 12.0f };			// 1 往復の秒数
		float Phase{ 0.0f };			// 周期に対する開始位置 0..1（0 = 端点 A から）
		float Dwell{ 0.5f };			// 0: 正弦のまま / 1: 両端で長めに留まる（smoothstep を重ねる）
	};

	struct CameraSway {
		bool Enabled{ false };
		CameraPose Hero{ -0.05f, 13.0f, 3.5f, 0.75f };
		CameraPose Overview{ 0.45f, 18.0f, 2.0f, 0.8f };
		// 周期をずらしておくと、組み合わせがすぐには繰り返さない
		SwayChannel Pitch{ true, 14.0f, 0.0f, 0.5f };
		SwayChannel Distance{ true, 19.0f, 0.25f, 0.5f };
		SwayChannel TargetY{ true, 11.0f, 0.5f, 0.3f };
		SwayChannel FovY{ true, 23.0f, 0.1f, 0.3f };
	};

	struct TimeScaleSway {
		bool Enabled{ false };
		float Min{ 0.35f };
		float Max{ 1.0f };
		SwayChannel Channel{ true, 9.0f, 0.0f, 0.6f };	// 重み 0 = Max / 1 = Min（Phase 0 で通常速度から始まる）
	};

	//----	------	------	------	------	----//
	// メニュー（Start / Exit）
	//----	------	------	------	------	----//

	// 見た目と演出の調整値（画面は 1280x720、Y は下向き）
	struct TitleMenuStyle {
		float RightX{ 1240.0f };			// ラベルの右端の X（画像は文字が右寄せなので右端基準で置く）
		float FirstY{ 540.0f };				// 1 項目目の中心 Y
		float Spacing{ 80.0f };				// 項目の間隔
		float Width{ 360.0f };				// 表示サイズ（画像は 720x180）
		float Height{ 90.0f };
		float UnselectedAlpha{ 0.35f };
		float UnselectedScale{ 0.9f };
		float SelectedOffsetX{ -12.0f };	// 選択中は少し左へ出す
		float Smoothing{ 14.0f };			// 選択の切り替えの追従の速さ（/秒）
		float FadeInDuration{ 0.8f };		// 起動時の黒からのフェードイン
		float BlinkDuration{ 0.45f };		// 決定後の点滅
		float BlinkFrequency{ 10.0f };		// Hz
		float FadeOutDuration{ 0.6f };		// 点滅後の黒へのフェードアウト
	};

	// タイトルロゴ（Assets/UI/Label.Title.png。今は仮画像。名前が決まったら画像を差し替えるだけ）
	struct TitleLogoStyle {
		bool Enabled{ true };
		float X{ 80.0f };					// アンカー位置（画面 1280x720、Y は下向き）
		float Y{ 200.0f };
		float AnchorX{ 0.0f };				// 0 = 左端基準（仮画像は左寄せ）/ 0.5 = 中央 / 1 = 右端
		float AnchorY{ 0.5f };
		float Width{ 720.0f };				// 表示サイズ（仮画像は 1440x360 なので半分）
		float Height{ 180.0f };
		float Alpha{ 1.0f };
		float Delay{ 0.3f };				// メニューのフェードイン開始からの遅れ
		float FadeDuration{ 1.2f };			// 透明 → 不透明
		float RiseDistance{ 16.0f };		// 下から浮き上がる距離（px）
	};

	// ラベル（ロゴ / Start / Exit）に掛ける、明滅するグロー
	struct TitleGlowStyle {
		bool Enabled{ true };
		Lumina::Float3 Color{ 1.00f, 0.72f, 0.40f };	// sRGB
		float Intensity{ 0.8f };
		float RadiusPx{ 14.0f };			// にじみの半径（画面 px）
		float PulseDepth{ 0.6f };			// 0: 明滅しない / 1: 一番暗いときに 0 まで落ちる
		float PulseFrequency{ 0.5f };		// Hz
		float PhaseStep{ 0.15f };			// ラベルごとの位相のずれ（周期に対する割合。上から順に少しずつ遅れる）
	};

	//----	------	------	------	------	----//
	// Outro（Start 決定 → 溜め → 振り抜き → 命中 → 割れる → 白 → 次のシーンへ）
	// Outro 開始からの秒 t を 1 本のタイムラインとして、各要素が t を読んで動く。
	// 時計は Pause / TimeScale の影響を受けない固定 1/60 秒（メニューと同じ）
	//----	------	------	------	------	----//

	// 各区間の終わりの時刻（Outro 開始からの秒）。前後が逆になった値は使う側で揃える
	struct OutroTimeline {
		float ChargeEnd{ 1.30f };			// 区間 1 溜め     → 区間 2 振り抜き
		float SwingEnd{ 1.55f };			// 区間 2 振り抜き → 区間 3 命中
		float ImpactEnd{ 1.65f };			// 区間 3 命中     → 区間 4 割れる
		float SplitEnd{ 2.70f };			// 区間 4 割れる   → 区間 5 白
		float WhiteEnd{ 3.20f };			// 区間 5 白       → 次のシーンへ
	};

	// カメラ：開始時のポーズから Approach へドリーイン（区間 1）→ さらに寄る（区間 2）→ シェイク（区間 3 の頭から）
	struct OutroCameraStyle {
		CameraPose Approach{ 0.10f, 6.5f, 3.0f, 1.00f };	// 区間 1 の終わりのポーズ（FovY は広げる）
		float YawOffset{ 2.2f };			// 区間 1 の終わりの Yaw（キャラの Yaw からの相対, rad）。側面〜背後へ
		float PushDistanceScale{ 0.8f };	// 区間 2 で Distance に掛ける
		float PushFovYAdd{ 0.12f };			// 区間 2 で FovY に足す
		float ShakeAmplitude{ 0.06f };		// 注視点をずらす量（ワールド単位）
		float ShakeDuration{ 0.25f };		// 区間 3 の頭から減衰しきるまでの秒数
		float ShakeFrequency{ 24.0f };		// Hz
	};

	// 魔法陣：回転を加速しつつ、外周のレイヤーから順に Reveal を戻して内側へ収束。区間 4 の頭で消す
	struct OutroMagicCircleStyle {
		float SpinMultiplier{ 6.0f };		// 区間 1 の終わりの回転速度の倍率（2 乗で加速し、以降は維持）
		float IntensityBoost{ 1.8f };		// 区間 1 の終わりの明るさの倍率
		float CollapseStart{ 0.25f };		// 外周のレイヤーが消え始める時刻（Outro 開始からの秒）
		float CollapseDuration{ 0.55f };	// 1 レイヤーの Reveal が 1 → 0 になる秒数
		float CollapseStagger{ 0.2f };		// 外周 → 内側の時間差（半径の大きい順）
		float ShrinkAmount{ 0.3f };			// 消えるまでに半径をどれだけ縮めるか（0..1）
		float FadeOutDuration{ 0.2f };		// 区間 4 の頭から、残っている分も含めて消す秒数
	};

	// 火柱：区間 1 で強度・高さを上げ、区間 2 以降は最大のまま
	struct OutroFirePillarStyle {
		float IntensityPeak{ 2.5f };		// IntensityScale に掛ける倍率（最大時）
		float HeightPeak{ 1.6f };			// Height に掛ける倍率（最大時）
		float LightPeak{ 2.0f };			// キャラへの照り返しの強さに掛ける倍率（最大時）
		float RiseCurve{ 2.0f };			// 上がり方（1 = 一定、大きいほど終盤で一気に上がる）
	};

	// ロゴ・メニュー：Outro の頭で、フェードインを逆再生して消す
	struct OutroUIStyle {
		float FadeDuration{ 0.35f };
		float SelectedGlowBoost{ 3.0f };	// 消え始めに、選んだ項目のグローを何倍まで上げるか
	};

	// キャラのアニメ：区間 1 は ChargeClip、区間 2 以降は SwingClip。どちらも t からクリップ上の時刻を決める
	// （同じクリップを指定すれば、切り替えなしで溜め → 振り抜きを通しで再生できる）
	struct OutroAnimationStyle {
		char ChargeClip[32]{ "ATK1" };
		float ChargeStart{ 0.0f };			// 区間 1 の頭のクリップ上の時刻（秒）
		float ChargeSpeed{ 0.6f };			// 区間 1 の再生速度（クリップ秒 / 実秒）
		char SwingClip[32]{ "ATK1" };
		float SlashTime{ 1.23f };			// 区間 2 の終わり（命中）に合わせる、SwingClip 上の時刻（秒）
		float SwingSpeed{ 1.8f };			// 区間 2 の再生速度（区間 2 の頭の時刻は SlashTime から逆算）
		float HitStopDuration{ 0.1f };		// 命中後、アニメの時間だけ止める秒数（Outro の時計は止めない）
		float FollowSpeedScale{ 0.5f };		// ヒットストップ明けの再生速度（SwingSpeed に掛ける）
	};

	// 武器の光：区間 1 で段階的に強め（粒数・Span・大きさ・白熱寄りの色）、先端にフレア
	struct OutroWeaponGlowStyle {
		int Steps{ 3 };						// 区間 1 で何段階に分けて強めるか（0 = 連続）
		float StartFlash{ 0.6f };			// Outro の頭の一瞬の発光（クリップの切り替わりを隠す）
		float StartFlashDuration{ 0.25f };
		float RateScale{ 4.0f };			// 最大時に Rate に掛ける倍率
		float SpanBegin{ 0.0f };			// 最大時の SpanBegin（根元側へ広げる）
		float SizeScale{ 1.5f };			// 最大時に SizeMin / SizeMax に掛ける倍率
		float AlphaScale{ 2.0f };			// 最大時に Alpha に掛ける倍率
		float WhiteHot{ 0.8f };				// 最大時に色をどれだけ白へ寄せるか

		// 先端のフレア（Particles.png の 4 番 = 四芒星をビルボードで 1 枚）
		float FlareChargeLevel{ 0.35f };	// 区間 1 の終わりの強さ（区間 2・3 で 1、区間 4 の頭から消える）
		float FlareFadeOutDuration{ 0.3f };
		float FlareSize{ 2.5f };
		float FlareAlpha{ 1.0f };
		Lumina::Float3 FlareColor{ 1.00f, 0.95f, 0.85f };	// sRGB
		float FlareFlickerDepth{ 0.35f };	// 明滅の深さ（0 = 明滅しない）
		float FlareFlickerFrequency{ 12.0f };	// Hz
		float FlareSpin{ 1.5f };			// rad/s
		float FlareCameraOffset{ 0.3f };	// カメラ側へ寄せる距離（刀身に半分埋もれないように）
		int FlareAtlas{ 4 };
	};

	// 画面に重ねる色（メニューのフェード用スプライトを一番手前に重ねる）
	// 命中の白フラッシュと、区間 5（フィナーレ）のフェードアウトの、濃い方を使う
	// フィナーレ：区間 4 の終わり → WhiteEnd − FadeHold で FadeColor へ。同じ間にズームブラー・色収差は Finale の値へ強まる
	struct OutroScreenStyle {
		Lumina::Float3 FadeColor{ 1.00f, 1.00f, 1.00f };	// フェードアウトの色（sRGB）
		float FadeCurve{ 1.5f };			// フェードの進み方（1 = 一定、大きいほど終盤で一気に）
		float FadeHold{ 0.1f };				// 最後に FadeColor のまま止めておく秒数
		float FlashPeak{ 0.8f };			// 命中（区間 3 の頭）の白フラッシュの濃さ
		float FlashAttack{ 0.02f };			// 命中からピークまでの秒数
		float FlashDecay{ 0.2f };			// ピークから消えるまでの秒数
	};

	// 放射光線：武器の先端のスクリーン位置から放射状に伸びる光（スクリーン空間・加算）
	// 区間 1 は武器の光に合わせて弱く、区間 2 で伸びて強まり、命中でピーク → 減衰
	struct OutroRaysStyle {
		bool Enabled{ true };
		float ChargeLevel{ 0.15f };			// 区間 1 の終わりの強さ（武器の光の段階に合わせて上がる）
		float Peak{ 1.0f };					// 命中の瞬間の強さ
		float DecayDuration{ 0.5f };		// 命中から消えるまでの秒数
		float LengthCharge{ 0.25f };		// 区間 1 の長さ（画面の高さに対する割合）
		float LengthPeak{ 1.2f };			// 命中の瞬間の長さ
		float LengthAfter{ 1.6f };			// 消えるまでに伸びる長さ
		int RayCount{ 40 };					// 一周あたりの光線の数
		float Sharpness{ 6.0f };			// 大きいほど細く・まばら
		float RotateSpeed{ 0.3f };			// 模様の回転（rad/s）
		float FlickerSpeed{ 8.0f };			// 光線の入れ替わりの速さ（/秒）
		Lumina::Float3 Color{ 1.00f, 0.85f, 0.60f };		// sRGB
		Lumina::Float3 CoreColor{ 1.00f, 0.97f, 0.90f };	// sRGB
		float CoreRadius{ 0.05f };			// 中心の光の半径（画面の高さに対する割合）
		float CoreIntensity{ 1.5f };
	};

	// ポストの効果 1 つ分の強さの流れ。各区間の終わりの値を補間する
	struct OutroPostCurve {
		float Base{ 0.0f };					// Outro 外と、Outro の頭（t = 0）
		float Charge{ 0.0f };				// 区間 1 の終わり
		float Hit{ 0.0f };					// 命中（区間 2 の終わり）
		float Split{ 0.0f };				// 区間 4 の終わり
		float Finale{ 0.0f };				// フィナーレの終わり（WhiteEnd − FadeHold。以降はこのまま）
	};

	// ポスト（ズームブラー・色収差・簡易ブルーム）。Title 用 Canvas → バックバッファ
	struct OutroPostStyle {
		OutroPostCurve ZoomBlur{ 0.0f, 0.02f, 0.10f, 0.25f, 0.6f };		// 中心へ向かってぼかす長さ（中心までの距離に対する割合）
		OutroPostCurve Chromatic{ 0.0f, 0.002f, 0.010f, 0.020f, 0.06f };	// 中心から離れた所での RGB のずれ（UV）
		OutroPostCurve Bloom{ 0.0f, 0.4f, 1.5f, 2.0f, 2.0f };			// ブルームの強さ
		float FinaleCurve{ 2.0f };			// フィナーレでの強まり方（1 = 一定、大きいほど終盤で一気に）
		int ZoomTaps{ 16 };					// ズームブラーのタップ数（1..32）
		int BloomTaps{ 16 };				// ブルームのタップ数（1..32）
		float BloomRadiusPx{ 32.0f };
		float BloomThreshold{ 0.6f };		// これより明るい分（リニア）だけにじませる
	};

	// 割れ目（方式 β：斬撃線で画面を 2 枚に割ってずらし、割れ目を白熱グラデーションで埋める。Assets/Title/TitleCrack.hlsli）
	// 命中（区間 3 の頭）で斬撃線が走り、区間 4 で開く。ズームブラー・光線の中心も、命中から割れ目へ移る
	struct OutroCrackStyle {
		bool Enabled{ true };
		float PointU{ 0.5f };				// 斬撃線が通る点（画面 UV、左上が 0,0）
		float PointV{ 0.5f };
		float Angle{ -0.35f };				// 斬撃線の向き（rad。画面の右向きから反時計回り）
		float LineAppear{ 0.06f };			// 命中から、斬撃線が端から端まで走りきる秒数
		float LineWidth{ 0.004f };			// 斬撃線の太さ（画面の高さ = 1）
		float LineIntensity{ 3.0f };
		float GapMax{ 0.12f };				// 区間 4 の終わりの開き（画面の高さ = 1）
		float OpenCurve{ 1.5f };			// 開き方（1 = 一定、大きいほど後半で一気に開く）
		float RotateMax{ 0.03f };			// 区間 4 の終わりに、片側の破片を回す角度（rad）
		float GlowWidth{ 0.03f };			// 割れ目のふちから破片側へにじむ光の幅（画面の高さ = 1）
		float GlowIntensity{ 1.2f };
		float LightIntensity{ 3.0f };		// 割れ目の光の明るさ（1 を超えた分はブルームでにじむ）
		Lumina::Float3 CoreColor{ 1.00f, 1.00f, 1.00f };	// 割れ目の中心（sRGB）
		Lumina::Float3 EdgeColor{ 1.00f, 0.78f, 0.45f };	// 割れ目のふち（sRGB）
		float CenterMove{ 0.15f };			// 命中から、ズームブラー・光線の中心が先端 → 割れ目へ移る秒数

		// 割れ目からの放射光線（見た目は Rays の設定を使う。割れ目より手前、ポストの後に描く）
		float RaysPeak{ 1.0f };
		float RaysRise{ 0.2f };				// 区間 4 の頭から最大になるまでの秒数
		float RaysLengthStart{ 0.3f };
		float RaysLengthEnd{ 1.6f };		// 区間 4 の終わりの長さ
	};

	struct OutroSettings {
		// 最後まで再生したら、次のシーンへ進まずにタイトルのイントロ（フェードイン）へ戻る（演出を見せる用）
		bool ReturnToTitle{ true };
		OutroTimeline Timeline{};
		OutroCameraStyle Camera{};
		OutroMagicCircleStyle MagicCircle{};
		OutroFirePillarStyle FirePillar{};
		OutroUIStyle UI{};
		OutroScreenStyle Screen{};
		OutroAnimationStyle Animation{};
		OutroWeaponGlowStyle WeaponGlow{};
		OutroRaysStyle Rays{};
		OutroPostStyle Post{};
		OutroCrackStyle Crack{};
	};

	// 小さな状態機械（State パターンのコンテキスト側の入れ物。Title のメニューと Outro で使う）。
	//   状態は Enter(Owner&) を持つクラス。次の状態は状態自身が決めて Change で頼む。
	//   状態の処理（Run の中）で Change されたら、その処理が終わってから切り替える（実行中の状態を消さないため）。
	//   それ以外（巻き戻し・ImGui など、外から）の Change はすぐ切り替わる。切り替えのたびに新しい状態の Enter を呼ぶ
	template<typename Owner, typename State>
	class StateMachine {
	public:
		explicit StateMachine(std::unique_ptr<State> initial_) noexcept : Current_{ std::move(initial_) } {}

		void Change(Owner& owner_, std::unique_ptr<State> next_) {
			Pending_ = std::move(next_);
			if (Depth_ == 0) { Flush(owner_); }
		}

		// func_(State&) を今の状態に対して呼ぶ
		template<typename Func>
		void Run(Owner& owner_, Func&& func_) {
			++Depth_;
			func_(*Current_);
			--Depth_;
			if (Depth_ == 0) { Flush(owner_); }
		}

		auto Current() const noexcept -> State const& { return *Current_; }

	private:
		void Flush(Owner& owner_) {
			++Depth_;		// Enter の中の Change も後回し（このループで続けて切り替える）
			while (Pending_) {
				Current_ = std::move(Pending_);
				Current_->Enter(owner_);
			}
			--Depth_;
		}

		std::unique_ptr<State> Current_{ nullptr };
		std::unique_ptr<State> Pending_{ nullptr };
		int Depth_{ 0 };
	};

	export class Title {
	private:
		template<typename...ArgTypes>
		void Startup(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void InBattle(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Win(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Lose(typename ArgTypes const&...args_);

	public:
		template<typename...ArgTypes>
		void Update(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Render(typename ArgTypes const&...args_);

	public:
		template<typename...ArgTypes>
		void Initialize(typename ArgTypes const&...args_);

		explicit Title(Lumina::SceneManager& sceneManager_);
		virtual ~Title();

		// タイトルに入ったとき（SceneManager が切り替えるとき。外側の Game::Scene::Title::OnEnter から）。
		// InGame から戻ってきたら Outro を巻き戻し、メニューを黒からのフェードインでやり直す
		void OnEnter();

	private:
		// シーンの持ち主（コンテキスト）。次のシーン・終了はこれに頼む（2026-11-26：グローバル変数 Game::Scene::Flag の代わり。実装は UI.cpp）
		Lumina::SceneManager* SceneManager_{ nullptr };
		void RequestStartGame();		// InGame へ
		void RequestQuit();				// アプリの終了（main.cpp がウィンドウを閉じる）

	private:
		//----	------	------	------	------	----//
		// Settings（ImGui での調整結果の保存・読み込み。実装は Game.Scene.Title.Settings.cpp）

		static constexpr char const* SettingsFilePath{ "Assets/Configs/TitleScene.json" };

		bool SaveSettings() const;
		bool LoadSettings();

		// 保存と読み込みで同じキー一覧を共有する（Archive = Writer / Reader、Self = Title / Title const）
		template<typename Archive, typename Self>
		static void VisitSettings(Archive& ar_, Self& self_);

		std::string SettingsStatus_{};		// ImGui に出す直近の結果

	private:
		//----	------	------	------	------	----//
		// Time

		bool IsPaused_{ false };
		float TimeScale_{ 1.0f };
		float Time_{ 0.0f };			// 魔法陣の回転・明滅に使う
		float SwayClock_{ 0.0f };		// 往復用の時計（TimeScale の影響を受けない実時間。Pause では止まる）
		TimeScaleSway TimeScaleSway_{};

		//----	------	------	------	------	----//
		// FirePillar prototype（キャラの足元基準。並び順 = 描画順で、外側 → 内側に並べる）

		Lumina::FlamePillar::Renderer FirePillarRenderer_{};
		std::vector<FirePillarEntry> FirePillars_{};
		std::vector<Lumina::FlamePillar::Instance> FirePillarInstances_{};	// 毎フレーム FirePillars_ から作り直す
		std::vector<Lumina::FlamePillar::Description> FirePillarRenderDescs_{};	// 描画用の Desc のコピー（Outro の駆動はこちらに掛ける。Instance::Desc はここを指す）
		float FirePillarAge_{ 0.0f };

		//----	------	------	------	------	----//
		// Character（プレイヤー＋武器、フォワード）

		Lumina::ShowcaseCharacter Character_{};
		Lumina::ShowcaseLighting Lighting_{};
		bool LinkFireToPillar_{ true };		// 照り返しの色・高さを、有効な最初の火柱の Ramp.Mid / Height に合わせる
		float FireLightIntensity_{ 1.6f };

		//----	------	------	------	------	----//
		// MagicCircle prototype

		Lumina::MagicCircle::Renderer MagicCircleRenderer_{};
		std::vector<Lumina::MagicCircle::Layer> MagicCircleLayers_{};
		std::vector<Lumina::MagicCircle::Layer> MagicCircleRenderLayers_{};	// 描画・パーティクル用のコピー（毎フレーム作る。Outro の駆動はこちらに掛ける）
		float MagicCircleIntensity_{ 1.0f };

		// 描き込み演出（Replay で全レイヤーの Reveal を 0 から順に伸ばす）
		bool IsRevealPlaying_{ false };
		float RevealTimer_{ 0.0f };
		float RevealDuration_{ 1.2f };		// 1 レイヤーの描き込みにかかる秒数
		float RevealStagger_{ 0.35f };		// レイヤー間の開始のずれ

		//----	------	------	------	------	----//
		// Camera（Update と Render の両方で使う。実装は Render.cpp）

		void BuildCamera(Lumina::Mat4& view_, Lumina::Mat4& viewProj_, Lumina::Vec3& eye_) const;

		//----	------	------	------	------	----//
		// Particles（実装は Game.Scene.Title.Particles.cpp）

		void InitializeParticles(Lumina::DX12::Context const& dxContext_, Lumina::AssetManager& assetMngr_);
		void UpdateParticles(
			Lumina::DX12::CommandList const& cmdList_,
			float deltaTime_,
			Lumina::Mat4 const& view_,
			Lumina::Mat4 const& viewProj_
		);
		void RenderParticles(Lumina::DX12::CommandList const& cmdList_);
		void EditParticles();		// ImGui（Release では何もしない）

		Lumina::DX12::RootSignature RS_Particle_{};
		Lumina::DX12::Shader VS_Particle_{};
		Lumina::DX12::Shader PS_Particle_{};
		Lumina::DX12::GraphicsPSO PSO_Particle_{};
		Lumina::DX12::UploadBuffer UB_ParticleViewToWorld_{};
		Lumina::DX12::UploadBuffer UB_ParticleVP_{};
		Lumina::DX12::DescriptorHeap LocalHeap_Particle_{};			// 0: ViewToWorld / 1: VP（non-shader-visible）
		Lumina::DX12::DescriptorTable GlobalTable_ParticleTextures_{};	// 0: Particles.png

		std::unique_ptr<ParticleSystem<TitleParticle>> Sparkles_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> Aura_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> Embers_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> CircleLights_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> WeaponGlow_{ nullptr };
		std::unique_ptr<ParticleSystem<TitleParticle>> WeaponFlare_{ nullptr };	// Outro の先端フレア（毎フレーム 1 枚だけ置き直す）

		SparkleEmitter SparkleParams_{};
		AuraEmitter AuraParams_{};
		EmberEmitter EmberParams_{};
		CircleLightEmitter CircleLightParams_{};
		WeaponGlowEmitter WeaponGlowParams_{};

		// 発生数の端数の持ち越し・本編の色計算用の時間
		float SparkleAccum_{ 0.0f };
		float AuraAccum_{ 0.0f };
		float EmberAccum_{ 0.0f };
		float CircleLightAccum_{ 0.0f };
		float WeaponGlowAccum_{ 0.0f };
		float SparkleTimeFactor_{ 0.0f };
		float AuraTimeFactor_{ 0.0f };
		int AuraSpawnIndex_{ 0 };

		//----	------	------	------	------	----//
		// Menu UI（実装は Game.Scene.Title.UI.cpp）。本編の SpriteRenderer を流用

		void InitializeUI(Lumina::DX12::Context const& dxContext_, Lumina::AssetManager& assetMngr_);
		void ResetMenu();
		void UpdateUI(Lumina::WinApp::RawInput const& input_);
		void RenderUI(Lumina::DX12::CommandList const& cmdList_);
		void EditUI();		// ImGui（Release では何もしない）

		// メニューの状態（State パターン。2026-11-26 に enum class MenuPhase と switch から置き換えた。実装は UI.cpp）
		//   FadeIn  … 黒から明ける（この間も選択・決定は受け付ける）→ Idle
		//   Idle    … 選択・決定 → Start：Outro / 設定：画面を開く / Exit：Decided
		//   Decided … 選んだ項目が点滅 → FadeOut
		//   FadeOut … 黒へ → 終了を頼んで Done
		//   Done    … シーン切り替え / 終了を要求済み
		//   Outro   … Start 決定後の演出中（入力は受け付けない。巻き戻しで Idle へ）
		// 各状態は Title の入れ子クラスなので Title の private を直接読み書きでき、次の状態は自分で ChangeMenuState に頼む
		struct MenuInput {
			bool Up{ false };			// 押した瞬間（ImGui がキーボードを掴んでいる間は false）
			bool Down{ false };
			bool Confirm{ false };
			bool SettingsOpen{ false };	// このフレームの頭で設定の画面が開いていた（開いている間は選ばない）
		};

		class MenuState {
		public:
			virtual ~MenuState() = default;
			virtual auto Name() const noexcept -> char const* = 0;		// ImGui の表示用
			virtual void Enter(Title& self_);							// 既定：メニューの時計（MenuTimer_）を 0 に戻す
			virtual void Update(Title& self_, MenuInput const& input_) = 0;
			virtual auto IsBlinking() const noexcept -> bool { return false; }	// 選んだ項目を点滅させる
			virtual auto IsOutro() const noexcept -> bool { return false; }		// Outro 中（選んだ項目のグローを強める）
		};
		class MenuFadeIn final : public MenuState {
		public:
			auto Name() const noexcept -> char const* override { return "FadeIn"; }
			void Update(Title& self_, MenuInput const& input_) override;
		};
		class MenuIdle final : public MenuState {
		public:
			auto Name() const noexcept -> char const* override { return "Idle"; }
			void Update(Title& self_, MenuInput const& input_) override;
		};
		class MenuDecided final : public MenuState {
		public:
			auto Name() const noexcept -> char const* override { return "Decided"; }
			void Update(Title& self_, MenuInput const& input_) override;
			auto IsBlinking() const noexcept -> bool override { return true; }
		};
		class MenuFadeOut final : public MenuState {
		public:
			auto Name() const noexcept -> char const* override { return "FadeOut"; }
			void Enter(Title& self_) override;
			void Update(Title& self_, MenuInput const& input_) override;
		};
		class MenuDone final : public MenuState {
		public:
			auto Name() const noexcept -> char const* override { return "Done"; }
			void Update(Title&, MenuInput const&) override {}
		};
		class MenuOutro final : public MenuState {
		public:
			auto Name() const noexcept -> char const* override { return "Outro"; }
			void Update(Title& self_, MenuInput const& input_) override;
			auto IsOutro() const noexcept -> bool override { return true; }
		};

		void ChangeMenuState(std::unique_ptr<MenuState> next_);
		void HandleMenuSelection(MenuInput const& input_);		// FadeIn / Idle の選択・決定
		void FadeInBehindMenu();								// 決めた後も、フェードイン途中の黒はそのまま明けていく
		static constexpr int NumMenuItems{ 3 };		// 0: Start / 1: 設定 / 2: Exit（2026-11-11 に設定を足した。設定が元の Start の位置）

		std::unique_ptr<Lumina::SpriteRenderer> UISpriteRenderer_{ nullptr };
		std::unique_ptr<Lumina::SpriteRenderer> UIGlowRenderer_{ nullptr };	// グロー専用（1 フレームに 1 回しかバッチできないので分ける）
		Lumina::DX12::Shader VS_UI_{};
		Lumina::DX12::Shader PS_UI_{};
		Lumina::DX12::GraphicsPSO PSO_UI_{};
		Lumina::DX12::Shader PS_UIGlow_{};
		Lumina::DX12::GraphicsPSO PSO_UIGlow_{};		// 加算・TitleLabelGlow.PS
		Lumina::DX12::UploadBuffer UB_UIOrtho_{};
		Lumina::DX12::DescriptorHeap LocalHeap_UI_{};			// 0: 正射影の CBV（non-shader-visible）
		Lumina::DX12::DescriptorTable GlobalTable_UITextures_{};	// 0: Start / 1: Exit / 2: Blank（フェード用）/ 3: Title / 4: 設定

		Lumina::Sprite UI_Items_[NumMenuItems]{};
		Lumina::Sprite UI_Fade_{};
		Lumina::Sprite UI_Logo_{};

		TitleLogoStyle LogoStyle_{};
		float LogoTimer_{ 0.0f };

		static constexpr int NumGlowTargets{ 4 };		// 0: ロゴ / 1 + i: メニューの項目 i
		Lumina::Sprite UI_Glows_[NumGlowTargets]{};
		TitleGlowStyle GlowStyle_{};
		float GlowTimer_{ 0.0f };

		TitleMenuStyle MenuStyle_{};
		StateMachine<Title, MenuState> MenuStates_{ std::make_unique<MenuFadeIn>() };
		float MenuTimer_{ 0.0f };
		int MenuSelected_{ 0 };
		float MenuBlend_[NumMenuItems]{ 1.0f, 0.0f, 0.0f };	// 選択の度合い 0..1（見た目用に遅れて追従）
		float FadeAlpha_{ 1.0f };						// 画面全体の黒の濃さ
		float FadeOutFrom_{ 0.0f };
		Lumina::Float3 IntroFadeColor_{ 0.0f, 0.0f, 0.0f };	// 黒のフェード（UI_Fade_）の色（リニア）。Outro から戻ったときだけ、その色から明ける
		bool PrevKeyUp_{ false };
		bool PrevKeyDown_{ false };
		bool PrevKeyConfirm_{ false };

		// 「設定」で開くキー割り当ての画面（メニューの上に重ねる。開いている間はメニューの入力を止める）
		Game::KeyConfigScreen::Screen SettingsScreen_{};
		// true の間、ESC でタイトル（アプリ）を閉じない。設定の画面を開いている間と、そこを ESC で閉じたあと ESC を離すまで
		// （2026-11-26：グローバル変数 Game::Scene::BlockEscapeQuit から移した。ESC で閉じる判定も main.cpp から UpdateUI へ）
		bool BlockEscapeQuit_{ false };

		//----	------	------	------	------	----//
		// オービットカメラ（注視点のまわりを回る）

		bool AutoOrbit_{ false };
		float OrbitSpeed_{ 0.15f };		// rad/s
		float CameraYaw_{ 0.0f };
		float CameraPitch_{ 0.2f };
		float CameraDistance_{ 16.0f };
		float CameraTargetY_{ 3.0f };
		float CameraFovY_{ 0.8f };
		CameraSway CameraSway_{};
		Lumina::Float3 CameraShake_{ 0.0f, 0.0f, 0.0f };	// 注視点のずれ（Outro のシェイク）

		// 黒で固定（Title 用 Canvas の RT のクリア値と同じにする。違う色でクリアするとデバッグレイヤーが警告する）
		static constexpr float ClearColor_[4]{ 0.0f, 0.0f, 0.0f, 1.0f };

		//----	------	------	------	------	----//
		// Outro（実装は Game.Scene.Title.Outro.cpp）

		// Outro の状態（State パターン。2026-11-26 に enum class OutroState と if から置き換えた。実装は Outro.cpp）
		//   None     … タイトル表示中
		//   Playing  … 時計（OutroTime_）を進め、終わりに着いたら自分で Finished へ
		//   Finished … 最後まで再生した（入ったときに、本番なら次のシーンへ渡す / プレビューは最後の状態で止めている）
		class OutroState {
		public:
			virtual ~OutroState() = default;
			virtual auto Name() const noexcept -> char const* = 0;		// ImGui の表示用
			virtual auto IsActive() const noexcept -> bool { return true; }	// None 以外
			virtual void Enter(Title&) {}
			virtual void Advance(Title&, float) {}						// 毎フレーム（Outro の deltaTime）
		};
		class OutroNone final : public OutroState {
		public:
			auto Name() const noexcept -> char const* override { return "None"; }
			auto IsActive() const noexcept -> bool override { return false; }
		};
		class OutroPlaying final : public OutroState {
		public:
			auto Name() const noexcept -> char const* override { return "Playing"; }
			void Advance(Title& self_, float deltaTime_) override;
		};
		class OutroFinished final : public OutroState {
		public:
			auto Name() const noexcept -> char const* override { return "Finished"; }
			void Enter(Title& self_) override;
		};

		void ChangeOutroState(std::unique_ptr<OutroState> next_);

		void StartOutro(bool isPreview_);	// isPreview_ = ImGui から再生（最後に次のシーンへ渡さない）
		void RewindOutro();					// タイトルの状態に戻す
		void FinishOutro();					// 次のシーンへ渡す（遷移シーンを作ったらここだけ変える）/ ReturnToTitle ならタイトルのイントロへ
		auto AdvanceOutro() -> float;		// Outro の時計を進め、Outro 中のシーンの deltaTime を返す
		void ApplyOutroCamera();
		void ApplyOutroAnimation();
		void ApplyOutroToMagicCircle(std::vector<Lumina::MagicCircle::Layer>& layers_) const;
		auto OutroFirePillarBoost() const -> float;	// 0..1（Outro 外は 0）
		auto OutroUIVisibility() const -> float;		// 1 = 表示 / 0 = 消えた（Outro 外は 1）
		auto OutroFinaleProgress() const -> float;	// フィナーレ（区間 4 の終わり → WhiteEnd − FadeHold）の進み具合 0..1
		void OutroOverlay(Lumina::Float3& colorLinear_, float& alpha_) const;	// 一番手前に重ねる色（リニア）と濃さ。Outro 外は α = 0
		auto OutroWeaponGlowLevel() const -> float;	// 0..1（Outro 外は 0）
		auto OutroWeaponFlareStrength() const -> float;	// 0..1（Outro 外は 0）
		auto OutroRaysIntensity() const -> float;		// Outro 外は 0
		auto OutroRaysLength() const -> float;
		auto OutroPostValue(OutroPostCurve const& curve_) const -> float;	// Outro 外は Base
		void OutroEffectCenter(float& u_, float& v_) const;		// ズームブラー・光線の中心（先端 → 命中から割れ目へ）
		auto IsOutroCrackActive() const -> bool;				// 命中以降（割れ目が有効）
		auto OutroCrackLineReveal() const -> float;			// 0..1
		auto OutroCrackOpen() const -> float;					// 開き具合 0..1（区間 4）
		auto OutroCrackRaysIntensity() const -> float;
		auto OutroCrackRaysLength() const -> float;
		void EditOutro();					// ImGui（Release では何もしない）
		bool IsOutroActive() const noexcept { return OutroStates_.Current().IsActive(); }

		OutroSettings Outro_{};
		StateMachine<Title, OutroState> OutroStates_{ std::make_unique<OutroNone>() };
		float OutroTime_{ 0.0f };
		bool OutroIsPreview_{ false };
		bool OutroSwitchSceneInPreview_{ false };	// Debug：プレビューでも最後に次のシーンへ渡す（保存しない）
		float OutroDebugRate_{ 1.0f };				// Debug：再生速度（保存しない）

		// 開始時のカメラ（ここから補間する。巻き戻しでも使う）
		CameraPose OutroStartPose_{};
		float OutroStartYaw_{ 0.0f };
		std::string OutroStartAnimation_{};	// 巻き戻しで再生し直す

		//----	------	------	------	------	----//
		// 放射光線（実装は Game.Scene.Title.Rays.cpp。スクリーン空間・加算。シーンのテクスチャは読まない）

		// 枠 0：武器の先端から（Title 用 Canvas に描く。ポストが掛かる）
		// 枠 1：割れ目から（ポストの後、バックバッファに描く。割れ目でずれないように）
		static constexpr uint32_t RaysSlot_Tip{ 0U };
		static constexpr uint32_t RaysSlot_Crack{ 1U };
		static constexpr uint32_t NumRaysSlots{ 2U };

		void InitializeRays(Lumina::DX12::Context const& dxContext_);
		void UpdateRays(Lumina::Mat4 const& viewProj_);		// Character の姿勢とカメラが決まった後に呼ぶ
		void RenderRays(Lumina::DX12::CommandList const& cmdList_, uint32_t slot_);

		Lumina::DX12::RootSignature RS_Rays_{};
		Lumina::DX12::Shader VS_Rays_{};
		Lumina::DX12::Shader PS_Rays_{};
		Lumina::DX12::GraphicsPSO PSO_Rays_{};
		Lumina::DX12::UploadBuffer UB_Rays_{};		// 枠ごとに 256 バイト
		bool IsRaysVisible_[NumRaysSlots]{ false, false };

		// 武器の先端のスクリーン位置（UpdateRays で毎フレーム更新。光線とズームブラーの中心）
		float TipU_{ 0.5f };
		float TipV_{ 0.5f };
		bool IsTipOnScreen_{ false };

		//----	------	------	------	------	----//
		// Title 用 Canvas とポスト（実装は Game.Scene.Title.Post.cpp）
		// シーンは Canvas（SRGB ＋ D24S8）に描き、ポスト PS でバックバッファへ写す。メニュー UI はその上に描く

		void InitializePost(Lumina::DX12::Context const& dxContext_);
		void UpdatePost();		// UpdateRays の後に呼ぶ
		void BeginSceneCanvas(Lumina::DX12::CommandList const& cmdList_);
		void EndSceneCanvasAndPost(Lumina::DX12::Context const& dxContext_, Lumina::DX12::CommandList const& cmdList_);

		Lumina::DX12::Canvas Canvas_{};
		Lumina::DX12::DescriptorTable GlobalTable_PostSRV_{};	// 0: Canvas の RT0
		bool IsCanvasDepthWritable_{ false };					// 深度は最初のフレームに DEPTH_WRITE へ遷移したらそのまま
		Lumina::DX12::RootSignature RS_Post_{};
		Lumina::DX12::Shader VS_Post_{};
		Lumina::DX12::Shader PS_Post_{};
		Lumina::DX12::GraphicsPSO PSO_Post_{};
		Lumina::DX12::UploadBuffer UB_Post_{};
	};
}
