using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ScreenRecorder.Native;
using ScreenRecorder.Utilities;

namespace ScreenRecorder;

public class ProgressEventArgs : EventArgs
{
    public string Messgage;
    public int Value;
    public int Minimum;
    public int Maximum;
}

public class Capture : ICapture
{
    byte[] buffer;
    int width;
    int height;
    int stride;
    uint captureHandle;
    bool started;
    const int channels = 4; // B8G8R8A8UIntNormalized;
    bool encoding;
    bool disposed;

    public event EventHandler<EncodingStats> EncodingCompleted;
    public event EventHandler<Exception> EncodingError;
    public event EventHandler<ProgressEventArgs> ProgressUpdate;

    public Capture()
    {
    }

    void NotifyProgress(string status, int value, int minimum, int maximum)
    {
        if (this.ProgressUpdate != null)
        {
            this.ProgressUpdate(this, new ProgressEventArgs()
            {
                Messgage = status,
                Minimum = minimum,
                Maximum = maximum,
                Value = value
            });
        }
    }

    public async Task StartCapture(int x, int y, int w, int h, int timeout = 100000)
    {
        this.width = w;
        this.height = h;
        this.stride = w * 4;
        uint bufferSize = (uint)(w * h * channels);
        Exception error = null;
        this.captureHandle = CaptureNative.StartCapture(x, y, w, h, true);
        this.started = true;

        await Task.Run(() =>
        {
            if (!CaptureNative.WaitForNextFrame(this.captureHandle, timeout))
            {
                error = new TimeoutException("Frames are not arriving");
            }
            else
            {
                RECT capture = CaptureNative.GetCaptureBounds(this.captureHandle);
                Debug.WriteLine($"Capturing bounds {capture}");
                w = capture.Right - capture.Left;
                h = capture.Bottom - capture.Top;
                this.stride = w * 4;
                bufferSize = (uint)(w * h * channels);
            }
        });
        if (error != null)
        {
            throw new Exception($"StartCapture returned error: {error.Message}");
        }
        this.buffer = new byte[bufferSize];
    }

    private void CheckStarted()
    {
        if (!this.started)
        {
            throw new Exception("Please call StartCapture first");
        }
    }

    public ImageSource CaptureImage()
    {
        CheckStarted();
        var timestamp = CaptureNative.ReadNextFrame(this.captureHandle, this.buffer, (uint)this.buffer.Length);
        return CreateBitmapImage(buffer);
    }

    public byte[] RawCaptureImageBuffer()
    {
        CheckStarted();
        CaptureNative.ReadNextFrame(this.captureHandle, this.buffer, (uint)this.buffer.Length);
        var result = new byte[this.buffer.Length];
        Array.Copy(this.buffer, result, this.buffer.Length);
        return result;
    }

    public BitmapSource CreateBitmapImage(byte[] buffer)
    {
        return BitmapSource.Create(
                this.width,
                this.height,
                96, // dpiX
                96, // dpiY
                PixelFormats.Bgra32, // must match what the C++ code is doing (B8G8R8A8UIntNormalized)
                null,
                buffer,
                this.stride
            );
    }

    public void StopEncoding()
    {
        encoding = false;
        CaptureNative.StopEncoding();
    }

    public void EncodeVideoNative(string file, VideoEncoderProperties properties)
    {
        file = System.IO.Path.GetFullPath(file);
        if (encoding)
        {
            StopEncoding();
        }
        if (System.IO.File.Exists(file))
        {
            System.IO.File.Delete(file);
        }
        var dir = System.IO.Path.GetDirectoryName(file);
        System.IO.Directory.CreateDirectory(dir);

        encoding = true;
        Task.Run(() =>
        {
            try
            {
                // Call the native ScreenCapture library.
                CaptureNative.EncodeVideo(this.captureHandle, file, ref properties);
            }
            catch (Exception ex)
            {
                var errorHandler = this.EncodingError;
                if (errorHandler != null)
                {
                    errorHandler(this, ex);
                }
            }

            encoding = false;
            var size = CaptureNative.GetSampleTimes(null, 0);
            double[] buffer = new double[size];
            CaptureNative.GetSampleTimes(buffer, size);

            size = CaptureNative.GetCaptureTimes(this.captureHandle, null, 0);
            double[] samples = new double[size];
            CaptureNative.GetCaptureTimes(this.captureHandle, samples, size);

            var handler = this.EncodingCompleted;
            if (handler != null && !disposed)
            {
                handler(this, new EncodingStats()
                {
                    FileName = file,
                    FrameTicks = buffer,
                    SampleTicks = samples,
                });
            }
        });
    }

