export module Game.MapGenerator;

import <cstdint>;

import <numeric>;

import <vector>;
import <array>;

import Lumina.Math.Numerics;
import Lumina.Math.Random;

namespace Game {
	namespace {
		enum DIRECTION : uint32_t {
			LEFTWARD = 0U,
			RIGHTWARD = 1U,
			UPWARD = 2U,
			DOWNWARD = 3U,
			FRONTWARD = 4U,
			BACKWARD = 5U,
		};

		constinit Lumina::Int2 const Displacements_2D[4]{
			{ -1, 0 }, { 1, 0 },
			{ 0, -1 }, { 0, 1 },
		};
		constinit Lumina::Int3 const Displacements_3D[6]{
			{ -1, 0, 0 }, { 1, 0, 0 },
			{ 0, -1, 0 }, { 0, 1, 0 },
			{ 0, 0, -1 }, { 0, 0, 1 },
		};
	}
}

namespace Game {

	// Credits : https://github.com/AtTheMatinee/dungeon-generation/blob/master/dungeonGenerationAlgorithms.py

	class RandomWalk2D {
	public:
		Lumina::Int2 const& Position() const noexcept { return Position_; }

	public:
		void operator()() {
			Probabilities_Direction_.fill(1.0f);

			//----	------	------	------	------	----//

			if (Position_.x < static_cast<int32_t>(LatticeWidth_ * Margins_[DIRECTION::LEFTWARD])) {
				Probabilities_Direction_[DIRECTION::RIGHTWARD] += Weights_TowardCenter_[DIRECTION::RIGHTWARD];
			}
			else if (Position_.x > static_cast<int32_t>(LatticeWidth_ * Margins_[DIRECTION::RIGHTWARD])) {
				Probabilities_Direction_[DIRECTION::LEFTWARD] += Weights_TowardCenter_[DIRECTION::LEFTWARD];
			}

			if (Position_.y < static_cast<int32_t>(LatticeHeight_ * Margins_[DIRECTION::UPWARD])) {
				Probabilities_Direction_[DIRECTION::DOWNWARD] += Weights_TowardCenter_[DIRECTION::DOWNWARD];
			}
			else if (Position_.y > static_cast<int32_t>(LatticeHeight_ * Margins_[DIRECTION::DOWNWARD])) {
				Probabilities_Direction_[DIRECTION::UPWARD] += Weights_TowardCenter_[DIRECTION::UPWARD];
			}

			//----	------	------	------	------	----//

			Probabilities_Direction_[PreviousDirection_] += Weight_TowardPreviousDirection_;

			//----	------	------	------	------	----//

			float const inv_Sum{
				1.0f / (
					std::reduce(
						Probabilities_Direction_.cbegin(),
						Probabilities_Direction_.cend()
					)
				)
			};
			float rnd{
				static_cast<float>(
					Lumina::Random::Generator()() & 0xFFFFU
				) * 0.0000152587890625f
			};
			for (size_t idx_Dir{ 0LLU }; idx_Dir < Probabilities_Direction_.size(); ++idx_Dir) {
				auto& prob_Dir{ Probabilities_Direction_[idx_Dir] };
				prob_Dir *= inv_Sum;
				if (rnd < prob_Dir) {
					Lumina::Int2 const& dPos{ Displacements_2D[idx_Dir] };
					Lumina::Int2 newPos{ Position_.x + dPos.x, Position_.y + dPos.y };
					if (
						(newPos.x >= 0 && newPos.x < static_cast<int32_t>(LatticeWidth_)) &&
						(newPos.y >= 0 && newPos.y < static_cast<int32_t>(LatticeHeight_))
					) {
						Position_ = newPos;
						PreviousDirection_ = static_cast<DIRECTION>(idx_Dir);
						break;
					}
				}
				rnd -= prob_Dir;
			}
		}

	public:
		RandomWalk2D(uint32_t width_, uint32_t height_) :
			LatticeWidth_{ width_ },
			LatticeHeight_{ height_ } {
			Position_.x = 1U + Lumina::Random::Generator()() % (LatticeWidth_ - 2U);
			Position_.y = 1U + Lumina::Random::Generator()() % (LatticeHeight_ - 2U);
		}

