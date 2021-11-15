#pragma once

#include "IG_Config.h"

#include <iomanip>

namespace IG {
#define REMOVE_LAST_LINE "\33[2K\r"
#define SAVE_CURSOR "\33[s"
#define RESTORE_CURSOR "\33[u"
class StatusObserver {
public:
	StatusObserver(bool beautify, uint64 updateCycleSeconds, uint64 targetIteration)
		: mBeautify(beautify)
		, mUpdateCycleSeconds(updateCycleSeconds)
		, mTargetSamples(targetIteration)
		, mFirstTime(false)
	{
	}

	static inline std::string timestr(uint64 sec)
	{
		if (sec == 0)
			return " <1s";

		std::stringstream sstream;

		uint64 s = sec % 60;
		sec /= 60;
		uint64 m = sec % 60;
		sec /= 60;
		uint64 h = sec;

		if (h > 0)
			sstream << h << "h ";

		if (m > 0)
			sstream << m << "m ";

		if (s > 0)
			sstream << s << "s ";

		return sstream.str();
	}

	inline void begin()
	{
		mStart		= std::chrono::high_resolution_clock::now();
		mLastUpdate = mStart;
		mFirstTime	= true;
	}

	inline void end()
	{
		if (mBeautify)
			std::cout << REMOVE_LAST_LINE;
		std::cout << "Done" << std::setw(120) << " " << std::endl;
	}

	inline void update(uint64 currentSamples)
	{
		constexpr int PERC_OUTPUT_FIELD_SIZE = 6; // Percentage
		constexpr int ITER_OUTPUT_FIELD_SIZE = 5; // Current Iteration
		constexpr int TIME_OUTPUT_FIELD_SIZE = 8; // Time spent and left

		const auto now			= std::chrono::high_resolution_clock::now();
		const auto duration		= std::chrono::duration_cast<std::chrono::seconds>(now - mLastUpdate);
		const auto fullDuration = std::chrono::duration_cast<std::chrono::seconds>(now - mStart);

		if ((uint64)duration.count() >= mUpdateCycleSeconds) {
			if (mBeautify) {
				if (mFirstTime) {
					std::cout << SAVE_CURSOR;
					mFirstTime = false;
				} else {
					std::cout << REMOVE_LAST_LINE << RESTORE_CURSOR;
				}

				if (mTargetSamples != 0)
					drawProgressbar(currentSamples / double(mTargetSamples));
			}
			if (mTargetSamples != 0) {
				const double percentage = 100 * (currentSamples / double(mTargetSamples));
				std::cout << std::setw(PERC_OUTPUT_FIELD_SIZE) << std::setprecision(4) << std::fixed << percentage << "% | ";
			}

			std::cout << "S: " << std::setw(ITER_OUTPUT_FIELD_SIZE) << currentSamples
					  << " | RT: " << std::setw(TIME_OUTPUT_FIELD_SIZE) << timestr(fullDuration.count());

			if (mTargetSamples != 0) {
				const double percentage = 100 * (currentSamples / double(mTargetSamples));
				const float etaFactor	= percentage > FltEps ? (100 - percentage) / percentage : 100.0f /* Just something high*/;
				std::cout << " ETA: " << std::setw(TIME_OUTPUT_FIELD_SIZE) << timestr(fullDuration.count() * etaFactor);
			}

			if (!mBeautify)
				std::cout << std::endl;
			else
				std::cout << std::flush;

			mLastUpdate = now;
		}
	}

private:
	inline void drawProgressbar(float perc)
	{
		constexpr size_t LENGTH = 20;
		size_t full_count		= (size_t)std::floor(LENGTH * perc);

		std::cout << "\u2563";
		for (size_t i = 0; i < full_count; ++i)
			std::cout << "\u2588";
		for (size_t i = full_count; i < LENGTH; ++i)
			std::cout << "\u2591";
		std::cout << "\u2560 ";
	}

	const bool mBeautify;
	const uint64 mUpdateCycleSeconds;
	const uint64 mTargetSamples;
	std::chrono::high_resolution_clock::time_point mStart;
	std::chrono::high_resolution_clock::time_point mLastUpdate;
	bool mFirstTime;
};
} // namespace IG