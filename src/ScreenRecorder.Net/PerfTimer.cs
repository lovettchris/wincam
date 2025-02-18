using System.Runtime.InteropServices;

namespace ScreenRecorder.Utilities
{
    public class PerfTimer
    {
        long m_Start;
        long m_Freq;

        [DllImport("KERNEL32.DLL", EntryPoint = "QueryPerformanceCounter", SetLastError = true,
                    CharSet = CharSet.Unicode, ExactSpelling = true,
                    CallingConvention = CallingConvention.StdCall)]
        public static extern int QueryPerformanceCounter(ref long time);

        [DllImport("KERNEL32.DLL", EntryPoint = "QueryPerformanceFrequency", SetLastError = true,
             CharSet = CharSet.Unicode, ExactSpelling = true,
             CallingConvention = CallingConvention.StdCall)]
        public static extern int QueryPerformanceFrequency(ref long freq);


        public PerfTimer()
        {
            m_Start = 0;
            QueryPerformanceFrequency(ref m_Freq);
        }

        public void Start()
        {
            m_Start = GetCounter();
        }

        /// <summary>
        /// Get time in seconds, (accurate to 100 nanoseconds).
        /// </summary>
        public double GetSeconds()
        {      
            return (double)GetTicks() / (double)m_Freq;
        }

        /// <summary>
        /// Get time in milliseconds, (accurate to 100 nanoseconds).
        /// </summary>
        public double GetMilliseconds()
        {
            return (double)(GetTicks() * (long)1000) / (double)m_Freq;
        }

        /// <summary>
        /// Get the time in 100 nanosecond ticks since the last call to Start().
        /// </summary>
        /// <returns></returns>
        public long GetTicks()
        {
            return GetCounter() - m_Start;
        }

        /// <summary>
        /// Get raw performance counter .
        /// </summary>
        /// <returns></returns>
        static private long GetCounter()
        {
            long i = 0;
            QueryPerformanceCounter(ref i);
            return i;
        }
    }
}