import os
from typing import List, Tuple

import cv2
import numpy as np

from wincam.camera import Camera
from wincam.native import EncodingProperties, NativeScreenRecorder, Rect
from wincam.throttle import FpsThrottle

script_dir = os.path.dirname(os.path.realpath(__file__))


class DXCamera(Camera):
    _instance = None

    """ Camera that captures frames from the screen using the ScreenCapture.dll native library
    which is based on Direct3D11CaptureFramePool.
    See https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool"""

    def __init__(self, left: int, top: int, width: int, height: int, fps: int = 30, capture_cursor: bool = True):
        super().__init__()
        if os.name != "nt":
            raise Exception("This class only works on Windows")

        self._width = width
        self._height = height
        self._left = left
        self._top = top
        self._capture_cursor = capture_cursor
        self._throttle = FpsThrottle(fps)
        self._native = NativeScreenRecorder()
        self._started = False
        self._buffer = None
        self._size = 0
        self._capture_bounds = Rect()
        self._handle = -1

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
            self._handle = self._native.start_capture(
                self._left, self._top, self._width, self._height, self._capture_cursor
            )
            if not self._native.wait_for_next_frame(self._handle, 10000):
                raise Exception("Frames are not being captured")

            self._capture_bounds = self._native.get_capture_bounds(self._handle)
            self._size = self._capture_bounds.width * self._capture_bounds.height * 4
            self._buffer = self._native.create_buffer(self._size)
            self._started = True
            self._throttle.reset()

        timestamp = self._native.read_next_frame(self._handle, self._buffer, len(self._buffer))
        image = np.reshape(
            np.frombuffer(self._buffer, dtype=np.uint8), (self._capture_bounds.height, self._capture_bounds.width, 4)
        )
        # strip out the alpha channel, and any 64-byte aligned extra width
        image = image[:, : self._width, :3]

        self._throttle.step()
        return image, timestamp

    def get_rgb_frame(self) -> Tuple[np.ndarray, float]:
        frame, timestamp = self.get_bgr_frame()
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        return frame, timestamp

    def encode_video(self, file_name: str, properties: EncodingProperties):
        self.get_bgr_frame()  # make sure we're getting frames.
        full_path = os.path.realpath(file_name)
        if os.path.isfile(full_path):
            os.remove(full_path)

        self._native.encode_video(self._handle, full_path, properties)

    def stop_encoding(self):
        self._native.stop_encoding()

    def stop_capture(self):
        self._native.stop_capture(self._handle)
        self._handle = -1

    def get_video_ticks(self) -> List[float]:
        return self._native.get_sample_times()

    def get_frame_times(self) -> List[float]:
        return self._native.get_capture_times(self._handle)

    def stop(self):
        self._started = False
        self.stop_encoding()
        self.stop_capture()
        self._buffer = None
