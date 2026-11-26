export module Lumina.AnimEvent : Data;

//****	******	******	******	******	****//

import <cassert>;
import <concepts>;
import <cstddef>;
import <cstdint>;
import <cstring>;
import <string_view>;
import <type_traits>;
import <vector>;

//////	//////	//////	//////	//////	//////

// 実行時データと、1 ステップぶんの出力 1 件の形（講義 前編 4.2, 4.7）
//
// 編集用（フレーム番号・JSON）と実行時用（秒・ソート済み配列）を分ける。ここにあるのは実行時用。
// 焼き込み（フレーム → 秒、検証）は :Bake、JSON からの読み込みは Adapter で行う。

export namespace Lumina::AnimEvent {
	//----	------	------	------	------	----//
	//	型ID									//
	//----	------	------	------	------	----//

	// 型名（JSON の "type" 文字列）の 32bit FNV-1a ハッシュ。
	// - ビルドをまたいでも値が変わらない（連番の enum や、型ごとの静的変数のアドレスでは変わる／ずれる）
	// - JSON の文字列から実行時に同じ値を作れる
	// 衝突は Adapter の型登録で検出する。0 は無効値として予約する（ハッシュが 0 になる名前は登録時に弾く）。
	using EventTypeId = std::uint32_t;
	inline constexpr EventTypeId InvalidTypeId{ 0U };

	constexpr auto TypeIdOf(std::string_view name_) noexcept -> EventTypeId {
		std::uint32_t h{ 2166136261U };
		for (char const ch : name_) {
			h ^= static_cast<std::uint8_t>(ch);
			h *= 16777619U;
		}
		return h;
	}

	// カーブの ID も同じ規則で作る（名前のハッシュ）
	using CurveId = EventTypeId;

	//----	------	------	------	------	----//
	//	分類									//
	//----	------	------	------	------	----//

	// 判定系（ヒットボックス・無敵・キャンセル）と演出系（効果音・エフェクト）。
	// 流し先とブレンド中の採用方針（前編 4.6）がこの区別で決まるので、最初から分けておく。
	enum class EventCategory : std::uint8_t { Gameplay, Presentation };

	enum class EventEdge : std::uint8_t { Instant, Begin, End };

	// 区間の終了理由。受け側で「中断されたら軌跡をすぐ消す／最後まで行ったらフェードアウト」を分けるため（前編 4.5）。
	// Reloaded はホットリロードで窓を張り直したとき（後編 8.8）。
	enum class EndReason : std::uint8_t { None, Completed, Interrupted, Reloaded };

	//----	------	------	------	------	----//
	//	ペイロード								//
	//----	------	------	------	------	----//

	// ペイロードは「ビット列としてコピーしてよい型」に限る。焼き込みデータのバイト列に詰め、取り出しは memcpy。
	// 型名は T::TypeName で型自身に持たせる。型IDとの対応が 1 か所に固定され、
	// PayloadAs<T> で「違う型として読む」誤りを assert で捕まえられる。
	//   struct HitboxPayload {
	//       static constexpr std::string_view TypeName{ "Hitbox" };
	//       std::uint32_t Slot; std::uint32_t Attack;
	//   };
	// static constexpr のデータメンバは trivially copyable / standard layout を崩さない。
	template<class T>
	concept EventPayload =
		std::is_trivially_copyable_v<T> &&
		std::is_standard_layout_v<T> &&
		std::default_initializable<T> &&
		requires { { T::TypeName } -> std::convertible_to<std::string_view>; };

	template<EventPayload T>
	constexpr auto TypeIdOf() noexcept -> EventTypeId { return TypeIdOf(T::TypeName); }

	//----	------	------	------	------	----//
	//	実行時データ（1 クリップぶん）			//
	//----	------	------	------	------	----//

	struct InstantEvent {
		float Time{ 0.0f };					// 秒
		EventTypeId Type{ InvalidTypeId };
		std::uint32_t PayloadOffset{ 0U };	// ClipEvents::PayloadBlob 内の位置
		std::uint32_t PayloadSize{ 0U };	// 0 ならペイロードなし
		float MinWeight{ 0.0f };			// 演出系の採用閾値（前編 4.6）。判定系では使わない
		EventCategory Category{ EventCategory::Presentation };
	};

