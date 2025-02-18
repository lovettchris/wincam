using ScreenRecorder.Utilities;
using System.ComponentModel;
using System.Diagnostics;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using Microsoft.Win32;
using ScreenRecorder;
using ScreenRecorder.Native;
using System.IO;

namespace WpfTestApp
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml. 
    /// </summary>
    public partial class MainWindow : Window
    {
        ICapture capture;
        bool capturing;
        bool encoding;
        DispatcherTimer showMouseTimer;
        bool captured;
        int x = -1; 
        int y = -1;
        Rect bounds;
        DispatcherTimer showDurationTimer;
        DateTime startTime;

        public MainWindow()
        {
            InitializeComponent();
            UpdateButtonState();
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            StopMouseTimer();
            capturing = false;
            base.OnClosing(e);
        }

        bool updateBusyThrottle = false;

        private void ShowStatus(string msg)
        {
            if (updateBusyThrottle) { return; }
            updateBusyThrottle = true;

            Dispatcher.BeginInvoke(new Action(() =>
            {
                StatusText.Text = msg;
                this.updateBusyThrottle = false;
            }));
        }

        private async void OnCapture(object sender, RoutedEventArgs e)
        {
            if (this.capturing)
            {
                ShowStatus("Terminating capture...");
                capturing = false;
                while (this.capture != null)
                {
                    await Task.Delay(100);
                }
                this.capturing = false;
                UpdateButtonState();
            }
            else
            {
                capturing = true;
                using var c = new Capture();
                c.EncodingCompleted += OnEncodingCompleted;
                this.capture = c;
                List<int> averageFps = new List<int>();
                try
                {
                    ShowStatus("Initializing...");
                    await c.StartCapture((int)this.x, (int)this.y, (int)bounds.Width, (int)bounds.Height, 10000);
                    UpdateButtonState();
                    int ignore = 2;
                    await Task.Run(async () =>
                    {
                        var throttle = new Throttle(60);
                        var start = Environment.TickCount;
                        int count = 0;
                        while (capturing)
                        {
                            await Dispatcher.InvokeAsync(new Action(() =>
                            {
                                var img = c.CaptureImage();
                                this.CapturedImage.Source = img;
                                if (img != null)
                                {
                                    count++;
                                }
                            }), DispatcherPriority.Background);

                            var end = Environment.TickCount;
                            if (end > start + 1000)
                            {
                                var fps = count;
                                count = 0;
                                start = end;
                                ShowStatus($"{fps} fps");
                                if (ignore == 0)
                                {
                                    averageFps.Add(fps);
                                }
                                else
                                {
                                    ignore--;
                                }
                            }
                            throttle.Step();
                        }
                    });
                }
                catch (TaskCanceledException)
                {
                    Debug.WriteLine("Task cancelled");
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, "Capture Failed", MessageBoxButton.OK, MessageBoxImage.Error);
                }

                this.CaptureButton.IsEnabled = true;
                this.capture = null;
                capturing = false;
                this.UpdateButtonState();
                if (averageFps.Count > 0)
                {
                    int avg = averageFps.Sum() / averageFps.Count;
                    this.ShowStatus($"average fps {avg}");
                }
            }
        }

        private void OnEncodingCompleted(object sender, EncodingStats e)
        {
            var ticks = e.FrameTicks;
            var data = string.Join(",", ticks);
            var folder = System.IO.Path.GetDirectoryName(e.FileName);
            var meta_file = System.IO.Path.GetFileNameWithoutExtension(e.FileName) + "_meta.json";
            meta_file = Path.Combine(folder, meta_file);
            File.WriteAllText(meta_file, "{\"video_ticks\":[" + data + "]}");

            var steps = new List<double>();
            for (int i = 1; i < ticks.Length; i++)
            {
                var step = ticks[i] - ticks[i - 1];
                steps.Add(step);
            }
            var min = steps.Min();
            var max = steps.Max();
            var avg = steps.Average();
            var msg = $"Frame step times, min: {min:F3}, max: {max:F3}, avg: {avg:F3}";
            Debug.WriteLine(msg);

            Dispatcher.BeginInvoke(new Action(() =>
            {
                ShowStatus(msg);
                this.encoding = false;
                UpdateButtonState();
                MessageBox.Show(msg, "Video completed", MessageBoxButton.OK, MessageBoxImage.Hand);
            }));
        }

        private void OnStop(object sender, RoutedEventArgs e)
        {
            this.capturing = false;
            this.UpdateButtonState();
        }

        private void UpdateButtonState()
        {
            CaptureButton.IsEnabled = this.x != -1;
            EncodeButton.IsEnabled = this.capturing;
            CaptureButton.Content = this.capturing ? "Stop Capture" : "Capture";
            EncodeButton.Content = this.encoding ? "Stop Encoding" : "Encode Video";
        }

        #region WindowMousePicker

        private void StopMouseTimer()
        {
            if (showMouseTimer != null)
            {
                showMouseTimer.Stop();
                showMouseTimer = null;
            }
        }


        private void OnPickWindowChecked(object sender, RoutedEventArgs e)
        {
            if (this.CaptureMouse())
            {
                captured = true;
                ShowStatus("Move mouse and click on the window you want to capture");
                showMouseTimer = new DispatcherTimer(TimeSpan.FromMilliseconds(30), DispatcherPriority.Background, OnShowMouse, this.Dispatcher);
            }
        }

        private void OnShowMouse(object sender, EventArgs e)
        {
            var mousePos = User32.GetScreenCursorPos();
            PositionText.Text = $"{mousePos.X},{mousePos.Y}";
        }

        protected override void OnMouseDown(MouseButtonEventArgs e)
        {
            StopMouseTimer();
            if (captured)
            {
                PickWindowButton.IsChecked = false;
                captured = false;
                this.ReleaseMouseCapture();
                e.Handled = true;
                var mousePos = User32.GetScreenCursorPos();
                var x = (int)mousePos.X;
                var y = (int)mousePos.Y;
                this.UpdateButtonState();
                POINT pt = new POINT() { X = x, Y = y };
                nint hwnd = User32.WindowFromPoint(pt);
                if (hwnd == nint.Zero)
                {
                    MessageBox.Show($"No window found at location {x}, {y}", "No Window Found", MessageBoxButton.OK, MessageBoxImage.Error);
                }
                else
                {
                    this.x = x;
                    this.y = y;
                    this.bounds = User32.GetClientScreenRect(hwnd);
                    ShowStatus($"Picked hwnd {hwnd:x8} bounds {bounds}");
                }
            }
            else
            {
                base.OnMouseDown(e);
            }
        }
        #endregion

        private void OnNewWindowClicked(object sender, RoutedEventArgs e)
        {
            new MainWindow().Show();
        }

        private void OnEncode(object sender, RoutedEventArgs e)
        {
            if (this.encoding)
            {
                StopCaptureTimer();
                if (this.capture != null)
                {
                    this.capture.StopEncoding();
                }
                this.encoding = false;
                UpdateButtonState();
            }
            else
            {
                SaveFileDialog sd = new SaveFileDialog();
                sd.Filter = ".mp4 files|*.mp4";
                sd.CheckPathExists = true;
                if (sd.ShowDialog() == true)
                {
                    var file = sd.FileName;
                    if (this.capture != null)
                    {
                        try
                        {
                            SafeDelete(file);
                            this.encoding = true;
                            UpdateButtonState();
                            this.startTime = DateTime.Now;
                            StartCaptureTimer();

                            var properties = new VideoEncoderProperties()
                            {
                                bitrateInBps = 9000000,
                                frameRate = 60,
                                quality = VideoEncodingQuality.QualityHD1080p,
                                memory_cache = 1,
                                seconds = 60,
                            };
                            this.capture.EncodeVideo(file, properties);
                        }
                        catch (Exception ex)
                        {
                            MessageBox.Show(ex.Message, "Encode Video Error", MessageBoxButton.OK, MessageBoxImage.Error);
                            this.encoding = false;
                            UpdateButtonState();
                        }
                    }
                }
            }
        }

        private void StopCaptureTimer()
        {
            if (this.showDurationTimer != null)
            {
                showDurationTimer.Stop();
            }
        }

        private void StartCaptureTimer()
        {
            StopCaptureTimer();
            showDurationTimer = new DispatcherTimer(TimeSpan.FromSeconds(1), DispatcherPriority.Normal, UpdateDuration, this.Dispatcher);
            showDurationTimer.Start();
        }

        private void UpdateDuration(object sender, EventArgs e)
        {
            var span = DateTime.Now - this.startTime;
            span = TimeSpan.FromSeconds((int)span.TotalSeconds);
            PositionText.Text = span.ToString("g");
        }

        private void SafeDelete(string file)
        {
            if (System.IO.File.Exists(file))
            {
                System.IO.File.Delete(file);
            }
        }

    }
}