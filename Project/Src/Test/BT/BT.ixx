//////	//////	//////	//////	//////	//////
//	BT (prototype) - behavior tree			//
//////	//////	//////	//////	//////	//////
// ビヘイビアツリーのコア。旧 BehaviorTree.ixx（ボス Kinoko が使っている）の作り直し。旧ファイルはボスを載せ替えるまで残す。
//
// 方針
// - 標準ライブラリしか import しない。名前空間 Lumina::BT は名前を借りているだけで、Lumina の他モジュールには依存しない。
//   旧版（Lumina::Behavior）と名前がぶつからないように、名前空間とモジュール名を変えてある。
// - ノードはコンテキストの型 Ctx（敵 1 体ぶんのブラックボード）で型付けする。旧版の Blackboard 基底＋ reinterpret_cast はやめる。
//   ブラックボードは敵 1 体ごとに持つ（旧版はファイルスコープのグローバルで、2 体目と共有されていた）。
// - BT は「何をしたいか」を決めるだけ。動きそのもの（移動・コンボ・ガード・スキル）は体（Game.Fighter）が行い、
//   リーフは体へ意図を書く・先行入力に積む・体の状態を見て Running / Success / Failure を返す。
//
// 1 ノードの一生
//   Tick: 走っていなければ OnEnter → OnTick。結果が Running 以外なら OnExit(結果)。
//   Halt: 走っている途中で外から止める（上の階層が別の枝を選んだ・ツリーごと止めた）。OnHalt を呼ぶ。走っていなければ何もしない。
//   → OnEnter 1 回につき、OnExit か OnHalt のどちらかがちょうど 1 回呼ばれる（テストで守っている）。
//     リーフは体に出していたもの（ガードの押しっぱなし など）を OnExit / OnHalt で片付ける。
//   合成ノードの OnHalt は、走っている子を Halt する（下の階層まで連鎖する）。
//
// 合成ノード（BehaviorTree.CPP の同名ノードの規則に合わせた。https://www.behaviortree.dev/docs/nodes-library/SequenceNode）
//   Sequence          子を順に。Running の子は次の tick もその子から再開（前の子は tick し直さない）。Failure なら次は先頭から。
//                     → 入口の条件は、行動が始まった後は見直されない（旧版の「スキルが自分で中断する」バグはこの形で起きない）。
//   ReactiveSequence  毎 tick 先頭から tick し直す。前の子（条件）が Failure になったら、走っている子を Halt して Failure。
//                     → 「続けてよいか」を見張り続けたい枝（ガードを上げ続ける、など）に使う。
//   Selector          子を順に試す（優先度順）。Running の子から再開。Success なら終わり、全部 Failure なら Failure。
//   ReactiveSelector  毎 tick 先頭（優先度の高い子）から試し直す。上の子が Running / Success になったら、走っている下の子を Halt。
//                     → 割り込み（のけぞり・ガード反応）はツリーの上の方のここに置く。
//
// デバッグ表示用に、各ノードは名前・最後の結果・最後に tick された番号（Tree の Stamp）を持つ。

export module Lumina.BT;

//****	******	******	******	******	****//

import <cstddef>;
import <cstdint>;
import <functional>;
import <memory>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

//////	//////	//////	//////	//////	//////

export namespace Lumina::BT {
	enum class Status : std::uint8_t {
		Running,
		Success,
		Failure,
	};

	// 最後に何が起きたか（デバッグ表示用）
	enum class LastResult : std::uint8_t {
		None,		// まだ tick されていない
		Running,
		Success,
		Failure,
		Halted,		// 走っている途中で外から止められた
	};

	template<class Ctx>
	class Node {
	public:
		explicit Node(std::string name_) : Name_{ std::move(name_) } {}
		virtual ~Node() = default;

		Node(Node const&) = delete;
		auto operator=(Node const&) -> Node& = delete;

		// 1 回ぶん実行する。stamp_ はデバッグ表示用の番号（Tree が毎 tick 増やす）。合成ノードは自分の stamp を子へ渡す
		auto Tick(Ctx& ctx_, std::uint32_t stamp_ = 0U) -> Status {
			Stamp_ = stamp_;
			if (!Running_) { OnEnter(ctx_); }
			Status const s{ OnTick(ctx_) };
			Running_ = (s == Status::Running);
			if (!Running_) { OnExit(ctx_, s); }
			Last_ = ToLast(s);
			return s;
		}

		// 走っている途中で止める。走っていなければ何もしない
		void Halt(Ctx& ctx_) {
			if (!Running_) { return; }
			Running_ = false;		// 先に下ろす（OnHalt の中から自分がもう一度 Halt されても 2 回目は何もしない）
			OnHalt(ctx_);
			Last_ = LastResult::Halted;
		}