	struct WindowEvent {
		float Begin{ 0.0f };				// [Begin, End)（秒）
		float End{ 0.0f };
		EventTypeId Type{ InvalidTypeId };
		std::uint32_t PayloadOffset{ 0U };
		std::uint32_t PayloadSize{ 0U };
		float MinWeight{ 0.0f };
		EventCategory Category{ EventCategory::Gameplay };
	};

	// 等間隔に焼き直したカーブ。Values[i] は時刻 i × KeyInterval の値
	struct CurveTrack {
		CurveId Id{ InvalidTypeId };
		float KeyInterval{ 1.0f / 30.0f };
		std::vector<float> Values{};
	};

	struct ClipEvents {
		// 開いている窓の集合を 64bit マスクで持つため（前編 4.4）、1 クリップの窓は 64 個まで
		static constexpr std::size_t MaxWindows{ 64U };

		float Length{ 0.0f };						// クリップの長さ（秒）。焼き込み時にエンジンから受け取る
		std::vector<InstantEvent> Instants{};		// Time 昇順
		std::vector<WindowEvent> Windows{};			// Begin 昇順。添字がそのまま窓ID
		std::vector<CurveTrack> Curves{};
		std::vector<std::byte> PayloadBlob{};		// ペイロード本体。各型の alignof に合わせて詰める（memcpy で読むので必須ではないが、将来の読み方の変更に備える）
	};

	//----	------	------	------	------	----//
	//	1 ステップの出力 1 件					//
	//----	------	------	------	------	----//

	using InstanceId = std::uint32_t;
	inline constexpr InstanceId InvalidInstance{ 0U };

	struct AnimEvent {
		EventTypeId Type{ InvalidTypeId };
		EventEdge Edge{ EventEdge::Instant };
		EndReason Reason{ EndReason::None };		// Edge == End のときだけ意味がある
		EventCategory Category{ EventCategory::Presentation };
		std::uint16_t WindowIndex{ 0U };			// 区間イベントのとき、ClipEvents::Windows の添字
		float StepFraction{ 0.0f };					// ステップ内のどのあたりで起きたか (0〜1)。同じステップ内の順序づけと、遷移時の余り時間の持ち越し（前編 2.5）に使う
		float Weight{ 1.0f };						// 発生元インスタンスのウェイト
		float ClipTime{ 0.0f };						// 瞬間：イベントの時刻／Begin・End：そのステップ終了時のクリップ時間
		InstanceId Source{ InvalidInstance };
		// クリップの焼き込みデータを指す。コピーしない。
		// 有効なのは、そのステップの中だけ（ClipEvents の差し替えはステップの境目で行う前提）。
		std::byte const* Payload{ nullptr };
		std::uint32_t PayloadSize{ 0U };
	};

	template<EventPayload T>
	constexpr auto Is(AnimEvent const& e_) noexcept -> bool { return e_.Type == TypeIdOf<T>(); }

	// ペイロードをコピーで取り出す。
	// C++20 では、バイト列上の値を reinterpret_cast で const T& として読むのは規格上グレー（オブジェクトの生存期間が始まっていない）。
	// memcpy でコピーを作るのが安全。ペイロードは小さい前提なのでコストは問題にならない。
	// （コピーせずに読む正攻法は C++23 の std::start_lifetime_as。プロジェクト方針で使わない）
	template<EventPayload T>
	auto PayloadAs(AnimEvent const& e_) -> T {
		assert(Is<T>(e_) && "PayloadAs: type mismatch");
		assert(e_.PayloadSize == sizeof(T) && "PayloadAs: size mismatch");
		T out{};
		if (e_.Payload != nullptr && e_.PayloadSize == sizeof(T)) {
			std::memcpy(&out, e_.Payload, sizeof(T));
		}
		return out;
	}
}
