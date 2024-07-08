import os
from typing import Tuple

import win32gui


class DesktopWindow:
    """This is a helper class that can be used to find window bounds."""

    def __init__(self):
        self.pid = 0
        self.handles = []
        if os.name != "nt":
            raise Exception("This class only works on Windows")

        self.camera = None

    def find(self, pid: int) -> Tuple[int, int, int, int]:
        self.pid = int(pid)
        win32gui.EnumWindows(self._check_window, None)
        hwnd = self._get_visible_window()
        if hwnd == -1:
            raise Exception(f"Visible window not found for process {pid}")

        return self.get_window_client_bounds(hwnd)

    def get_window_under_point(self, point: Tuple[int, int]):
        hwnd = win32gui.WindowFromPoint(point)
        if not hwnd:
            raise Exception(f"Visible window not found at location {point}")
        return self.get_window_client_bounds(hwnd)

    def get_window_client_bounds(self, hwnd):
        rc = win32gui.GetClientRect(hwnd)
        if rc is None:
            raise Exception(f"Window not found {hwnd}")

        left, top, right, bottom = rc
        w = right - left
        h = bottom - top
        left, top = win32gui.ClientToScreen(hwnd, (left, top))
        return (left, top, w, h)

    def _get_visible_window(self):
        for hwnd in self.handles:
            left, top, right, bottom = self.get_window_client_bounds(hwnd)
            w = right - left
            h = bottom - top
            if w > 0 and h > 0:
                return hwnd
        return -1

    def find_window_by_title(self, title: str) -> int:
        for i in range(len(self.handles)):
            hwnd = self.handles[i]
            text = win32gui.GetWindowText(hwnd)
            if text == title:
                return i
        return -1

    def _check_window(self, hwnd, lparam):
        import win32process

        _, pid = win32process.GetWindowThreadProcessId(hwnd)
        if pid == self.pid:
            self.handles += [hwnd]
