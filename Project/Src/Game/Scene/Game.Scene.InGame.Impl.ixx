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
import Game.PlayerInput;
import Game.Boss;

import Lumina.CG3D;
import Lumina.CG3D.Struct;
import Lumina.CG3D.Animation;

import Lumina.Skybox;

import Lumina.SimpleFX2;
import Lumina.SimpleFX3;

import Lumina.Phys;

import Game.AnimEventDemo;
import Game.AnimEventVfx;
import Game.CombatEvents;
import Game.CombatVfx;
import Game.SceneLight;

import Lumina.VsBanner;

import Game.PauseMenu;
import Game.DebugDock;
import Game.FighterSettings;
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

		explicit InGame(Lumina::SceneManager& sceneManager_);
		virtual ~InGame();

		// InGame を離れるとき（SceneManager が切り替えるとき。外側の Game::Scene::InGame::OnExit から）
		void OnExit();

	private:
		// シーンの持ち主（コンテキスト）。タイトルへ戻るときはこれに頼む（2026-11-26：グローバル変数 Game::Scene::Flag の代わり）
		Lumina::SceneManager* SceneManager_{ nullptr };
		// 戦闘を最初の状態に戻して、タイトルへの切り替えを頼む（ポーズメニューの Return to Title・勝利演出の暗転のあと）
		void ReturnToTitle();

	private:
		std::unique_ptr<Player> Player_{ nullptr };
		std::unique_ptr<TrackingCamera> PlayerCamera_{ nullptr };
		float HitCameraShake_{ 0.0f };		// ボスに当てたときのカメラの揺れ（当てた瞬間 1 / 弾かれ 0.6 → 毎フレーム 0.9 倍）。以前は Update.cpp のファイルスコープ変数 hitBoss

		std::unique_ptr<Game::Boss> Boss_Kinoko_{ nullptr };		// ボス（体＝Game.Fighter、脳＝Game.BossBrain）。名前は旧 Boss_Kinoko のまま（InGame を見直すときに変える）

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
		Lumina::Sprite UI_VICTORY_;			// 「Victory」（ResultRenderer_ で描く。位置と大きさは毎フレーム Victory_ の値）
		Lumina::Sprite UI_VICTORY_PROMPT_;	// 「Press Space to Return to Title」
		Lumina::Sprite UI_EXIT_FADE_;		// 暗転（勝利演出の続行）。全画面の黒。ResultRenderer_ の一番最後
		Lumina::Sprite UI_PlayerHPLabel_;	// プレイヤーの HP バーの左上の「Player」（Assets/UI/Label.HP.Player.png）
		Lumina::Sprite UI_BossHPLabel_;		// ボスの HP バーの左下の「Boss」（Assets/UI/Label.HP.Boss.png）
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
		// 被弾のポスエフェ（Basic.PS の色ずれ＋光。強さ = exp(−Time × 5〜7)）の経過時間。負 = 出ていない。
		// どちらかののけぞりの始まりで 0 から始め、のけぞりが終わっても進め続けて、HitPostFxEndTime で切る（2026-11-11）
		float HitPostFxTime_{ -1.0f };
		bool PrevPlayerKnockedBack_{ false };
		bool PrevBossKnockedBack_{ false };
		static constexpr float HitPostFxStep{ 0.01f };		// 1 フレームに進める量（以前と同じ）
		static constexpr float HitPostFxEndTime{ 1.0f };	// ここで切る（光 0.7·e^−7 ≈ 0.06%、色ずれ e^−5 ≈ 0.7%）

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
		Lumina::Mat4 World_BossHand_{};		// ボスの右手（Bone.024）のワールド行列。ボスの武器はこれに付く
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

		// プレイヤーの脳: 入力（カメラ基準）とアニメの状態から、体（Game.Fighter）へ渡す 1 フレームぶんを作る（InBattle.cpp）
		auto PlayerFrame(bool controlEnabled_) -> Game::FighterFrame;

		// 巻き戻し（LOSE で SPACE を押してから再開するまで）の始まりの位置と向き。終わりは初期配置（Startup.cpp の PlayerStartX など）
		float RewindPlayerFromX_{ 0.0f };
		float RewindPlayerFromZ_{ 0.0f };
		float RewindPlayerAngleFrom_{ 0.0f };
		float RewindCameraYawFrom_{ 0.0f };		// 視点の左右も同じ進みで初期の向き（プレイヤーの後ろ）へ戻す
		float RewindBossFromX_{ 0.0f };
		float RewindBossFromZ_{ 0.0f };
		float RewindBossRotateFrom_{ 0.0f };

		// プレイヤーのアニメの時刻とアニメーションイベント（当たり判定のタイミングのテスト）。
		// animTimer_ の代わりにこれが時刻を持つ。Debug では ImGui「AnimEvent Hitbox」で調整する
		Game::AnimEventDemo::PlayerAnimEvents PlayerAnimEvents_{};
		// ボスのアニメの時刻とアニメーションイベント（プレイヤーと同じクリップ・サイドカー）。Debug では ImGui「AnimEvent Hitbox (Boss)」
		Game::AnimEventDemo::PlayerAnimEvents BossAnimEvents_{};

		// エディタ（ImGui）の置き場。右の列にまとめ、メニュー「Editors」で表示を切り替える。F2 で全部隠す（Debug のみ）
		Game::DebugDock DebugDock_{};
		std::string PlayerSettingsStatus_{};		// ImGui「Player」の Save / Load の結果（Assets/Configs/Player.json）
		// ボスの攻撃を受けた結果の回数（ImGui「Player」に出す。並びは Game::HitResult と同じ Hit / Guarded / Deflected / Dodged）。
		// Dodged は「無敵の間にボスの武器が体に重なり始めた回数」（無敵の間は TryHit しないので、ReceiveHit からは数えられない）
		uint32_t PlayerHitCounts_[4]{};
		bool PlayerDodgingHitbox_{ false };			// 前のフレームに、無敵でボスの Hitbox をすり抜けていた（Dodged を窓ごとに 1 回だけ数えるため）

		// VfxLoop の窓からエフェクトを出す（今は ATK3 の刃先の放射光線だけ）。Debug では ImGui「AnimEvent VFX」で調整する
		Game::AnimEventVfx::VfxPresenter AnimEventVfx_{};

		// 戦闘の仮エフェクト（足元・ボスのオーラとダッシュ・剣の軌跡・ガード・命中・スキルの予兆）。命中などは Game.CombatEvents で受け取る。
		// Debug では ImGui「Combat VFX」で調整する（2026-11-11）
		Game::CombatVfx::Presenter CombatVfx_{};

		// 平行光 1 本＋環境光（点光源しか無くて暗すぎるのを和らげる）。MergePass で加算。Debug では ImGui「Scene Light」（2026-11-11）
		Game::SceneLight SceneLight_{};

		// バトル開始の合図（VS バナー）。STARTUP で再生し、終わったら IN_BATTLE へ。Debug では ImGui「VS Banner」で調整する
		Lumina::VsBanner::Presenter VsBanner_{};

		// ポーズメニュー（ESC）。開いている間はゲームの Update を丸ごと止める（Render は止まった状態を描き続け、その上に暗幕とメニューを重ねる）
		Game::PauseMenu::Menu PauseMenu_{};
		// プレイヤーとカメラの操作（入力の抽象化。行動はセキロ基準）。Update の頭で 1 回取り込み、プレイヤーは State() から読む
		Game::PlayerInput::Controls PlayerInput_{};
		Lumina::Vec2 PlayerMoveFrame_{ 0.0f, 1.0f };	// プレイヤーの移動の基準（カメラが地面の上で見ている向き）。Update で毎フレーム決める

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

		// 勝利演出（2026-11-17）。ゲームオーバーの仕組みを 2 台目で流用する：倒れて粉々になるのはボス、カメラは別のキーフレーム
		// （注視点をボスへ寄せられる）、色抜けも 2 台目（グラデーションなどを別に調整できる）、続行は暗転してタイトルへ。
		// Debug では ImGui「Victory Director」「Victory Camera」「Victory Grade」。粉々の見た目は「GameOver Shatter」と同じ（毎フレーム写す）
		Game::GameOverCamera::Rig VictoryCameraFx_{};
		Game::GameOverGrade::Effect VictoryGradeFx_{};
		// いま画面に掛ける色抜け（勝利演出の色抜けが動いていればそちら。エディタのプレビューも含む）
		auto ActiveGrade() -> Game::GameOverGrade::Effect& { return VictoryGradeFx_.IsActive() ? VictoryGradeFx_ : GradeFx_; }
		Game::GameOverDirector::Director Victory_{};
		Game::GameOverShatter::Effect BossShatter_{};

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