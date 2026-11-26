//////	//////	//////	//////	//////	//////
//	AnimEvent (prototype) - GE3 adapter		//
//////	//////	//////	//////	//////	//////
// コア（Lumina.AnimEvent）と GE3 のエンジンをつなぐ部分。エンジンに依存するのはこのモジュールだけ。
// 新リポへ移植するときは、コアはそのまま持っていき、このモジュールを新リポの JSON・アニメ系に合わせて書き直す。
//
// やること
// - サイドカー JSON（<クリップ>.notify.json）を読んで ClipEventSource に変換し、焼き込む（講義 前編 4.2、後編 8.8）
// - ペイロードの型の登録（型名 → 分類・既定の MinWeight・JSON からの変換）。型IDの衝突をここで検出する
// - GE3 のクリップ（CG3D::MyAnimation）からクリップ長を取る。コアの時刻でポーズを作る
//
// やらないこと（スコープ外）
// - ファイルの変更検知（ホットリロードの引き金）。呼び出し側のサンプルを下に書いておく
// - 演出系イベントの配送先（GE3 にイベントキューが無いため）。受け取ったイベントの処理は呼び出し側で書く
//
// サイドカー JSON の形（version 1）
//   {
//     "version": 1,
//     "clip": "ATK1",                                  任意。診断メッセージ用の名前（無ければ読み込み時に渡した名前）
//     "take": "Armature.001|mixamo.com|Layer0",        任意。MyAnimation::Name と違ったら警告（1 つの glTF に複数テイクがあるため）
//     "frameRate": 30,                                 必須。glTF にはフレームレートが入っていないので明記する
//     "instants": [ { "type": "Swing", "frame": 12, "minWeight": 0.5, ...ペイロードのキー } ],
//     "windows":  [ { "type": "Hitbox", "begin": 13, "end": 16, ...ペイロードのキー } ],
//     "curves":   [ { "name": "TurnRate", "keys": [[0, 1.0], [9, 1.0], [12, 0.0]] } ]
//   }
//   - frame / begin / end は整数のフレーム番号（等速で見たとき。GE3 の 4 倍速再生は PlayParams::Rate = 4 で掛ける）
//   - "minWeight" は任意。無ければ型の登録時の既定値
//   - 型が登録されていない "type" はエラー（綴りの間違いを黙って無視しないため）
//   - 上に無いキー（"_note" など）は無視する
//
// 呼び出し側のサンプル（InGame 相当。既存コードは変更しないので、組み込むときの参考）
//
//   import Lumina.AnimEvent;
//   import Lumina.AnimEvent.Adapter;
//   namespace AE = Lumina::AnimEvent;
//
//   // ペイロードの型（ゲーム側で定義する。コアは中身を知らない）
//   struct HitboxPayload {
//       static constexpr std::string_view TypeName{ "Hitbox" };
//       std::uint32_t Slot;      // "slot": "RightBlade" → TypeIdOf("RightBlade")
//       float Damage;
//   };
//
//   // 初期化（1 回）
//   AE::Adapter::PayloadRegistry registry{};
//   registry.Register<HitboxPayload>(AE::EventCategory::Gameplay, 0.0f,
//       [](AE::Adapter::JSON const& e_, HitboxPayload& p_) {
//           return AE::Adapter::ReadNameId(e_, "slot", p_.Slot) && AE::Adapter::ReadNumber(e_, "damage", p_.Damage);
//       });
//   registry.RegisterTag("SuperArmor", AE::EventCategory::Gameplay, 0.0f);     // ペイロードなし
//   registry.RegisterTag("Swing", AE::EventCategory::Presentation, 0.5f);
//   registry.RegisterTag("Trail", AE::EventCategory::Presentation, 0.5f);
//
//   // クリップごとに読み込む。ClipEvents のアドレスは再生中に動かさない（unique_ptr か map のノードに置く）
//   std::vector<AE::BakeIssue> issues{};
//   if (auto ev{ AE::Adapter::LoadClipEvents("Assets/Neki/ATKY1_H2.notify.json", animDatabase_["ATK1"], registry, issues) }) {
//       clipEvents_["ATK1"] = std::make_unique<AE::ClipEvents>(std::move(*ev));
//   }
//   for (auto const& i : issues) { /* Logger などで表示。Warning は読み込み自体は成功している */ }
//
//   // 毎フレーム（Update、1/60 固定）
//   if (name != currentAnimName_) {
//       curId_ = animEvents_.Play({ .Events = clipEvents_[name].get(), .Rate = 4.0f, .Loop = isLoop,
//                                   .OwnsAction = true, .StopOthers = true });   // サイドカーが無いクリップには空の ClipEvents を渡す
//   }
//   events_.clear();
//   animEvents_.Step(1.0f / 60.0f, events_);
//   AE::Adapter::ApplyPose(PlayerSkinnedInstance_->SkinCluster_, PlayerSkinnedInstance_->Skeleton_,
//                          *currentAnim_, animEvents_, curId_);                      // animTimer_ の代わり
//   for (AE::AnimEvent const& e : events_) {
//       if (AE::Is<HitboxPayload>(e) && e.Edge == AE::EventEdge::Begin) { /* ヒット済みリストのリセットなど、副作用だけ */ }
//   }
//   bool const armor{ animEvents_.IsOpen(AE::TypeIdOf("SuperArmor")) };            // 状態は問い合わせで
//
//   // ホットリロード（ステップの境目で）。差し替え先は別のアドレスに作る（同じアドレスの中身を書き換えると Rebind できない）
//   if (std::filesystem::last_write_time(path) != lastWrite_) {
//       if (auto ev{ AE::Adapter::LoadClipEvents(path, animDatabase_["ATK1"], registry, issues) }) {
//           auto next{ std::make_unique<AE::ClipEvents>(std::move(*ev)) };
//           animEvents_.Rebind(clipEvents_["ATK1"].get(), next.get(), events_);
//           clipEvents_["ATK1"] = std::move(next);      // 古いものはここで破棄される
//       }
//   }
//
//   // シーン終了時
//   animEvents_.StopAllNow(AE::EndReason::Interrupted, events_);

