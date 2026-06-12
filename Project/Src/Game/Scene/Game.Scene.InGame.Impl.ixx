module;

#include<memory>
#include<vector>
#include<string>
#include<unordered_map>

export module Game.Scene.InGame : Impl;

import Lumina;
import Lumina.Grassland;

import Lumina.DeferredLighting;

import Game.Player;
import Game.PlayerCamera;
import Game.Boss.Kinoko;

import Lumina.CG3D;
import Lumina.CG3D.Struct;
import Lumina.CG3D.Animation;

namespace Game::Scene::Impl {
	struct SkinnedModel {
		Lumina::CG3D::Collection Collection_;

		Lumina::DX12::UploadBuffer VertexBuffer_;
		Lumina::DX12::UploadBuffer IndexBuffer_;
		Lumina::DX12::VBV VBV_;
		Lumina::DX12::IBV IBV_;
	};

	struct SkinnedInstance {
		Lumina::CG3D::Skeleton Skeleton_;
		Lumina::CG3D::SkinCluster SkinCluster_;

		Lumina::Float3 MeshScale_;
		Lumina::Float3 MeshRotate_;
		Lumina::Float3 MeshTranslate_;
	};

	struct PlayerBullet : public Particle {
		float ATK;
	};

	struct MeshMaterial {
		Lumina::Float4 RGBA{ 1.0f, 1.0f, 1.0f, 1.0f };
		uint32_t ID_DiffuseMap;
		uint32_t ID_SpecularMap;
		uint32_t ID_NormalMap;
	};

	struct PostProcessingConstants {
		Lumina::Float2 PlayerNDCPos;
		float Time;
		uint32_t IsEnemySuccessfullyAttacking;
	};

	export class InGame {
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

		InGame();
		virtual ~InGame();

	private:
		std::unique_ptr<Player> Player_{ nullptr };
		std::unique_ptr<TrackingCamera> PlayerCamera_{ nullptr };

		std::unique_ptr<Boss_Kinoko> Boss_Kinoko_{ nullptr };

		Lumina::DX12::UploadBuffer UB_ViewToWorld_{};
		Lumina::DX12::UploadBuffer UB_ScreenToWorld_{};
		Lumina::DX12::UploadBuffer UB_PlayerMaterial_{};
		Lumina::DX12::UploadBuffer UB_BossMaterial_{};

		MeshMaterial PlayerMaterial_{};
		MeshMaterial BossMaterial_{};

		Lumina::DX12::DescriptorHeap LocalHeap_Scene_{};
		Lumina::DX12::DescriptorHeap LocalHeap_Materials_{};

		std::unique_ptr<Lumina::DeferredLighting> DeferredLighting_{ nullptr };
		Lumina::List<Lumina::PointLight> List_PointLight_{};
		Lumina::List<Lumina::Mat4> List_LocalToWorld_LightSphere_{};
		std::vector<uint32_t> Arr_Index_ActivePointLight_{};

		Lumina::DX12::RootSignature RS_ParticleSystem_{};
		Lumina::DX12::Shader VS_BasicParticle_{};
		Lumina::DX12::Shader PS_BasicParticle_{};
		Lumina::DX12::GraphicsPSO GraphicsPSO_BasicParticle_AdditiveMode_{};
		std::unique_ptr<ParticleSystem<Particle>> AmbientSparkles_{ nullptr };
		std::unique_ptr<ParticleSystem<Particle>> PlayerEffects_{ nullptr };
		std::unique_ptr<ParticleSystem<Particle>> KnockEffects_{ nullptr };

		std::unique_ptr<ParticleSystem<PlayerBullet>> PlayerBullets_{ nullptr };

		std::unique_ptr<Lumina::Grassland> Grassland_{ nullptr };

		std::unique_ptr<Lumina::PrimitiveManager> PrimitiveManager_{ nullptr };
		std::unique_ptr<Lumina::PrimitiveManager> PrimitiveManager1_{ nullptr };
		std::unique_ptr<Lumina::PrimitiveManager> PrimitiveManager2_{ nullptr };