		auto IsRunning() const noexcept -> bool { return Running_; }
		auto Name() const noexcept -> std::string_view { return Name_; }
		auto Last() const noexcept -> LastResult { return Last_; }
		auto LastStamp() const noexcept -> std::uint32_t { return Stamp_; }

		// 子（デバッグ表示用）。リーフは 0 個
		virtual auto ChildCount() const noexcept -> std::size_t { return 0U; }
		virtual auto Child([[maybe_unused]] std::size_t i_) const noexcept -> Node const* { return nullptr; }

	protected:
		virtual void OnEnter([[maybe_unused]] Ctx& ctx_) {}
		virtual auto OnTick(Ctx& ctx_) -> Status = 0;
		virtual void OnExit([[maybe_unused]] Ctx& ctx_, [[maybe_unused]] Status result_) {}	// 自分で終わった（Success / Failure）
		virtual void OnHalt([[maybe_unused]] Ctx& ctx_) {}									// 外から止められた

		auto Stamp() const noexcept -> std::uint32_t { return Stamp_; }

	private:
		static constexpr auto ToLast(Status s_) noexcept -> LastResult {
			switch (s_) {
			case Status::Running: return LastResult::Running;
			case Status::Success: return LastResult::Success;
			case Status::Failure: return LastResult::Failure;
			}
			return LastResult::None;
		}

		std::string Name_;
		bool Running_{ false };
		LastResult Last_{ LastResult::None };
		std::uint32_t Stamp_{ 0U };
	};

	template<class Ctx>
	using NodePtr = std::unique_ptr<Node<Ctx>>;

	//==== 合成ノード ====//

	template<class Ctx>
	class Composite : public Node<Ctx> {
	public:
		explicit Composite(std::string name_) : Node<Ctx>{ std::move(name_) } {}

		auto Add(NodePtr<Ctx> child_) -> Composite& {
			Children_.emplace_back(std::move(child_));
			return *this;
		}

		auto ChildCount() const noexcept -> std::size_t override { return Children_.size(); }
		auto Child(std::size_t i_) const noexcept -> Node<Ctx> const* override {
			return (i_ < Children_.size()) ? Children_[i_].get() : nullptr;
		}

	protected:
		// 走っている子をすべて止める（except_ の子は除く）
		void HaltChildren(Ctx& ctx_, std::size_t except_ = static_cast<std::size_t>(-1)) {
			for (std::size_t i = 0; i < Children_.size(); ++i) {
				if (i != except_) { Children_[i]->Halt(ctx_); }
			}
		}

		void OnHalt(Ctx& ctx_) override { HaltChildren(ctx_); }

		std::vector<NodePtr<Ctx>> Children_;
	};

	// 子を順に。Running の子から再開する（前の子は tick し直さない）。Failure なら次は先頭から
	template<class Ctx>
	class Sequence : public Composite<Ctx> {
	public:
		using Composite<Ctx>::Composite;

	protected:
		void OnEnter([[maybe_unused]] Ctx& ctx_) override { Current_ = 0U; }

		auto OnTick(Ctx& ctx_) -> Status override {
			auto& ch{ this->Children_ };
			while (Current_ < ch.size()) {
				Status const s{ ch[Current_]->Tick(ctx_, this->Stamp()) };
				if (s == Status::Running) { return Status::Running; }
				if (s == Status::Failure) { return Status::Failure; }
				++Current_;
			}
			return Status::Success;
		}

		void OnHalt(Ctx& ctx_) override {
			Composite<Ctx>::OnHalt(ctx_);
			Current_ = 0U;
		}

	private:
		std::size_t Current_{ 0U };
	};

	// 毎 tick 先頭から。前の子が Failure になったら、走っている子を Halt して Failure
	template<class Ctx>
	class ReactiveSequence : public Composite<Ctx> {
	public:
		using Composite<Ctx>::Composite;

	protected:
		auto OnTick(Ctx& ctx_) -> Status override {
			auto& ch{ this->Children_ };
			for (std::size_t i = 0; i < ch.size(); ++i) {
				Status const s{ ch[i]->Tick(ctx_, this->Stamp()) };
				if (s == Status::Running) {
					this->HaltChildren(ctx_, i);		// 前の tick に走っていた後ろの子は止める
					return Status::Running;
				}
				if (s == Status::Failure) {
					this->HaltChildren(ctx_);
					return Status::Failure;
				}
			}
			return Status::Success;
		}
	};

	// 子を優先度順に試す。Running の子から再開する。Success なら終わり、全部 Failure なら Failure
	template<class Ctx>
	class Selector : public Composite<Ctx> {
	public:
		using Composite<Ctx>::Composite;

