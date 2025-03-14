# README

This package contains a .NET 8.0 wrapper on the native ScreenCapture  library that uses Direct3D11CaptureFramePool to do
capture frames from your screen and optional efficient GPU only video encoding of these captured frames.

## Usage

Assume you have a .Net 8.0 WPF app containing an `<Image x:Name="CapturedImage">` then you can fill it with a screen capture
like this:

```csharp
using ScreenRecorder;
Capture c = new Capture();
await c.StartCapture(0, 0, 1024, 700);
CapturedImage.Source = c.CaptureImage();
```

You can also record a 20 second video of this region to a file using:

```csharp
VideoEncoderProperties props = new VideoEncoderProperties()
{
    frameRate = 60,
    quality = VideoEncodingQuality.HD1080p,
    seconds = 20
};
c.EncodingCompleted += OnEncodingCompleted;
c.EncodeVideo("d:\\temp\\test.mp4", props);
```

## FFmpeg

You can also optionally uses FFmpeg if you install the binaries and add them to your PATH,
you can then enable FFmpeg encoding using `props.ffmpeg = 1`.


See https://github.com/lovettchris/wincam/tree/main/src/ScreenRecorder.Net