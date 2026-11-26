export module Lumina.AnimEvent : Player;

//****	******	******	******	******	****//

import <bit>;
import <cassert>;
import <cstddef>;
import <cstdint>;
import <optional>;
import <span>;
import <vector>;

import : Time;
import : Data;

//////	//////	//////	//////	//////	//////

// 再生中インスタンスの管理と、1 ステップぶんのイベントの収集（講義 前編 2.4, 4.3〜4.7, 4.9）
// カーブの現在値（前編 4.8）と、ホットリロード時のデータの差し替え（後編 8.8）もここ
//
// ポーズは扱わない。このクラスが決めるのは「時刻」と「イベント」だけで、エンジンは Time(id) を読んでポーズを作る。
// ブレンドも扱わない。ウェイトは外から SetWeight で渡される値として、演出系の採用判定（前編 4.6）にだけ使う。
//
// 1 ステップの流れ（Step）
//   1. 溜まった要求（Play / Stop / SetWeight / SetRate）を、積まれた順に反映する（前編 2.4）
//   2. 各インスタンスの時刻を進め、通過区間を得る（:Time）
//   3. 瞬間イベント：通過区間に入っているものを集める（前編 4.3）
//   4. 区間イベント：開いている窓の集合（64bit マスク）を作り、前ステップとの差分から Begin / End を作る（前編 4.4）
//   5. このステップで追記した分を、ステップ内の位置順に並べる。同じ位置なら End が先（前編 4.7）
//
// 判定系（Gameplay）と演出系（Presentation）の採用方針（前編 4.6）
//   判定系：行動の持ち主（OwnsAction）のインスタンスからだけ採る。持ち主でないインスタンスの判定系の窓は開かない。
//           新しい持ち主が Play されたら、前の持ち主の開いている判定系の窓は Interrupted で閉じる。
//   演出系：インスタンスのウェイトが、イベントの MinWeight 以上のときだけ採る。
//           窓の途中でウェイトが閾値を割ったら、その窓は Interrupted で閉じる。
//   この絞り込みをコアの中で行うのは、呼び出し側ごとに同じ判断を書き直さなくて済むようにするため。
//   その結果、Step の出力も IsOpen の問い合わせも、すでに絞り込んだ後の状態になる。
//   GE3 のようにクリップを 1 本ずつしか再生しない使い方では、毎回 OwnsAction = true で Play すればよい。
//
// 再入について
//   Step は呼び出し側のコールバックを呼ばない。結果を out_ に積んで返すだけ。
//   受け取ったイベントの処理中に Play / Stop を呼んでもよい（要求として積まれ、次の Step の頭で反映される）。
//   その場合、Stop で閉じる窓の End は次の Step の出力に入る。そのステップの当たり判定より前に届くので、
//   講義の「1 ステップの流れ」（前編 2.1）を守っていれば、ヒットボックスが余分に 1 回判定されることはない。

export namespace Lumina::AnimEvent {
	using WindowMask = std::uint64_t;		// 1 クリップの窓は 64 個まで（ClipEvents::MaxWindows）

	struct PlayParams {
		ClipEvents const* Events{ nullptr };	// 寿命は呼び出し側が保証する。インスタンスが生きている間は動かさない・消さない
		float Rate{ 1.0f };						// GE3 の 4 倍速再生なら 4
		float StartTime{ 0.0f };				// 途中から始めるとき（前編 2.5 の余り時間の持ち越しにも使う）。開始時刻ちょうどのイベントは拾う
		float Weight{ 1.0f };
		bool Loop{ false };
		bool OwnsAction{ false };				// 行動の持ち主か（攻撃・回避・被弾など。待機や移動は持ち主にしない）
		bool StopOthers{ false };				// 再生中の他のインスタンスをすべて Interrupted で止めてから始める（単一クリップ再生向け）
	};

