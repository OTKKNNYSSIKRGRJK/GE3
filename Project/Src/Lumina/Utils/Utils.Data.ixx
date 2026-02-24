module;

#include<vector>
#include<unordered_map>

#include<string>
#include<format>

#include<fstream>
#include<sstream>

#include<External/nlohmann.JSON/single_include/nlohmann/json.hpp>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.Utils.Data;

import Lumina.Utils.Debug;

import Lumina.Math.Numerics;

export using NLohmannJSON = nlohmann::json;

//////	//////	//////	//////	//////	//////

export namespace Lumina::Utils {
	class WavefrontOBJ;
	class WavefrontMTL;

	//////	//////	//////	//////	//////	//////

	template<typename ParsedDataType>
	ParsedDataType LoadFromFile(std::string_view fileName_, std::string_view directory_ = "");

	template<> NLohmannJSON LoadFromFile(std::string_view fileName_, std::string_view directory_);
	template<> WavefrontOBJ LoadFromFile(std::string_view fileName_, std::string_view directory_);
	template<> WavefrontMTL LoadFromFile(std::string_view fileName_, std::string_view directory_);
}

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

namespace Lumina::Utils {
	namespace {
		std::string GetFilePath(std::string_view fileName_, std::string_view directory_) {
			std::string filePath{ directory_ };
			if (filePath.size() > 0LLU) { filePath += "/"; }
			filePath += fileName_;

			return filePath;
		}

		std::ifstream LoadFileStream(std::string_view filePath_) {
			std::ifstream ifs{ filePath_.data() };
			(!!ifs) ||
				Debug::ThrowIfFailed{
					std::format(
						"<Utils.LoadFileStream> Failed to open {}!\n",
						filePath_
					)
				};
			return ifs;
		}
	}
}

//////	//////	//////	//////	//////	//////
//	JavaScript Object Notation				//
//////	//////	//////	//////	//////	//////

namespace Lumina::Utils {

	//////	//////	//////	//////	//////	//////
	//	LoadFromFile<NLohmannJSON>				//
	//////	//////	//////	//////	//////	//////

	template<>
	NLohmannJSON LoadFromFile(std::string_view fileName_, std::string_view directory_) {
		std::string&& filePath{ GetFilePath(fileName_, directory_) };
		std::ifstream&& ifs{ LoadFileStream(filePath) };

		NLohmannJSON parsedData{ NLohmannJSON::parse(ifs) };
		Debug::Logger::Default().Message<0U>(
			"<Lumina.Utils.LoadFromFile.NLohmannJSON> File \"{}\" parsed successfully.\n",
			filePath
		);

		ifs.close();

		return parsedData;
	}
}

//////	//////	//////	//////	//////	//////
//	Wavefront .OBJ							//
//////	//////	//////	//////	//////	//////

namespace Lumina::Utils {

	//////	//////	//////	//////	//////	//////
	//	WavefrontOBJ							//
	//////	//////	//////	//////	//////	//////

	class WavefrontOBJ {
	private:
		struct Vertex {
			uint32_t Index_Position;
			uint32_t Index_TexCoord;
			uint32_t Index_Normal;
		};

		struct Face {
			uint32_t Index_Vertex_First;
			uint32_t Index_Vertex_Last;
		};

		//////	//////	//////	//////	//////	//////

	public:
		constexpr auto Position(uint32_t idx_) const -> Numerics::Float4 const& { return Positions_.at(idx_); }
		constexpr auto TexCoord(uint32_t idx_) const -> Numerics::Float2 const& { return TexCoords_.at(idx_); }
		constexpr auto Normal(uint32_t idx_) const -> Numerics::Float3 const& { return Normals_.at(idx_); }

		constexpr auto Positions() const noexcept -> std::vector<Numerics::Float4> const& { return Positions_; }
		constexpr auto TexCoords() const noexcept -> std::vector<Numerics::Float2> const& { return TexCoords_; }
		constexpr auto Normals() const noexcept -> std::vector<Numerics::Float3> const& { return Normals_; }
		constexpr auto Vertices() const noexcept -> std::vector<Vertex> const& { return Vertices_; }
		constexpr auto Faces() const noexcept -> std::vector<Face> const& { return Faces_; }

		//////	//////	//////	//////	//////	//////

	protected:
		std::vector<Numerics::Float4> Positions_{};
		std::vector<Numerics::Float2> TexCoords_{};
		std::vector<Numerics::Float3> Normals_{};

		std::vector<Vertex> Vertices_{};
		std::vector<Face> Faces_{};

		std::vector<std::string> MTLFileNames_{};
	};

	//////	//////	//////	//////	//////	//////
	//	WavefrontOBJParser						//
	//////	//////	//////	//////	//////	//////

	namespace {
		class WavefrontOBJParser : public WavefrontOBJ {
			using Specifier = std::string;
			using Statement = void(WavefrontOBJParser::*)(std::istringstream&);

			//////	//////	//////	//////	//////	//////

		private:
			void ReadPosition(std::istringstream& iss_);
			void ReadTexCoord(std::istringstream& iss_);
			void ReadNormal(std::istringstream& iss_);
			void ReadFace(std::istringstream& iss_);
			void ReadMTLFileName(std::istringstream& iss_);

			//////	//////	//////	//////	//////	//////

