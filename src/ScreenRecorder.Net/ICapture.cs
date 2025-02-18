using System.Runtime.InteropServices;
using System.Windows.Media;

namespace ScreenRecorder
{
    public  class EncodingStats
    {
        /// <summary>
        /// The time that frames are sent to the video encoder.
        /// </summary>
        public double[] SampleTicks;
        /// <summary>
        /// The time that frames are captured.
        /// </summary>
        public double[] FrameTicks;
        public string FileName;
        public double StartDelay;
    }

    public enum VideoEncodingQuality : uint
    {
        Auto = 0,
        HD1080p = 1,
        HD720p = 2,
        Wvga = 3,
        Ntsc = 4,
        Pal = 5,
        Vga = 6,
        Qvga = 7,
        Uhd2160p = 8,
        Uhd4320p = 9
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VideoEncoderProperties
    {
        public uint bitrateInBps; // e.g. 9000000 for 9 mbps.
        public uint frameRate; // e.g 30 or 60
        public VideoEncodingQuality quality;
        public uint seconds; // maximum length before encoding finishes or 0 for infinite.
        public uint memory_cache; // 1=use in-memory caching so no disk activity until encoding is done.
    };

    public interface ICapture : IDisposable
    {
        /// <summary>
        /// Start capturing frames of the screen at the given location (in screen coordinates, 
        /// not WPF device independent coordinates). This capture uses the new
        /// Windows::Graphics::Capture::Direct3D11CaptureFramePool.
        /// </summary>
        /// <param name="x">In screen coordinates</param>
        /// <param name="y">In screen coordinates</param>
        /// <param name="width">In screen coordinates</param>
        /// <param name="heigth">In screen coordinates</param>
        /// <param name="timeout">Time to wait for first frame (to make sure frames are arriving).</param>
        /// <returns></returns>
        public Task StartCapture(int x, int y, int width, int heigth, int timeout = 10000);

        /// <summary>
        /// Return the next captured frame.
        /// </summary>
        /// <returns></returns>
        public ImageSource CaptureImage();

        /// <summary>
        /// Start encoding video using GPU hardware H264 encoding and write the video to the
        /// specified file.  This call blocks until we read the "seconds" defined in the
        /// VideoEncoderProperties or until another thread calls StopEncoding.
        /// </summary>
        /// <param name="file"></param>
        /// <param name="properties"></param>
        public void EncodeVideo(string file, VideoEncoderProperties properties);

        /// <summary>
        /// Stop an existing video encoding.
        /// </summary>
        public void StopEncoding();

        /// <summary>
        /// This event is raised when video encoding is finished and contains the precise
        /// timing information of each frame of the video captured.
        /// </summary>
        public event EventHandler<EncodingStats> EncodingCompleted;
    }

}
