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
		enum FLAG {
			UPDATE = 0x1,
			RENDER = 0x2,
		};

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
			int32_t ActiveFlags{ 0 };
		};

	public:
		static inline SceneManager& Instance() {
			static SceneManager inst{};
			return inst;
		}

	public:
		template<Scene::FLAG SceneFlag>
		bool IsActive(std::string_view name_) {
			auto&& it{ LoadedScenes_.find(name_.data()) };
			return (
				(it != LoadedScenes_.cend()) &&
				(it->second.ActiveFlags & static_cast<int32_t>(SceneFlag))
			);
		}

	public:
		template<Concept_Scene SceneType, typename...ParameterTypes>
		void Load(std::string_view name_, ParameterTypes&&...params_) {
			if (LoadedScenes_.find(name_.data()) == LoadedScenes_.cend()) {
				LoadedScenes_.emplace(
					name_,
					SceneNode{
						.Data{ new SceneType{ params_... } },
						/*.ActiveFlags{
							static_cast<int32_t>(Scene::UPDATE) |
							static_cast<int32_t>(Scene::RENDER)
						}*/
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

		template<Scene::FLAG SceneFlag>
		void Activate(std::string_view name_) {
			ActivationQueue_.emplace_back(name_, SceneFlag);
		}

		template<Scene::FLAG SceneFlag>
		void Deactivate(std::string_view name_) {
			DeactivationQueue_.emplace_back(name_, SceneFlag);
		}

		void Update() {
			for (auto const& activation : ActivationQueue_) {
				auto const& name{ activation.first };
				auto const& sceneFlag{ activation.second };
				auto&& it_Scene{ LoadedScenes_.find(name) };
				if (it_Scene != LoadedScenes_.cend()) {
					it_Scene->second.ActiveFlags |= static_cast<int32_t>(sceneFlag);
				}
			}
			ActivationQueue_.clear();

			for (auto const& deactivation : DeactivationQueue_) {
				auto const& name{ deactivation.first };
				auto const& sceneFlag{ deactivation.second };
				auto&& it_Scene{ LoadedScenes_.find(name) };
				if (it_Scene != LoadedScenes_.cend()) {
					it_Scene->second.ActiveFlags &= ~(static_cast<int32_t>(sceneFlag));
				}
			}
			DeactivationQueue_.clear();
		}

		void UpdateActive() {
			for (auto& kv : LoadedScenes_) {
				auto& sceneNode{ kv.second };
				if (sceneNode.ActiveFlags & static_cast<int32_t>(Scene::UPDATE)) {
					sceneNode.Data->Update();
				}
			}
		}

		void RenderActive() {
			for (auto& kv : LoadedScenes_) {
				auto& sceneNode{ kv.second };
				if (sceneNode.ActiveFlags & static_cast<int32_t>(Scene::RENDER)) {
					sceneNode.Data->Render();
				}
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

		std::vector<std::pair<std::string, Scene::FLAG>> ActivationQueue_{};
		std::vector<std::pair<std::string, Scene::FLAG>> DeactivationQueue_{};
	};
}