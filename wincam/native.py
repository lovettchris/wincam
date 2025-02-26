import ctypes as ct
from enum import Enum
import os
from typing import Any, List

script_dir = os.path.dirname(os.path.realpath(__file__))


class Rect(ct.Structure):
    _fields_ = [("x", ct.c_int), ("y", ct.c_int), ("width", ct.c_int), ("height", ct.c_int)]


class _EncoderPropertiesStruct(ct.Structure):
    _fields_ = [
        ("bit_rate", ct.c_uint32),
        ("frame_rate", ct.c_uint32),
        ("quality", ct.c_uint32),
        ("seconds", ct.c_uint32),
        ("memory_cache", ct.c_uint32),
    ]


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
    def __init__(
        self,
        frame_rate: int = 60,
        quality: VideoEncodingQuality = VideoEncodingQuality.Auto,
        bit_rate: int = 0,
        seconds: int = 0,
        memory_cache: bool = False,
    ):
        # if you send bit_rate 0 it will compute the best bitrate from your frame rate and quality and set this field
        # for you which you can then query after calling start_encoding.
        self.bit_rate = bit_rate
        self.frame_rate = frame_rate
        self.quality = quality
        self.seconds = seconds
        self.memory_cache = memory_cache


class NativeScreenRecorder:
    def __init__(self):
        full_path = os.path.realpath(os.path.join(script_dir, "native", "runtimes", "x64", "ScreenCapture.dll"))
        if not os.path.exists(full_path):
            raise Exception(f"ScreenCapture.dll not found at: {full_path}")
        self.lib = ct.cdll.LoadLibrary(full_path)
        self.lib.GetCaptureBounds.restype = Rect
        self.lib.EncodeVideo.argtypes = [ct.c_uint32, ct.c_wchar_p, ct.POINTER(_EncoderPropertiesStruct)]
        self.lib.EncodeVideo.restype = ct.c_uint32
        self.lib.GetSampleTimes.argtypes = [ct.POINTER(ct.c_double), ct.c_int]
        self.lib.GetSampleTimes.restype = ct.c_uint32
        self.lib.GetCaptureTimes.argtypes = [ct.c_uint32, ct.POINTER(ct.c_double), ct.c_int]
        self.lib.GetCaptureTimes.restype = ct.c_uint32
        self.lib.SleepMicroseconds.argtypes = [ct.c_uint64]

    def start_capture(self, left: int, top: int, width: int, height: int, capture_cursor: bool) -> int:
        return self.lib.StartCapture(left, top, width, height, capture_cursor)

    def stop_capture(self, handle: int) -> None:
        self.lib.StopCapture(handle)

    def get_capture_bounds(self, handle: int) -> Rect:
        return self.lib.GetCaptureBounds(handle)

    def wait_for_next_frame(self, handle: int, timeout: int) -> bool:
        return self.lib.WaitForNextFrame(handle, timeout)

    def create_buffer(self, size: int) -> Any:
        return ct.create_string_buffer(size)  # type: ignore

    def read_next_frame(self, handle: int, buffer: Any, size: int) -> float:
        return self.lib.ReadNextFrame(handle, buffer, size)

    def encode_video(self, handle: int, file_name: str, properties: EncodingProperties) -> int:
        props = _EncoderPropertiesStruct()
        props.bit_rate = properties.bit_rate
        props.frame_rate = properties.frame_rate
        props.quality = properties.quality.value
        props.seconds = properties.seconds
        props.memory_cache = 1 if properties.memory_cache else 0

        result = self.lib.EncodeVideo(handle, file_name, ct.byref(props))
        properties.bit_rate = props.bit_rate
        return result

    def stop_encoding(self) -> None:
        self.lib.StopEncoding()

    def get_sample_times(self) -> List[float]:
        len = self.lib.GetSampleTimes(None, 0)
        if len > 0:
            array = (ct.c_double * len)()
            self.lib.GetSampleTimes(array, len)
            return list(array)
        return []

    def get_capture_times(self, handle: int) -> List[float]:
        len = self.lib.GetCaptureTimes(handle, None, 0)
        if len > 0:
            array = (ct.c_double * len)()
            self.lib.GetCaptureTimes(handle, array, len)
            return list(array)
        return []

    def sleep_microseconds(self, microseconds: int):
        if microseconds < 0:
            raise ValueError("sleep microseconds must be >= 0")
        self.lib.SleepMicroseconds(microseconds)
