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

import Lumina.Skybox;

import Lumina.SimpleFX2;
import Lumina.SimpleFX3;

import Lumina.Phys;

import Game.AnimEventDemo;
import Game.AnimEventVfx;

import Lumina.VsBanner;

import Game.PauseMenu;
import Game.GameOverGrade;
import Game.GameOverCamera;
import Game.GameOverDirector;
import Game.GameOverShatter;

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
		void Update(ArgTypes&&...args_);
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
		std::unique_ptr<ParticleSystem<Particle>> BossBullets_{ nullptr };

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
		// 結果表示（LOSE）専用。色抜けとぼかしの後ろ（バックバッファへ直描き）に描くので、HP バーなどとは別のレンダラ
		// （SpriteRenderer は 1 フレーム 1 バッチ前提）
		std::unique_ptr<Lumina::SpriteRenderer> ResultRenderer_{ nullptr };
		//Lumina::Sprite UI_StartButton_;
		//Lumina::Sprite UI_ExitButton_;
		Lumina::Sprite UI_PlayerHPBar_;
		Lumina::Sprite UI_BossHPBar_;
		Lumina::Sprite UI_WIN_;
		Lumina::Sprite UI_LOSE_;
		Lumina::Sprite UI_GAMEOVER_;		// 「Game Over」（ResultRenderer_ で描く。位置と大きさは毎フレーム Director の値）
		Lumina::Sprite UI_GAMEOVER_PROMPT_;	// 「Press Space to Continue」
		float UI_PlayerHPValue_;
		float MaxWidth_UI_PlayerHPBar_;
		float MaxWidth_UI_BossHPBar_;
		//int SelectedButton_;

		Lumina::DX12::GraphicsPSO PSO_Sprite_{};
		Lumina::DX12::Shader VS_Sprite_{};
		Lumina::DX12::Shader PS_Sprite_{};

		Lumina::DX12::GraphicsPSO PSO_SpriteUI_{};
		Lumina::DX12::GraphicsPSO PSO_SpriteResult_{};		// ResultRenderer_ の RS で作る（PSO_SpriteUI_ と同じ設定）
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
		Lumina::DX12::UploadBuffer UB_Transforms2_;

		Lumina::DX12::DescriptorTable GlobalTable_CBV_Scene_;

		std::unique_ptr<SkinnedModel> PlayerSkinnedModel_;
		std::unique_ptr<SkinnedInstance> PlayerSkinnedInstance_;
		std::unique_ptr<SkinnedInstance> BossSkinnedInstance_;

		using AnimationDatabase = std::unordered_map<std::string, Lumina::CG3D::MyAnimation>;
		AnimationDatabase animDatabase_;
		Lumina::CG3D::MyAnimation* currentAnim_ = nullptr;
		Lumina::CG3D::MyAnimation* currentAnim_Boss_ = nullptr;
		std::string currentAnimName_;
		std::string currentAnimName_Boss_;
		float animTimer_ = 0.0f;
		float animTimer_Boss_ = 0.0f;
		bool IsAnimationLoop_ = 0;
		bool IsAnimationLoop_Boss_ = 0;
		Lumina::Mat4 World_Hands[2]{};
		Lumina::Vec4 WorldPos_Weapon_{};

		void PlayAnimation(
			Lumina::CG3D::MyAnimation*& curAnim_,
			std::string& curName_,
			float& timer_,
			bool& isAnimLoop_,
			std::string_view name_,
			bool isLoop_ = false
		);

		int playerATKFlag = 0;

		// バトルを最初から（STARTUP の VS バナーから）やり直す。WIN / LOSE の SPACE と、ポーズメニューの Restart / Return to Title から呼ぶ
		void RestartBattle();

		// 巻き戻し（LOSE で SPACE を押してから再開するまで）の始まりの位置と向き。終わりは初期配置（Startup.cpp の PlayerStartX など）
		float RewindPlayerFromX_{ 0.0f };
		float RewindPlayerFromZ_{ 0.0f };
		float RewindPlayerAngleFrom_{ 0.0f };
		float RewindBossFromX_{ 0.0f };
		float RewindBossFromZ_{ 0.0f };
		float RewindBossRotateFrom_{ 0.0f };

		// プレイヤーのアニメの時刻とアニメーションイベント（当たり判定のタイミングのテスト）。
		// animTimer_ の代わりにこれが時刻を持つ。Debug では ImGui「AnimEvent Hitbox」で調整する
		Game::AnimEventDemo::PlayerAnimEvents PlayerAnimEvents_{};

		// VfxLoop の窓からエフェクトを出す（今は ATK3 の刃先の放射光線だけ）。Debug では ImGui「AnimEvent VFX」で調整する
		Game::AnimEventVfx::VfxPresenter AnimEventVfx_{};

		// バトル開始の合図（VS バナー）。STARTUP で再生し、終わったら IN_BATTLE へ。Debug では ImGui「VS Banner」で調整する
		Lumina::VsBanner::Presenter VsBanner_{};

		// ポーズメニュー（ESC）。開いている間はゲームの Update を丸ごと止める（Render は止まった状態を描き続け、その上に暗幕とメニューを重ねる）
		Game::PauseMenu::Menu PauseMenu_{};

		// ゲームオーバー（LOSE）の色抜け演出（グラデーションマップ＋ビネット）。演出中だけ UIPass を中間 RT に描き、その後ろでバックバッファへ写す。
		// Debug では ImGui「GameOver Grade」で調整する
		Game::GameOverGrade::Effect GradeFx_{};

		// ゲームオーバーのカメラワーク（キーフレーム）。UpdatePlayerCamera の直後に Apply して View / VP / FOV を上書きする。
		// Debug では ImGui「GameOver Camera」で調整する
		Game::GameOverCamera::Rig CameraFx_{};

		// ゲームオーバー演出の時計。色抜け・カメラ・倒れるモーション・結果表示・再開の解禁をひとつの時刻で動かす
		// （致命打の瞬間に Start()。色抜けとカメラの Start / Reset もここが呼ぶ）。Debug では ImGui「GameOver Director」
		Game::GameOverDirector::Director GameOver_{};

		// 倒れた体が粉々になって消える演出（ディゾルブ＋グループ別の粒子）。時刻は GameOver_ の経過秒。Debug では ImGui「GameOver Shatter」
		Game::GameOverShatter::Effect Shatter_{};

		//----	Skybox							----//
		//----	------	------	------	------	----//

		std::unique_ptr<Lumina::Skybox> Skybox_;

		std::unique_ptr<Lumina::SimpleFX2> SimpleFX2_;
		std::unique_ptr<Lumina::SimpleFX3> SimpleFX3_;

		//----	Collision						----//
		//----	------	------	------	------	----//

		Lumina::Phys::CollisionWorld CollisionWorld_{};
		Lumina::Phys::DebugRenderer CollisionDebug_{};
		bool ShowColliders_{ true };
	};
}