		std::unique_ptr<Lumina::MeshManager> MeshManager_{ nullptr };
		std::vector<Lumina::MeshShaderAsset> MeshShaderAssets_{};
		Lumina::DX12::Shader VS_MeshDeferredGeometry_{};
		Lumina::DX12::Shader PS_MeshDeferredGeometry_{};
		Lumina::DX12::GraphicsPSO GraphicsPSO_MeshDeferredGeometry_{};

		Lumina::DX12::Canvas Canvas_{};
		Lumina::DX12::Canvas Canvas_Merge_{};
		Lumina::DX12::Canvas Canvas_PostProcessing_{};
		Lumina::DX12::RenderPass DeferredGeometryPass_{};
		Lumina::DX12::RenderPass MergePass_{};
		Lumina::DX12::RenderPass PostProcessingPass_{};
		Lumina::DX12::RenderPass UIPass_{};

		Lumina::DX12::DescriptorTable GlobalTable_SRV_ImageTexture_{};
		Lumina::DX12::DescriptorTable GlobalTable_SRV_CanvasTexture_{};

		Lumina::DX12::UploadBuffer UB_PostProcessingConstants_{};
		Lumina::DX12::UploadBuffer UB_Dummy_{};
		Lumina::DX12::DescriptorTable GlobalTable_CBV_PostProcessing_{};

		int CurrentPhase_{};
		int NextPhaseCountDown_{};

		//----	Sprite							----//
		//----	------	------	------	------	----//

		std::unique_ptr<Lumina::SpriteRenderer> SpriteRenderer_{ nullptr };
		//Lumina::Sprite UI_StartButton_;
		//Lumina::Sprite UI_ExitButton_;
		Lumina::Sprite UI_PlayerHPBar_;
		Lumina::Sprite UI_BossHPBar_;
		Lumina::Sprite UI_WIN_;
		Lumina::Sprite UI_LOSE_;
		float UI_PlayerHPValue_;
		float MaxWidth_UI_PlayerHPBar_;
		float MaxWidth_UI_BossHPBar_;
		//int SelectedButton_;

		Lumina::DX12::GraphicsPSO PSO_Sprite_{};
		Lumina::DX12::Shader VS_Sprite_{};
		Lumina::DX12::Shader PS_Sprite_{};

		Lumina::DX12::GraphicsPSO PSO_SpriteUI_{};
		Lumina::DX12::Shader PS_SpriteUI_{};

		Lumina::DX12::UploadBuffer UB_OrthoProj_{};
		Lumina::DX12::DescriptorHeap LocalHeap_OrthoProj_{};

		PostProcessingConstants PPConstants_{};

		enum Phase {
			PHASE_TITLE,
			PHASE_STARTUP,
			PHASE_IN_BATTLE,
			PHASE_WIN,
			PHASE_LOSE,
		};

	private:
		Lumina::DX12::RootSignature RS_Skinning_;
		Lumina::DX12::Shader VS_SkinnedMeshDeferredGeometry_;
		Lumina::DX12::Shader PS_SkinnedMeshDeferredGeometry_;
		Lumina::DX12::GraphicsPSO GraphicsPSO_SkinnedMeshDeferredGeometry_;

		Lumina::DX12::DescriptorTable GlobalTable_Materials_;
		Lumina::DX12::UploadBuffer UB_Transforms_;

		Lumina::DX12::DescriptorTable GlobalTable_CBV_Scene_;

		std::unique_ptr<SkinnedModel> PlayerSkinnedModel_;
		std::unique_ptr<SkinnedInstance> PlayerSkinnedInstance_;

		using AnimationDatabase = std::unordered_map<std::string, Lumina::CG3D::MyAnimation>;
		AnimationDatabase animDatabase_;
		Lumina::CG3D::MyAnimation* currentAnim_ = nullptr;
		std::string currentAnimName_;
		float animTimer_ = 0.0f;
		bool IsAnimationLoop_ = 0.0f;

		void PlayAnimation(
			std::string_view name_,
			bool isLoop_ = false
		);
	};
}