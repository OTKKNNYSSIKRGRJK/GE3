export module Lumina.Utils.Time;

import <chrono>;

namespace Lumina::Utils {
	export template<typename DurationType = std::chrono::seconds>
		requires (std::chrono::_Is_duration_v<DurationType>)
	auto Timestamp() -> std::chrono::zoned_time<DurationType> {
		auto utcTimestamp_Raw{ std::chrono::system_clock::now() };
		// Timestamp represented in DurationType
		auto utcTimestamp{ std::chrono::time_point_cast<DurationType>(utcTimestamp_Raw) };
		std::chrono::zoned_time<DurationType> localTimestamp{ std::chrono::current_zone(), utcTimestamp };
		return localTimestamp;
	}

	export class Horometer {
		using SystemClock = std::chrono::system_clock;
		using SystemTimePoint = SystemClock::time_point;

		using SteadyClock = std::chrono::steady_clock;
		using SteadyTimePoint = SteadyClock::time_point;

	public:
		using Nanosecond = std::chrono::nanoseconds;
		using Millisecond = std::chrono::milliseconds;

	public:
		static auto CurrentTime() noexcept { return SystemClock::now(); }

		constexpr uint64_t FrameCount() const noexcept { return Count_FramesSinceStartup_; }

		template<typename TimeUnit = Millisecond>
		constexpr TimeUnit DeltaTime() const noexcept {
			return std::chrono::duration_cast<TimeUnit>(DeltaTime_);
		}

	public:
		void Initialize();

	public:
		void Update();

	private:
		SteadyTimePoint TimePoint_CurrentFrame_{};
		SteadyTimePoint TimePoint_PreviousFrame_{};
		SteadyTimePoint TimePoint_Startup_{};

		Nanosecond DeltaTime_{ 0 };
		Nanosecond TimeSinceStartup_{ 0 };

		uint64_t Count_FramesSinceStartup_{ 0LLU };

		float FrameRate_{ 0.0f };
	};

	void Horometer::Initialize() {
		TimePoint_CurrentFrame_ = SteadyClock::now();
		TimePoint_Startup_ = SteadyClock::now();
	}

	void Horometer::Update() {
		TimePoint_PreviousFrame_ = TimePoint_CurrentFrame_;
		TimePoint_CurrentFrame_ = SteadyClock::now();
		DeltaTime_ = TimePoint_CurrentFrame_ - TimePoint_PreviousFrame_;
		TimeSinceStartup_ = TimePoint_CurrentFrame_ - TimePoint_Startup_;

		++Count_FramesSinceStartup_;
	}
}