module Lumina.AnimEvent.Adapter;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれもビルド対象の .ixx で import 済み（GE3 の制約。実装メモ参照）
import <cmath>;
import <cstddef>;
import <cstdint>;
import <format>;
import <fstream>;
import <optional>;
import <span>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

import nlohmann.json;

import Lumina.AnimEvent;
import Lumina.CG3D.Struct;
import Lumina.CG3D.Animation;

//////	//////	//////	//////	//////	//////

namespace Lumina::AnimEvent::Adapter {
	//====	======	======	======	======	====//
	//	小道具									//
	//====	======	======	======	======	====//

	auto ReadNameId(JSON const& obj_, std::string_view key_, std::uint32_t& out_) -> bool {
		if (!obj_.is_object()) { return false; }
		auto const itr{ obj_.find(std::string{ key_ }) };
		if (itr == obj_.end()) { return true; }
		if (!itr->is_string()) { return false; }
		out_ = TypeIdOf(itr->get<std::string>());
		return true;
	}

	//====	======	======	======	======	====//
	//	型の登録								//
	//====	======	======	======	======	====//

	auto PayloadRegistry::RegisterTag(std::string_view name_, EventCategory category_, float defaultMinWeight_) -> bool {
		Entry e{};
		e.Name = std::string{ name_ };
		e.Category = category_;
		e.DefaultMinWeight = defaultMinWeight_;
		return Add(std::move(e));
	}

	auto PayloadRegistry::Add(Entry&& e_) -> bool {
		// 型IDは名前のハッシュなので、別の名前が同じ値になることがありうる（32bit なので稀）。
		// 黙って上書きすると、片方の型のイベントがもう片方として読まれてしまうため、ここで弾く
		EventTypeId const id{ TypeIdOf(e_.Name) };
		if (e_.Name.empty() || id == InvalidTypeId) { return false; }
		if (Entries_.contains(id)) { return false; }	// 同じ名前の二重登録、または別名との衝突。contains は C++20
		Entries_.emplace(id, std::move(e_));
		return true;
	}

	auto PayloadRegistry::Find(EventTypeId id_) const -> Entry const* {
		auto const itr{ Entries_.find(id_) };
		return itr == Entries_.end() ? nullptr : &itr->second;
	}

	//====	======	======	======	======	====//
	//	サイドカーの解釈						//
	//====	======	======	======	======	====//

	namespace {
		// "Assets/Neki/ATKY1_H2.notify.json" → "ATKY1_H2"（"clip" キーが無いときの名前）
		auto ClipNameFromPath(std::string_view path_) -> std::string {
			std::size_t const slash{ path_.find_last_of("/\\") };
			std::string_view name{ slash == std::string_view::npos ? path_ : path_.substr(slash + 1U) };
			for (std::string_view const suffix : { std::string_view{ ".json" }, std::string_view{ ".notify" } }) {
				if (name.ends_with(suffix)) { name.remove_suffix(suffix.size()); }	// ends_with は C++20
			}
			return std::string{ name };
		}

		class SidecarParser {
		public:
			SidecarParser(JSON const& root_, std::string_view clipName_, PayloadRegistry const& registry_, std::vector<BakeIssue>& issues_)
				: Root_{ root_ }, Registry_{ registry_ }, Issues_{ issues_ }, Name_{ clipName_ } {}

			auto Run() -> std::optional<ClipEventSource> {
				if (!Root_.is_object()) {
					Error("ルートが JSON オブジェクトではありません");
					return std::nullopt;
				}

				// "clip" があれば、診断メッセージにはその名前を使う
				if (auto const itr{ Root_.find("clip") }; itr != Root_.end()) {
					if (itr->is_string()) { Name_ = itr->get<std::string>(); }
					else { Warning("\"clip\" が文字列ではありません。無視します"); }
				}

				// 版番号。形式を変えたら、ここで古い版から変換する（後編 8.8）
				if (auto const itr{ Root_.find("version") }; itr == Root_.end()) {
					Warning(std::format("\"version\" がありません。{} として読みます", SidecarVersion));
				}
				else if (!itr->is_number_integer() || itr->get<std::int64_t>() != SidecarVersion) {
					Error(std::format("対応していない version です（対応しているのは {}）", SidecarVersion));
					return std::nullopt;
				}

				ClipEventSource src{};
				src.ClipName = Name_;

				// frameRate は必須。glTF には入っていないので、既定値で黙って読むと時刻が全部ずれる
				if (auto const itr{ Root_.find("frameRate") }; itr == Root_.end() || !itr->is_number()) {
					Error("\"frameRate\"（数値）が必要です");
					return std::nullopt;
				}
				else {
					src.FrameRate = static_cast<float>(itr->get<double>());
				}

				ParseInstants(src);
				ParseWindows(src);
				ParseCurves(src);

				if (HasError(std::span{ Issues_ }.subspan(IssuesAtStart_))) { return std::nullopt; }
				return src;
			}