	private:
		Lumina::Int2 Position_{ 0U, 0U };
		DIRECTION PreviousDirection_{};
		std::array<float, 4> Probabilities_Direction_{};

		uint32_t LatticeWidth_{};
		uint32_t LatticeHeight_{};

		float Weight_TowardPreviousDirection_{ 0.7f };
		float Weights_TowardCenter_[4]{ 0.15f, 0.15f, 0.15f, 0.15f, };
		float Margins_[4]{ 0.25f, 0.75f, 0.25f, 0.75f, };
	};

	class RandomWalk3D {
	public:
		Lumina::Int3 const& Position() const noexcept { return Position_; }

	public:
		void operator()() {
			Probabilities_Direction_.fill(1.0f);

			//----	------	------	------	------	----//

			if (Position_.x < static_cast<int32_t>(MapWidth_ * Margins_[DIRECTION::LEFTWARD])) {
				Probabilities_Direction_[DIRECTION::RIGHTWARD] += Weights_TowardCenter_[DIRECTION::RIGHTWARD];
			}
			else if (Position_.x > static_cast<int32_t>(MapWidth_ * Margins_[DIRECTION::RIGHTWARD])) {
				Probabilities_Direction_[DIRECTION::LEFTWARD] += Weights_TowardCenter_[DIRECTION::LEFTWARD];
			}

			if (Position_.y < static_cast<int32_t>(MapHeight_ * Margins_[DIRECTION::UPWARD])) {
				Probabilities_Direction_[DIRECTION::DOWNWARD] += Weights_TowardCenter_[DIRECTION::DOWNWARD];
			}
			else if (Position_.y > static_cast<int32_t>(MapHeight_ * Margins_[DIRECTION::DOWNWARD])) {
				Probabilities_Direction_[DIRECTION::UPWARD] += Weights_TowardCenter_[DIRECTION::UPWARD];
			}

			if (Position_.z < static_cast<int32_t>(MapDepth_ * Margins_[DIRECTION::FRONTWARD])) {
				Probabilities_Direction_[DIRECTION::BACKWARD] += Weights_TowardCenter_[DIRECTION::BACKWARD];
			}
			else if (Position_.z > static_cast<int32_t>(MapDepth_ * Margins_[DIRECTION::BACKWARD])) {
				Probabilities_Direction_[DIRECTION::FRONTWARD] += Weights_TowardCenter_[DIRECTION::FRONTWARD];
			}

			//----	------	------	------	------	----//

			Probabilities_Direction_[PreviousDirection_] += Weight_TowardPreviousDirection_;

			//----	------	------	------	------	----//

			float const inv_Sum{
				1.0f / (
					std::reduce(
						Probabilities_Direction_.cbegin(),
						Probabilities_Direction_.cend()
					)
				)
			};
			float rnd{
				static_cast<float>(
					Lumina::Random::Generator()() & 0xFFFFU
				) * 0.0000152587890625f
			};
			for (size_t idx_Dir{ 0LLU }; idx_Dir < Probabilities_Direction_.size(); ++idx_Dir) {
				auto& prob_Dir{ Probabilities_Direction_[idx_Dir] };
				prob_Dir *= inv_Sum;
				if (rnd < prob_Dir) {
					Lumina::Int3 const& dPos{ Displacements_3D[idx_Dir] };
					Lumina::Int3 newPos{ Position_.x + dPos.x, Position_.y + dPos.y, Position_.z + dPos.z };
					if (
						(newPos.x >= 0 && newPos.x < static_cast<int32_t>(MapWidth_)) &&
						(newPos.y >= 0 && newPos.y < static_cast<int32_t>(MapHeight_)) &&
						(newPos.z >= 0 && newPos.z < static_cast<int32_t>(MapDepth_))
					) {
						Position_ = newPos;
						PreviousDirection_ = static_cast<DIRECTION>(idx_Dir);
						break;
					}
				}
				rnd -= prob_Dir;
			}
		}

	public:
		RandomWalk3D(uint32_t width_, uint32_t height_, uint32_t depth_) :
			MapWidth_{ width_ },
			MapHeight_{ height_ },
			MapDepth_{ depth_ } {
			Position_.x = 1U + Lumina::Random::Generator()() % (MapWidth_ - 2U);
			Position_.y = 1U + Lumina::Random::Generator()() % (MapHeight_ - 2U);
			Position_.z = 1U + Lumina::Random::Generator()() % (MapDepth_ - 2U);
		}

