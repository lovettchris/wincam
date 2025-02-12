using System;
using System.Collections.Generic;
using System.Diagnostics;
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

        public Capture()
        {
        }

        public void StartCapture(int x, int y)
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
            uint expected = (uint)(w * h * 4);
            int len = StartCapture(x, y, w, h, true);
            if (expected != len)
            {
                Debug.WriteLine($"StartCapture {len} instead of {expected}");
            }
            this.buffer = new byte[len]; // B8G8R8A8UIntNormalized
        }

        public ImageSource CaptureImage()
        {
            var len = ReadNextFrame(this.buffer, (uint)this.buffer.Length);
            if (len > 0)
            {
                int stride = this.width * 4; // 4 bytes per pixel (B8G8R8A8)
                return BitmapSource.Create(
                    this.width,
                    this.height,
                    96, // dpiX
                    96, // dpiY
                    PixelFormats.Bgra32,
                    null,
                    buffer,
                    stride
                );
            }
            return null;
        }

        public void Dispose()
        {
            StopCapture();
        }

        [DllImport("ScreenCapture.dll")]
        private static extern int StartCapture(int x, int y, int width, int height, bool captureCursor);

        [DllImport("ScreenCapture.dll")]
        private static extern void StopCapture();

        [DllImport("ScreenCapture.dll")]
        private static extern ulong ReadNextFrame(byte[] buffer, uint size);

    }
}
