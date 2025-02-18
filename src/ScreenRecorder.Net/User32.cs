namespace ScreenRecorder.Native;

using System;
using System.Windows;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Interop;

[StructLayout(LayoutKind.Sequential)]
public struct RECT
{
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
}

[StructLayout(LayoutKind.Sequential)]
public struct POINT
{
    public int X;
    public int Y;

    public static implicit operator Point(POINT point)
    {
        return new Point(point.X, point.Y);
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct TITLEBARINFO
{
    public int cbSize;
    public RECT rcTitleBar;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
    public int[] rgstate;
}

[StructLayout(LayoutKind.Sequential)]
public struct INPUT
{
    public SendInputEventType type;
    public MouseKeybdhardwareInputUnion mkhi;
}

[StructLayout(LayoutKind.Explicit)]
public struct MouseKeybdhardwareInputUnion
{
    [FieldOffset(0)]
    public MouseInputData mi;

    [FieldOffset(0)]
    public KEYBDINPUT ki;

    [FieldOffset(0)]
    public HARDWAREINPUT hi;
}

[StructLayout(LayoutKind.Sequential)]
public struct KEYBDINPUT
{
    public ushort wVk;
    public ushort wScan;
    public uint dwFlags;
    public uint time;
    public IntPtr dwExtraInfo;
}

[StructLayout(LayoutKind.Sequential)]
public struct HARDWAREINPUT
{
    public int uMsg;
    public short wParamL;
    public short wParamH;
}

[StructLayout(LayoutKind.Sequential)]
public struct MouseInputData
{
    public int dx;
    public int dy;
    public uint mouseData;
    public MouseEventFlags dwFlags;
    public uint time;
    public IntPtr dwExtraInfo;
}

[Flags]
public enum MouseEventFlags : uint
{
    MOUSEEVENTF_MOVE = 0x0001,
    MOUSEEVENTF_LEFTDOWN = 0x0002,
    MOUSEEVENTF_LEFTUP = 0x0004,
    MOUSEEVENTF_RIGHTDOWN = 0x0008,
    MOUSEEVENTF_RIGHTUP = 0x0010,
    MOUSEEVENTF_MIDDLEDOWN = 0x0020,
    MOUSEEVENTF_MIDDLEUP = 0x0040,
    MOUSEEVENTF_XDOWN = 0x0080,
    MOUSEEVENTF_XUP = 0x0100,
    MOUSEEVENTF_WHEEL = 0x0800,
    MOUSEEVENTF_VIRTUALDESK = 0x4000,
    MOUSEEVENTF_ABSOLUTE = 0x8000
}

public enum SendInputEventType : int
{
    InputMouse,
    InputKeyboard,
    InputHardware
}

[StructLayout(LayoutKind.Sequential)]
unsafe public struct MonitorInfoEx
{
    public int cbSize;
    public RECT rcMonitor;
    public RECT rcWork;
    public int dwFlags;
    public fixed byte deviceName[64];

    public MonitorInfoEx()
    {
        cbSize = Marshal.SizeOf(this);
    }
}

public static class User32
{
    public delegate bool Callback(nint hwnd, nint lParam);

    [DllImport("user32.dll")]
    public static extern nint GetDesktopWindow();
    [DllImport("user32.dll")]
    public static extern nint GetDC(nint hWnd);

    [DllImport("user32.dll")]
    public static extern nint GetWindowDC(nint hWnd);
    [DllImport("user32.dll")]
    public static extern nint GetWindowRect(nint hWnd, ref RECT rect);
    [DllImport("user32.dll")]
    public static extern nint GetClientRect(nint hWnd, ref RECT rect);
    [DllImport("user32.dll")]
    public static extern nint WindowFromPoint(POINT pos);
    [DllImport("user32.dll")]
    public static extern nint ReleaseDC(nint hWnd, nint hDC);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int Width, int Height, bool Repaint);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(nint hWnd, out uint lpdwProcessId);

    [DllImport("user32.Dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool EnumChildWindows(nint parentHandle, Callback callback, nint lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumThreadWindows(int dwThreadId, Callback lpfn, nint lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(nint hwnd);

    [DllImport("user32.dll")]
    public static extern bool IsIconic(nint hwnd);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern int GetClassName(nint hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ClientToScreen(nint hWnd, ref POINT lpPoint);

    [DllImport("user32.dll", ExactSpelling = true, SetLastError = true)]
    public static extern nint GetForegroundWindow();

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(nint hWnd);

    [DllImport("user32.dll")]
    public static extern bool BringWindowToTop(nint hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(nint hWnd, int nCmdShow);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetTitleBarInfo(nint hwnd, ref TITLEBARINFO pti);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowEx(IntPtr parentHandle, IntPtr childAfter, string lclassName, string windowTitle);

    [DllImport("user32", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maxCount);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint nInputs, INPUT pInputs, int cbSize);

    [DllImport("user32.dll")]
    public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

    [DllImport("user32.dll")]
    private static extern bool GetCursorPos(out POINT lpPoint);

    [DllImport("user32.dll")]
    private static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern bool CloseWindow(nint hwnd);

    [DllImport("GDI32.dll")]
    private static extern int GetDeviceCaps(nint hDC, int nIndex);

    [DllImport("User32", CharSet = CharSet.Auto)]
    public static extern int GetWindowThreadProcessId(IntPtr hwnd, out int procId);

    public delegate bool MonitorEnumProc(nint hMonitor, nint hdcMonitor, ref RECT lprcMonitor, nint dwData);

    [DllImport("User32", CharSet = CharSet.Auto)]
    public static extern bool EnumDisplayMonitors(nint hdc, nint lprcClip, MonitorEnumProc lpfnEnum, nint dwData);

    [DllImport("User32", CharSet = CharSet.Auto)]
    public static extern bool GetMonitorInfo(nint hmonitor, ref MonitorInfoEx info);

    [DllImport("user32.dll")]
    static extern int GetDpiForWindow(nint hWnd);

    struct MSG
    {

    }

    [DllImport("user32.dll")]
    static extern bool TranslateMessage(
        ref MSG lpMsg
    );


    public static float GetDisplayScaleFactor(nint windowHandle)
    {
        try
        {
            return GetDpiForWindow(windowHandle) / 96f;
        }
        catch
        {
            // Or fallback to GDI solutions above
            return 1;
        }
    }
    public static Point ConvertFromDeviceIndependentPixels(nint hwnd, Point p)
    {
        var factor = GetDisplayScaleFactor(hwnd);
        return new Point(p.X * factor, p.Y * factor);
    }

    public static Point ConvertToDeviceIndependentPixels(nint hwnd, Point p)
    {
        var factor = GetDisplayScaleFactor(hwnd);
        return new Point(p.X / factor, p.Y / factor);
    }

    public static Point GetScreenCursorPos()
    {
        POINT pt;
        if (GetCursorPos(out pt))
        {
            return new Point(pt.X, pt.Y);
        }
        return new Point(0, 0);
    }

    public static bool GetCursorPos(nint hwnd, out Point lpPoint)
    {
        POINT pt;
        if (GetCursorPos(out pt))
        {
            lpPoint = ConvertToDeviceIndependentPixels(hwnd, new Point(pt.X, pt.Y));
            return true;
        }
        return false;
    }

    public static bool SetCursorPos(nint hwnd, Point pt)
    {
        if (hwnd == nint.Zero)
        {
            return SetCursorPos((int)pt.X, (int)pt.Y);
        }
        Point screenPt = ConvertFromDeviceIndependentPixels(hwnd, pt);
        return SetCursorPos((int)screenPt.X, (int)screenPt.Y);
    }

    public static Point GetCursorPos(nint hwnd)
    {
        Point result = new Point() { X = 0, Y = 0 };
        POINT lpPoint = new POINT() { X = 0, Y = 0 };
        if (GetCursorPos(out lpPoint))
        {
            result = new Point() { X = lpPoint.X, Y = lpPoint.Y };
            if (hwnd != nint.Zero)
            {
                result = ConvertToDeviceIndependentPixels(hwnd, result);
            }
        }
        return result;
    }

    public static bool MoveWindow(nint hwnd, Rect bounds)
    {
        Point topLeft = ConvertFromDeviceIndependentPixels(hwnd, bounds.TopLeft);
        Point bottomRight = ConvertFromDeviceIndependentPixels(hwnd, bounds.BottomRight);
        return MoveWindow(hwnd, (int)topLeft.X, (int)topLeft.Y, (int)(bottomRight.X - topLeft.X), (int)(bottomRight.Y - topLeft.Y), true);
    }

    public static bool MoveWindowInScreenCoordinates(nint hwnd, Rect bounds)
    {
        return MoveWindow(hwnd, (int)bounds.X, (int)bounds.Y, (int)bounds.Width, (int)bounds.Height, true);
    }

    /// <summary>
    /// Return window rect in screen coordinates
    /// </summary>
    public static Rect GetWindowScreenRect(nint hwnd)
    {
        RECT rect = new RECT();
        User32.GetWindowRect(hwnd, ref rect);

        Point topLeft = new Point(rect.Left, rect.Top);
        Point bottomRight = new Point(rect.Right, rect.Bottom);
        return new Rect(topLeft, bottomRight);
    }

    /// <summary>
    /// Return window client rect in screen coordinates
    /// </summary>
    public static Rect GetClientScreenRect(nint hwnd)
    {
        RECT rect = new RECT();
        POINT point = new POINT();
        User32.GetClientRect(hwnd, ref rect);
        bool ret = User32.ClientToScreen(hwnd, ref point);
        var x = point.X;
        var y = point.Y;
        var w = rect.Right - rect.Left;
        var h = rect.Bottom - rect.Top;
        Point topLeft = new Point(x, y);
        Point bottomRight = new Point(x + w, y + h);
        return new Rect(topLeft, bottomRight);
    }

    /// <summary>
    /// Return window rect in scaled device independent coordinates.
    /// </summary>
    public static Rect GetWindowRect(nint hwnd)
    {
        RECT rect = new RECT();
        User32.GetWindowRect(hwnd, ref rect);
        Point topLeft = new Point(rect.Left, rect.Top);
        Point bottomRight = new Point(rect.Right, rect.Bottom);

        return new Rect(ConvertToDeviceIndependentPixels(hwnd, topLeft), ConvertToDeviceIndependentPixels(hwnd, bottomRight));
    }

    /// <summary>
    /// Return client rect in scaled device independent coordinates.
    /// </summary>
    public static Rect GetClientRect(nint hwnd)
    {
        RECT rect = new RECT();
        User32.GetClientRect(hwnd, ref rect);
        Point topLeft = new Point(rect.Left, rect.Top);
        Point bottomRight = new Point(rect.Right, rect.Bottom);

        return new Rect(ConvertToDeviceIndependentPixels(hwnd, topLeft), ConvertToDeviceIndependentPixels(hwnd, bottomRight));
    }


    public static void SetWindowClientSize(nint hWnd, nint width, nint height)
    {
        var clientRect = GetClientRect(hWnd);
        var windowRect = GetWindowRect(hWnd);
        var chromeWidth = windowRect.Width - clientRect.Width;
        var chromeHeight = windowRect.Height - clientRect.Height;

        MoveWindow(
            hWnd, (int)windowRect.Left, (int)windowRect.Top,
            (int)(width + chromeWidth),
            (int)(height + chromeHeight),
            true);
    }

    public static int GetTitleBarHeight(nint hWnd)
    {
        TITLEBARINFO titleBarInfo = new TITLEBARINFO();
        titleBarInfo.cbSize = Marshal.SizeOf(titleBarInfo);
        User32.GetTitleBarInfo(hWnd, ref titleBarInfo);
        int height = (int)((titleBarInfo.rcTitleBar.Bottom - titleBarInfo.rcTitleBar.Top));
        var pos = User32.ConvertToDeviceIndependentPixels(hWnd, new Point(0, height));
        return (int)(pos.Y);
    }


    [DllImport("d2d1")]
    private static extern int D2D1CreateFactory(D2D1_FACTORY_TYPE factoryType, [MarshalAs(UnmanagedType.LPStruct)] Guid riid, nint pFactoryOptions, out ID2D1Factory ppIFactory);

    private enum D2D1_FACTORY_TYPE
    {
        D2D1_FACTORY_TYPE_SINGLE_THREADED = 0,
        D2D1_FACTORY_TYPE_MULTI_THREADED = 1,
    }

    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("06152247-6f50-465a-9245-118bfd3b6007")]
    private interface ID2D1Factory
    {
        int ReloadSystemMetrics();

        [PreserveSig]
        void GetDesktopDpi(out float dpiX, out float dpiY);
    }

    public static void LeftMouseButtonDown()
    {
        INPUT mouseDownInput = new INPUT();
        mouseDownInput.type = SendInputEventType.InputMouse;
        mouseDownInput.mkhi.mi.dwFlags = MouseEventFlags.MOUSEEVENTF_LEFTDOWN;
        SendInput(1, mouseDownInput, Marshal.SizeOf(new INPUT()));
    }


    public static void LeftMouseButtonUp()
    {
        INPUT mouseUpInput = new INPUT();
        mouseUpInput.type = SendInputEventType.InputMouse;
        mouseUpInput.mkhi.mi.dwFlags = MouseEventFlags.MOUSEEVENTF_LEFTUP;
        SendInput(1, mouseUpInput, Marshal.SizeOf(new INPUT()));
    }

    public static void ClickLeftMouseButton()
    {
        LeftMouseButtonDown();
        LeftMouseButtonUp();
    }

    public static void RightMouseButtonDown()
    {
        INPUT mouseDownInput = new INPUT();
        mouseDownInput.type = SendInputEventType.InputMouse;
        mouseDownInput.mkhi.mi.dwFlags = MouseEventFlags.MOUSEEVENTF_RIGHTDOWN;
        SendInput(1, mouseDownInput, Marshal.SizeOf(new INPUT()));
    }

    public static void RightMouseButtonUp()
    {
        INPUT mouseUpInput = new INPUT();
        mouseUpInput.type = SendInputEventType.InputMouse;
        mouseUpInput.mkhi.mi.dwFlags = MouseEventFlags.MOUSEEVENTF_RIGHTUP;
        SendInput(1, mouseUpInput, Marshal.SizeOf(new INPUT()));
    }

    public static void ClickRightMouseButton()
    {
        RightMouseButtonDown();
        RightMouseButtonUp();
    }

    [DllImport("user32.dll")]
    public static extern short GetKeyState(byte virtKey);

    [DllImport("user32.dll")]
    public static extern int ToUnicode(uint virtualKeyCode, uint scanCode,
                                        byte[] keyboardState,
                                        [Out, MarshalAs(UnmanagedType.LPWStr, SizeConst = 64)]
                                        StringBuilder receivingBuffer,
                                        int bufferSize, uint flags);

    public const int KEYEVENTF_KEYUP = 0x002;
    public static int SW_NORMAL = 1;
    public static int SW_MAXIMIZE = 3;
    public static int SW_MINIMIZE = 6;

}