	// ForEachOpenWindow に渡される、開いている窓 1 つぶんの情報
	struct OpenWindowInfo {
		InstanceId Source{ InvalidInstance };
		std::uint16_t WindowIndex{ 0U };
		WindowEvent const* Window{ nullptr };
		std::byte const* Payload{ nullptr };	// ペイロードなしなら nullptr
		float Weight{ 1.0f };
		bool OwnsAction{ false };
	};

	// 等間隔に焼き直したカーブを時刻 t_（秒）で引く。範囲外は端の値。値が無ければ 0
	auto SampleCurve(CurveTrack const& c_, float t_) noexcept -> float;

	class AnimEventPlayer {
	public:
		AnimEventPlayer() = default;
		AnimEventPlayer(AnimEventPlayer const&) = default;
		AnimEventPlayer(AnimEventPlayer&&) noexcept = default;
		auto operator=(AnimEventPlayer const&) -> AnimEventPlayer& = default;
		auto operator=(AnimEventPlayer&&) noexcept -> AnimEventPlayer& = default;
		// 開いている窓を残したまま破棄しない（前編 4.5）。
		// デストラクタでイベントを出すと受け手が先に消えていることがあるので、自動では閉じず、確認だけする。
		// 破棄する前に StopAllNow を呼んで、出てきた End を処理すること。
		~AnimEventPlayer() { assert(!HasOpenWindows() && "AnimEventPlayer: call StopAllNow() before destruction"); }

		//----	要求（次の Step の頭で、積んだ順に反映）	----//

		// ID はその場で返す（反映前でも Time / Stop / SetWeight に使える）。Events が nullptr なら InvalidInstance
		auto Play(PlayParams const& params_) -> InstanceId;
		void Stop(InstanceId id_, EndReason reason_ = EndReason::Interrupted);
		void SetWeight(InstanceId id_, float weight_);
		void SetRate(InstanceId id_, float rate_);

		//----	1 ステップ	------	------	------	----//

		// 要求を反映して dt_ だけ進め、このステップのイベントを out_ の末尾に追記する（out_ のクリアは呼び出し側）
		void Step(float dt_, std::vector<AnimEvent>& out_);

		// 溜まった要求を捨て、全インスタンスをその場で止める（シーンの終了時など）。開いていた窓の End を out_ に追記する
		void StopAllNow(EndReason reason_, std::vector<AnimEvent>& out_);

		// ホットリロード：from_ を参照しているインスタンス（と、反映前の Play）を to_ に差し替える（後編 8.8）。
		// 窓IDは焼き込みのたびに振り直されるので、開いている窓のマスクは差し替えると意味を失う。そこで
		//   1. from_ の開いている窓をすべて閉じる（End、理由は Reloaded）
		//   2. to_ の窓のうち、現在時刻を含むものを開き直す（Begin、StepFraction は 0）
		// の順に行い、出てきたイベントを out_ に追記する。瞬間イベントは出さない（時間は進めていないので）。
		// 現在時刻は to_ の長さに収める。
		// ステップの境目で呼ぶこと（Step の出力を処理し終えてから）。from_ を指していたペイロードのポインタは、
		// それまでに使い切っている前提。呼んだ後なら from_ を破棄してよい。
		void Rebind(ClipEvents const* from_, ClipEvents const* to_, std::vector<AnimEvent>& out_);

		//----	問い合わせ	------	------	------	----//

		// 反映前の Play なら StartTime を返す。止めた・知らない ID なら nullopt
		auto Time(InstanceId id_) const -> std::optional<float>;
		auto IsFinished(InstanceId id_) const -> bool;		// 非ループで端に着いて止まっている
		auto IsPlaying(InstanceId id_) const -> bool;		// 反映済みで、まだ止めていない
		auto ActionOwner() const noexcept -> InstanceId;	// 行動の持ち主。いなければ InvalidInstance

