//////	//////	//////	//////	//////	//////
//	Lumina.Scene - シーンと SceneManager	//
//////	//////	//////	//////	//////	//////
// State パターン（2026-11-26 に作り直し）
//   コンテキスト = SceneManager（今のシーンを 1 つ持ち、Update / Render をそれに任せる）
//   状態         = Scene（具象シーンが継承する）。次にどのシーンへ行くかはシーン自身が決め、
//                  自分の持つ SceneManager への参照を通して RequestChange<次のシーン>() で頼む。
//                  外から文字列のキーで有効・無効を切り替えることはしない。
//
// 使い方
//   Lumina::SceneManager manager{};
//   auto& title{ manager.Add<MyTitle>(引数...) };		// 具象シーンのコンストラクタの第 1 引数は SceneManager&
//   manager.RequestChange<MyTitle>();					// 最初のシーン
//   manager.ApplyChange();
//   毎フレーム: manager.Update(); manager.Render(); …描画の後で… manager.ApplyChange();
//
// 切り替えはフレームの切れ目（ApplyChange）でまとめて行う。シーンの Update の中で頼んでも、そのフレームの Render は今のシーンのまま。
// 切り替えるときは 今のシーンの OnExit → 次のシーンの OnEnter の順に呼ぶ。

export module Lumina.Scene;

import <memory>;
import <type_traits>;
import <unordered_map>;
import <utility>;

import Lumina.Mixins;

namespace Lumina {
	export class Scene;
	export class SceneManager;

	// シーンの種類を表すキー（型ごとに 1 つの静的変数のアドレス。文字列を使わない）
	// ※ const にしない（読み取り専用だと、リンカの同一 COMDAT の畳み込み /OPT:ICF で型どうしのアドレスが重なりうるため）
	using SceneKey = void const*;
	template<typename T>
	struct SceneKeyTag {
		static inline char Value{ 0 };
	};
	template<typename T>
	constexpr auto SceneKeyOf() noexcept -> SceneKey { return &SceneKeyTag<T>::Value; }

	//////	//////	//////	//////	//////	//////

	export class Scene : public NonCopyable<Scene> {
	public:
		virtual void Update() = 0;
		virtual void Render() = 0;

		// SceneManager が切り替えるときに呼ぶ（既定は何もしない）
		virtual void OnEnter() {}
		virtual void OnExit() {}

	public:
		virtual ~Scene() = default;

	protected:
		explicit Scene(SceneManager& manager_) noexcept : Manager_{ &manager_ } {}

		// 自分の持ち主（コンテキスト）。次のシーンへの切り替え・終了はこれに頼む
		auto Manager() const noexcept -> SceneManager& { return *Manager_; }

	private:
		SceneManager* Manager_{ nullptr };
	};

	template<typename T>
	concept Concept_Scene = std::is_base_of_v<Scene, T>;

	//////	//////	//////	//////	//////	//////

	export class SceneManager : public NonCopyable<SceneManager> {
	public:
		// シーンを作って持つ（まだ今のシーンにはしない）。同じ型をもう一度 Add したら作り直す
		template<Concept_Scene SceneType, typename...ParameterTypes>
		auto Add(ParameterTypes&&...params_) -> SceneType& {
			auto scene{ std::make_unique<SceneType>(*this, std::forward<ParameterTypes>(params_)...) };
			SceneType& ref{ *scene };
			auto const key{ SceneKeyOf<SceneType>() };
			if (Current_ == Scenes_[key].get()) { Current_ = nullptr; }
			Scenes_[key] = std::move(scene);
			return ref;
		}

		// 次のフレームの切れ目で SceneType へ切り替える（シーン自身が呼ぶ。最後に頼んだものが有効）
		template<Concept_Scene SceneType>
		void RequestChange() noexcept { Next_ = SceneKeyOf<SceneType>(); }

		// アプリの終了を頼む（ウィンドウを閉じるのは呼び出し側）
		void RequestQuit() noexcept { IsQuitRequested_ = true; }
		auto IsQuitRequested() const noexcept -> bool { return IsQuitRequested_; }

		template<Concept_Scene SceneType>
		auto IsCurrent() const noexcept -> bool {
			auto const it{ Scenes_.find(SceneKeyOf<SceneType>()) };
			return it != Scenes_.cend() && it->second.get() == Current_;
		}

		// 頼まれていた切り替えを行う（フレームの切れ目で 1 回）
		void ApplyChange() {
			if (Next_ == nullptr) { return; }
			auto const it{ Scenes_.find(Next_) };
			Next_ = nullptr;
			if (it == Scenes_.cend() || it->second.get() == Current_) { return; }

			if (Current_ != nullptr) { Current_->OnExit(); }
			Current_ = it->second.get();
			Current_->OnEnter();
		}

		void Update() { if (Current_ != nullptr) { Current_->Update(); } }
		void Render() { if (Current_ != nullptr) { Current_->Render(); } }

	public:
		void Finalize() noexcept {
			Current_ = nullptr;
			Next_ = nullptr;
			Scenes_.clear();
		}

		SceneManager() noexcept = default;
		~SceneManager() { Finalize(); }

	private:
		std::unordered_map<SceneKey, std::unique_ptr<Scene>> Scenes_{};
		Scene* Current_{ nullptr };
		SceneKey Next_{ nullptr };
		bool IsQuitRequested_{ false };
	};
}