	private:
		Lumina::Int3 Position_{ 0U, 0U, 0U };
		DIRECTION PreviousDirection_{};
		std::array<float, 6> Probabilities_Direction_{};

		uint32_t MapWidth_{};
		uint32_t MapHeight_{};
		uint32_t MapDepth_{};

		float Weight_TowardPreviousDirection_{ 0.5f };
		float Weights_TowardCenter_[6]{ 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, };
		float Margins_[6]{ 0.25f, 0.75f, 0.25f, 0.75f, 0.25f, 0.75f, };
	};

	export class CellularAutomata {
	private:
		constexpr uint32_t Num_AdjacentWalls(Lumina::Int2 const& tilePos_) {
			auto const* mapRowAbove{ Map_[tilePos_.y - 1].data() };
			auto const* mapRow{ Map_[tilePos_.y].data() };
			auto const* mapRowBelow{ Map_[tilePos_.y + 1].data() };

			uint32_t cnt{ 0U };
			{
				cnt += (mapRowAbove[tilePos_.x - 1] != 0);
				cnt += (mapRowAbove[tilePos_.x] != 0);
				cnt += (mapRowAbove[tilePos_.x + 1] != 0);
				cnt += (mapRow[tilePos_.x - 1] != 0);
				cnt += (mapRow[tilePos_.x + 1] != 0);
				cnt += (mapRowBelow[tilePos_.x - 1] != 0);
				cnt += (mapRowBelow[tilePos_.x] != 0);
				cnt += (mapRowBelow[tilePos_.x + 1] != 0);
			}

			return cnt;
		}

		// To be revised
		constexpr bool CheckConnectivity(uint32_t idx_Cave0_, uint32_t idx_Cave1_) {
			std::vector<int> checkTable{};
			checkTable.assign(CaveConnectivities_.size(), 0);

			uint32_t idx_CurrentCave{ idx_Cave0_ };
			for (size_t cnt_It{ 0U }; (cnt_It < CaveConnectivities_.size()) && (idx_CurrentCave != idx_Cave1_); ++cnt_It) {
				checkTable[idx_CurrentCave] = 1;
				for (
					uint32_t idx_NextCave = 0U;
					(idx_NextCave < static_cast<uint32_t>(CaveConnectivities_.size()));
					++idx_NextCave
				) {
					if (
						(idx_CurrentCave != idx_NextCave) &&
						CaveConnectivities_[idx_CurrentCave][idx_NextCave] &&
						!checkTable[idx_NextCave]
					) {
						idx_CurrentCave = idx_NextCave;
						idx_NextCave = static_cast<uint32_t>(CaveConnectivities_.size());
					}
				}
			}
			
			return (idx_CurrentCave == idx_Cave1_);
		}

	private:
		void Flood(uint32_t tileX_, uint32_t tileY_) {
			if ((tileX_ < 0) || (tileX_ >= MapWidth_)) { return; }
			if ((tileY_ < 0) || (tileY_ >= MapHeight_)) { return; }
			if (Map_[tileY_][tileX_] != 0) { return; }

			Map_[tileY_][tileX_] ^= ((Caves_.size()) << 20U);
			Caves_.back().emplace_back(tileX_, tileY_);

			Flood(tileX_ - 1U, tileY_);
			Flood(tileX_ + 1U, tileY_);
			Flood(tileX_, tileY_ - 1U);
			Flood(tileX_, tileY_ + 1U);
		}

