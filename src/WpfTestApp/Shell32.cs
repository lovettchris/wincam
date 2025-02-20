using System.Runtime.InteropServices;

namespace WpfTestApp
{
    public class Shell32
    {
        [DllImport("Shell32.dll", EntryPoint = "ShellExecuteA",
             SetLastError = true, CharSet = CharSet.Ansi, ExactSpelling = true,
             CallingConvention = CallingConvention.StdCall)]
        public static extern int ShellExecute(nint handle, string verb, string file,
            string args, string dir, int show);

        public static void OpenUrl(nint hwnd, string url)
        {
            const int SW_SHOWNORMAL = 1;
            var cwd = System.IO.Directory.GetCurrentDirectory();
            ShellExecute(hwnd, "open", url, null, cwd, SW_SHOWNORMAL);
        }

    }
}
