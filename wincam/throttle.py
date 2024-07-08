import numpy as np

from .timer import Timer


class FpsThrottle:
    """Helper class that throttles the frame rate to a given fps. Simply set the frame rate in the constructor then call
    step and step will sleep the right amount to make the time between step calls equal the given fps.  In order to
    avoid large initial times it smooths this time only over the window_size (in seconds) so it settles into a nice
    steady state more quickly than it would if it was taking the overall average."""

    def __init__(self, fps: int, window_size=10):
        self.fps = fps
        self.window_size = window_size
        self.reset()
        self.target_ms_per_frame = 1000.0 / self.fps
        self.window: np.ndarray | None = None
        self.pos = 0

    def reset(self):
        self.timer = Timer()
        self.started = False
        self.window = None
        self.pos = 0

    def step(self):
        if not self.started:
            self.started = True
            throttle_ms = 0.0
        else:
            # get time for full step loop since we were last called
            average_ms_per_frame = self.timer.ticks() * 1000.0
            if self.window is None:
                self.window = np.ones((self.window_size,)) * average_ms_per_frame
            else:
                self.window[self.pos] = average_ms_per_frame
                self.pos = (self.pos + 1) % self.window_size
            smoothed_average = np.mean(self.window)
            throttle_ms = self.target_ms_per_frame - smoothed_average

        # sync to match the given frame rate.
        if throttle_ms > 0:
            self.timer.sleep(int(throttle_ms))
        self.timer.start()
        return throttle_ms
