using SmartReplayApp.Utilities;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace WpfTestApp
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        ICapture capture;
        bool running;

        public MainWindow()
        {
            InitializeComponent();
            UpdateButtonState();
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            running = false;
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
            this.CaptureButton.IsEnabled = false;
            if (this.capture != null)
            {
                ShowStatus("Terminating previous capture...");
                running = false;
                while (this.capture != null)
                {
                    await Task.Delay(100);
                }
            }

            running = true;
            using var c = new Capture();
            this.capture = c;
            List<int> averageFps = new List<int>();
            try
            {
                ShowStatus("Initializing...");
                c.StartCapture(100, 100);
                UpdateButtonState();
                int ignore = 2;
                await Task.Run(async () =>
                {
                    var throttle = new Throttle(60);
                    var start = Environment.TickCount;
                    int count = 0;
                    while (running)
                    {
                        await Dispatcher.InvokeAsync(new Action(() =>
                        {
                            var img = c.CaptureImage();
                            this.CapturedImage.Source = img;
                        }));
                        count++;
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
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message, "Capture Failed", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            this.CaptureButton.IsEnabled = true;
            this.capture = null;
            running = false;
            this.UpdateButtonState();
            int avg = averageFps.Sum() / averageFps.Count;
            this.ShowStatus($"average fps {avg}");
        }

        private void OnStop(object sender, RoutedEventArgs e)
        {
            this.running = false;
            this.UpdateButtonState();
        }

        private void UpdateButtonState()
        {
            CaptureButton.Visibility = running ? Visibility.Collapsed : Visibility.Visible;
            StopButton.Visibility = running ? Visibility.Visible : Visibility.Collapsed;
        }
    }
}