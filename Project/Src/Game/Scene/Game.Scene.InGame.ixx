export module Game.Scene.InGame;

import <memory>;

import Lumina;
import Game.Scene;

namespace Game::Scene {
	namespace Impl { class InGame; }

	// Lumina::Scene の具象シーン。SceneManager（main.cpp）が持ち、今のシーンのときだけ Update / Render が呼ばれる。
	// タイトルへ戻るのは、Impl が SceneManager に頼む
	export class InGame final : public Lumina::Scene {
	public:
		void Update() override;
		void Render() override;
		void OnExit() override;			// 離れるとき、次に入ったときの入力を Sync からにする（タイトルで押していたキーで行動が出ないように）

	public:
		template<typename...ArgTypes>
		void Initialize(typename ArgTypes const&...args_);

		InGame(Lumina::SceneManager& manager_, FrameResources const& frame_);
		~InGame() override;

	private:
		FrameResources Frame_;
		std::unique_ptr<Impl::InGame> Impl_{ nullptr };
	};
}