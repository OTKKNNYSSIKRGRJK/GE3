//////	//////	//////	//////	//////	//////
//	DebugDock - InGame のエディタの置き場	//
//////	//////	//////	//////	//////	//////
// InGame の ImGui のウィンドウ（Boss AI、AnimEvent Hitbox など）を、画面の右の列にまとめる（Debug のみ）。
//
// - 右の列は上下 2 段。どちらもタブ（同じ段のウィンドウは 1 枚ずつ表示）。真ん中（ゲームの画面）は透けたままで、
//   そこへはドッキングさせない（ゲームが隠れないように）。列の幅・段の高さは境目をドラッグで変えられ、imgui.ini に残る。
// - 画面の上のメニュー「Editors」で、ウィンドウごとに表示する／しないを切り替える。表示しないウィンドウは DrawEditor も呼ばない。
//   既定で出すのは、戦闘の調整に使うもの（Boss AI、Player、Collision、AnimEvent Hitbox の 2 つ）。
// - F2: メニューも含めて全部隠す／戻す。
// - 「Reset layout」: 右の列への配置をやり直す（ウィンドウを外へ出してしまったときなど）。
// - カーソルを固定している間（IN_BATTLE）は ImGui にマウスを渡していないので、触るときは F1 でカーソルを解放する（Game.PlayerInput）。
//
// 使い方（InGame の Update。ほかの ImGui のウィンドウより前に 1 回）:
//   DebugDock_.Begin(keyboard.IsPressed(KEY::F2));
//   if (DebugDock_.Visible("Boss AI")) { Boss_Kinoko_->DrawEditor(); }
// 名前は各ウィンドウの ImGui::Begin の名前と同じにすること（違うと配置されない）。

export module Game.DebugDock;

//****	******	******	******	******	****//

import <array>;
import <cstddef>;
import <cstdint>;
import <string_view>;

//////	//////	//////	//////	//////	//////

export namespace Game {
	class DebugDock {
	public:
		enum class Area : std::uint8_t {
			Upper,		// 右の列の上の段
			Lower,		// 右の列の下の段
		};

		struct Entry {
			std::string_view Title;		// ImGui::Begin の名前（文字列リテラル。DockBuilder に渡すので NUL 終端が要る）
			Area Where;
			bool Visible;
		};

		// 毎フレーム 1 回、ほかの ImGui のウィンドウより前に呼ぶ。toggleHeld_ は F2 を押しているか（押した瞬間に切り替える）
		void Begin(bool toggleHeld_);

		// そのウィンドウを描くか（F2 で隠している間は全部 false）。知らない名前は true
		auto Visible(std::string_view title_) const noexcept -> bool;

	private:
		// 大きさは登録数より大きくてよい（余りは名前が空で、メニューとドックでは飛ばす）
		std::array<Entry, 24> Entries_{ {
			{ "Boss AI", Area::Upper, true },
			{ "Player", Area::Upper, true },
			{ "Collision", Area::Upper, true },
			{ "Player Input", Area::Upper, false },
			{ "VS Banner", Area::Upper, false },
			{ "Pause Menu", Area::Upper, false },
			{ "Scene Light", Area::Upper, false },
			{ "AnimEvent Hitbox (Boss)", Area::Lower, true },
			{ "AnimEvent Hitbox", Area::Lower, true },
			{ "AnimEvent VFX", Area::Lower, false },
			{ "Combat VFX", Area::Lower, false },
			{ "GameOver Director", Area::Lower, false },
			{ "GameOver Grade", Area::Lower, false },
			{ "GameOver Camera", Area::Lower, false },
			{ "GameOver Shatter", Area::Lower, false },
			{ "Victory Director", Area::Lower, false },
			{ "Victory Camera", Area::Lower, false },
			{ "Victory Grade", Area::Lower, false },
		} };
		bool AllHidden_{ false };
		[[maybe_unused]] bool PrevToggle_{ false };		// Release では使わない
		[[maybe_unused]] bool ResetLayout_{ false };
	};
}
