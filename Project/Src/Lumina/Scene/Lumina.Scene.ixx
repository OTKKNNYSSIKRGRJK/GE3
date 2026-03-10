export module Lumina.Scene;

import <cstdint>;

import <vector>;
import <list>;
import <unordered_map>;

import <string>;
import <format>;

import Lumina.Mixins;

import Lumina.Container.List;

namespace Lumina {
	class Scene;
	class SceneManager;

	//////	//////	//////	//////	//////	//////

	export class Scene : public NonCopyable<Scene> {
	public:
		virtual void Update() = 0;
		virtual void Render() = 0;

	public:
		virtual ~Scene() = default;
	};

	template<typename T>
	concept Concept_Scene = std::is_base_of_v<Scene, T>;

	//////	//////	//////	//////	//////	//////

	export class SceneManager : public NonCopyable<SceneManager> {
	public:
		static constexpr int MaxNum_Scenes{ 128 };

	private:
		struct SceneNode {
			std::unique_ptr<Scene> Data{ nullptr };
		};

	public:
		static inline SceneManager& Instance() {
			static SceneManager inst{};
			return inst;
		}

	public:
		template<Concept_Scene SceneType, typename...ParameterTypes>
		void Load(std::string_view name_, ParameterTypes&&...params_) {
			if (LoadedScenes_.find(name_.data()) == LoadedScenes_.cend()) {
				LoadedScenes_.emplace(
					name_,
					SceneNode{
						.Data{ new SceneType{ params_... } }
					}
				);
			}
		}

		void Unload(std::string_view name_) {
			auto&& it_SceneNodeKV{ LoadedScenes_.find(name_.data()) };
			if (it_SceneNodeKV != LoadedScenes_.cend()) {
				it_SceneNodeKV->second.Data.reset(nullptr);
				LoadedScenes_.erase(it_SceneNodeKV);
			}
		}

		void Activate(std::string_view name_) {
			auto&& it_Scene{ LoadedScenes_.find(name_.data()) };
			if (it_Scene != LoadedScenes_.cend()) {
				ActiveScene_ = &it_Scene->second;
			}
		}

		void Update() {
			if (ActiveScene_ != nullptr) {
				ActiveScene_->Data->Update();
			}
		}

		void Render() {
			if (ActiveScene_ != nullptr) {
				ActiveScene_->Data->Render();
			}
		}

	public:
		void Initialize() {}
		void Finalize() {
			for (auto& kv : LoadedScenes_) {
				kv.second.Data.reset(nullptr);
			}
			LoadedScenes_.clear();
		}

	private:
		constexpr SceneManager() noexcept = default;
	public:
		~SceneManager() { Finalize(); }

	private:
		std::unordered_map<std::string, SceneNode> LoadedScenes_{};
		SceneNode* ActiveScene_{ nullptr };
	};
}