export module Lumina.AnimEvent.Adapter;

//****	******	******	******	******	****//

import <concepts>;
import <cstddef>;
import <cstdint>;
import <cstring>;
import <functional>;
import <optional>;
import <string>;
import <string_view>;
import <type_traits>;
import <unordered_map>;
import <utility>;
import <vector>;

import nlohmann.json;

import Lumina.AnimEvent;
import Lumina.CG3D.Struct;

//////	//////	//////	//////	//////	//////

export namespace Lumina::AnimEvent::Adapter {
	using JSON = nlohmann::json;

	inline constexpr int SidecarVersion{ 1 };

	//----	------	------	------	------	----//
	//	JSON から値を読む小道具（ペイロードの変換用）	//
	//----	------	------	------	------	----//

	// キーが無ければ out_ を変えずに true（任意のキー）。あって型が合わない・範囲外なら false。
	// 整数は範囲を確かめてから入れる（大きすぎる値が黙って折り返さないように）
	template<class T>
		requires std::is_arithmetic_v<T>
	auto ReadNumber(JSON const& obj_, std::string_view key_, T& out_) -> bool {
		if (!obj_.is_object()) { return false; }
		auto const itr{ obj_.find(std::string{ key_ }) };
		if (itr == obj_.end()) { return true; }

		if constexpr (std::same_as<T, bool>) {
			if (!itr->is_boolean()) { return false; }
			out_ = itr->template get<bool>();
		}
		else if constexpr (std::is_integral_v<T>) {
			if (!itr->is_number_integer()) { return false; }
			if (itr->is_number_unsigned()) {
				auto const v{ itr->template get<std::uint64_t>() };
				if (!std::in_range<T>(v)) { return false; }		// std::in_range は C++20
				out_ = static_cast<T>(v);
			}
			else {
				auto const v{ itr->template get<std::int64_t>() };
				if (!std::in_range<T>(v)) { return false; }
				out_ = static_cast<T>(v);
			}
		}
		else {
			if (!itr->is_number()) { return false; }
			out_ = static_cast<T>(itr->template get<double>());
		}
		return true;
	}

	// 文字列を名前のハッシュ（TypeIdOf と同じ規則）として読む。スロット名・効果音名・攻撃データ名など、
	// 「名前で指して、実行時は整数で比べたい」ものに使う。キーが無ければ out_ を変えずに true
	auto ReadNameId(JSON const& obj_, std::string_view key_, std::uint32_t& out_) -> bool;

