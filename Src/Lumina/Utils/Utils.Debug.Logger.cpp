module Lumina.Utils.Debug : Logger;

namespace Lumina::Utils::Debug {
	void Logger::SeverityLevelThreshold(uint32_t threshold_) {
		SeverityLevelThreshold_ = threshold_;
	}

	uint32_t Logger::SeverityLevelThreshold() {
		return SeverityLevelThreshold_;
	}

	//----	------	------	------	------	----//

	void Logger::TimestampedOutput(std::string&& str_) {
		std::string output{
			std::format(
				"{:%Y-%m-%d %H:%M:%S},{:s}",
				Timestamp<std::chrono::milliseconds>(),
				str_
			)
		};
		FileOutputStream_ << output;
		CurrentBufferSize_ += output.length();
		if (CurrentBufferSize_ >= FlushThreshold_) {
			FileOutputStream_.flush();
			CurrentBufferSize_ = 0U;
		}
	}

	//----	------	------	------	------	----//

	Logger& Logger::Default() {
		static Logger defaultLogger{ "Default" };
		return defaultLogger;
	}

	//----	------	------	------	------	----//

	void Logger::Initialize(std::string_view logFileTitle_) {
		std::filesystem::create_directory("Logs");
		std::string logFilePath{
			std::format(
				"Logs/{:%Y%m%d_%H%M%S}_{:s}.log",
				Timestamp(),
				logFileTitle_
			)
		};
		FileOutputStream_.open(logFilePath);

		#if defined(_DEBUG)
		SeverityLevelThreshold_ = 0U;
		#else
		SeverityLevelThreshold_ = 1U;
		#endif
	}

	//----	------	------	------	------	----//

	Logger::Logger() noexcept {}

	Logger::Logger(std::string_view logFileTitle_) {
		Initialize(logFileTitle_);
	}

	Logger::~Logger() noexcept {
		FileOutputStream_.flush();
		FileOutputStream_.close();
	}
}