		private:
			JSON const& Root_;
			PayloadRegistry const& Registry_;
			std::vector<BakeIssue>& Issues_;
			std::size_t const IssuesAtStart_{ Issues_.size() };
			std::string Name_;

			void Error(std::string msg_) { Issues_.push_back({ BakeSeverity::Error, std::format("[{}] {}", Name_, msg_) }); }
			void Warning(std::string msg_) { Issues_.push_back({ BakeSeverity::Warning, std::format("[{}] {}", Name_, msg_) }); }

			// 配列のキーを取る。無ければ nullptr（空と同じ扱い）。あって配列でなければエラー
			auto ArrayOf(char const* key_) -> JSON const* {
				auto const itr{ Root_.find(key_) };
				if (itr == Root_.end()) { return nullptr; }
				if (!itr->is_array()) {
					Error(std::format("\"{}\" が配列ではありません", key_));
					return nullptr;
				}
				return &*itr;
			}

			// 必須の整数（フレーム番号）
			auto RequiredFrame(JSON const& e_, char const* key_, std::string const& what_, int& out_) -> bool {
				if (!e_.contains(key_)) {
					Error(std::format("{} に \"{}\" がありません", what_, key_));
					return false;
				}
				if (!ReadNumber(e_, key_, out_)) {
					Error(std::format("{} の \"{}\" は整数のフレーム番号にしてください", what_, key_));
					return false;
				}
				return true;
			}

			// 型・MinWeight・ペイロードの共通部分。型が登録されていなければ nullptr
			auto CommonFields(JSON const& e_, std::string const& what_, std::string& typeName_, float& minWeight_, PayloadBytes& payload_)
				-> PayloadRegistry::Entry const* {
				auto const itrType{ e_.find("type") };
				if (itrType == e_.end() || !itrType->is_string()) {
					Error(std::format("{} に \"type\"（文字列）がありません", what_));
					return nullptr;
				}
				typeName_ = itrType->get<std::string>();
				PayloadRegistry::Entry const* entry{ Registry_.Find(typeName_) };
				// 登録されていない型はエラー。綴りの間違いを黙って無視すると、判定が出ない理由を探すのに時間がかかるため
				if (entry == nullptr || entry->Name != typeName_) {
					Error(std::format("{} の type \"{}\" は登録されていません（PayloadRegistry::Register / RegisterTag）", what_, typeName_));
					return nullptr;
				}

				minWeight_ = entry->DefaultMinWeight;
				if (!ReadNumber(e_, "minWeight", minWeight_)) {
					Error(std::format("{} の \"minWeight\" が数値ではありません", what_));
				}
				if (entry->Parse && !entry->Parse(e_, payload_)) {
					Error(std::format("{} のペイロードを読めませんでした（型 \"{}\" の変換関数が false を返した）", what_, typeName_));
				}
				return entry;
			}

			void ParseInstants(ClipEventSource& src_) {
				JSON const* arr{ ArrayOf("instants") };
				if (arr == nullptr) { return; }
				for (std::size_t i = 0; i < arr->size(); ++i) {
					JSON const& e{ (*arr)[i] };
					std::string const what{ std::format("instants[{}]", i) };
					if (!e.is_object()) { Error(what + " がオブジェクトではありません"); continue; }

					InstantSource s{};
					PayloadRegistry::Entry const* entry{ CommonFields(e, what, s.TypeName, s.MinWeight, s.Payload) };
					bool const frameOk{ RequiredFrame(e, "frame", what, s.Frame) };
					if (entry == nullptr || !frameOk) { continue; }
					s.Category = entry->Category;
					src_.Instants.push_back(std::move(s));
				}
			}

			void ParseWindows(ClipEventSource& src_) {
				JSON const* arr{ ArrayOf("windows") };
				if (arr == nullptr) { return; }
				for (std::size_t i = 0; i < arr->size(); ++i) {
					JSON const& e{ (*arr)[i] };
					std::string const what{ std::format("windows[{}]", i) };
					if (!e.is_object()) { Error(what + " がオブジェクトではありません"); continue; }

					WindowSource s{};
					PayloadRegistry::Entry const* entry{ CommonFields(e, what, s.TypeName, s.MinWeight, s.Payload) };
					bool const beginOk{ RequiredFrame(e, "begin", what, s.BeginFrame) };
					bool const endOk{ RequiredFrame(e, "end", what, s.EndFrame) };
					if (entry == nullptr || !beginOk || !endOk) { continue; }
					s.Category = entry->Category;
					src_.Windows.push_back(std::move(s));	// 長さ 0 や逆転の検証は Bake が行う
				}
			}

