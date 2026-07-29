module;

#include<memory>
#include<vector>
#include<string>
#include<unordered_map>

export module Game.Scene.Title : Impl;

import Lumina;

namespace Game::Scene::Impl {
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

		Title();
		virtual ~Title();
	};
}