		void GenerateTunnel(Lumina::Int2 const& pos_Start_, Lumina::Int2 const& pos_Goal_) {
			Lumina::Int2 pos{ pos_Start_ };
			std::array<float, 4> probs_Dir{};

			//----	------	------	------	------	----//

			while ((pos.x != pos_Goal_.x) || (pos.y != pos_Goal_.y)) {
				probs_Dir.fill(1.0f);

				//----	------	------	------	------	----//

				if (pos.x > pos_Goal_.x) {
					probs_Dir[DIRECTION::LEFTWARD] += 2.0f;
				}
				else if (pos.x < pos_Goal_.x) {
					probs_Dir[DIRECTION::RIGHTWARD] += 2.0f;
				}

				if (pos.y > pos_Goal_.y) {
					probs_Dir[DIRECTION::UPWARD] += 2.0f;
				}
				else if (pos.y < pos_Goal_.y) {
					probs_Dir[DIRECTION::DOWNWARD] += 2.0f;
				}

				//----	------	------	------	------	----//

				float const inv_Sum{
					1.0f / (
						std::reduce(
							probs_Dir.cbegin(),
							probs_Dir.cend()
						)
					)
				};
				float rnd{
					static_cast<float>(
						Lumina::Random::Generator()() & 0xFFFFU
					) * 0.0000152587890625f
				};
				for (size_t idx_Dir{ 0LLU }; idx_Dir < probs_Dir.size(); ++idx_Dir) {
					auto& prob_Dir{ probs_Dir[idx_Dir] };
					prob_Dir *= inv_Sum;
					if (rnd < prob_Dir) {
						Lumina::Int2 const& dPos{ Displacements_2D[idx_Dir] };
						Lumina::Int2 newPos{ pos.x + dPos.x, pos.y + dPos.y };
						if (
							(newPos.x >= 1 && newPos.x < static_cast<int32_t>(MapWidth_) - 1) &&
							(newPos.y >= 1 && newPos.y < static_cast<int32_t>(MapHeight_) - 1)
						) {
							pos = newPos;
							Map_[pos.y][pos.x] = 0;
							break;
						}
					}
					rnd -= prob_Dir;
				}
			}
		}

	private:
		void SetInitialState() {
			auto& rndGen{ Lumina::Random::Generator() };

			for (uint32_t y{ MapMargin_ }; y < (MapHeight_ - MapMargin_); ++y) {
				auto* mapRow{ Map_.at(y).data() };
				for (uint32_t x{ MapMargin_ }; x < (MapWidth_ - MapMargin_); ++x) {
					float rnd{
						static_cast<float>(
							rndGen() & 0xFFFFU
						) * 0.0000152587890625f
					};
					if (rnd >= Probability_Wallification_) {
						mapRow[x] = 0;
					}
				}
			}
		}

		void GenerateCaves() {
			Lumina::Int2 tile{};
			for (uint32_t cnt_It{ 0U }; cnt_It < Iterations_; ++cnt_It) {
				tile.x = 1U + Lumina::Random::Generator()() % (MapWidth_ - 2U);
				tile.y = 1U + Lumina::Random::Generator()() % (MapHeight_ - 2U);
				uint32_t num_Walls{ Num_AdjacentWalls(tile) };
				if (num_Walls > Condition_WallifiedByNeighbors_) { Map_[tile.y][tile.x] = 1; }
				else if (num_Walls < Condition_WallifiedByNeighbors_) { Map_[tile.y][tile.x] = 0; }
			}
		}

		void IdentifyCaves() {
			for (uint32_t y{ 0U }; y < MapHeight_; ++y) {
				auto* mapRow{ Map_.at(y).data() };
				for (uint32_t x{ 0U }; x < MapWidth_; ++x) {
					if (mapRow[x] == 0) {
						auto& newCave{ Caves_.emplace_back() };
						Flood(x, y);
						if (newCave.size() < MinSize_Cave_) {
							for (auto const& tilePos : newCave) {
								Map_[tilePos.y][tilePos.x] = 1;
							}
							Caves_.pop_back();
						}
						else {
							CaveConnectivities_.emplace_back();
						}
					}
				}
			}
		}