	//----	------	------	------	------	----//
	//	ペイロードの型の登録							//
	//----	------	------	------	------	----//

	class PayloadRegistry {
	public:
		struct Entry {
			std::string Name{};
			EventCategory Category{ EventCategory::Presentation };
			float DefaultMinWeight{ 0.0f };
			std::size_t Size{ 0U };		// 0 ならペイロードなし（RegisterTag）
			std::size_t Align{ 1U };
			// イベント 1 件ぶんの JSON オブジェクトからバイト列を作る。false なら変換失敗
			std::function<bool(JSON const&, PayloadBytes&)> Parse{};
		};

		// 型を登録する。parse_ は bool(JSON const& entry, T& out)。entry はイベント 1 件ぶんのオブジェクト（"type" や "frame" も入っている）。
		// 同じ名前が登録済み、型IDが 0、別の名前と型IDが衝突、のいずれかなら登録せずに false
		template<EventPayload T, class F>
			requires std::is_invocable_r_v<bool, F&, JSON const&, T&>
		auto Register(EventCategory category_, float defaultMinWeight_, F parse_) -> bool {
			static_assert(alignof(T) <= alignof(std::max_align_t), "payload alignment too large");
			Entry e{};
			e.Name = std::string{ T::TypeName };
			e.Category = category_;
			e.DefaultMinWeight = defaultMinWeight_;
			e.Size = sizeof(T);
			e.Align = alignof(T);
			e.Parse = [parse = std::move(parse_)](JSON const& json_, PayloadBytes& out_) mutable -> bool {
				T value{};
				if (!parse(json_, value)) { return false; }
				out_.Bytes.resize(sizeof(T));
				std::memcpy(out_.Bytes.data(), &value, sizeof(T));
				out_.Align = alignof(T);
				return true;
			};
			return Add(std::move(e));
		}

		// ペイロードを持たない型（SuperArmor のような「開いているかどうか」だけの窓、効果音を鳴らすだけの印など）
		auto RegisterTag(std::string_view name_, EventCategory category_, float defaultMinWeight_) -> bool;

		auto Find(EventTypeId id_) const -> Entry const*;
		auto Find(std::string_view name_) const -> Entry const* { return Find(TypeIdOf(name_)); }

	private:
		std::unordered_map<EventTypeId, Entry> Entries_{};
		auto Add(Entry&& e_) -> bool;
	};

	//----	------	------	------	------	----//
	//	サイドカー JSON の読み込み						//
	//----	------	------	------	------	----//

	// JSON（パース済み）→ ソース。ファイルは読まない（テストや、別の場所から JSON を得る場合用）。
	// clipName_ は "clip" キーが無いときの名前。問題は issues_ に積み、エラーがあれば nullopt
	auto ParseSidecar(
		JSON const& root_, std::string_view clipName_,
		PayloadRegistry const& registry_, std::vector<BakeIssue>& issues_
	) -> std::optional<ClipEventSource>;

	// ファイルを読み、パースして、焼き込みまで行う。例外は使わない（壊れた JSON でもゲームを止めない）。
	// clipLength_ はエンジン側のクリップ長（秒）
	auto LoadClipEvents(
		std::string const& path_, float clipLength_,
		PayloadRegistry const& registry_, std::vector<BakeIssue>& issues_
	) -> std::optional<ClipEvents>;

	// 上の GE3 版。クリップ長を MyAnimation から取り、サイドカーの "take" が MyAnimation::Name と違えば警告する
	auto LoadClipEvents(
		std::string const& path_, CG3D::MyAnimation const& clip_,
		PayloadRegistry const& registry_, std::vector<BakeIssue>& issues_
	) -> std::optional<ClipEvents>;

	//----	------	------	------	------	----//
	//	GE3 のアニメとの接続						//
	//----	------	------	------	------	----//

	// GE3 のクリップ長（秒）。負や非有限なら 0
	auto ClipLength(CG3D::MyAnimation const& clip_) noexcept -> float;

	// コアが持つ時刻でポーズを作る（CG3D::Update を呼ぶ）。時刻はエンジン側で進めない。
	// id_ が再生中でも反映前でもなければ何もせず false
	auto ApplyPose(
		CG3D::SkinCluster& skinCluster_, CG3D::Skeleton& skeleton_, CG3D::MyAnimation const& clip_,
		AnimEventPlayer const& player_, InstanceId id_
	) -> bool;
}
