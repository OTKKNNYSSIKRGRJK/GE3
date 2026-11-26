module Lumina.BT.Tests;

//****	******	******	******	******	****//

import <cstddef>;
import <cstdint>;
import <memory>;
import <string>;
import <string_view>;
import <vector>;

import Lumina.BT;

//////	//////	//////	//////	//////	//////

namespace Lumina::BT::Tests {
	namespace {
		class Context {
		public:
			Context(std::vector<Failure>& out_, std::string_view name_) : Out_{ out_ }, Name_{ name_ } {}
			void Check(bool ok_, std::string_view what_) {
				if (!ok_) { Out_.push_back(Failure{ std::string{ Name_ }, std::string{ what_ } }); }
			}
		private:
			std::vector<Failure>& Out_;
			std::string_view Name_;
		};

		// テスト用のブラックボード
		struct BB {
			bool Flag{ true };
			int Value{ 0 };
		};

		// 呼ばれた回数を数えるリーフ。結果は Script（無ければ Default）から順に返す
		struct ProbeCounts {
			int Enter{ 0 };
			int Tick{ 0 };
			int Exit{ 0 };
			int Halt{ 0 };
		};

		class Probe : public Node<BB> {
		public:
			Probe(std::string name_, ProbeCounts& counts_, Status default_ = Status::Running)
				: Node<BB>{ std::move(name_) }, C_{ counts_ }, Default_{ default_ } {}

			std::vector<Status> Script{};

		protected:
			void OnEnter(BB&) override { ++C_.Enter; }
			auto OnTick(BB&) -> Status override {
				++C_.Tick;
				if (Next_ < Script.size()) { return Script[Next_++]; }
				return Default_;
			}
			void OnExit(BB&, Status) override { ++C_.Exit; }
			void OnHalt(BB&) override { ++C_.Halt; }

		private:
			ProbeCounts& C_;
			Status Default_;
			std::size_t Next_{ 0U };
		};

		auto MakeProbe(std::string name_, ProbeCounts& c_, Status default_ = Status::Running) -> std::unique_ptr<Probe> {
			return std::make_unique<Probe>(std::move(name_), c_, default_);
		}

		auto FlagCondition(std::string name_) -> NodePtr<BB> {
			return Make<Condition<BB>>(std::move(name_), [](BB const& bb_) { return bb_.Flag; });
		}

		//==== 1 ノードの一生 ====//

		void NodeLifecycle(std::vector<Failure>& out_) {
			Context c{ out_, "Node.Lifecycle" };
			BB bb{};
			ProbeCounts n{};
			auto p{ MakeProbe("p", n) };
			p->Script = { Status::Running, Status::Running, Status::Success, Status::Running };

			c.Check(p->Tick(bb) == Status::Running && n.Enter == 1 && n.Exit == 0, "最初の tick で OnEnter");
			p->Tick(bb);
			c.Check(n.Enter == 1 && n.Tick == 2, "走っている間は OnEnter しない");
			c.Check(p->Tick(bb) == Status::Success && n.Exit == 1 && !p->IsRunning(), "Success で OnExit");
			p->Tick(bb);
			c.Check(n.Enter == 2 && p->IsRunning(), "終わった後の tick はまた OnEnter から");
			p->Halt(bb);
			c.Check(n.Halt == 1 && n.Exit == 1 && !p->IsRunning() && p->Last() == LastResult::Halted, "Halt で OnHalt（OnExit は呼ばない）");
			p->Halt(bb);
			c.Check(n.Halt == 1, "走っていなければ Halt は何もしない");
		}

		//==== Sequence ====//

		// 旧版のバグの再発防止: 行動が始まった後に入口の条件が false になっても、行動は続く
		void SequenceResumes(std::vector<Failure>& out_) {
			Context c{ out_, "Sequence.Resumes" };
			BB bb{};
			ProbeCounts cond{}, act{};
			auto seq{ Make<Sequence<BB>>("seq") };
			auto condProbe{ MakeProbe("cond", cond, Status::Success) };
			auto actProbe{ MakeProbe("act", act) };
			actProbe->Script = { Status::Running, Status::Running, Status::Success };
			seq->Add(std::move(condProbe)).Add(std::move(actProbe));

			c.Check(seq->Tick(bb) == Status::Running, "1 tick 目: 条件 → 行動 Running");
			c.Check(seq->Tick(bb) == Status::Running && cond.Tick == 1, "2 tick 目: 条件は tick し直さない");
			c.Check(seq->Tick(bb) == Status::Success && cond.Tick == 1 && act.Exit == 1, "行動が終われば Success");
			seq->Tick(bb);
			c.Check(cond.Tick == 2, "終わった後は先頭（条件）から");
		}

