//////	//////	//////	//////	//////	//////
//	GameOverShatter							//
//////	//////	//////	//////	//////	//////
// ゲームオーバーで、倒れるプレイヤーが粉々になって消えていく演出。
//   * ディゾルブ：体がノイズに沿ってまんべんなく削れて消える。消える縁は光る。ピクセルシェーダ（MeshSkinning.PS.hlsl）で clip する。
//   * 粒子：削れた所から、複数のグループ（Dust / Ash / Embers / Mist）の粒子が舞う。グループごとに別の ParticleSystem（同じ加算 PSO）。
// 同期：粒子は「いま消えた場所」から出す。ディゾルブと同じノイズ式を C++ でも計算して（DissolveNoise。HLSL と式を揃えてある）、
//       体表面のランダムな点（三角形の面積で重みづけ）が、閾値のすぐ後ろ（消えたばかり）〜すぐ前（縁）に入っているときだけ粒子にする。
//       点は現在のポーズ（スキニング後）の位置で、法線方向に飛ばす。グループごとに「閾値からどれだけ後ろ／前を採用するか」を変えられる。
// 時間：Director（Game.GameOverDirector）の経過秒を Update に渡す。DissolveDelay 秒後から DissolveDuration 秒かけて閾値が 0 → 1 へ進む。
//       ゲームの進行中（running）のときだけ粒子を出す。スクラブ中は閾値だけ動く（粒子は出ない）。時刻が巻き戻ったら粒子は消す。
// 描画：シェーダへは SkinnedMeshConstants（12 個のルート定数）で渡す。ボス（同じシェーダ）は NoDissolveConstants で何も消えない。
//       縁の光りは、点光源で照らされる前のアルベドに色を入れるだけなので、周りのライト次第。火の粉の粒子が主役。
//       粒子は既定で色抜けを掛けない（Group::BypassGrade）。色抜けのあとにバックバッファへ直接描くので、粒子の色がそのまま見える（体は色抜けされる）。
// 調整：ImGui「GameOver Shatter」（Debug のみ）。Assets/Configs/GameOverShatter.json に保存し、Initialize で読み込む（無いキーは初期値）。

export module Game.GameOverShatter;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <array>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <memory>;
import <span>;
import <string>;
import <vector>;
import <d3d12.h>;

import Lumina;
import Lumina.CG3D.Struct;

export namespace Game::GameOverShatter {
	inline constexpr uint32_t NumGroups{ 4U };
	inline constexpr uint32_t NumConstants{ 12U };		// MeshSkinning.PS.hlsl のルート定数の数（スロット 6）

	// MeshSkinning.PS.hlsl の cbuffer Parameter_Space17Slot0 と同じ並び
	struct SkinnedMeshConstants {
		Lumina::Float3 CameraPos;
		float Threshold;			// -1 未満ならディゾルブなし
		float NoiseScale;
		float NoiseContrast;
		float EdgeWidth;
		float EdgeBoost;
		Lumina::Float3 EdgeColor;
		uint32_t Seed;
	};
	static_assert(sizeof(SkinnedMeshConstants) == NumConstants * sizeof(uint32_t));

	// 粒子をどこで描くか（Render に渡す）。色抜けを掛けるかどうかで描く場所が違う
	enum class Layer : int {
		Graded,		// MergePass（色抜けの前）。グラデーションマップとビネットが掛かる
		Direct,		// 色抜けのあと、バックバッファへ直接。色抜けの影響を受けない（Group::BypassGrade が true のグループ）
	};

	// 1 グループの粒子。長さの単位はワールド、時間は秒
	struct Group {
		std::string Name{};
		bool Enabled{ true };
		bool BypassGrade{ true };			// true なら色抜け（グラデーションマップ・ビネット）を掛けず、元の色のまま描く
		uint32_t AtlasID{ 0U };				// Particles.png の何番目か（0 丸いぼかし 1 楕円 2 星 3 ぼかし星 4 十字の輝き 5 小さい輝き 6 ざらついた欠片 7 円 8 縦グラデ）

		// どこから出すか
		float Rate{ 600.0f };				// 1 秒あたりの「候補の点」の数。候補のうち、下の帯に入ったものだけが粒子になる
		bool Continuous{ false };			// true なら、ディゾルブが終わったあとも（リスタートまで）出し続ける。出す場所は「もう消えた所すべて」
		float BandBehind{ 0.15f };			// 閾値より後ろ（もう消えた側）をどれだけ採用するか（ノイズ値の幅 0..1）。Continuous のときは使わない
		float BandAhead{ 0.0f };			// 閾値より前（まだ見えている縁の側）をどれだけ採用するか

		// 粒子
		float LifeMin{ 1.5f };
		float LifeMax{ 3.0f };
		float SizeMin{ 0.1f };
		float SizeMax{ 0.2f };
		float SpeedMin{ 0.2f };				// 出るときの速さ（体の表面の法線方向）
		float SpeedMax{ 0.8f };
		float Rise{ 0.5f };					// 上向きの加速度。負なら落ちる
		float Drag{ 1.0f };					// 速度の減衰（1/秒）。Rise / Drag が終端速度
		float Turbulence{ 1.0f };			// ゆらぎの強さ（加速度）
		float TurbulenceFreq{ 1.5f };		// ゆらぎの速さ（Hz に近い）
		float SpinMax{ 0.0f };				// 回転の最大角速度（ラジアン/秒）
		Lumina::Float3 ColourA{ 0.9f, 0.85f, 0.8f };	// 出るときに A と B の間でランダム
		Lumina::Float3 ColourB{ 0.9f, 0.85f, 0.8f };
		float Alpha{ 0.5f };				// 加算合成の強さ
		float FadeIn{ 0.1f };				// 寿命のうち、現れるまでの割合
		float FadeOut{ 0.6f };				// 寿命のうち、消えていく割合
		float EndScale{ 0.4f };				// 寿命の終わりの大きさ（出たときの何倍か）
		float Flicker{ 0.0f };				// 明滅の深さ（0..1）
	};

