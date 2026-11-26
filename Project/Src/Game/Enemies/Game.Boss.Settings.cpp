module Game.Boss;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれも Game.Boss.ixx で import 済み
import <cmath>;
import <cstddef>;
import <cstdint>;
import <optional>;
import <string>;
import <string_view>;
import <utility>;

import nlohmann.json;

import Game.Fighter;
import Game.FighterSettings;
import Game.BossBrain;
import Game.BossSkills;

//////	//////	//////	//////	//////	//////
//	ボスの調整値の保存・読み込み			//
//////	//////	//////	//////	//////	//////
// Assets/Configs/Boss.json = { "version": 1, "Body": { FighterSetup の調整値 }, "AI": { BossTuning },
//                              "Skills": { "ChargeSlash": { ChargeSlashTuning }, "ThornWave": { ThornWaveTuning } } }
// 読み込みは「ファイルにあるキーだけ上書き」（Game.FighterSettings と同じ方針）

namespace Game {
	namespace {
		using JSON = nlohmann::json;

		// 0.6f → 0.6000000238… のような表記を避ける（FighterSettings と同じ）
		auto Round(float v_) -> double {
			return std::round(static_cast<double>(v_) * 1e6) / 1e6;
		}

		class Writer {
		public:
			explicit Writer(JSON& j_) : J_{ j_ } {}
			void operator()(char const* key_, float const& v_) { J_[key_] = Round(v_); }
			void operator()(char const* key_, std::int32_t const& v_) { J_[key_] = v_; }
		private:
			JSON& J_;
		};

		class Reader {
		public:
			explicit Reader(JSON const& j_) : J_{ j_ } {}
			void operator()(char const* key_, float& v_) const {
				if (!J_.is_object()) { return; }
				if (auto const itr{ J_.find(key_) }; itr != J_.end() && itr->is_number()) { v_ = itr->get<float>(); }
			}
			void operator()(char const* key_, std::int32_t& v_) const {
				if (!J_.is_object()) { return; }
				if (auto const itr{ J_.find(key_) }; itr != J_.end() && itr->is_number_integer()) { v_ = itr->get<std::int32_t>(); }
			}
		private:
			JSON const& J_;
		};

		// BossTuning の全項目（Writer / Reader の両方がこれを通る）
		template<class Archive, class Tuning>
		void Visit(Archive& ar_, Tuning& t_) {
			ar_("CloseRange", t_.CloseRange);
			ar_("MidRange", t_.MidRange);
			ar_("ApproachSpeed", t_.ApproachSpeed);
			ar_("ApproachTimeout", t_.ApproachTimeout);
			ar_("ComboMinCloseHits", t_.ComboMinCloseHits);
			ar_("ComboMaxCloseHits", t_.ComboMaxCloseHits);
			ar_("ComboMinRushHits", t_.ComboMinRushHits);
			ar_("ComboMaxRushHits", t_.ComboMaxRushHits);
			ar_("RecoverMin", t_.RecoverMin);
			ar_("RecoverMax", t_.RecoverMax);
			ar_("StrafeMin", t_.StrafeMin);
			ar_("StrafeMax", t_.StrafeMax);
			ar_("StrafeSpeed", t_.StrafeSpeed);
			ar_("CloseAttackWeight", t_.CloseAttackWeight);
			ar_("CloseRetreatWeight", t_.CloseRetreatWeight);
			ar_("CloseCircleWeight", t_.CloseCircleWeight);
			ar_("MidRushWeight", t_.MidRushWeight);
			ar_("MidWalkWeight", t_.MidWalkWeight);
			ar_("MidCircleWeight", t_.MidCircleWeight);
			ar_("MidSlashWeight", t_.MidSlashWeight);
			ar_("MidWaveWeight", t_.MidWaveWeight);
			ar_("FarWalkWeight", t_.FarWalkWeight);
			ar_("FarWaveWeight", t_.FarWaveWeight);
			ar_("DamageScale", t_.DamageScale);
		}

