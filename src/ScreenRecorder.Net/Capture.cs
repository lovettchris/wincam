using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ScreenRecorder.Native;

namespace ScreenRecorder
{
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

        public Capture()
        {
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

        public void EncodeVideo(string file, VideoEncoderProperties properties)
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
                var size = CaptureNative.GetTicks(null, 0);
                double[] buffer = new double[size];
                CaptureNative.GetTicks(buffer, size);

                size = CaptureNative.GetArrivalTimes(this.captureHandle, null, 0);
                double[] samples = new double[size];
                CaptureNative.GetArrivalTimes(this.captureHandle, samples, size);

                var handler = this.EncodingCompleted;
                if (handler != null && !disposed)
                {
                    handler(this, new EncodingStats()
                    {
                        FileName = file,
                        FrameTicks = buffer,
                        SampleTicks = samples,
                        StartDelay = CaptureNative.GetStartDelay()
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
        internal static extern uint GetTicks(double[] buffer, uint size);

        [DllImport("ScreenCapture.dll")]
        internal static extern uint GetArrivalTimes(uint captureHandle, double[] buffer, uint size);

        [DllImport("ScreenCapture.dll")]
        internal static extern double GetStartDelay();

    }
}
