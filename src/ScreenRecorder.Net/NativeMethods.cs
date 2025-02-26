using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WpfTestApp
{
    static class NativeMethods
    {
        [DllImport("kernel32.dll")]
        public static extern nint LoadLibrary(string dllToLoad);

        [DllImport("kernel32.dll")]
        public static extern nint GetProcAddress(nint hModule, string procedureName);

        [DllImport("kernel32.dll")]
        public static extern bool FreeLibrary(nint hModule);

        [DllImport("ScreenCapture.dll")]
        internal static extern void SleepMicroseconds(Int64 microseconds);
    }
}