		void ConnectCaves() {
			for (auto& connectivity : CaveConnectivities_) {
				connectivity.assign(CaveConnectivities_.size(), 0);
			}

			uint32_t idx_Cave0{};
			uint32_t idx_Cave1{};
			for (idx_Cave0 = 0U; idx_Cave0 < static_cast<uint32_t>(Caves_.size()); ++idx_Cave0) {
				CaveConnectivities_[idx_Cave0][idx_Cave0] = 1;
				uint32_t shortestDist2{ 0xFFFFFFFFU };

				auto& cave0{ Caves_[idx_Cave0] };
				auto const idx0{ Lumina::Random::Generator()() % static_cast<uint32_t>(cave0.size()) };
				Lumina::Int2 pos0{ cave0[idx0] };
				Lumina::Int2 pos1{};

				for (uint32_t idx_Cave1Canditate = 0U; idx_Cave1Canditate < static_cast<uint32_t>(Caves_.size()); ++idx_Cave1Canditate) {
					if (CheckConnectivity(idx_Cave0, idx_Cave1Canditate)) { continue; }

					auto& cave1{ Caves_[idx_Cave1Canditate] };
					auto const idx1{ Lumina::Random::Generator()() % static_cast<uint32_t>(cave1.size()) };
					auto const& pos1Candidate{ cave1[idx1] };

					int32_t const dx{ pos0.x - pos1Candidate.x };
					int32_t const dy{ pos0.y - pos1Candidate.y };
					uint32_t const dist2{ static_cast<uint32_t>(dx * dx) + static_cast<uint32_t>(dy * dy) };
					if (shortestDist2 > dist2) {
						shortestDist2 = dist2;
						pos1 = pos1Candidate;
						idx_Cave1 = idx_Cave1Canditate;
					}
				}

				if ((pos1.x != 0) && (pos1.y != 0)) {
					GenerateTunnel(pos0, pos1);
					CaveConnectivities_[idx_Cave0][idx_Cave1] = 1;
					CaveConnectivities_[idx_Cave1][idx_Cave0] = 1;
				}
			}
		}

		void GenerateFeatures() {
			for (uint32_t y{ 1U }; y < MapHeight_ - 1U; ++y) {
				for (uint32_t x{ 1U }; x < MapWidth_ - 1U; ++x) {
					int tile = Map_[y][x] & ((1 << 20U) - 1);
					int tileBelow = Map_[y + 1U][x] & ((1 << 20U) - 1);
					if ((tile == 0) && (tileBelow != 0)) {
						Map_[y + 1U][x] &= ~((1 << 20U) - 1);
						Map_[y + 1U][x] |= 2;
					}
				}
			}
		}

		void GenerateItems() {

		}

	public:
		void Run(uint32_t mapWidth_, uint32_t mapHeight_) {
			MapWidth_ = mapWidth_;
			MapHeight_ = mapHeight_;

			Map_.resize(MapHeight_);
			for (auto& mapRow : Map_) {
				mapRow.assign(MapWidth_, 1);
			}

			SetInitialState();
			GenerateCaves();
			IdentifyCaves();
			ConnectCaves();
			GenerateFeatures();
		}

		// Temporary
		void GetMap(std::vector<std::vector<int>>& map_) {
			map_ = Map_;
		}

		std::vector<std::vector<Lumina::Int2>> const& GetCaves() const {
			return Caves_;
		}

	private:
		std::vector<std::vector<int>> Map_{};
		std::vector<std::vector<Lumina::Int2>> Caves_{};
		std::vector<std::vector<int>> CaveConnectivities_{};

		uint32_t MapWidth_{ 128U };
		uint32_t MapHeight_{ 64U };
		uint32_t MapMargin_{ 1U };

		uint32_t MaxSize_Cave_{ 500U };
		uint32_t MinSize_Cave_{ 16U };

		uint32_t Iterations_{ 30000U };
		uint32_t Condition_WallifiedByNeighbors_{ 4U };
		float Probability_Wallification_{ 0.45f };
	};

	export class MapGenerator {
		static inline constinit int const width = 100U;
		static inline constinit int const height = 30U;

	public:
		void f() {
			std::array<std::array<int, width>, height> map{};
			for (auto& mapRow : map) {
				mapRow.fill(1);
			}

			RandomWalk2D rw2D{ width, height };
			uint32_t goal{ static_cast<uint32_t>(width * height * Portion_Goal_) };
			uint32_t num_Filled{ 0U };
			uint32_t cnt_Iter{ 0U };
			while (num_Filled < goal && cnt_Iter < MaxIteration_) {
				rw2D();
				auto const& pos{ rw2D.Position() };
				if (map[pos.y][pos.x] == 1) {
					map[pos.y][pos.x] = 0;
					++num_Filled;
				}

				++cnt_Iter;
			}
		}

	private:
		float Portion_Goal_{ 0.3f };
		uint32_t MaxIteration_{ 25000U };
	};
}