		void SequenceFailureRestarts(std::vector<Failure>& out_) {
			Context c{ out_, "Sequence.FailureRestarts" };
			BB bb{};
			ProbeCounts a{}, b{};
			auto seq{ Make<Sequence<BB>>("seq") };
			auto pa{ MakeProbe("a", a, Status::Success) };
			auto pb{ MakeProbe("b", b) };
			pb->Script = { Status::Running, Status::Failure };
			seq->Add(std::move(pa)).Add(std::move(pb));

			seq->Tick(bb);
			c.Check(seq->Tick(bb) == Status::Failure, "後ろの子の Failure で Failure");
			seq->Tick(bb);
			c.Check(a.Tick == 2, "Failure の後は先頭から");
		}

		//==== ReactiveSequence ====//

		void ReactiveSequenceInterrupts(std::vector<Failure>& out_) {
			Context c{ out_, "ReactiveSequence.Interrupts" };
			BB bb{};
			ProbeCounts act{};
			auto seq{ Make<ReactiveSequence<BB>>("guard") };
			seq->Add(FlagCondition("incoming")).Add(MakeProbe("hold", act));

			seq->Tick(bb);
			seq->Tick(bb);
			c.Check(act.Enter == 1 && act.Tick == 2, "条件が真の間、行動は続く");
			bb.Flag = false;
			c.Check(seq->Tick(bb) == Status::Failure, "条件が偽になれば Failure");
			c.Check(act.Halt == 1 && act.Exit == 0 && act.Tick == 2, "走っていた行動は Halt（tick はされない）");
			bb.Flag = true;
			seq->Tick(bb);
			c.Check(act.Enter == 2, "条件が戻れば行動は最初から");
		}

		//==== Selector ====//

		void SelectorResumes(std::vector<Failure>& out_) {
			Context c{ out_, "Selector.Resumes" };
			BB bb{};
			ProbeCounts hi{}, lo{};
			auto sel{ Make<Selector<BB>>("sel") };
			auto phi{ MakeProbe("hi", hi, Status::Failure) };
			auto plo{ MakeProbe("lo", lo) };
			plo->Script = { Status::Running, Status::Running, Status::Success };
			sel->Add(std::move(phi)).Add(std::move(plo));

			c.Check(sel->Tick(bb) == Status::Running && hi.Tick == 1, "上が Failure なら下を試す");
			sel->Tick(bb);
			c.Check(hi.Tick == 1, "走っている子から再開（上は試し直さない）");
			c.Check(sel->Tick(bb) == Status::Success, "子の Success で Success");
			sel->Tick(bb);
			c.Check(hi.Tick == 2, "終わった後は先頭から");

			ProbeCounts f1{}, f2{};
			auto all{ Make<Selector<BB>>("allFail") };
			all->Add(MakeProbe("f1", f1, Status::Failure)).Add(MakeProbe("f2", f2, Status::Failure));
			c.Check(all->Tick(bb) == Status::Failure && f1.Tick == 1 && f2.Tick == 1, "全部 Failure なら Failure");
		}

		//==== ReactiveSelector（割り込み）と Halt の連鎖 ====//

