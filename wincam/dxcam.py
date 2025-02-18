import ctypes as ct
import os
from typing import Tuple
from enum import Enum
import cv2
import numpy as np

from wincam.camera import Camera
from wincam.throttle import FpsThrottle

script_dir = os.path.dirname(os.path.realpath(__file__))


class Rect(ct.Structure):
    _fields_ = [("x", ct.c_int), ("y", ct.c_int), ("width", ct.c_int), ("height", ct.c_int)]


class _EncoderPropertiesStruct(ct.Structure):
    _fields_ = [("bit_rate", ct.c_uint32), ("frame_rate", ct.c_uint32), ("quality", ct.c_uint32), ("seconds", ct.c_uint32), ("memory_cache", ct.c_uint32)]


class EncodingErrorReason(Enum):
    Unknown = 1
    InvalidProfile = 2
    CodecNotFound = 3


class VideoEncodingQuality(Enum):
    Auto = 0
    HD1080p = 1
    HD720p = 2
    Wvga = 3
    Ntsc = 4
    Pal = 5
    Vga = 6
    Qvga = 7
    Uhd2160p = 8
    Uhd4320p = 9


class EncodingProperties:
    def __init__(self, bit_rate: int = 9000000, frame_rate: int = 60, quality: VideoEncodingQuality = VideoEncodingQuality.Auto, seconds: int = 0, memory_cache: bool = False):
        self.bit_rate = bit_rate
        self.frame_rate = frame_rate
        self.quality = quality
        self.seconds = seconds
        self.memory_cache = memory_cache


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
        full_path = os.path.realpath(os.path.join(script_dir, "native", "runtimes", "x64", "ScreenCapture.dll"))
        if not os.path.exists(full_path):
            raise Exception(f"ScreenCapture.dll not found at: {full_path}")
        self.lib = ct.cdll.LoadLibrary(full_path)
        self.lib.GetCaptureBounds.restype = Rect
        self.lib.EncodeVideo.argtypes = [ct.c_uint32, ct.c_wchar_p, _EncoderPropertiesStruct]
        self.lib.EncodeVideo.restype = ct.c_uint32
        self.lib.GetTicks.argtypes = [ct.POINTER(ct.c_double), ct.c_int]
        self.lib.GetTicks.restype = ct.c_uint32
        self.lib.GetArrivalTimes.argtypes = [ct.c_uint32, ct.POINTER(ct.c_double), ct.c_int]
        self.lib.GetArrivalTimes.restype = ct.c_uint32
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
            self._handle = self.lib.StartCapture(self._left, self._top, self._width, self._height, self._capture_cursor)
            if not self.lib.WaitForNextFrame(self._handle, 10000):
                raise Exception("Frames are not being captured")

            self._capture_bounds = self.lib.GetCaptureBounds(self._handle)
            self._size = self._capture_bounds.width * self._capture_bounds.height * 4
            self._buffer = ct.create_string_buffer(self._size)  # type: ignore
            self._started = True
            self._throttle.reset()

        timestamp = self.lib.ReadNextFrame(self._handle, self._buffer, len(self._buffer))
        image = np.resize(
            np.frombuffer(self._buffer, dtype=np.uint8), (self._capture_bounds.height, self._capture_bounds.width, 4)
        )
        # strip out the alpha channel, and any 64-byte aligned extra width
        image = image[:, : self._width, :3]

        self._throttle.step()
        return image, timestamp / 1000000.0

    def get_rgb_frame(self) -> Tuple[np.ndarray, float]:
        frame, timestamp = self.get_bgr_frame()
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        return frame, timestamp

    def encode_video(self, file_name: str, properties: EncodingProperties):
        self.get_bgr_frame()  # make sure we're getting frames.
        full_path = os.path.realpath(file_name)
        if os.path.isfile(full_path):
            os.remove(full_path)

        if properties.memory_cache and properties.seconds == 0:
            raise Exception("You must specify a max seconds that will fit in memory if you enable the memory_cache.")

        props = _EncoderPropertiesStruct()
        props.bit_rate = properties.bit_rate
        props.frame_rate = properties.frame_rate
        props.quality = properties.quality.value
        props.seconds = properties.seconds
        props.memory_cache = 1 if properties.memory_cache else 0

        self.lib.EncodeVideo(self._handle, full_path, props)

    def stop_encoding(self):
        self.lib.StopEncoding()

    def get_video_ticks(self):
        len = self.lib.GetTicks(None, 0)
        if len > 0:
            array = (ct.c_double * len)()
            self.lib.GetTicks(array, len)
            return list(array)
        return []

    def get_frame_times(self):
        len = self.lib.GetArrivalTimes(self._handle, None, 0)
        if len > 0:
            array = (ct.c_double * len)()
            self.lib.GetArrivalTimes(self._handle, array, len)
            return list(array)
        return []

    def stop(self):
        self._started = False
        self.lib.StopEncoding()
        self.lib.StopCapture(self._handle)
        self._buffer = None
        self._handle = -1
