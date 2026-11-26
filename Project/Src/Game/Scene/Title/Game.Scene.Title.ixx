export module Game.Scene.Title;

import <memory>;
import <concepts>;

import Lumina;
import Game.Scene;

namespace Game::Scene {
	namespace Impl { class Title; }

	// Lumina::Scene の具象シーン。SceneManager（main.cpp）が持ち、今のシーンのときだけ Update / Render が呼ばれる。
	// 次のシーン（InGame）・終了は、Impl が SceneManager に頼む
	export class Title final : public Lumina::Scene {
	public:
		void Update() override;
		void Render() override;
		void OnEnter() override;		// InGame から戻ってきた：Outro を巻き戻し、メニューをやり直す

	public:
		template<typename...ArgTypes>
		void Initialize(typename ArgTypes const&...args_);

		Title(Lumina::SceneManager& manager_, FrameResources const& frame_);
		~Title() override;

	private:
		FrameResources Frame_;
		std::unique_ptr<Impl::Title> Impl_{ nullptr };
	};
}