		void ReactiveSelectorPreempts(std::vector<Failure>& out_) {
			Context c{ out_, "ReactiveSelector.Preempts" };
			BB bb{};
			bb.Flag = false;
			ProbeCounts urgent{}, combo{}, approach{};

			// Root: ReactiveSelector[ Seq[flag, urgent], Selector[ Seq[combo], approach ] ]
			auto root{ Make<ReactiveSelector<BB>>("root") };
			auto hiSeq{ Make<Sequence<BB>>("react") };
			hiSeq->Add(FlagCondition("flag")).Add(MakeProbe("urgent", urgent));
			auto lo{ Make<Selector<BB>>("combat") };
			auto comboSeq{ Make<Sequence<BB>>("comboSeq") };
			comboSeq->Add(MakeProbe("combo", combo));
			lo->Add(std::move(comboSeq)).Add(MakeProbe("approach", approach));
			root->Add(std::move(hiSeq)).Add(std::move(lo));

			root->Tick(bb);
			root->Tick(bb);
			c.Check(combo.Enter == 1 && combo.Tick == 2, "割り込みが無い間は下の枝が続く");
			bb.Flag = true;
			c.Check(root->Tick(bb) == Status::Running, "上の枝が Running");
			c.Check(urgent.Enter == 1 && combo.Halt == 1 && combo.Exit == 0, "下の枝はリーフまで Halt される（連鎖）");
			c.Check(root->Child(1) && !root->Child(1)->IsRunning(), "下の Selector も止まっている");
			root->Tick(bb);
			c.Check(urgent.Tick == 2 && combo.Tick == 2, "上が続く間、下は tick されない");
			bb.Flag = false;
			root->Tick(bb);
			c.Check(urgent.Halt == 0, "Sequence の中の条件は見直されないので、上の枝はまだ続く（割り込みを解くのは ReactiveSequence）");
		}

		void ReactiveSelectorReturns(std::vector<Failure>& out_) {
			Context c{ out_, "ReactiveSelector.Returns" };
			BB bb{};
			bb.Flag = false;
			ProbeCounts guard{}, combo{};
			auto root{ Make<ReactiveSelector<BB>>("root") };
			auto react{ Make<ReactiveSequence<BB>>("guardBranch") };
			react->Add(FlagCondition("incoming")).Add(MakeProbe("guard", guard));
			root->Add(std::move(react)).Add(MakeProbe("combo", combo));

			root->Tick(bb);
			bb.Flag = true;
			root->Tick(bb);
			c.Check(combo.Halt == 1 && guard.Enter == 1, "条件が立ったら下を止めてガード");
			bb.Flag = false;
			root->Tick(bb);
			c.Check(guard.Halt == 1 && combo.Enter == 2, "条件が下りたらガードを止め、下の枝を最初から");
		}

		//==== Tree ====//

		void TreeStampAndHalt(std::vector<Failure>& out_) {
			Context c{ out_, "Tree.StampAndHalt" };
			BB bb{};
			ProbeCounts a{}, b{};
			Tree<BB> tree{};
			c.Check(tree.Tick(bb) == Status::Failure, "ルートが無ければ Failure");

			auto sel{ Make<Selector<BB>>("root") };
			sel->Add(MakeProbe("a", a, Status::Failure)).Add(MakeProbe("b", b));
			tree.SetRoot(std::move(sel));
			tree.Tick(bb);
			std::uint32_t const s1{ tree.Stamp() };
			tree.Tick(bb);
			auto const* root{ tree.Root() };
			c.Check(root->LastStamp() == tree.Stamp() && root->Child(1)->LastStamp() == tree.Stamp(), "通ったノードは今の番号");
			c.Check(root->Child(0)->LastStamp() == s1 && root->Child(0)->Last() == LastResult::Failure, "通らなかったノードは前の番号と結果のまま");
			c.Check(root->Child(1)->Last() == LastResult::Running, "最後の結果");
			tree.Halt(bb);
			c.Check(b.Halt == 1 && !root->IsRunning(), "Tree::Halt で全部止まる");
		}

		void InverterAndCondition(std::vector<Failure>& out_) {
			Context c{ out_, "Inverter.Condition" };
			BB bb{};
			auto inv{ Make<Inverter<BB>>("not", FlagCondition("flag")) };
			c.Check(inv->Tick(bb) == Status::Failure, "真 → Failure");
			bb.Flag = false;
			c.Check(inv->Tick(bb) == Status::Success, "偽 → Success");
			ProbeCounts p{};
			auto run{ Make<Inverter<BB>>("notRun", MakeProbe("p", p)) };
			c.Check(run->Tick(bb) == Status::Running, "Running はそのまま");
			run->Halt(bb);
			c.Check(p.Halt == 1, "Halt は子へ");
			Condition<BB> empty{ "empty", nullptr };
			c.Check(empty.Tick(bb) == Status::Failure, "関数が無い条件は Failure");
		}

		//==== ランダムな木: OnEnter 1 回につき OnExit か OnHalt がちょうど 1 回 ====//

		struct Rng {
			std::uint32_t S{ 0x12345678U };
			auto Next() -> std::uint32_t { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
		};

