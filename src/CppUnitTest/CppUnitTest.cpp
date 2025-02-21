// CppUnitTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include "Timer.h"

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

int main()
{
	Timer t;
	t.Start();

	uint64_t microseconds = 1000000;
	std::vector<double> errors;
	errors.reserve(microseconds);

	while (microseconds > 0) {
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