		template<class Archive, class Tuning>
		void VisitSlash(Archive& ar_, Tuning& t_) {
			ar_("MinRange", t_.MinRange);
			ar_("MaxRange", t_.MaxRange);
			ar_("Cooldown", t_.Cooldown);
			ar_("ChargeFrames", t_.ChargeFrames);
			ar_("ChargeTurnRate", t_.ChargeTurnRate);
			ar_("SlamTurnRate", t_.SlamTurnRate);
			ar_("LungeMax", t_.LungeMax);
			ar_("LungeStop", t_.LungeStop);
			ar_("LungeFrames", t_.LungeFrames);
			ar_("LungeFallbackFrame", t_.LungeFallbackFrame);
			ar_("ArmorUntil", t_.ArmorUntil);
		}

		template<class Archive, class Tuning>
		void VisitWave(Archive& ar_, Tuning& t_) {
			ar_("MinRange", t_.MinRange);
			ar_("MaxRange", t_.MaxRange);
			ar_("Cooldown", t_.Cooldown);
			ar_("WindupFrames", t_.WindupFrames);
			ar_("WindupTurnRate", t_.WindupTurnRate);
			ar_("FireFallbackFrame", t_.FireFallbackFrame);
			ar_("RecoverFrames", t_.RecoverFrames);
			ar_("Lines", t_.Lines);
			ar_("Spread", t_.Spread);
			ar_("StartOffset", t_.StartOffset);
			ar_("Speed", t_.Speed);
			ar_("MaxDistance", t_.MaxDistance);
			ar_("HitRadius", t_.HitRadius);
			ar_("HitHeight", t_.HitHeight);
			ar_("SpikeSpacing", t_.SpikeSpacing);
			ar_("SineAmplitude", t_.SineAmplitude);
			ar_("SineWavelength", t_.SineWavelength);
			ar_("Damage", t_.Damage);
			ar_("Knockback", t_.Knockback);
		}
	}

	auto Boss::Save() -> bool {
		JSON root = JSON::object();
		root["version"] = 1;
		JSON body = JSON::object();
		FighterSettings::Write(body, Setup());
		root["Body"] = std::move(body);
		JSON ai = JSON::object();
		Writer w{ ai };
		BossTuning const& tuning{ Tuning_ };
		Visit(w, tuning);
		root["AI"] = std::move(ai);
		JSON slash = JSON::object();
		JSON wave = JSON::object();
		{
			Writer ws{ slash };
			ChargeSlashTuning const& st{ SlashTuning_ };
			VisitSlash(ws, st);
			Writer ww{ wave };
			ThornWaveTuning const& wt{ WaveTuning_ };
			VisitWave(ww, wt);
		}
		JSON skills = JSON::object();
		skills["ChargeSlash"] = std::move(slash);
		skills["ThornWave"] = std::move(wave);
		root["Skills"] = std::move(skills);
		return FighterSettings::SaveFile(ConfigPath, root, Status_);
	}

	auto Boss::Load() -> bool {
		auto const root{ FighterSettings::LoadFile(ConfigPath, Status_) };
		if (!root) { return false; }
		FighterSetup s{ Setup() };
		if (auto const itr{ root->find("Body") }; itr != root->end()) { FighterSettings::Read(*itr, s); }
		FighterSettings::Apply(*this, s);
		if (auto const itr{ root->find("AI") }; itr != root->end()) {
			Reader r{ *itr };
			Visit(r, Tuning_);
		}
		if (auto const skills{ root->find("Skills") }; skills != root->end() && skills->is_object()) {
			if (auto const itr{ skills->find("ChargeSlash") }; itr != skills->end()) {
				Reader r{ *itr };
				VisitSlash(r, SlashTuning_);
			}
			if (auto const itr{ skills->find("ThornWave") }; itr != skills->end()) {
				Reader r{ *itr };
				VisitWave(r, WaveTuning_);
			}
		}
		return true;
	}
}
