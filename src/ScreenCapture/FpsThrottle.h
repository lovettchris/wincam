#pragma once
#include "Timer.h"

namespace util
{
    class FpsThrottle
    {
    private:
        Timer _timer;
        double _fps = 0;
        bool _started = false;
        double _start_time = 0;
        int _step_count = 0;


    public:
        FpsThrottle(int fps) {
            _fps = static_cast<double>(fps);
        }

        void Reset() {
            _started = false;
            _start_time = 0;
            _step_count = 0;
            _timer.Start();
        }


        void Step() {
            _step_count += 1;
            double throttle_ms = 0;
            if (!_started) {
                _started = true;
                _start_time = _timer.Seconds();
            }
            else {
                double elapsed = (_timer.Seconds() - _start_time);
                double expected = ((double)_step_count / _fps);
                throttle_ms = (expected - elapsed);
            }
            // sync to match the given frame rate.
            if (throttle_ms > 0) {
                _timer.Sleep(static_cast<int64_t>(throttle_ms * 1000000.0)); // Sleep expects microseconds
            }
        }
    };
}