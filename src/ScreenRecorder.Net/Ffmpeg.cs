using System.Diagnostics;
using System.IO;
using System.Windows;

namespace ScreenRecorder
{
    public class Ffmpeg
    {
        static string ffmpeg = null;

        public static bool FindFFMPeg()
        {
            if (string.IsNullOrEmpty(ffmpeg))
            {
                ffmpeg = FileHelpers.FindProgramInPath("ffmpeg.exe");
            }
            if (string.IsNullOrEmpty(ffmpeg))
            {
                MessageBox.Show("Please ensure ffpeg is in your PATH and restart this app", "ffmpeg not found",
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }
            return true;
        }

        public static async Task<List<string>> SplitVideo(string videoFile, string outputFiles)
        {
            List<string> frames = new List<string>();
            if (string.IsNullOrEmpty(ffmpeg))
            {
                return frames;
            }

            Directory.CreateDirectory(outputFiles);
            var proc = Process.Start(new ProcessStartInfo()
            {
                FileName = ffmpeg,
                Arguments = "-i " + "\"" + videoFile + "\" frame%04d.png",
                WorkingDirectory = outputFiles

            });
            await proc.WaitForExitAsync();
            int rc = proc.ExitCode;
            if (rc != 0)
            {
                Debug.WriteLine($"ffmpeg returned {rc}");
            }

            return new List<string>(Directory.GetFiles(outputFiles));
        }

        public static async Task<int> EncodeVideo(string videoFile, string inputFrameFolder, int frameRate, string pattern = "frame_%04d.png")
        {
            // ffmpeg -i img%04d.png -c:v libx264 -r 25 -pix_fmt yuv420p -c:a copy video_path_output
            var proc = Process.Start(new ProcessStartInfo()

            {
                FileName = ffmpeg,
                Arguments = "-i " + pattern + " -c:v libx264 -pix_fmt yuv420p -r " + frameRate + " -vf \"scale=trunc(iw/2)*2:trunc(ih/2)*2\" " + videoFile,
                WorkingDirectory = inputFrameFolder

            });
            await proc.WaitForExitAsync();
            return  proc.ExitCode;
        }
    }
}
