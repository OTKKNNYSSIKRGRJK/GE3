export module Game.Scene.Title;

import <memory>;

import Lumina;

namespace Game::Scene {
	namespace Impl { class Title; }

	export class Title {
	public:
		template<typename...ArgTypes>
		void Update(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Render(typename ArgTypes const&...args_);

	public:
		template<typename...ArgTypes>
		void Initialize(typename ArgTypes const&...args_);

		Title();
		virtual ~Title();

	private:
		std::unique_ptr<Impl::Title> Impl_{ nullptr };
	};
}