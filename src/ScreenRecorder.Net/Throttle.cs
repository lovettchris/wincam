using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Policy;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media.Media3D;
using WpfTestApp;

namespace ScreenRecorder.Utilities
{
    /// <summary>
    /// This class provides an accurate way of achieving a target frames per second
    /// on a given operation.  Simply call Step and it will time how long it takes between
    /// each Step call and it will add any necessary sleep needed to achieve your target
    /// fps accurate to about 1 millisecond.
    /// </summary>
    public class Throttle
    {
        private int fps;
        private double microsecondsPerFrame;
        private double previousTime;
        const int WindowSize = 10;
        private double[] window = new double[WindowSize];
        private int windowPos = 0;
        private double sum = 0;
        private PerfTimer timer = new PerfTimer();
        private double slippage;


        public Throttle(int fps) 
        {
            this.fps = fps;
            this.microsecondsPerFrame = 1000000.0 / fps;
            this.Reset();
        }

        /// <summary>
        /// Call Reset to start or reset the throttle.
        /// </summary>
        public double Reset()
        {
            timer.Start();
            previousTime = timer.GetMicroseconds();
            slippage = 0;
            windowPos = 0;
            Array.Fill(window, 0);
            sum = 0;
            return 0;
        }

        public double Ticks()
        {
            return timer.GetSeconds();
        }

        /// <summary>
        /// Throttle to the specified fps and return the time in second since Reset.
        /// </summary>
        public double Step()
        {
            var now = timer.GetMicroseconds();
            if (previousTime != 0)
            {
                var step_time = now - previousTime;
                // compute sliding window mean.
                this.sum -= this.window[windowPos];
                this.window[windowPos] = step_time;
                this.sum += step_time;
                this.windowPos = (this.windowPos + 1) % WindowSize;
                var smoothed_average = this.sum / WindowSize;
                var throttle_ms = this.microsecondsPerFrame - smoothed_average;
                if (throttle_ms > 0)
                {
                    NativeMethods.SleepMicroseconds((long)(throttle_ms));
                } 
                else
                {
                    slippage += (throttle_ms / 1000000.0);
                }
            }
            previousTime = timer.GetMicroseconds();
            return previousTime;
        }

        public double Slippage => slippage;

    }
}
