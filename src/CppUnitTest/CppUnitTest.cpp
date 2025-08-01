// CppUnitTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <algorithm> // Add this include for std::min
#include <numeric> // For std::accumulate
#include "Timer.h"
#include "FpsThrottle.h"
#undef min
#undef max

using namespace util;

struct MinMaxAvg
{
	double min;
	double max;
	double avg;

	static MinMaxAvg GetStats(std::vector<double> data) {
		double min = std::numeric_limits<double>::max();
		double max = std::numeric_limits<double>::min();
		double sum = 0;
		for (auto d : data) {
			if (d < min) min = d;
			if (d > max) max = d;
			sum += std::abs(d);
		}
		return { min, max, sum / data.size() };
	}
};

void TestTimer()
{
	Timer t;
	t.Start();

	uint64_t microseconds = 1000000;
	std::vector<double> errors;
	errors.reserve(microseconds);

	while (microseconds > 1) {
		errors.clear();
		std::cout << "testing sleep for " << microseconds << " microseconds" << std::endl;
		auto iterations = 10000000 / microseconds; // 10 seconds each

		for (int i = 0; i < iterations; i++) {
			util::Timer timer = util::Timer();
			timer.Start();
			timer.Sleep(microseconds);
			auto seconds = timer.Seconds();
			auto error = seconds - (microseconds / 1000000.0);
			errors.push_back(error);
		}

		auto sleep_stats = MinMaxAvg::GetStats(errors);

		std::cout << std::fixed << std::setprecision(6) << "sleep errors min=" << sleep_stats.min << " max=" << sleep_stats.max << " avg=" << sleep_stats.avg << std::endl;
		
		microseconds /= 10;
	}
}

void TestFpsThrottle() {
	std::cout << "Testing FpsThrottle at 60fps for 20 seconds..." << std::endl;
	FpsThrottle throttle(60);
	throttle.Step(); // warmup
	throttle.Reset();
	Timer timer;
	timer.Start();
	std::vector<double> ticks;
	for (int i = 0; i < 60 * 20; i++) {
		throttle.Step();
		ticks.push_back(timer.Milliseconds());
	}

	std::vector<double> steps;
	for (int i = 1; i < ticks.size(); i++) {
		steps.push_back(ticks[i] - ticks[i - 1]);
	}

	auto minStep = *std::min_element(steps.begin(), steps.end());
	auto maxStep = *std::max_element(steps.begin(), steps.end());
	double sum = std::accumulate(steps.begin(), steps.end(), 0.0);
	double mean = sum / steps.size();

	std::cout << "min=" << minStep << " max=" << maxStep << " mean=" << mean << std::endl;
}



int main()
{
	TestFpsThrottle();
	TestTimer();
}