	protected:
		void OnEnter([[maybe_unused]] Ctx& ctx_) override { Current_ = 0U; }

		auto OnTick(Ctx& ctx_) -> Status override {
			auto& ch{ this->Children_ };
			while (Current_ < ch.size()) {
				Status const s{ ch[Current_]->Tick(ctx_, this->Stamp()) };
				if (s == Status::Running) { return Status::Running; }
				if (s == Status::Success) { return Status::Success; }
				++Current_;
			}
			return Status::Failure;
		}

		void OnHalt(Ctx& ctx_) override {
			Composite<Ctx>::OnHalt(ctx_);
			Current_ = 0U;
		}

	private:
		std::size_t Current_{ 0U };
	};

	// 毎 tick 優先度の高い子から試し直す。上の子が Running / Success になったら、走っている他の子を Halt
	template<class Ctx>
	class ReactiveSelector : public Composite<Ctx> {
	public:
		using Composite<Ctx>::Composite;

	protected:
		auto OnTick(Ctx& ctx_) -> Status override {
			auto& ch{ this->Children_ };
			for (std::size_t i = 0; i < ch.size(); ++i) {
				Status const s{ ch[i]->Tick(ctx_, this->Stamp()) };
				if (s != Status::Failure) {
					this->HaltChildren(ctx_, i);
					return s;
				}
			}
			return Status::Failure;
		}
	};

	//==== デコレータ ====//

	template<class Ctx>
	class Decorator : public Node<Ctx> {
	public:
		Decorator(std::string name_, NodePtr<Ctx> child_) : Node<Ctx>{ std::move(name_) }, Child_{ std::move(child_) } {}

		auto ChildCount() const noexcept -> std::size_t override { return Child_ ? 1U : 0U; }
		auto Child(std::size_t i_) const noexcept -> Node<Ctx> const* override { return (i_ == 0U) ? Child_.get() : nullptr; }

	protected:
		auto TickChild(Ctx& ctx_) -> Status { return Child_ ? Child_->Tick(ctx_, this->Stamp()) : Status::Failure; }
		void OnHalt(Ctx& ctx_) override { if (Child_) { Child_->Halt(ctx_); } }

		NodePtr<Ctx> Child_;
	};

	// Success と Failure を入れ替える（Running はそのまま）
	template<class Ctx>
	class Inverter : public Decorator<Ctx> {
	public:
		using Decorator<Ctx>::Decorator;

	protected:
		auto OnTick(Ctx& ctx_) -> Status override {
			Status const s{ this->TickChild(ctx_) };
			if (s == Status::Success) { return Status::Failure; }
			if (s == Status::Failure) { return Status::Success; }
			return s;
		}
	};

	//==== リーフ ====//

	// 条件。関数が true なら Success、false なら Failure（Running にはならない）
	template<class Ctx>
	class Condition : public Node<Ctx> {
	public:
		Condition(std::string name_, std::function<bool(Ctx const&)> pred_) : Node<Ctx>{ std::move(name_) }, Pred_{ std::move(pred_) } {}

	protected:
		auto OnTick(Ctx& ctx_) -> Status override {
			return (Pred_ && Pred_(ctx_)) ? Status::Success : Status::Failure;
		}

	private:
		std::function<bool(Ctx const&)> Pred_;
	};

	//==== ツリー ====//

	template<class Ctx>
	class Tree {
	public:
		void SetRoot(NodePtr<Ctx> root_) { Root_ = std::move(root_); }
		auto Root() const noexcept -> Node<Ctx> const* { return Root_.get(); }

		// 1 回ぶん。ルートが無ければ Failure
		auto Tick(Ctx& ctx_) -> Status {
			if (!Root_) { return Status::Failure; }
			++Stamp_;
			if (Stamp_ == 0U) { Stamp_ = 1U; }		// 0 は「まだ tick されていない」と区別できないので飛ばす
			return Root_->Tick(ctx_, Stamp_);
		}

		// ツリーごと止める（のけぞり中に BT を止めたい・敵が倒れた・シーンを抜ける、など）
		void Halt(Ctx& ctx_) { if (Root_) { Root_->Halt(ctx_); } }

		// 最後の Tick の番号。ノードの LastStamp() と同じなら、そのノードは最後の Tick で通った
		auto Stamp() const noexcept -> std::uint32_t { return Stamp_; }

	private:
		NodePtr<Ctx> Root_;
		std::uint32_t Stamp_{ 0U };
	};

	// 組み立て用: auto seq{ Make<Sequence<Ctx>>("Attack") }; seq->Add(...);
	template<class T, class... Args>
	auto Make(Args&&... args_) -> std::unique_ptr<T> { return std::make_unique<T>(std::forward<Args>(args_)...); }
}
