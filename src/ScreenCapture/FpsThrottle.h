#pragma once
#include "Timer.h"

namespace util
{
    class FpsThrottle
    {
    private:
        Timer _timer;
        int _fps = 0;
        bool _started = false;
        double _start_time = 0;
        int _step_count = 0;


    public:
        FpsThrottle(int fps) {
            _fps = fps;
        }

        void Reset() {
            _started = false;
            _start_time = 0;
            _step_count = 0;
            _timer.Start();
        }


        void Step() {
            double throttle_ms = 0;
            if (!_started) {
                _started = true;
                _start_time = _timer.Microseconds();
            }
            else {
                double elapsed = (_timer.Microseconds() - _start_time);
                double expected = (_step_count * 1000000.0) / _fps;
                double throttle_ms = expected - elapsed;
            }
            _step_count += 1;
            // sync to match the given frame rate.
            if (throttle_ms > 0) {
                _timer.Sleep(static_cast<int64_t>(throttle_ms)); // Sleep expects microseconds
            }
        }
    };
}