export module Lumina.Utils.Debug : Logger;

//****	******	******	******	******	****//

import <cstdint>;

import <fstream>;
import <filesystem>;

import <string>;
import <format>;

import Lumina.Utils.Time;

//////	//////	//////	//////	//////	//////

namespace Lumina::Utils::Debug {
	export class Logger {
	public:
		void SeverityLevelThreshold(uint32_t threshold_);
		uint32_t SeverityLevelThreshold();

		//----	------	------	------	------	----//

	private:
		void TimestampedOutput(std::string&& str_);

	public:
		template<uint32_t SeverityLevel, typename...ArgTypes>
		void Message(std::format_string<ArgTypes...> const formatStr_, ArgTypes&&...args_) {
			if (SeverityLevelThreshold_ > SeverityLevel) { return; }

			TimestampedOutput(
				// Implementation of std::format
				std::vformat(
					formatStr_.get(),
					std::make_format_args(args_...)
				)
			);
		}

		//----	------	------	------	------	----//

	public:
		static Logger& Default();

		//----	------	------	------	------	----//

	public:
		void Initialize(std::string_view logFileTitle_);

		//----	------	------	------	------	----//

	public:
		Logger() noexcept;
		Logger(std::string_view logFileTitle_);
		~Logger() noexcept;

		Logger(Logger const&) = delete;
		Logger& operator=(Logger const&) = delete;

		//====	======	======	======	======	====//

	private:
		std::ofstream FileOutputStream_{};
		size_t CurrentBufferSize_{ 0U };

		uint32_t SeverityLevelThreshold_{};

		//----	------	------	------	------	----//

	private:
		static constexpr size_t FlushThreshold_{ 1024LLU };

		//////	//////	//////	//////	//////	//////

	public:
		template<typename...Types>
		inline void ConsolePrint(std::format_string<Types...> const formatStr_, Types&&...args_) {
			// Implementation of std::format
			auto&& logStr{
				std::forward<std::string>(
					std::vformat(formatStr_.get(), std::make_format_args(args_...))
				)
			};
			::OutputDebugStringA(logStr.data());
		}
	};
}