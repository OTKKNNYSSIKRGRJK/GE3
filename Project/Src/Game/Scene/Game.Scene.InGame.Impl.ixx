export module Game.Scene.InGame : Impl;

import <memory>;

import <vector>;

import Lumina;
import Lumina.Grassland;

import Lumina.DeferredLighting;

import Game.Player;
import Game.PlayerCamera;
import Game.Boss.Kinoko;

namespace Game::Scene::Impl {
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

	/// <summary>
	/// In-game scene implementation
	/// </summary>
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

	private:
		void InitializePlayerAndBoss(Lumina::DX12::Context const& dxContext_);
		void InitializeCanvas(Lumina::DX12::Context const& dxContext_);
		void InitializeRenderPass();
		void InitializeSpriteRenderer(Lumina::DX12::Context const& dxContext_);
		void InitializeSprites();
		void LoadImageTextures(
			Lumina::DX12::Context const& dxContext_,
			Lumina::AssetManager& assetMngr_
		);
		void InitializeMeshManager(Lumina::DX12::Context const& dxContext_);
		void InitializeMeshes(Lumina::DX12::Context const& dxContext_);
		void InitializeParticles(Lumina::DX12::Context const& dxContext_);
		void InitializeShaderConstants(Lumina::DX12::Context const& dxContext_);

	public:
		template<typename...ArgTypes>
		void Update(typename ArgTypes const&...args_);
		template<typename...ArgTypes>
		void Render(typename ArgTypes const&...args_);

	public:
		void Initialize(
			Lumina::DX12::Context const& dxContext_,
			Lumina::AssetManager& assetMngr_
		);

		InGame();
		virtual ~InGame();

	private:
		std::unique_ptr<Player> Player_{ nullptr };
		std::unique_ptr<TrackingCamera> PlayerCamera_{ nullptr };

		std::unique_ptr<Boss_Kinoko> Boss_Kinoko_{ nullptr };

		// Buffer for view-to-world matrix
		Lumina::DX12::UploadBuffer UB_ViewToWorld_{};
		// Buffer for screen-to-world matrix
		Lumina::DX12::UploadBuffer UB_ScreenToWorld_{};
		// Buffer for player material
		Lumina::DX12::UploadBuffer UB_PlayerMaterial_{};
		// Buffer for boss material
		Lumina::DX12::UploadBuffer UB_BossMaterial_{};

		MeshMaterial PlayerMaterial_{};
		MeshMaterial BossMaterial_{};

		// Shader-invisible heap for scene data
		Lumina::DX12::DescriptorHeap LocalHeap_Scene_{};
		// Shader-invisible heap for materials
		Lumina::DX12::DescriptorHeap LocalHeap_Materials_{};

		std::unique_ptr<Lumina::DeferredLighting> DeferredLighting_{ nullptr };
		Lumina::List<Lumina::PointLight> List_PointLight_{};
		Lumina::List<Lumina::Mat4> List_LocalToWorld_LightSphere_{};
		std::vector<uint32_t> Arr_Index_ActivePointLight_{};

		// Common root signature for particle systems
		Lumina::DX12::RootSignature RS_ParticleSystem_{};
		// Basic particle vertex shader
		Lumina::DX12::Shader VS_BasicParticle_{};
		// Basic particle pixel shader
		Lumina::DX12::Shader PS_BasicParticle_{};
		// Basic particle additive mode PSO
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
		// Mesh vertex shader used in the geometry pass
		Lumina::DX12::Shader VS_MeshDeferredGeometry_{};
		// Mesh pixel shader used in the geometry pass
		Lumina::DX12::Shader PS_MeshDeferredGeometry_{};
		// PSO used in the geometry pass
		Lumina::DX12::GraphicsPSO GraphicsPSO_MeshDeferredGeometry_{};

		// Contains albedo, normal and depth-stencil textures
		Lumina::DX12::Canvas Canvas_{};
		// Merges result of lighting and particles
		Lumina::DX12::Canvas Canvas_Merge_{};
		// Canvas for post-processing
		Lumina::DX12::Canvas Canvas_PostProcessing_{};

		Lumina::DX12::RenderPass DeferredGeometryPass_{};
		Lumina::DX12::RenderPass MergePass_{};
		Lumina::DX12::RenderPass PostProcessingPass_{};
		Lumina::DX12::RenderPass UIPass_{};

		// SRV table for image textures
		Lumina::DX12::DescriptorTable GlobalTable_SRV_ImageTexture_{};
		// SRV table for render/depth-stencil textures
		Lumina::DX12::DescriptorTable GlobalTable_SRV_CanvasTexture_{};

		// Buffer for post-processing constants
		Lumina::DX12::UploadBuffer UB_PostProcessingConstants_{};
		// Dummy buffer
		Lumina::DX12::UploadBuffer UB_Dummy_{};
		// CBV table for post-processing
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
	};
}