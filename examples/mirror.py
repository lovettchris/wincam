import argparse
import os

import cv2

from wincam import DXCamera


def get_argument_parser():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser("Mirror a region of the screen.")
    parser.add_argument(
        "--fps",
        type=int,
        help="Desired frame rate for recording (default 30)",
        default=30,
    )
    parser.add_argument(
        "--x",
        type=int,
        help="Desired X origin of the mirror (default 0)",
        default=0,
    )
    parser.add_argument(
        "--y",
        type=int,
        help="Desired Y origin of the mirror (default 0)",
        default=0,
    )
    parser.add_argument(
        "--width",
        type=int,
        help="Desired width of the mirror (default 1024)",
        default=1024,
    )
    parser.add_argument(
        "--height",
        type=int,
        help="Desired height of the mirror (default 720)",
        default=720,
    )
    parser.add_argument(
        "--hwnd",
        help="Desired window handle to record.  You can find these using the Windows Kit tool named 'inspect'."
        + "Support hex format starting with 0x.",
    )
    parser.add_argument(
        "--point",
        help="Find window under the given point provided as 'x,y'",
    )
    parser.add_argument(
        "--process",
        help="Find window belonging to the given process id.",
    )
    parser.add_argument(
        "--debug",
        help="For debugging only",
        action="store_true",
    )
    return parser


def parse_handle(hwnd) -> int:
    if hwnd.startswith("0x"):
        return int(hwnd[2:], 16)
    return int(hwnd)


def main():
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

    with DXCamera(x, y, w, h) as cam:
        while True:
            frame, timestamp = cam.get_bgr_frame()
            cv2.imshow("frame", frame)
            if cv2.waitKey(1) & 0xFF == 27:
                break


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Error: {e}")
