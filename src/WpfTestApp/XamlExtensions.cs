using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace WpfTestApp
{
    internal static class XamlExtensions
    {

        public static byte[] GetRgb24Pixels(this BitmapSource bitmap)
        {
            if (bitmap.Format != PixelFormats.Rgb24)
            {
                bitmap = new FormatConvertedBitmap(bitmap, PixelFormats.Rgb24, null, 0);
            }
            var bitsPerPixel = bitmap.Format.BitsPerPixel;
            int stride = ((bitmap.PixelWidth * bitsPerPixel) + 7) / 8;
            int bytesPerPixel = bitmap.Format.BitsPerPixel / 8;
            var pixels = new byte[bitmap.PixelHeight * stride];
            bitmap.CopyPixels(new Int32Rect(0, 0, bitmap.PixelWidth, bitmap.PixelHeight), pixels, stride, 0);
            return pixels;
        }

    }
}
