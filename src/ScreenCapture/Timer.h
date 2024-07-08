#pragma once

#include <chrono>

namespace util
{
    class Timer
    {
    public:
        Timer()
        {
            _start = 0;
        }
        void start()
        {
            _start = now();
        }
        double seconds()
        {
            auto diff = static_cast<double>(now() - _start);
            return diff / 1000000.0;
        }
        double milliseconds()
        {
            return static_cast<double>(now() - _start) / 1000.0;
        }
        double microseconds()
        {
            return static_cast<double>(now() - _start);
        }

    private:
        int64_t now()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        int64_t _start;
    };
}
