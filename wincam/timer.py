import os
import time


class Timer:
    """This class provides accurate timer for recording when user inputs happen
    so that the playback of those events can synchronize the playback to run in the
    same time as the recording."""

    WINDOWS_SLEEP_ACCURACY_MS = 15

    def __init__(self):
        self._start_time = None

    def start(self):
        """Save the start time"""
        self._start_time = time.perf_counter()

    def ticks(self) -> float:
        """Returns 0 on the first call and the delta from this first call in seconds
        on subsequent calls using time.perf_counter() for higher precision."""
        if self._start_time is None:
            self.start()
            return 0
        return time.perf_counter() - self._start_time

    def sleep(self, milliseconds: int):
        """Sleep for the given number of milliseconds."""
        if os.name == "nt":
            self._windows_sleep(milliseconds)
        else:
            self._unix_sleep(milliseconds)

    def _windows_sleep(self, milliseconds: int):
        # On windows the Win32 Sleep function has an accuracy of about 15ms which is not good enough
        # so we sleep up to without that tolerance, then spin wait the rest to hit our target.
        now = self.ticks()
        if milliseconds > Timer.WINDOWS_SLEEP_ACCURACY_MS:
            time.sleep(float(milliseconds - Timer.WINDOWS_SLEEP_ACCURACY_MS) / 1000.0)

        # spin wait for the rest.
        while (self.ticks() - now) * 1000 < milliseconds:
            pass

    def _unix_sleep(self, milliseconds: int):
        """Unix sleep call."""
        time.sleep(float(milliseconds) / 1000.0)
