export module BehaviorTree;

import <memory>;

import <vector>;

/// > Composites are responsible for directing the path
/// traced through the tree on a given tick (execution).
/// > They are the factories (Sequences and Parallels)
/// and decision makers (Selectors) of a behaviour tree.
/// # References
/// 1. https://docs.ros.org/en/jazzy/p/py_trees/py_trees.composites.html

namespace Lumina::Behavior {
	export enum class Status {
		Running,
		Success,
		Failure,
	};

	export class Blackboard {};



	export class Node {		
	public:
		virtual auto Tick(Blackboard& blackboard_) -> Status = 0;

	public:
		virtual auto OnTerminate([[maybe_unused]] Status status_) -> void {}

	public:
		constexpr virtual ~Node() noexcept = default;
	};

	export class Composite : public Node {
	public:
		auto AddChild(std::unique_ptr<Node> child_) -> void {
			Children_.emplace_back(std::move(child_));
		}

	protected:
		std::vector<std::unique_ptr<Node>> Children_;
	};

	export class Decorator : public Node {
	public:
		auto SetChild(std::unique_ptr<Node> child_) -> void {
			Child_ = std::move(child_);
		}

	protected:
		std::unique_ptr<Node> Child_;
	};

	export class Leaf : public Node {};
}



namespace Lumina::Behavior {

	/// Tries to run **all** children

	export class Sequence : public Composite {
	public:
		auto Tick(Blackboard& blackboard_) -> Status override {
			bool const isRunningChildExistent{ IDX_RunningChild_ != -1 };

			for (size_t idx{ 0LLU }; idx < Children_.size(); ++idx) {
				auto& child{ Children_.at(idx) };
				Status status{ child->Tick(blackboard_) };

				// * Change of the index of the running child means
				// * the progress has been interrupted by a child with higher priority.
				bool const isInterrupted{
					(IDX_RunningChild_ != -1) &&
					(IDX_RunningChild_ != static_cast<int>(idx))
				};

				if (status == Status::Running) {
					if (isInterrupted) { child->OnTerminate(Status::Failure); }
					
					IDX_RunningChild_ = static_cast<int>(idx);
					
					return Status::Running;
				}
				else if (status == Status::Failure) {
					if (isRunningChildExistent) { child->OnTerminate(Status::Failure); }
					
					IDX_RunningChild_ = -1;

					return Status::Failure;
				}

				// * `Sequence` tries to run the next child
				// * if the current child ends in `Status::Success`.
			}

			IDX_RunningChild_ = -1;
			
			// * `Sequence` succeeds when **all** of its children ends in `Status::Success`.
			return Status::Success;
		}

	public:
		Sequence() : IDX_RunningChild_{ -1 } {}

	private:
		int IDX_RunningChild_;
	};

	//class Parallel : public Node {};
}

namespace Lumina::Behavior {

	/// Tries to run **high-priority** children

	export class Selector : public Composite {
	public:
		auto Tick(Blackboard& blackboard_) -> Status override {
			for (size_t idx{ 0LLU }; idx < Children_.size(); ++idx) {
				auto& child{ Children_.at(idx) };
				Status status{ child->Tick(blackboard_) };

				// * Change of the index of the running child means
				// * the progress has been interrupted by a child with higher priority.
				bool const isInterrupted{
					(IDX_RunningChild_ != -1) &&
					(IDX_RunningChild_ != static_cast<int>(idx))
				};

				if (status == Status::Running) {
					if (isInterrupted) { child->OnTerminate(Status::Failure); }
					
					IDX_RunningChild_ = static_cast<int>(idx);
					
					return Status::Running;

				}
				else if (status == Status::Success) {
					if (isInterrupted) { child->OnTerminate(Status::Failure); }
					
					IDX_RunningChild_ = -1;
					
					return Status::Success;

				}

				// * `Selector` tries to run the next child
				// * if the current child ends in `Status::Failure` .
			}

			IDX_RunningChild_ = 0;
			
			// * `Selector` fails when **none** of its children
			// * ends in `Status::Success` or `Status::Running`.
			return Status::Failure;
		}

	public:
		Selector() : IDX_RunningChild_{ 0 } {}

	private:
		int IDX_RunningChild_;
	};
}