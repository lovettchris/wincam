using Microsoft.Win32;
using ScreenRecorder;
using ScreenRecorder.Native;
using ScreenRecorder.Utilities;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Input;
using System.Windows.Interop;
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
        DispatcherTimer showMouseTimer;
        bool captured;
        int x = -1;
        int y = -1;
        uint frameRate;
        RECT bounds;
        DispatcherTimer showDurationTimer;
        DateTime startTime; 
        bool calibrating;
        nint hwnd;
        DelayedActions actions = new DelayedActions();

        public MainWindow()
        {
            UiDispatcher.Initialize();
            InitializeComponent();
            UpdateButtonState();
            this.Loaded += OnWindowLoaded;
        }

        private uint GetFrameRate()
        {
            return FpsCombo.SelectedIndex == 0 ? 30u : 60u;
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
            if (capturing)
            {
                capturing = false;
                while (this.capture != null)
                {
                    await Task.Delay(100);
                }
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
                _ = RunCapture();
            }
        }

        async Task InitializeCapture()
        {
            capturing = true;
            var c = new Capture();
            c.EncodingCompleted += OnEncodingCompleted;
            c.ProgressUpdate += OnProgressUpdate;
            c.EncodingError += OnEncodingError;
            this.capture = c;
            ShowStatus("Initializing....");
            int w = bounds.Right - bounds.Left;
            int h = bounds.Bottom - bounds.Top;
            await c.StartCapture(this.x, this.y, w, h, 10000);
        }

        private void OnProgressUpdate(object sender, ProgressEventArgs e)
        {
            this.actions.StartDelayedAction("UpdateStatus", () => UpdateProgress(e), TimeSpan.FromMilliseconds(30));
        }

        private void UpdateProgress(ProgressEventArgs e)
        {            
            if (e.Value < e.Maximum)
            {
                Progress.Visibility = Visibility.Visible;
                Progress.Minimum = e.Minimum;
                Progress.Value = e.Value;
                Progress.Maximum = e.Maximum;
            }
            else
            {
                Progress.Visibility = Visibility.Collapsed;
            }
            if (!string.IsNullOrEmpty(e.Messgage))
            {
                StatusText.Text = e.Messgage;
            }
        }

        async Task RunCapture()
        {
            List<int> averageFps = new List<int>();
            try
            {
                await InitializeCapture();
                UpdateButtonState();
                int ignore = 2;
                var c = this.capture;
                this.frameRate = this.GetFrameRate();
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
            this.capture?.Dispose();
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

        private void OnEncodingError(object sender, Exception e)
        {
            var msg = e.Message;
            if (e is COMException ex)
            {
                msg = this.capture.GetErrorMessage(e.HResult); 
            }
            Dispatcher.BeginInvoke(new Action(() =>
            {
                MessageBox.Show(msg, "Encoding Error", MessageBoxButton.OK, MessageBoxImage.Error);
                this.encoding = false;
                UpdateButtonState();
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
            if (steps.Count > 0)
            {
                var min = steps.Min();
                var max = steps.Max();
                var avg = steps.Average();
                var msg = $"Frame step times, min: {min:F3}, max: {max:F3}, avg: {avg:F3}";
                Debug.WriteLine(msg);
            }
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
                    EncodeVideo(file, seconds:60, this.Native.IsChecked == true, this.FFmpeg.IsChecked == true);
                }
            }
        }

        private async void EncodeVideo(string file, uint seconds, bool native = true, bool ffmpeg = false)
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
                    this.frameRate = this.GetFrameRate();

                    var properties = new VideoEncoderProperties()
                    {
                        bitrateInBps = 0,
                        frameRate = this.frameRate,
                        quality = VideoEncodingQuality.HD720p,
                        seconds = seconds,
                        ffmpeg = ffmpeg ? 1u : 0u,
                    };

                    // kicks off an internal async task
                    if (native) 
                    {
                        this.capture.EncodeVideoNative(file, properties);
                    }
                    else
                    {
                        var outputFiles = GetTempPath("frames");
                        await Task.Run(async () =>
                        {
                            await this.capture.EncodeVideoFrames(file, properties, outputFiles);
                        });
                        this.capture?.Dispose();
                        this.capture = null;
                    }
                }
                catch (Exception ex)
                {
                    this.OnEncodingError(this, ex);
                }
            }
        }

        private void ShowVideo(string filename)
        {
            Shell32.OpenUrl(hwnd, filename);
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

        EncodingStats stats;

        private async void OnCompleteCalibration(EncodingStats e)
        {
            await StopCapture();
            CalibrationView.Visibility = Visibility.Visible;
            CalibrateButton.Content = "Computing...";
            CalibrateButton.IsEnabled = false;
            this.stats = e;
            await this.CalibrationView.SyncVideoToShapes(this.calibrationVideo, (int)this.frameRate, e.FrameTicks);

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
                await StopCapture();
            }
            else
            {
                StartCalibration(false);
            }
        }
        async void StartCalibration(bool native)
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
            calibrationVideo = GetTempPath("calibration.mp4");

            await this.InitializeCapture();
            EncodeVideo(calibrationVideo, 0, this.Native.IsChecked == true);
            this.CalibrationView.OnRecordingStarted();
        }

        string calibrationVideo;

        private void HideCalibrator(object sender, RoutedEventArgs e)
        {
            CalibrationView.Visibility = Visibility.Collapsed;
        }

        private void OnCalibrationProgress(object sender, ProgressEventArgs e)
        {
            actions.StartDelayedAction("ShowProgress", () => OnShowCalibrationProgress(e), TimeSpan.FromMilliseconds(30));
        }

        void OnShowCalibrationProgress(ProgressEventArgs e)
        {
            if (e.Value < e.Maximum)
            {
                Progress.Visibility = Visibility.Visible;
                Progress.Value = e.Value;
                Progress.Maximum = e.Maximum;
            }
            else
            {
                Progress.Visibility = Visibility.Collapsed;
            }
            if (!string.IsNullOrEmpty(e.Messgage))
            {
                StatusText.Text = e.Messgage;   
            }
        }

        private void OnCalibrationStatus(object sender, string msg)
        {
            ShowStatus(msg);
        }

        protected override void OnPreviewKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.F5)
            {
                e.Handled = true;
                this.CalibrationView.Clear();
            }
            else if (e.Key == Key.F9)
            {
                e.Handled = true;
                OpenFileDialog sd = new OpenFileDialog();
                sd.Filter = ".mp4 files|*.mp4";
                sd.CheckPathExists = true;
                if (sd.ShowDialog() == true)
                {
                    var fileName = sd.FileName;
                    _ = this.CalibrationView.SyncVideoToShapes(fileName, (int)this.frameRate, stats.FrameTicks);
                }
            }

            base.OnPreviewKeyDown(e);
        }

        private void OnCalibrateNative(object sender, RoutedEventArgs e)
        {

        }
    }
}