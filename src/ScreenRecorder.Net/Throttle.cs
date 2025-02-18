using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media.Media3D;

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
        private double millisecondsPerFrame;
        private double previousTime;
        private PerfTimer timer = new PerfTimer();
        const int SleepTolerance = 15;

        public Throttle(int fps) 
        {
            this.fps = fps;
            this.millisecondsPerFrame = 1000.0 / fps;
        }

        /// <summary>
        /// Call Reset to start or reset the throttle.
        /// </summary>
        public double Reset()
        {
            previousTime = 0;
            timer.Start();
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
            var now = timer.GetMilliseconds();
            var actualMilliseconds = now - previousTime;
            var throttle_ms = millisecondsPerFrame - actualMilliseconds;
            this.Delay(throttle_ms, previousTime);
            this.previousTime = timer.GetMilliseconds();
            return this.previousTime / 1000;
        }

        public double Delay(double milliseconds)
        {
            return this.Delay(milliseconds, timer.GetMilliseconds());
        }

        private double Delay(double milliseconds, double currentTime)
        {
            // Thread.Sleep is only accurate to about 15ms (on Windows)
            if (milliseconds > 0)
            {
                if (milliseconds > SleepTolerance)
                {
                    var ms = (int)(milliseconds / SleepTolerance) * SleepTolerance;
                    Thread.Sleep(ms);
                }

                do
                {
                    // we should be much closer now, we can close the remaining gap
                    // more accurately with this spin wait.
                    var actualMilliseconds = timer.GetMilliseconds() - currentTime;
                    milliseconds = millisecondsPerFrame - actualMilliseconds;
                }
                while (milliseconds > 0);
            }

            return timer.GetSeconds();

        }
    }
}