			void ParseCurves(ClipEventSource& src_) {
				JSON const* arr{ ArrayOf("curves") };
				if (arr == nullptr) { return; }
				for (std::size_t i = 0; i < arr->size(); ++i) {
					JSON const& e{ (*arr)[i] };
					std::string const what{ std::format("curves[{}]", i) };
					if (!e.is_object()) { Error(what + " がオブジェクトではありません"); continue; }

					auto const itrName{ e.find("name") };
					auto const itrKeys{ e.find("keys") };
					if (itrName == e.end() || !itrName->is_string()) { Error(what + " に \"name\"（文字列）がありません"); continue; }
					if (itrKeys == e.end() || !itrKeys->is_array()) { Error(what + " に \"keys\"（配列）がありません"); continue; }

					CurveSource s{};
					s.Name = itrName->get<std::string>();
					bool ok{ true };
					for (JSON const& k : *itrKeys) {
						// [フレーム, 値]。カーブのキーはフレームの途中に置いてもよい（イベントと違い整数に限らない）
						if (!k.is_array() || k.size() != 2U || !k[0].is_number() || !k[1].is_number()) {
							Error(std::format("{} \"{}\" のキーは [フレーム, 値] の形にしてください", what, s.Name));
							ok = false;
							break;
						}
						s.Keys.emplace_back(static_cast<float>(k[0].get<double>()), static_cast<float>(k[1].get<double>()));
					}
					if (ok) { src_.Curves.push_back(std::move(s)); }	// 同じ名前・キーなしの検証は Bake が行う
				}
			}
		};

		// ファイルを読んで JSON にする。例外は使わない（Title の Settings.cpp と同じ読み方）
		auto LoadRoot(std::string const& path_, std::string_view clipName_, std::vector<BakeIssue>& issues_) -> std::optional<JSON> {
			std::ifstream ifs{ path_ };
			if (!ifs) {
				issues_.push_back({ BakeSeverity::Error, std::format("[{}] ファイルを開けません: {}", clipName_, path_) });
				return std::nullopt;
			}
			JSON root = JSON::parse(ifs, nullptr, false);
			if (root.is_discarded()) {
				issues_.push_back({ BakeSeverity::Error, std::format("[{}] JSON として読めません: {}", clipName_, path_) });
				return std::nullopt;
			}
			return std::optional<JSON>{ std::move(root) };
		}

		auto LoadImpl(
			std::string const& path_, float clipLength_, std::string const* expectedTake_,
			PayloadRegistry const& registry_, std::vector<BakeIssue>& issues_
		) -> std::optional<ClipEvents> {
			std::string const fallbackName{ ClipNameFromPath(path_) };
			std::optional<JSON> const root{ LoadRoot(path_, fallbackName, issues_) };
			if (!root) { return std::nullopt; }

			std::optional<ClipEventSource> const src{ ParseSidecar(*root, fallbackName, registry_, issues_) };
			if (!src) { return std::nullopt; }

			// 1 つの glTF に複数のテイクが入っていることがあるので、サイドカーがどのテイクのものかを確かめる
			if (expectedTake_ != nullptr && root->is_object()) {
				if (auto const itr{ root->find("take") }; itr != root->end() && itr->is_string() && itr->get<std::string>() != *expectedTake_) {
					issues_.push_back({ BakeSeverity::Warning, std::format(
						"[{}] サイドカーの take \"{}\" と、再生するクリップのテイク \"{}\" が違います",
						src->ClipName, itr->get<std::string>(), *expectedTake_) });
				}
			}
			return Bake(*src, clipLength_, issues_);
		}
	}

	auto ParseSidecar(
		JSON const& root_, std::string_view clipName_,
		PayloadRegistry const& registry_, std::vector<BakeIssue>& issues_
	) -> std::optional<ClipEventSource> {
		return SidecarParser{ root_, clipName_, registry_, issues_ }.Run();
	}

	auto LoadClipEvents(
		std::string const& path_, float clipLength_,
		PayloadRegistry const& registry_, std::vector<BakeIssue>& issues_
	) -> std::optional<ClipEvents> {
		return LoadImpl(path_, clipLength_, nullptr, registry_, issues_);
	}

	auto LoadClipEvents(
		std::string const& path_, CG3D::MyAnimation const& clip_,
		PayloadRegistry const& registry_, std::vector<BakeIssue>& issues_
	) -> std::optional<ClipEvents> {
		return LoadImpl(path_, ClipLength(clip_), &clip_.Name, registry_, issues_);
	}

	//====	======	======	======	======	====//
	//	GE3 のアニメとの接続					//
	//====	======	======	======	======	====//

	auto ClipLength(CG3D::MyAnimation const& clip_) noexcept -> float {
		float const d{ clip_.DurationInSeconds };
		return (std::isfinite(d) && d > 0.0f) ? d : 0.0f;
	}

	auto ApplyPose(
		CG3D::SkinCluster& skinCluster_, CG3D::Skeleton& skeleton_, CG3D::MyAnimation const& clip_,
		AnimEventPlayer const& player_, InstanceId id_
	) -> bool {
		std::optional<float> const t{ player_.Time(id_) };
		if (!t) { return false; }
		// 時刻はコアが持つ。ここでは渡された時刻の姿勢を作るだけで、時刻を進めたり折り返したりしない
		CG3D::Update(skinCluster_, skeleton_, clip_, *t);
		return true;
	}
}
