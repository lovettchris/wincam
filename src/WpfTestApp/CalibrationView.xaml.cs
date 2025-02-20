using ScreenRecorder.Native;
using ScreenRecorder.Utilities;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace WpfTestApp
{
    /// <summary>
    /// Interaction logic for CalibrationView.xaml.  This class records some mouse events on the screen
    /// then compares the result in the encoded videoto see how well the video lines up with the original.
    /// </summary>
    public partial class CalibrationView : UserControl
    {
        const int ShapeWidth = 100;
        const int ShapeHeight = 50;
        PerfTimer reference = new PerfTimer();
        string outputFiles;
        bool closed;
        bool calibrating;
        string videoFile;
        Size textLabelSize;
        List<string> frames;
        ColorPalette palette = new ColorPalette();

        class ShapeEvent
        {
            public Color color;
            public int milliseconds;
            public Rect bounds;
            public Point windowPos;
            public TextBlock frameTime;
        }
        List<ShapeEvent> shapeEvents = new List<ShapeEvent>();
        private TextBlock clockLabel;
        private DispatcherTimer timer;

        public event EventHandler<string> StatusEvent;

        public CalibrationView()
        {
            InitializeComponent();
            StartTimer();
        }

        private void StartTimer()
        {
            this.reference.Start();
        }

        public void OnClosed()
        {
            this.closed = true;
            CleanupOutputFiles();
        }

        private void OnMouseLeftButtonClick(object sender, MouseButtonEventArgs e)
        {
            if (!this.calibrating)
            {
                return;
            }
            e.Handled = true; 
            var now = (int)(1000 * this.reference.GetSeconds());
            var shapeColor = palette.GetNextColor();
            var r = new ShapeEvent() { color = shapeColor, milliseconds = now };
            this.shapeEvents.Add(r);

            var pos = e.GetPosition(DrawingCanvas);
            Rectangle rectangle = new Rectangle();
            rectangle.Fill = new SolidColorBrush(r.color);
            rectangle.Width = ShapeWidth;
            rectangle.Height = ShapeHeight;
            var x = pos.X - ShapeWidth / 2;
            var y = pos.Y - ShapeHeight / 2;
            Canvas.SetLeft(rectangle, x);
            Canvas.SetTop(rectangle, y);
            var owner = Application.Current.MainWindow;
            r.windowPos = e.GetPosition(owner);
            r.bounds = new Rect(x, y, ShapeWidth, ShapeHeight);
            DrawingCanvas.Children.Add(rectangle);

            var labelColor = ColorPalette.GetContrastingLabelColor(shapeColor);
            AddLabel(new Point(x, y), now.ToString(), 18, new SolidColorBrush(labelColor));
            var label = this.AddLabel(r.bounds.BottomLeft, "frame time?", 11);
            label.Foreground = Brushes.Blue;
            r.frameTime = label;
        }

        private TextBlock AddLabel(Point pos, string label, int fontSize = 18, Brush foreground = null)
        {
            if (foreground == null) foreground = Brushes.Black;
            TextBlock text = new TextBlock();
            text.Text = label;
            text.Padding = new Thickness(10);
            text.Foreground = foreground;
            text.FontSize = fontSize;
            text.FontWeight = FontWeights.Bold;
            Canvas.SetLeft(text, pos.X);
            Canvas.SetTop(text, pos.Y);
            text.Measure(new Size(this.ActualWidth, this.ActualHeight));
            textLabelSize.Width = Math.Max(textLabelSize.Width, text.DesiredSize.Width);
            textLabelSize.Height = Math.Max(textLabelSize.Height, text.DesiredSize.Height);
            DrawingCanvas.Children.Add(text);
            return text;
        }

        internal void OnRecordingStarted()
        {
            this.calibrating = true;
            this.Clear();
            StartTimer();
        }

        void Clear()
        {
            this.DrawingCanvas.Children.Clear();
            this.shapeEvents.Clear();
            // This animating clock ensures the frames keep changing (should be no duplicates).
            this.clockLabel = this.AddLabel(new Point(10, 10), "0:00:00.000", 48, Brushes.Black);
            this.timer = new DispatcherTimer(TimeSpan.FromMilliseconds(16), DispatcherPriority.Normal, OnTick, this.Dispatcher);
            this.timer.Start();
        }

        private void OnTick(object sender, EventArgs e)
        {
            var now = (int)(1000 * this.reference.GetSeconds());
            var text = TimeSpan.FromMilliseconds(now).ToString("g");
            // 0:00:00.000
            if (text.Length < 11)
            {
                text += new string('0', 11 - text.Length);
            }
            this.clockLabel.Text = text;
        }

        private void StopTimer()
        {
            if (this.timer != null)
            {
                this.timer.Stop();
            }
        }

        public event EventHandler<int> Progress;

        internal void NotifyProgress(int value)
        {
            if (Progress != null)
            {
                Progress(this, value);
            }
        }

        private string GetRgb(Color c)
        {
            return $"{c.R},{c.G},{c.B}";
        }

        internal void StopCalibrating()
        {
            StopTimer();
            this.calibrating = false;
        }

        internal async Task SyncVideoToShapes(string videoFile, int frameRate)
        {
            if (!Ffmpeg.FindFFMPeg())
            {
                return;
            }
            if (shapeEvents.Count == 0)
            {
                return;
            }
            int startDelayMilliseconds = 0;
            var owner = Application.Current.MainWindow;
            var hwnd = new WindowInteropHelper(owner).Handle;
            this.outputFiles = System.IO.Path.Combine(System.IO.Path.GetDirectoryName(videoFile), "frames");

            CleanupOutputFiles();
            this.frames = await Ffmpeg.SplitVideo(videoFile, this.outputFiles);

            this.TotalFrames = frames.Count;
            int debugFrame = -1;
            var pos = 0;

            ShowStatus("Matching events in the video...");
            foreach (var e in this.shapeEvents)
            {
                e.frameTime.Text = "searching...";
            }

            for (var frameIndex = 0; frameIndex < frames.Count; frameIndex++)
            {
                NotifyProgress(frameIndex);
                if (closed)
                {
                    break;
                }
                var video = frames[frameIndex];
                var bitmap = await LoadBitmapAsync(video);
                if (closed)
                {
                    break;
                }
                var e = shapeEvents[pos];
                var pt = e.windowPos;
                // pick a top left corner
                pt.X -= (ShapeWidth / 2) - 10;
                pt.Y -= (ShapeHeight / 2) - 10;
                pt = User32.ConvertFromDeviceIndependentPixels(hwnd, pt);
                var x = (int)pt.X;
                var y = (int)pt.Y;
                var bytesPerPixel = 3;
                var pixels = bitmap.GetRgb24Pixels();
                var stride = bitmap.PixelWidth * bytesPerPixel;
                bitmap.StreamSource = null;
                bitmap = null;
                var index = (y * stride) + (x * bytesPerPixel);
                if (index < pixels.Length)
                {
                    byte r = pixels[index];
                    byte g = pixels[index + 1];
                    byte b = pixels[index + 2];
                    var found = Color.FromRgb(r, g, b);
                    if (frameIndex == debugFrame)
                    {
                        Debug.WriteLine("debug me");
                    }
                    if (ColorPalette.IsColorClose(e.color, found))
                    {
                        // found it!!
                        int ms = (int)((frameIndex * 1000) / frameRate);
                        int delta = (ms - e.milliseconds);
                        if (pos == 0)
                        {
                            // fudge the first one so it matches, we only care about the delta after this one.
                            startDelayMilliseconds = delta;
                            delta = 0;
                        }
                        else
                        {
                            ms -= startDelayMilliseconds;
                            delta -= startDelayMilliseconds;
                        }
                        Debug.WriteLine($"Found shape {pos} at time {ms}, expecting {e.milliseconds}, delta {delta}" +
                            $", found color {GetRgb(found)} expecting color {GetRgb(e.color)}");
                        var text = $"{ms} ({delta})";
                        e.frameTime.Text = text;
                        pos++;
                        await Task.Delay(30); // give this label time to appear as a type of progress indication.
                        if (pos >= shapeEvents.Count)
                        {
                            break;
                        }
                    }
                    else if (!ColorPalette.IsColorClose(Colors.White, found))
                    {
                        Debug.WriteLine($"Frame {frameIndex} at {x},{y} found color {GetRgb(found)} expecting color {GetRgb(e.color)}");
                    }

                    // DEBUGGING find something in the bitmap!
                    //int skipTitleBar = 40 * stride;
                    //for (int i = skipTitleBar; i + 3 < pixels.Length; i += 3)
                    //{
                    //    y = (i / stride);
                    //    x = (i - (y * stride)) / bytesPerPixel;
                    //    if (y > 550)
                    //    {
                    //        // skip status bar.
                    //        break;
                    //    }
                    //    r = pixels[i];
                    //    g = pixels[i + 1];
                    //    b = pixels[i + 2];
                    //    if (r < 250 || g < 250 || b < 250) {
                    //        // found non white pixel at...
                    //        Debug.WriteLine($"Found non-white pixel {r},{g},{b} at {x},{y}");
                    //        break;
                    //    }
                    //}
                }
            }
            if (pos < shapeEvents.Count)
            {
                var msg = $"Missing matches at shape number {pos} of {shapeEvents.Count}";
                Debug.WriteLine(msg);
                ShowStatus(msg);
                ;
                while (pos < this.shapeEvents.Count)
                {
                    var e = this.shapeEvents[pos++];
                    e.frameTime.Text = "not found";
                }
            }
            else
            {
                ShowStatus("All events matched in the video");
            }
        }

        private void ShowStatus(string msg)
        {
            if (StatusEvent != null)
            {
                StatusEvent(this, msg);
            }
        }

        private void CleanupOutputFiles()
        {
            if (!string.IsNullOrEmpty(outputFiles) && Directory.Exists(outputFiles))
            {
                try
                {
                    Directory.Delete(outputFiles, true);
                }
                catch { }
            }
        }

        public int TotalFrames { get; internal set; }

        public async Task<BitmapImage> LoadBitmapAsync(string imageUrl)
        {
            await Task.CompletedTask;
            var bitmap = new BitmapImage();
            bitmap.CacheOption = BitmapCacheOption.Default;
            bitmap.BeginInit();
            bitmap.UriSource = new Uri(imageUrl, UriKind.RelativeOrAbsolute);
            bitmap.EndInit();
            return bitmap;
        }

        internal void OnRecordingDeleted(string filename)
        {
            if (videoFile == filename)
            {
                videoFile = null;
                this.frames = null;
                this.DrawingCanvas.Children.Clear();
            }
        }

    }
}
