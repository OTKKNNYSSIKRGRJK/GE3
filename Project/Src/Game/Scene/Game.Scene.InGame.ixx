export module Game.Scene.InGame;

import <memory>;

import Lumina;

namespace Game::Scene {
	namespace Impl { class InGame; }

	export class InGame {
	public:
		template<typename...ArgTypes>
		void Update(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Render(typename ArgTypes const&...args_);

	public:
		template<typename...ArgTypes>
		void Initialize(typename ArgTypes const&...args_);

		InGame();
		virtual ~InGame();

	private:
		std::unique_ptr<Impl::InGame> Impl_{ nullptr };
	};
}