using Microsoft.Win32;
using ScreenRecorder;
using ScreenRecorder.Native;
using ScreenRecorder.Utilities;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

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
        bool encodingFfmpeg;
        DispatcherTimer showMouseTimer;
        bool captured;
        int x = -1;
        int y = -1;
        RECT bounds;
        DispatcherTimer showDurationTimer;
        DateTime startTime;
        bool calibrating;
        uint frameRate = 60;
        nint hwnd;
        DelayedActions actions = new DelayedActions();

        public MainWindow()
        {
            UiDispatcher.Initialize();
            InitializeComponent();
            UpdateButtonState();
            this.Loaded += OnWindowLoaded;
        }

        private void OnWindowLoaded(object sender, RoutedEventArgs e)
        {
            this.hwnd = new WindowInteropHelper(this).Handle;
        }

        private string GetTempPath(string name)
        {
            return Path.Combine(Path.GetTempPath(), "WpfTestApp", name);
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            this.actions.Close();
            this.CalibrationView.OnClosed();
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

        private async Task StopCapture()
        {
            capturing = false;
            while (this.capture != null)
            {
                await Task.Delay(100);
            }
            this.capturing = false;
            UpdateButtonState();
        }

        private async void OnCapture(object sender, RoutedEventArgs e)
        {
            if (this.capturing)
            {
                await StopCapture();
            }
            else
            {
                _ = StartCapture();
            }
        }

        async Task StartCapture()
        {
            capturing = true;
            using var c = new Capture();
            c.EncodingCompleted += OnEncodingCompleted;
            this.capture = c;
            List<int> averageFps = new List<int>();
            try
            {
                ShowStatus("Initializing...");
                int w = bounds.Right - bounds.Left;
                int h = bounds.Bottom - bounds.Top;
                await c.StartCapture(this.x, this.y, w, h, 10000);
                UpdateButtonState();
                int ignore = 2;
                await Task.Run(async () =>
                {
                    var throttle = new Throttle((int)frameRate);
                    var start = Environment.TickCount;
                    int count = 0;
                    while (capturing)
                    {
                        if (this.encoding)
                        {
                            await Task.Delay(1000);
                            throttle.Reset();
                        }
                        else
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

        private void OnEncodingCompleted(object sender, EncodingStats e)
        {
            var ticks = e.FrameTicks;
            var frames = e.SampleTicks;
            var file = e.FileName;
            var folder = System.IO.Path.GetDirectoryName(file);

            ShowVideo(file);
            ProcessFrameTicks(ticks, frames, file);

            Dispatcher.BeginInvoke(new Action(() =>
            {
                this.encoding = false;
                UpdateButtonState();
                if (calibrating)
                {
                    OnCompleteCalibration(e);
                }
                else
                {
                    // MessageBox.Show(msg, "Video completed", MessageBoxButton.OK, MessageBoxImage.Hand);
                }
            }));
        }

        private void ProcessFrameTicks(double[] ticks, double[] samples, string videoFileName)
        {
            var folder = System.IO.Path.GetDirectoryName(videoFileName);
            var meta_file = System.IO.Path.GetFileNameWithoutExtension(videoFileName) + "_meta.json";
            meta_file = Path.Combine(folder, meta_file);
            var sampleData = "";
            if (samples != null)
            {
                var text = string.Join(",", samples);
                sampleData = ", \"frame_times\":[" + text + "]";
            }
            var data = string.Join(",", ticks);
            File.WriteAllText(meta_file, "{\"video_ticks\":[" + data + "]" + sampleData + "}");

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
            ShowStatus(msg);

        }

        private void OnStop(object sender, RoutedEventArgs e)
        {
            this.capturing = false;
            this.UpdateButtonState();
        }

        private void UpdateButtonState()
        {
            CaptureButton.IsEnabled = this.x != -1;
            EncodeFfmpegButton.IsEnabled = this.capturing;
            EncodeButton.IsEnabled = this.capturing && !this.encodingFfmpeg;
            CaptureButton.Content = this.capturing ? "Stop Capture" : "Capture";
            EncodeButton.Content = this.encoding ? "Stop Encoding" : "Encode Video";
            EncodeFfmpegButton.Content = this.encoding ? "Stop Encoding" : "Ffmpeg Encode";
            CapturedImage.Visibility = this.encoding ? Visibility.Collapsed : Visibility.Visible;
            OverlayMessage.Visibility = this.encoding ? Visibility.Visible : Visibility.Collapsed;
            if (calibrating)
            {
                OverlayMessage.Visibility = Visibility.Collapsed;
                this.CalibrationView.Visibility = Visibility.Visible;
            }
            else
            {
                this.CalibrationView.Visibility = Visibility.Collapsed;
            }
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
                POINT pt = new POINT() { X = x, Y = y };
                nint hwnd = User32.WindowFromPoint(pt);
                if (hwnd == nint.Zero)
                {
                    MessageBox.Show($"No window found at location {x}, {y}", "No Window Found", MessageBoxButton.OK, MessageBoxImage.Error);
                }
                else
                {
                    PickWindow(hwnd);
                }
            }
            else
            {
                base.OnMouseDown(e);
            }
        }

        private void PickWindow(nint hwnd)
        {
            var windowBounds = User32.GetWindowRectWithoutDropshadow(hwnd);
            this.bounds = User32.GetClientCroppingRect(hwnd);
            this.x = windowBounds.Left + bounds.Left;
            this.y = windowBounds.Top + bounds.Top;
            this.UpdateButtonState();
            var w = bounds.Right - bounds.Left;
            var h = bounds.Bottom - bounds.Top;
            ShowStatus($"Picked hwnd {hwnd:x8} bounds {this.x},{this.y},{w},{h}");
        }

        #endregion

        private void OnNewWindowClicked(object sender, RoutedEventArgs e)
        {
            new MainWindow().Show();
        }

        private void StopEncoding()
        {
            StopCaptureTimer();
            if (this.capture != null)
            {
                this.capture.StopEncoding();
            }
            this.encoding = false;
            UpdateButtonState();
        }

        private void OnEncode(object sender, RoutedEventArgs e)
        {
            if (this.encoding)
            {
                StopEncoding();
            }
            else
            {
                SaveFileDialog sd = new SaveFileDialog();
                sd.Filter = ".mp4 files|*.mp4";
                sd.CheckPathExists = true;
                if (sd.ShowDialog() == true)
                {
                    var file = sd.FileName;
                    EncodeVideo(file, seconds:60);
                }
            }
        }

        private void EncodeVideo(string file, uint seconds)
        {
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
                        frameRate = this.frameRate,
                        quality = VideoEncodingQuality.HD1080p,
                        memory_cache = (uint)((seconds == 0) ? 0 : 1),
                        seconds = seconds,
                    };

                    // kicks off an internal async task.
                    this.capture.EncodeVideo(file, properties);

                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, "Encoding Error", MessageBoxButton.OK, MessageBoxImage.Error);
                    this.encoding = false;
                    UpdateButtonState();
                }
            }
        }

        private void OnEncodeFfmpeg(object sender, RoutedEventArgs e)
        {
            if (this.encoding)
            {
                StopEncoding();
            }
            else
            {
                SaveFileDialog sd = new SaveFileDialog();
                sd.Filter = ".mp4 files|*.mp4";
                sd.CheckPathExists = true;
                if (sd.ShowDialog() == true)
                {
                    var file = sd.FileName;
                    EncodeFfmpeg(file, 60);
                }
            }
        }

        private async void EncodeFfmpeg(string file, int frameRate)
        {
            this.encoding = true;
            this.encodingFfmpeg = true;
            UpdateButtonState();
            this.startTime = DateTime.Now;
            StartCaptureTimer();
            if (!Ffmpeg.FindFFMPeg())
            {
                return;
            }

            // Keep the frames in memory and write them out to png files then encode the result
            // using ffmpeg.
            await Task.Run(async () =>
            {
                var throttle = new Throttle((int)frameRate);
                var start = Environment.TickCount;
                var timer = new PerfTimer();
                timer.Start();
                List<byte[]> images = new List<byte[]>();
                List<double> frameTicks = new List<double>();
                ShowStatus("Capturing frames...");
                while (this.encoding)
                {
                    var img = this.capture.RawCaptureImageBuffer();
                    images.Add(img);
                    frameTicks.Add(timer.GetSeconds());
                    throttle.Step();
                }

                ShowStatus("Encoding frames...");
                int i = 0;
                DispatcherTimer statusTimer = new DispatcherTimer(TimeSpan.FromMilliseconds(100), DispatcherPriority.Normal, (s, e) =>
                {
                    ShowStatus($"Saving frame {i} of {images.Count}...");
                }, this.Dispatcher);

                ProcessFrameTicks(frameTicks.ToArray(), null, file);

                // ok, now save the frames and encode them into the file.
                var outputFiles = GetTempPath("frames");
                CleanupOutputFiles(outputFiles);
                Directory.CreateDirectory(outputFiles);
                Debug.WriteLine($"Saving frames to {outputFiles}");

                for (i = 0; i < images.Count; i++)
                {
                    var img = this.capture.CreateBitmapImage(images[i]);
                    var fileName = $"frame_{i:D4}.png";
                    var filePath = Path.Combine(outputFiles, fileName);
                    SavePng(img, filePath);
                }

                ShowStatus("Encoding video...");
                var rc = await Ffmpeg.EncodeVideo(file, outputFiles, (int)this.frameRate);
                if (rc != 0)
                {
                    Debug.WriteLine($"ffmpeg returned {rc}");
                }
                else
                {
                    ShowVideo(file);
                }

                statusTimer.Stop();

            });

            this.encodingFfmpeg = false;
        }

        private void ShowVideo(string filename)
        {
            Shell32.OpenUrl(hwnd, filename);
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
            showDurationTimer = new DispatcherTimer(TimeSpan.FromSeconds(0.5), DispatcherPriority.Normal, UpdateDuration, this.Dispatcher);
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

        private async void OnCompleteCalibration(EncodingStats e)
        {
            await StopCapture();
            CalibrationView.Visibility = Visibility.Visible;
            CalibrateButton.Content = "Computing...";
            CalibrateButton.IsEnabled = false;

            int ms = (int)(e.StartDelay * 1000);
            Debug.WriteLine($"Encoder says start delay was {ms} milliseconds");
            await this.CalibrationView.SyncVideoToShapes(this.calibrationVideo, (int)this.frameRate);

            CalibrationView.Visibility = Visibility.Visible;
            CalibrateButton.Content = "Calibrate";
            CalibrateButton.IsEnabled = true;
            calibrating = false;
        }

        private async void OnCalibrate(object sender, RoutedEventArgs e)
        {
            if (calibrating)
            {
                CalibrateButton.Content = "Stopping...";
                CalibrateButton.IsEnabled = false;
                this.CalibrationView.StopCalibrating();
                StopEncoding();
            }
            else
            {
                calibrating = true;
                CalibrateButton.Content = "Complete";
                StopEncoding();
                await StopCapture();

                ShowStatus("Do a few mouse clicks to create calibration events...");
                var hwnd = new WindowInteropHelper(this).Handle;
                PickWindow(hwnd);
                calibrating = true;
                CalibrationView.Visibility = Visibility.Visible;
                _ = StartCapture();
                calibrationVideo = GetTempPath("calibration.mp4");
                EncodeVideo(calibrationVideo, 0);
                this.CalibrationView.OnRecordingStarted();
            }

        }

        string calibrationVideo;

        private void HideCalibrator(object sender, RoutedEventArgs e)
        {
            CalibrationView.Visibility = Visibility.Collapsed;
        }

        private void OnCalibrationProgress(object sender, int e)
        {
            actions.StartDelayedAction("ShowProgress", () => OnShowCalibrationProgress(e), TimeSpan.FromMilliseconds(30));
        }

        void OnShowCalibrationProgress(int e)
        {
            var total = this.CalibrationView.TotalFrames;
            ShowStatus($"processing {e} of {total}");
        }

        private void OnCalibrationStatus(object sender, string msg)
        {
            ShowStatus(msg);
        }

        protected override void OnPreviewKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.F5 && !string.IsNullOrEmpty(this.calibrationVideo))
            {
                e.Handled = true;
                _ = this.CalibrationView.SyncVideoToShapes(this.calibrationVideo, (int)this.frameRate);
            }
            else if (e.Key == Key.F9)
            {
                e.Handled = true;
                OpenFileDialog sd = new OpenFileDialog();
                sd.Filter = ".mp4 files|*.mp4";
                sd.CheckPathExists = true;
                if (sd.ShowDialog() == true)
                {
                    _ = this.CalibrationView.SyncVideoToShapes(sd.FileName, (int)this.frameRate);
                }
            }

            base.OnPreviewKeyDown(e);
        }

    }
}