    public void Dispose()
    {
        this.disposed = true;
        if (this.started)
        {
            CaptureNative.StopEncoding();
            CaptureNative.StopCapture(this.captureHandle);
            this.started = false;
        }
    }

    private void CleanupOutputFiles(string temp)
    {
        if (Directory.Exists(temp))
        {
            try
            {
                Directory.Delete(temp, true);
            }
            catch { }
        }
    }

    public async Task EncodeVideoFrames(string file, VideoEncoderProperties properties, string outputFiles)
    {
        if (!Ffmpeg.FindFFMPeg())
        {
            return;
        }
        if (encoding)
        {
            StopEncoding();
        }
        this.encoding = true;

        int frameRate = (int)properties.frameRate;
        var throttle = new Throttle(frameRate);
        var start = Environment.TickCount;
        var timer = new PerfTimer();
        timer.Start();
        List<byte[]> images = new List<byte[]>();
        List<double> frameTicks = new List<double>();

        while (this.encoding)
        {
            var img = this.RawCaptureImageBuffer();
            images.Add(img);
            frameTicks.Add(timer.GetSeconds());
            throttle.Step();
        }

        double duration = timer.GetSeconds();
        var fps = images.Count / duration;
        Debug.WriteLine($"Captured {images.Count} frames in {duration} seconds which is {fps} fps");

        NotifyProgress("Saving frames...", 0, 0, images.Count);

        // ok, now save the frames and encode them into the mp4 video...
        CleanupOutputFiles(outputFiles);
        Directory.CreateDirectory(outputFiles);
        Debug.WriteLine($"Saving frames to {outputFiles}");

        for (int i = 0; i < images.Count; i++)
        {
            var img = this.CreateBitmapImage(images[i]);
            var fileName = $"frame_{i:D4}.png";
            var filePath = Path.Combine(outputFiles, fileName);
            SavePng(img, filePath);
            this.NotifyProgress("", i, 0, images.Count);
        }

        this.NotifyProgress("Encoding video...", 0, 0, 0);
        var rc = await Ffmpeg.EncodeVideo(file, outputFiles, frameRate);
        if (rc != 0)
        {
            NotifyProgress($"ffmpeg returned {rc}", 0, 0, 0);
        }
        else
        {
            if (this.EncodingCompleted != null)
            {
                this.EncodingCompleted(this, new EncodingStats()
                {
                    FileName = file,
                    FrameTicks = frameTicks.ToArray(),
                    SampleTicks = null
                });
            }
        }
    }

    private void SavePng(BitmapSource img, string fileName)
    {
        BitmapEncoder encoder = new PngBitmapEncoder();
        encoder.Frames.Add(BitmapFrame.Create(img));
        using (var fileStream = new FileStream(fileName, System.IO.FileMode.Create))
        {
            encoder.Save(fileStream);
        }
    }
}


class CaptureNative
{

    [DllImport("ScreenCapture.dll")]
    internal static extern uint StartCapture(int x, int y, int width, int height, bool captureCursor);

    [DllImport("ScreenCapture.dll")]
    internal static extern bool WaitForNextFrame(uint handle, int timeout);

    [DllImport("ScreenCapture.dll")]
    internal static extern RECT GetCaptureBounds(uint handle);

    [DllImport("ScreenCapture.dll")]
    internal static extern void StopCapture(uint handle);

    [DllImport("ScreenCapture.dll")]
    internal static extern ulong ReadNextFrame(uint handle, byte[] buffer, uint size);

    [DllImport("ScreenCapture.dll", CharSet = CharSet.Unicode)]
    internal static extern int EncodeVideo(uint captureHandle, string filename, ref VideoEncoderProperties properties);

    [DllImport("ScreenCapture.dll")]
    internal static extern int StopEncoding();

    [DllImport("ScreenCapture.dll")]
    internal static extern uint GetSampleTimes(double[] buffer, uint size);

    [DllImport("ScreenCapture.dll")]
    internal static extern uint GetCaptureTimes(uint captureHandle, double[] buffer, uint size);

}