		// その型の窓がどれか 1 つでも開いているか（「状態は問い合わせ、副作用はエッジ」前編 4.4）。
		// 無敵やキャンセル可否は、Begin / End でフラグを上げ下げせず、毎ステップこれで作り直す。
		auto IsOpen(EventTypeId type_) const -> bool;
		template<EventPayload T>
		auto IsOpen() const -> bool { return IsOpen(TypeIdOf<T>()); }

		// カーブの現在値（前編 4.8）。カーブは区間ではなく現在時刻で 1 回引く（通過区間は使わない）。
		// そのカーブを持つインスタンスすべてを、インスタンスのウェイトで加重平均する（ポーズと同じく連続値なので混ぜてよい）。
		// 持ち主かどうかは問わない。持っているインスタンスが無い、またはウェイトの合計が 0 なら fallback_。
		// 0/1 の切り替えは区間（IsOpen）、連続的な変化はカーブ、と使い分ける。
		auto Curve(CurveId id_, float fallback_) const -> float;

		// 開いている窓すべてに f_(OpenWindowInfo const&) を呼ぶ。順序はインスタンスの再生順 → 窓ID 順
		template<class F>
		void ForEachOpenWindow(F&& f_) const {
			for (Instance const& inst : Instances_) {
				for (WindowMask m = inst.Open; m != 0U; m &= m - 1U) {
					auto const idx{ static_cast<std::size_t>(std::countr_zero(m)) };	// <bit> は C++20
					WindowEvent const& w{ inst.Events->Windows[idx] };
					f_(OpenWindowInfo{
						inst.Id,
						static_cast<std::uint16_t>(idx),
						&w,
						PayloadPtr(*inst.Events, w.PayloadOffset, w.PayloadSize),
						inst.Weight,
						inst.OwnsAction
					});
				}
			}
		}

	private:
		struct Instance {
			InstanceId Id{ InvalidInstance };
			ClipEvents const* Events{ nullptr };
			PlaybackCursor Cursor{};
			float Weight{ 1.0f };
			bool OwnsAction{ false };
			WindowMask Open{ 0U };		// 前ステップまでに開いている窓（絞り込み後）
		};

		struct Request {
			enum class Kind : std::uint8_t { Play, Stop, SetWeight, SetRate };
			Kind What{ Kind::Play };
			InstanceId Id{ InvalidInstance };
			PlayParams Params{};		// Play のとき
			EndReason Reason{ EndReason::Interrupted };	// Stop のとき
			float Value{ 0.0f };		// SetWeight / SetRate のとき
		};

		std::vector<Instance> Instances_{};
		std::vector<Request> Requests_{};
		InstanceId NextId_{ 1U };

		static auto PayloadPtr(ClipEvents const& ev_, std::uint32_t offset_, std::uint32_t size_) noexcept -> std::byte const* {
			return size_ == 0U ? nullptr : ev_.PayloadBlob.data() + offset_;
		}

		auto HasOpenWindows() const noexcept -> bool;
		auto Find(InstanceId id_) -> Instance*;
		auto Find(InstanceId id_) const -> Instance const*;
		auto FindPendingPlay(InstanceId id_) const -> Request const*;

		void ApplyRequests(std::vector<AnimEvent>& out_);
		void AdvanceInstance(Instance& inst_, float dt_, std::vector<AnimEvent>& out_);

		// 開いている窓のうち mask_ に入っているものをすべて閉じる（End を出す）
		void CloseWindows(Instance& inst_, WindowMask mask_, EndReason reason_, std::vector<AnimEvent>& out_);
		// 判定系の窓の集合
		static auto GameplayMask(ClipEvents const& ev_) noexcept -> WindowMask;
		// いまの持ち主・ウェイトで採用してよい窓の集合
		static auto EligibleMask(Instance const& inst_) noexcept -> WindowMask;

		static auto MakeWindowEdge(Instance const& inst_, std::size_t index_, EventEdge edge_, EndReason reason_, float stepFraction_) -> AnimEvent;
	};
}
