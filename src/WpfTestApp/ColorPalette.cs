using System.Diagnostics;
using System.Windows.Media;

namespace WpfTestApp
{
    internal class ColorPalette
    {
        Color[] palette = new Color[]
        {
            Colors.Maroon,
            Colors.Brown,
            Colors.Olive,
            Colors.Teal,
            Colors.Navy,
            Colors.Black,
            Colors.Red,
            Colors.Orange,
            Colors.Yellow,
            Colors.Lime,
            Colors.Green,
            Colors.Cyan,
            Colors.Blue,
            Colors.Purple,
            Colors.Magenta,
            Colors.Gray,
            Colors.Pink,
            Color.FromRgb(255, 216, 177),
            Colors.Beige,
            Color.FromRgb(170, 255, 195),
            Colors.Lavender,
            Colors.DarkGray
        };

        public ColorPalette()
        {
            CheckPalette();
        }

        int nextColor;

        public Color GetNextColor()
        {
            var result = palette[nextColor];
            nextColor = (nextColor + 1) % palette.Length;
            return result;
        }

        private void CheckPalette()
        {
            for (int i = 0; i < palette.Length; i++)
            {
                Color a = palette[i];
                for (int j = 0; j < palette.Length; j++)
                {
                    var b = palette[j];
                    if (i != j && IsColorClose(a, b))
                    {
                        Debug.WriteLine($"Color {i} {a} is too close to color {j} {b}");
                    }
                }
            }
        }

        public static bool IsColorClose(Color a, Color b)
        {
            const int delta = 50;
            var diff = Math.Abs(a.R - b.R) + Math.Abs(a.G - b.G) + Math.Abs(a.B - b.B);
            return diff < delta;
        }

        public static Color GetContrastingLabelColor(Color backgroundColor)
        {
            if (IsContrastAccessible(backgroundColor, Colors.White))
            {
                return Colors.White;
            }
            return Colors.Black;
        }

        public static bool IsContrastAccessible(Color background, Color text)
        {
            // WCAG AA standard for normal text is 4.5:1
            return CalculateContrastRatio(background, text) >= 4.5;
        }

        public static double CalculateContrastRatio(Color background, Color text)
        {
            // Relative luminance of white text (1.0)
            double luminanceWhite = CalculateRelativeLuminance(text);

            // Calculate relative luminance of the background color
            double luminanceBackground = CalculateRelativeLuminance(background);

            // Calculate contrast ratio
            double contrastRatio = (luminanceWhite + 0.05) / (luminanceBackground + 0.05);
            return contrastRatio;
        }

        public static double CalculateRelativeLuminance(Color c)
        {
            int r = c.R;
            int g = c.G;
            int b = c.B;
            // Convert RGB to sRGB
            double sr = r / 255.0;
            double sg = g / 255.0;
            double sb = b / 255.0;

            // Apply gamma correction
            sr = (sr <= 0.03928) ? sr / 12.92 : Math.Pow((sr + 0.055) / 1.055, 2.4);
            sg = (sg <= 0.03928) ? sg / 12.92 : Math.Pow((sg + 0.055) / 1.055, 2.4);
            sb = (sb <= 0.03928) ? sb / 12.92 : Math.Pow((sb + 0.055) / 1.055, 2.4);

            // Calculate relative luminance
            return 0.2126 * sr + 0.7152 * sg + 0.0722 * sb;
        }
    }
}