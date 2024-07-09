# wincam

A fast screen capture library for python on Windows 10 and above (x64 platform only).

```python
from wincam import DXCamera

with DXCamera(x, y, w, h, fps=30) as camera:
    while True:
        frame, timestamp = camera.get_bgr_frame()
```

## Introduction

When you need to capture video frames fast to get a nice smooth 30 or 60 fps video
this library will do it, so long as you are on Windows 10.0.19041.0 or newer.

This is using a new Windows 10 API called [Direct3D11CaptureFramePool](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool?view=winrt-26100) which requires DirectX 11 and a GPU.

To get the fastest time possible, this library is implemented in C++ and the
C++ library copies each frame directly into a buffer provided by the python code.
This C++ library is loaded into your python process.
Only one instance of DXCamera can be used per python process.

## Installation

```
pip install wincam
```

Note: OpenCV is required for color space conversion.

## Multiple Monitors

This supports multiple monitors.  Windows can define negative X, and Y locations when a monitor is to the left or above
the primary monitor.  The `DXCamera` will find and capture the appropriate monitor from the `x, y` locations you provide
and it will crop the image to the bounds you provide.

The `DXCamera` does not support regions that span more than one monitor and it will report an error if you try.

## Examples

The following example scripts are provided in this repo.

- [examples/mirror.py](examples/mirror.py) - shows the captured frames in real time so you can see how it is performing
on your machine.  Have some fun with infinite frames with frames!  Press ESCAPE to close the window.

- [examples/video.py](examples/video.py) - records an .mp4 video to disk.

In each example can specify what to record using:
- `--x`, `--y`, `--width`, `--height`, in screen coordinates
- `--hwnd`, a native win32 window handle (which you can find using the inspect tool that comes with the windows SDK)
- `--process`, a win32 process id
- `--point`, an `x,y` screen location from which to find the window that you want to record.


## Performance

Each call to `camera.get_bgr_frame()` can be as fast as 1 millisecond because the C++ code is asynchronously writing to
the buffer provided.  This way your python code is not blocking waiting for that frame. For this reason it is crucial
that you use `DXCamera` in a `with` block as shown above since this ensures the life time of the python buffer used by
the C++ code is managed correctly.  If you cannot use a `with` block for some reason then you must call the `stop()`
method.

In order to hit a smooth target frame rate while recording video the `DXCamera` takes a target fps as input, which
defaults to 30 frames per second. The calls to `camera.get_bgr_frame()` will self regulate with an accurate sleep
to hit that target as closely as possible so that the frames you collect form a nice smooth video as shown in the
[video.py example](examples/video.py).

Note that this sleep is more accurate that python `time.sleep()` which on Windows is very inaccurate with a
tolerance of +/- 15 milliseconds!




