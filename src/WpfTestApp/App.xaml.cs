using System.Configuration;
using System.IO;
using System.Windows;

namespace WpfTestApp
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            var path = Directory.GetCurrentDirectory();
            var configuration = path.Contains("\\Debug\\") ? "Debug" : "Release";
            // find D:\git\lovettchris\wincam\src\x64\Debug\.
            var binaries = Path.Combine(path, $"..\\..\\..\\..\\x64\\{configuration}");
            var files = Directory.GetFiles(binaries);
            bool found = false;
            foreach (var file in files)
            {
                var name = Path.GetFileName(file);
                if (name == "ScreenCapture.dll")
                {
                    found = true;
                    break;
                }
            }
            if (found)
            {
                path = Environment.GetEnvironmentVariable("PATH");
                Environment.SetEnvironmentVariable("PATH", path + ";" + binaries);
                var mod = NativeMethods.LoadLibrary("ScreenCapture.dll");
                if (mod != nint.Zero)
                {
                    NativeMethods.FreeLibrary(mod);
                }
            }
            else
            {
                MessageBox.Show($"ScreenCapture.dll not found in {files}, please build it");
            }
            base.OnStartup(e);
        }
    }

}
