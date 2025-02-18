#pragma once

#include <chrono>

namespace util
{
    class Timer
    {
    public:
        Timer();
        ~Timer();

        void Start()
        {
            _start = now();
        }
        double Seconds()
        {
            auto diff = static_cast<double>(now() - _start);
            return diff / 1000000.0;
        }
        double Milliseconds()
        {
            return static_cast<double>(now() - _start) / 1000.0;
        }
        double Microseconds()
        {
            return static_cast<double>(now() - _start);
        }

        // A much more accurate sleep...
        void Sleep(__int64 usec);

    private:
        int64_t now()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        int64_t _start = 0;
        void* _timer = nullptr;
        unsigned long _currentResolution = 0;
    };
}