	auto DefaultGroups() -> std::array<Group, NumGroups>;

	struct Style {
		// ディゾルブ（Director の秒）
		float DissolveDelay{ 2.2f };
		float DissolveDuration{ 2.6f };
		float NoiseFrequency{ 5.0f };		// 体の大きさの間にノイズの格子が何個並ぶか（大きいほど細かく削れる）
		float NoiseContrast{ 2.2f };		// ノイズ値のコントラスト（値が真ん中に偏るのを、時間に対して均すため）
		float EdgeWidth{ 0.06f };			// 消える縁の幅（ノイズ値）
		Lumina::Float3 EdgeColor{ 1.0f, 0.55f, 0.18f };
		float EdgeBoost{ 1.0f };
		int Seed{ 7 };

		std::array<Group, NumGroups> Groups{ DefaultGroups() };
	};

	// 粒子 1 個ぶんの追加情報（ParticleSystem は Particle を継承した型を扱える）
	struct ShardParticle : Particle {
		float Age{ 0.0f };
		float MaxLife{ 1.0f };
		float BaseSize{ 1.0f };
		float BaseAlpha{ 1.0f };
		float Seed{ 0.0f };
		float Spin{ 0.0f };
		Lumina::Float3 Colour{ 1.0f, 1.0f, 1.0f };
	};

	class Effect {
	public:
		// mesh_ / 影響の読み取り用。mesh_ は呼び出し側が持ち続けること（覚えるだけ）。skinCluster_ は CreateSkinCluster 済みのもの
		void Initialize(
			Lumina::DX12::Context const& dx12Context_,
			Lumina::CG3D::Mesh const& mesh_,
			Lumina::CG3D::SkinCluster const& skinCluster_
		);

		void Reset();		// 粒子を全部消す

		// 毎フレーム 1 回（ParticleSystem の更新が GPU 用データの転送コマンドを積むので、パスの外で）。
		//   elapsed_ … Director の経過秒、active_ … 演出中か（Director::IsActive）、running_ … ゲームの進行中か（スクラブ中は false）
		//   meshWorld_ … プレイヤーのメッシュのワールド行列（Render.cpp の meshWorld と同じ）
		void Update(
			Lumina::DX12::CommandList const& cmdList_,
			Lumina::Mat4 const& viewToWorld_,
			float elapsed_,
			bool active_,
			bool running_,
			Lumina::Mat4 const& meshWorld_,
			Lumina::CG3D::SkinCluster const& skinCluster_
		);

		// layer_ に属するグループだけを描く（Graded は MergePass の中、Direct は色抜けのあとでバックバッファへ）。
		// Direct のときは、呼び出し側が先にバックバッファを描画先にして、ビューポート／シザーを設定しておくこと
		void Render(
			Layer layer_,
			Lumina::DX12::CommandList const& cmdList_,
			Lumina::DX12::RootSignature const& rs_,
			Lumina::DX12::GraphicsPSO const& graphicsPSO_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_SceneVars_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_VP_,
			Lumina::DX12::DescriptorTable const& globalTable_Textures_,
			Lumina::DX12::DescriptorTable const& globalTable_Textures2_
		);

		auto PlayerConstants(Lumina::Vec3 const& cameraPos_) const -> SkinnedMeshConstants;
		static auto NoDissolveConstants(Lumina::Vec3 const& cameraPos_) -> SkinnedMeshConstants;

		auto DissolveProgress(float elapsed_) const -> float;	// 0..1
		auto TotalTime() const -> float { return Look.DissolveDelay + Look.DissolveDuration; }

		void DrawEditor();					// ImGui「GameOver Shatter」（Debug のみ）

		auto Load() -> bool;				// Look を設定ファイルから読む（あるキーだけ上書き）
		auto Save() -> bool;

	public:
		Style Look{};
		static constexpr char const* ConfigPath{ "Assets/Configs/GameOverShatter.json" };

	private:
		auto NoiseScale() const -> float { return Look.NoiseFrequency / BodySize_; }
		auto Threshold(float progress_) const -> float;
		auto Random01() -> float;
		void TryEmit(
			uint32_t groupIndex_,
			float threshold_,
			Lumina::Mat4 const& meshWorld_,
			Lumina::CG3D::SkinCluster const& skinCluster_
		);

	private:
		bool Ready_{ false };
		Lumina::CG3D::Mesh const* Mesh_{ nullptr };
		std::vector<Lumina::CG3D::VertexInfluence> Influences_{};
		std::vector<float> AreaCdf_{};		// 三角形の面積の累積（バインドポーズ）
		float BodySize_{ 1.0f };			// メッシュの一番長い辺（バインドポーズ）。ノイズ座標 = 位置 × NoiseFrequency / BodySize_

		std::array<std::unique_ptr<ParticleSystem<ShardParticle>>, NumGroups> Systems_{};
		std::array<float, NumGroups> Accumulator_{};

		float Elapsed_{ 0.0f };
		float LastElapsed_{ 0.0f };
		bool Active_{ false };
		uint32_t RngState_{ 0x9E3779B9U };

		// エディタの状態（保存しない）
		std::string Status_{};
	};
}
