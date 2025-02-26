#include "pch.h"

#include "Timer.h"
#include <Windows.h>

using namespace util;

unsigned long SetHighestTimerResolution(unsigned long timer_res_us)
{
    unsigned long timer_current_res = ULONG_MAX;
    const HINSTANCE ntdll = LoadLibrary(L"NTDLL.dll");
    if (ntdll != NULL)
    {
        typedef long(NTAPI* pNtSetTimerResolution)(unsigned long RequestedResolution, BOOLEAN Set, unsigned long* ActualResolution);

        pNtSetTimerResolution NtSetTimerResolution = (pNtSetTimerResolution)GetProcAddress(ntdll, "NtSetTimerResolution");
        if (NtSetTimerResolution != NULL)
        {
            // bounds are validated and set to the highest allowed resolution
            NtSetTimerResolution(timer_res_us, TRUE, &timer_current_res);
            NtSetTimerResolution(timer_res_us, TRUE, &timer_current_res);
        }
        // we can decrement the internal reference count by one
        // and NTDLL.DLL still remains loaded in the process
        FreeLibrary(ntdll);
    }

    return timer_current_res;
}

Timer::Timer() {
    _currentResolution = SetHighestTimerResolution(1);
    _timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (_timer == nullptr) {
        throw std::exception("CreateWaitableTimer return null");
    }
}

Timer::~Timer() {
    if (_timer != nullptr) {
        CloseHandle(_timer);
    }
}

void Timer::Sleep(int64_t usec)
{
    auto start = Microseconds();
    if (usec > 1000) {
        // this can do millisecond accurate sleeps
        LARGE_INTEGER period;
        // negative values are for relative time
        period.QuadPart = -(10 * (usec - 200));
        SetWaitableTimer(_timer, &period, 0, NULL, NULL, 0);
        WaitForSingleObject(_timer, INFINITE);
    }

    // for less than millisecond we need to spin wait, this will also correct any remainder
    // left on the table by the SetWaitableTimer.
	while (Microseconds() - start < usec) {
        // tradeoff: burn up the core in this tight loop in order to get precise timing...
    }
}