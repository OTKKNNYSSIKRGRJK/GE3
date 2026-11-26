module Game.FighterSettings;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれも Game.FighterSettings.ixx で import 済み
import <cmath>;
import <cstddef>;
import <cstdint>;
import <fstream>;
import <optional>;
import <string>;
import <utility>;

import nlohmann.json;

import Lumina.Math;
import Game.Fighter;

//////	//////	//////	//////	//////	//////

namespace Game::FighterSettings {
	namespace {
		// 0.6f → 0.6000000238… のような表記を避ける（差分を読みやすく。Title の Settings.cpp と同じ）
		auto Round(float v_) -> double {
			return std::round(static_cast<double>(v_) * 1e6) / 1e6;
		}

		class Writer {
		public:
			explicit Writer(JSON& j_) : J_{ j_ } {}
			void operator()(char const* key_, float const& v_) { J_[key_] = Round(v_); }
			void operator()(char const* key_, std::int32_t const& v_) { J_[key_] = v_; }
			void operator()(char const* key_, Lumina::Float3 const& v_) {
				J_[key_] = JSON::array({ Round(v_.x), Round(v_.y), Round(v_.z) });
			}
		private:
			JSON& J_;
		};

		class Reader {
		public:
			explicit Reader(JSON const& j_) : J_{ j_ } {}
			void operator()(char const* key_, float& v_) const {
				if (JSON const* n{ Find(key_) }; n && n->is_number()) { v_ = n->get<float>(); }
			}
			void operator()(char const* key_, std::int32_t& v_) const {
				if (JSON const* n{ Find(key_) }; n && n->is_number_integer()) { v_ = n->get<std::int32_t>(); }
			}
			void operator()(char const* key_, Lumina::Float3& v_) const {
				JSON const* n{ Find(key_) };
				if (!n || !n->is_array() || n->size() != 3U) { return; }
				for (std::size_t i = 0; i < 3U; ++i) { if (!(*n)[i].is_number()) { return; } }
				v_ = { (*n)[0].get<float>(), (*n)[1].get<float>(), (*n)[2].get<float>() };
			}
		private:
			auto Find(char const* key_) const -> JSON const* {
				if (!J_.is_object()) { return nullptr; }
				auto const itr{ J_.find(key_) };
				return (itr == J_.end()) ? nullptr : &*itr;
			}
			JSON const& J_;
		};

		// 保存する調整値の一覧（Writer / Reader の両方がこれを通る）
		template<class Archive, class Setup>
		void Visit(Archive& ar_, Setup& s_) {
			ar_("MaxHP", s_.MaxHP);
			ar_("MoveSpeed", s_.MoveSpeed);
			ar_("TurnRate", s_.TurnRate);
			ar_("GuardMoveScale", s_.GuardMoveScale);
			ar_("GuardHalfAngle", s_.GuardHalfAngle);
			ar_("GuardDamageScale", s_.GuardDamageScale);
			ar_("GuardKnockbackScale", s_.GuardKnockbackScale);
			ar_("GuardStunFrames", s_.GuardStunFrames);
			ar_("DeflectFrames", s_.DeflectFrames);
			ar_("DeflectRepressLockout", s_.DeflectRepressLockout);
			ar_("DeflectedKnockback", s_.DeflectedKnockback);
			ar_("DeflectedStunFrames", s_.DeflectedStunFrames);
			ar_("DashSpeed", s_.DashSpeed);
			ar_("DashDecay", s_.DashDecay);
			ar_("DashFrames", s_.DashFrames);
			ar_("DodgeInvincibleStart", s_.DodgeInvincibleStart);
			ar_("DodgeInvincibleFrames", s_.DodgeInvincibleFrames);
			ar_("JumpInitialSpeed", s_.JumpInitialSpeed);
			ar_("Gravity", s_.Gravity);
			ar_("SecondJumpDelay", s_.SecondJumpDelay);
			ar_("KnockbackScale", s_.KnockbackScale);
			ar_("KnockbackDecay", s_.KnockbackDecay);
			ar_("HitStunFrames", s_.HitStunFrames);
			ar_("BodyCenter", s_.BodyCenter);
			ar_("BodyHalf", s_.BodyHalf);
			ar_("PushRadius", s_.PushRadius);
		}
	}

	void Write(JSON& j_, FighterSetup const& s_) {
		Writer w{ j_ };
		Visit(w, s_);
	}

	void Read(JSON const& j_, FighterSetup& s_) {
		Reader r{ j_ };
		Visit(r, s_);
	}

	void Apply(Fighter& fighter_, FighterSetup const& s_) {
		fighter_.SetSetup(s_);
		fighter_.RefreshBodyShape();
		fighter_.SetMaxHP(s_.MaxHP);
	}

	auto SaveFile(char const* path_, JSON const& root_, std::string& status_) -> bool {
		std::ofstream ofs{ path_, std::ios::out | std::ios::trunc };
		if (!ofs.is_open()) { status_ = "Save failed (cannot open file)."; return false; }
		ofs << root_.dump(2) << '\n';
		if (!ofs.good()) { status_ = "Save failed (write error)."; return false; }
		status_ = "Saved.";
		return true;
	}

	auto LoadFile(char const* path_, std::string& status_) -> std::optional<JSON> {
		std::ifstream ifs{ path_ };
		if (!ifs.is_open()) { status_ = "No file (using defaults)."; return std::nullopt; }
		JSON root = JSON::parse(ifs, nullptr, false);		// 例外なし
		if (root.is_discarded() || !root.is_object()) { status_ = "Load failed (broken JSON)."; return std::nullopt; }
		status_ = "Loaded.";
		return std::optional<JSON>{ std::in_place, std::move(root) };	// json は何にでも変換できるので、optional へは明示的に作る
	}

	auto SavePlayer(Fighter const& player_, std::string& status_) -> bool {
		JSON root = JSON::object();
		root["version"] = 1;
		JSON body = JSON::object();
		Write(body, player_.Setup());
		root["Body"] = std::move(body);
		return SaveFile(PlayerConfigPath, root, status_);
	}

	auto LoadPlayer(Fighter& player_, std::string& status_) -> bool {
		auto const root{ LoadFile(PlayerConfigPath, status_) };
		if (!root) { return false; }
		FighterSetup s{ player_.Setup() };
		if (auto const itr{ root->find("Body") }; itr != root->end()) { Read(*itr, s); }
		Apply(player_, s);
		return true;
	}
}
