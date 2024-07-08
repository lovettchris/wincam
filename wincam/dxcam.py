import ctypes as ct
import os
from typing import Tuple

import cv2
import numpy as np

from .camera import Camera
from .throttle import FpsThrottle

script_dir = os.path.dirname(os.path.realpath(__file__))


class DXCamera(Camera):
    _instance = None

    """ Camera that captures frames from the screen using the ScreenCapture.dll native library
    which is based on Direct3D11CaptureFramePool.  Only one DXCamera can be activate at a time.
    See https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool"""

    def __init__(self, left: int, top: int, width: int, height: int, target_fps: int = 30):
        super().__init__()
        if os.name != "nt":
            raise Exception("This class only works on Windows")

        self._width = width
        self._height = height
        self._left = left
        self._top = top
        self._throttle = FpsThrottle(target_fps)
        self.target_fps = target_fps
        full_path = os.path.realpath(os.path.join(script_dir, "native", "runtimes", "x64", "ScreenCapture.dll"))
        if not os.path.exists(full_path):
            raise Exception(f"ScreenCapture.dll not found at: {full_path}")
        self.lib = ct.cdll.LoadLibrary(full_path)
        self._started = False
        self._buffer = None
        self._size = 0

    def __enter__(self):
        if self._instance is None:
            DXCamera._instance = self
        else:
            raise Exception("You can only use 1 instance of DXCamera at a time.")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        DXCamera._instance = None
        self.stop()

    def reset_throttle(self):
        self._throttle.reset()

    def get_bgr_frame(self) -> Tuple[np.ndarray, float]:
        if not self._started:
            hr = self.lib.StartCapture(self._left, self._top, self._width, self._height)
            if hr <= 0:
                raise Exception(f"Failed to start capture, error code {f'{-hr:02x}'}")
            self._size = hr
            self._buffer = ct.create_string_buffer(self._size)  # type: ignore
            self._started = True
            self._throttle.reset()

        timestamp = self.lib.ReadNextFrame(self._buffer, len(self._buffer))
        image = np.resize(np.frombuffer(self._buffer, dtype=np.uint8), (self._height, self._width, 4))
        # strip out the alpha channel
        image = image[:, :, :3]

        self._throttle.step()
        return image, timestamp / 1000000.0

    def get_rgb_frame(self) -> Tuple[np.ndarray, float]:
        frame, timestamp = self.get_bgr_frame()
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        return frame, timestamp

    def stop(self):
        self._started = False
        self.lib.StopCapture()
        self._buffer = None