		class RandomLeaf : public Node<BB> {
		public:
			RandomLeaf(Rng& rng_, ProbeCounts& c_) : Node<BB>{ "leaf" }, Rng_{ rng_ }, C_{ c_ } {}
		protected:
			void OnEnter(BB&) override { ++C_.Enter; }
			auto OnTick(BB&) -> Status override {
				++C_.Tick;
				switch (Rng_.Next() % 4U) {
				case 0U: return Status::Success;
				case 1U: return Status::Failure;
				default: return Status::Running;
				}
			}
			void OnExit(BB&, Status) override { ++C_.Exit; }
			void OnHalt(BB&) override { ++C_.Halt; }
		private:
			Rng& Rng_;
			ProbeCounts& C_;
		};

		// 決まった形の木を作る（深さ 3、各合成ノードに子 3 つ）。合成の種類はランダム
		auto BuildFixed(Rng& rng_, std::vector<ProbeCounts>& counts_, std::size_t& next_, int depth_) -> NodePtr<BB> {
			if (depth_ == 0) {
				return std::make_unique<RandomLeaf>(rng_, counts_[next_++]);
			}
			std::unique_ptr<Composite<BB>> node{};
			switch (rng_.Next() % 4U) {
			case 0U: node = Make<Sequence<BB>>("seq"); break;
			case 1U: node = Make<ReactiveSequence<BB>>("rseq"); break;
			case 2U: node = Make<Selector<BB>>("sel"); break;
			default: node = Make<ReactiveSelector<BB>>("rsel"); break;
			}
			for (int i = 0; i < 3; ++i) { node->Add(BuildFixed(rng_, counts_, next_, depth_ - 1)); }
			return node;
		}

		auto CountRunningLeaves(Node<BB> const& n_) -> int {
			if (n_.ChildCount() == 0U) { return n_.IsRunning() ? 1 : 0; }
			int sum{ 0 };
			for (std::size_t i = 0; i < n_.ChildCount(); ++i) { sum += CountRunningLeaves(*n_.Child(i)); }
			return sum;
		}

		// 走っているノードの親も走っている（走っていない合成ノードの下に、走っている子が残っていない）
		auto NoOrphans(Node<BB> const& n_) -> bool {
			for (std::size_t i = 0; i < n_.ChildCount(); ++i) {
				Node<BB> const& ch{ *n_.Child(i) };
				if (ch.IsRunning() && !n_.IsRunning()) { return false; }
				if (!NoOrphans(ch)) { return false; }
			}
			return true;
		}

		void RandomTrees(std::vector<Failure>& out_) {
			Context c{ out_, "Random.Balanced" };
			bool balanced{ true }, single{ true }, orphans{ true }, halted{ true };
			for (std::uint32_t seed = 1U; seed <= 40U; ++seed) {
				Rng rng{ seed * 2654435761U };
				std::vector<ProbeCounts> counts(27U);
				std::size_t next{ 0U };
				Tree<BB> tree{};
				tree.SetRoot(BuildFixed(rng, counts, next, 3));
				BB bb{};
				for (int t = 0; t < 500; ++t) {
					tree.Tick(bb);
					if (CountRunningLeaves(*tree.Root()) > 1) { single = false; }
					if (!NoOrphans(*tree.Root())) { orphans = false; }
					if (rng.Next() % 50U == 0U) { tree.Halt(bb); }
				}
				tree.Halt(bb);
				if (CountRunningLeaves(*tree.Root()) != 0 || tree.Root()->IsRunning()) { halted = false; }
				for (auto const& k : counts) {
					if (k.Enter != k.Exit + k.Halt) { balanced = false; }
				}
			}
			c.Check(balanced, "どのリーフも OnEnter の回数 = OnExit + OnHalt の回数");
			c.Check(single, "走っているリーフは常に 1 つ以下");
			c.Check(orphans, "走っていない合成ノードの下に、走っている子が残らない");
			c.Check(halted, "Tree::Halt の後は何も走っていない");
		}
	}

	auto RunAll() -> std::vector<Failure> {
		std::vector<Failure> out{};
		NodeLifecycle(out);
		SequenceResumes(out);
		SequenceFailureRestarts(out);
		ReactiveSequenceInterrupts(out);
		SelectorResumes(out);
		ReactiveSelectorPreempts(out);
		ReactiveSelectorReturns(out);
		TreeStampAndHalt(out);
		InverterAndCondition(out);
		RandomTrees(out);
		return out;
	}
}