		public:
			WavefrontOBJParser(std::istream& is_) {
				Specifier specifier{};
				std::string str_Line{};

				while (std::getline(is_, str_Line)) {
					std::istringstream iss_Line{ str_Line };
					iss_Line >> specifier;

					auto&& it{ Statements_.find(specifier) };
					if (it != Statements_.cend()) { (this->*(it->second))(iss_Line); }
				}
			}
			~WavefrontOBJParser() noexcept = default;

			//////	//////	//////	//////	//////	//////

		private:
			static inline std::unordered_map<Specifier, Statement> const Statements_{
				{ "v", &ReadPosition },
				{ "vt", &ReadTexCoord },
				{ "vn", &ReadNormal },
				{ "f", &ReadFace },
				{ "mtllib", &ReadMTLFileName },
			};
		};

		//////	//////	//////	//////	//////	//////

		void WavefrontOBJParser::ReadPosition(std::istringstream& iss_) {
			Numerics::Float4& pos{ Positions_.emplace_back() };
			iss_ >> pos.x >> pos.y >> pos.z;
			pos.w = 1.0f;
		}

		void WavefrontOBJParser::ReadTexCoord(std::istringstream& iss_) {
			Numerics::Float2& texCoord{ TexCoords_.emplace_back() };
			iss_ >> texCoord.x >> texCoord.y;
		}

		void WavefrontOBJParser::ReadNormal(std::istringstream& iss_) {
			Numerics::Float3& norm{ Normals_.emplace_back() };
			iss_ >> norm.x >> norm.y >> norm.z;
		}

		void WavefrontOBJParser::ReadFace(std::istringstream& iss_) {
			std::string str_Vert{};
			std::string str_VertElemIdx{};

			auto& face{ Faces_.emplace_back() };
			face.Index_Vertex_First = static_cast<uint32_t>(Vertices_.size());
			
			while (iss_ >> str_Vert) {
				std::istringstream iss_Vert{ str_Vert };

				auto& vert{ Vertices_.emplace_back() };
				for (uint32_t i_VertElem{ 0U }; i_VertElem < 3U; ++i_VertElem) {
					std::getline(iss_Vert, str_VertElemIdx, '/');
					// 0 -> PositionID
					// 1 -> TexCoordID
					// 2 -> NormalID
					*(reinterpret_cast<uint32_t*>(&vert) + i_VertElem) = std::stoi(str_VertElemIdx) - 1U;
				}
			}

			face.Index_Vertex_Last = static_cast<uint32_t>(Vertices_.size()) - 1U;
		}

		void WavefrontOBJParser::ReadMTLFileName(std::istringstream& iss_) {
			auto& fileName{ MTLFileNames_.emplace_back() };
			iss_ >> fileName;
		}
	}

	//////	//////	//////	//////	//////	//////
	//	LoadFromFile<WavefrontOBJ>				//
	//////	//////	//////	//////	//////	//////

	template<>
	WavefrontOBJ LoadFromFile(std::string_view fileName_, std::string_view directory_) {
		std::string&& filePath{ GetFilePath(fileName_, directory_) };
		std::ifstream&& ifs{ LoadFileStream(filePath) };

		WavefrontOBJ parsedData{ WavefrontOBJParser{ ifs } };
		Debug::Logger::Default().Message<0U>(
			"<Lumina.Utils.LoadFromFile.WavefrontOBJ> File \"{}\" parsed successfully.\n",
			filePath
		);

		ifs.close();

		return parsedData;
	}
}

//////	//////	//////	//////	//////	//////
//	Wavefront .MTL							//
//////	//////	//////	//////	//////	//////

namespace Lumina::Utils {
	class WavefrontMTL {
	protected:
		std::string Directory_{};
		std::string TextureFileName_{};
	};

	namespace {
		class WavefrontMTLParser : public WavefrontMTL {
			using Specifier = std::string;
			using Statement = void(WavefrontMTLParser::*)(std::istringstream&);

		private:
			void ReadTextureFileName(std::istringstream& iss_);

		public:
			WavefrontMTLParser(std::istream& is_) {
				Specifier specifier{};
				std::string str_Line{};

				while (std::getline(is_, str_Line)) {
					std::istringstream iss_Line{ str_Line };
					iss_Line >> specifier;

					auto&& it{ Statements_.find(specifier) };
					if (it != Statements_.cend()) { (this->*(it->second))(iss_Line); }
				}
			}
			~WavefrontMTLParser() noexcept = default;

		private:
			static inline std::unordered_map<Specifier, Statement> Statements_{
				{ "map_Kd", &ReadTextureFileName },
			};
		};

		void WavefrontMTLParser::ReadTextureFileName(std::istringstream& iss_) {
			iss_ >> TextureFileName_;
		}
	}

	//////	//////	//////	//////	//////	//////
	//	LoadFromFile<WavefrontMTL>				//
	//////	//////	//////	//////	//////	//////

	template<>
	WavefrontMTL LoadFromFile(std::string_view fileName_, std::string_view directory_) {
		std::string&& filePath{ GetFilePath(fileName_, directory_) };
		std::ifstream&& ifs{ LoadFileStream(filePath) };

		WavefrontMTL parsedData{ WavefrontMTLParser{ ifs } };
		Debug::Logger::Default().Message<0U>(
			"<Lumina.Utils.LoadFromFile.WavefrontMTL> File \"{}\" parsed successfully.\n",
			filePath
		);

		ifs.close();

		return parsedData;
	}
}