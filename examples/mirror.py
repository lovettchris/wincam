import argparse
import ctypes
import os

from common import add_common_args
import cv2

from wincam import DXCamera


def get_argument_parser():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser("Mirror a region of the screen.")
    add_common_args(parser)
    return parser


def parse_handle(hwnd) -> int:
    if hwnd.startswith("0x"):
        return int(hwnd[2:], 16)
    return int(hwnd)


def main():
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
    parser = get_argument_parser()
    args = parser.parse_args()
    print("Press ESCAPE to close the window...")
    if args.debug:
        input(f"debug process {os.getpid()}")

    x, y, w, h = args.x, args.y, args.width, args.height
    if args.hwnd:
        hwnd = parse_handle(args.hwnd)
        from wincam.desktop import DesktopWindow

        desktop = DesktopWindow()
        x, y, w, h = desktop.get_window_client_bounds(hwnd)
    elif args.point:
        x, y = map(int, args.point.split(","))
        from wincam.desktop import DesktopWindow

        desktop = DesktopWindow()
        x, y, w, h = desktop.get_window_under_point((x, y))
    elif args.process:
        pid = int(args.process)
        from wincam.desktop import DesktopWindow

        desktop = DesktopWindow()
        x, y, w, h = desktop.find(pid)

    with DXCamera(x, y, w, h, args.fps) as camera:
        while True:
            frame, timestamp = camera.get_bgr_frame()
            cv2.imshow("frame", frame)
            if cv2.waitKey(1) & 0xFF == 27:
                break


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Error: {e}")
