using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using WpfTestApp.Native;

namespace WpfTestApp
{
    internal class Capture : ICapture
    {
        byte[] buffer;
        int width;
        int height;
        int stride;
        uint captureHandle;
        bool started;
        const int channels = 4; // B8G8R8A8UIntNormalized;
        bool encoding;

        public event EventHandler<EncodingStats> EncodingCompleted;

        public Capture()
        {
        }

        public async Task StartCapture(int x, int y, int timeout = 100000)
        {
            POINT pos = new POINT()
            {
                X = x,
                Y = y
            };
            nint innerHwnd = User32.WindowFromPoint(pos);
            var bounds = User32.GetClientScreenRect(innerHwnd);

            x = (int)bounds.Left;
            y = (int)bounds.Top;
            int w = (int)bounds.Width;
            int h = (int)bounds.Height;
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
            if (encoding)
            {
                // just to see what the perf impact is...
                return null;
            }
            CheckStarted();
            var len = CaptureNative.ReadNextFrame(this.captureHandle, this.buffer, (uint)this.buffer.Length);
            if (len > 0)
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
            return null;
        }

        public void StopEncoding()
        {
            encoding = false;
            CaptureNative.StopEncoding();
        }

        public void EncodeVideo(string file)
        {
            if (encoding)
            {
                StopEncoding();
            }

            encoding = true;
            uint bitrateInBps = 9000000;
            uint frameRate = 60;
            encoding = true;
            Task.Run(() =>
            {
                try
                {
                    CaptureNative.EncodeVideo(this.captureHandle, file, bitrateInBps, frameRate);
                }
                catch (Exception e)
                {
                    MessageBox.Show(e.Message, "Encoding Failed", MessageBoxButton.OK, MessageBoxImage.Error);
                }
                encoding = false;
                var size = CaptureNative.GetTicks(null, 0);
                double[] buffer = new double[size];
                CaptureNative.GetTicks(buffer, size);
                if (EncodingCompleted != null)
                {
                    EncodingCompleted(this, new EncodingStats()
                    {
                        FileName = file,
                        FrameTicks = buffer
                    });
                }
            });
        }

        public void Dispose()
        {
            if (this.started)
            {
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
        internal static extern int EncodeVideo(uint captureHandle, string filename, uint bitrateInBps, uint frameRate);

        [DllImport("ScreenCapture.dll")]
        internal static extern int StopEncoding();

        [DllImport("ScreenCapture.dll")]
        internal static extern uint GetTicks(double[] buffer, uint size);

    }
}
