//////	//////	//////	//////	//////	//////
//	AnimEvent demo - VFX from VfxLoop windows	//
//////	//////	//////	//////	//////	//////
// PlayerAnimEvents の VfxLoop の窓（VfxTrack）を読んで、エフェクトを出すテスト。今は刃先の放射光線（"TipRays" @ "WeaponTip"）だけ。
//
// 役割分担
// - いつ  : サイドカーの VfxLoop の窓 → PlayerAnimEvents が開閉を VfxTrack（開いてからの秒・閉じてからの秒・閉じた理由）として持つ
// - どこに: ソケット "WeaponTip" = 武器メッシュのローカル座標 SocketOffset。武器メッシュを描くのと同じ行列（Render.cpp の weaponWorld）で
//           毎フレームワールドへ変換し、画面へ投影する。判定は描画から 1 フレーム遅れるが、こちらは描画と同じ行列なので刃と光がずれない
// - どう  : 立ち上がり（Attack）・消え方（Completed は Release、Interrupted は ReleaseFast）・長さの変化はこのモジュールが年齢から計算する。
//           見た目の値は TipRaysStyle（ImGui「AnimEvent VFX」で調整、Assets/Configs/AnimEventVfx.json に保存）
//
// 描画は Lumina.RadialRays（スクリーン空間・加算）。InGame の MergePass でパーティクルの後に描く（ポストのヒット時のゆがみが掛かる）

export module Game.AnimEventVfx;

//****	******	******	******	******	****//

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cmath>;
import <cstdint>;
import <fstream>;
import <span>;
import <string>;
import <vector>;
import <d3d12.h>;

import nlohmann.json;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;

import Lumina.RadialRays;
import Lumina.AnimEvent;
import Game.AnimEventDemo;

//////	//////	//////	//////	//////	//////

export namespace Game::AnimEventVfx {
	// 刃先の放射光線の定義（"TipRays"）。色は sRGB で指定する（描画時にリニアへ変換）
	struct TipRaysStyle {
		bool Enabled{ true };

		// ソケット "WeaponTip"：武器メッシュ（Kinoko.obj）のローカル座標。Kinoko.obj の Y の最大が約 5.695（= 先端）
		Lumina::Float3 SocketOffset{ 0.0f, 5.695f, 0.0f };

		// エンベロープ（実時間の秒。4 倍速の再生とは無関係）
		float Attack{ 0.10f };				// 窓が開いてから最大になるまで
		float Release{ 0.35f };				// 窓を最後まで通過して閉じたとき（Completed）に消えるまで
		float ReleaseFast{ 0.12f };			// 途中で切られたとき（Interrupted：コンボの切り替えなど）に消えるまで

		// 強さと長さ（長さは画面の高さに対する割合）
		float Intensity{ 1.5f };
		float LengthStart{ 0.3f };			// 開いた瞬間
		float LengthPeak{ 1.2f };			// 立ち上がりきったとき
		float LengthAfter{ 1.5f };			// 消えきるまでに伸びる長さ

		// 見た目（初期値は Title の Outro で調整した値）
		int RayCount{ 40 };
		float Sharpness{ 6.0f };
		float RotateSpeed{ 0.3f };
		float FlickerSpeed{ 8.0f };
		Lumina::Float3 Color{ 0.946f, 0.898f, 0.672f };
		Lumina::Float3 CoreColor{ 0.549f, 0.438f, 0.280f };
		float CoreRadius{ 0.048f };
		float CoreIntensity{ 1.087f };
	};

	class VfxPresenter {
	public:
		// rtvFormat_ / dsvFormat_ は描き込むパスに合わせる（InGame の MergePass は SRGB 1 枚。パーティクルの PSO に合わせて DSV は既定値）。
		// 設定ファイルがあれば読み込む
		void Initialize(
			Lumina::DX12::Context const& dxContext_,
			DXGI_FORMAT rtvFormat_,
			DXGI_FORMAT dsvFormat_
		);

		// 描画の直前に 1 回。weaponWorld_ は武器メッシュを描くのと同じ行列（ソケットはこれで変換する）
		void Prepare(
			std::span<Game::AnimEventDemo::VfxTrack const> tracks_,
			Lumina::Mat4 const& weaponWorld_,
			Lumina::Mat4 const& viewProj_
		);

		// RT / Viewport / DescriptorHeap は呼び出し側で設定済みであること
		void Render(Lumina::DX12::CommandList const& cmdList_);

		// ImGui「AnimEvent VFX」（Debug のみ）
		void DrawEditor();

		auto Load() -> bool;
		auto Save() -> bool;

	public:
		TipRaysStyle TipRays{};

		static constexpr char const* ConfigPath{ "Assets/Configs/AnimEventVfx.json" };

	private:
		Lumina::RadialRays::Renderer Rays_{};
		std::vector<Lumina::RadialRays::Instance> RaysInstances_{};

		// エディタの表示用（直近の Prepare の結果）
		std::uint32_t NumTracks_{ 0U };
		bool IsTipOnScreen_{ false };
		float ShownIntensity_{ 0.0f };
		float ShownLength_{ 0.0f };
		Lumina::Float2 TipUV_{ 0.0f, 0.0f };
		std::string Status_{};
	};
}
