import argparse
import ctypes
import json
import os
import signal
from threading import Thread
from typing import List

from common import add_common_args
import cv2

from wincam import DXCamera, Timer, VideoEncodingQuality, EncodingProperties


def get_argument_parser():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser("Record an mp4 video of the screen.")
    add_common_args(parser)
    parser.add_argument(
        "--output",
        help="Name of video file to save (default 'video.mp4')",
        default="video.mp4",
    )
    parser.add_argument(
        "--seconds_per_video",
        type=int,
        default=0,
        help="Break up the video into multiple files with this many seconds per video",
    )
    parser.add_argument("--native", help="use GPU provided video encoder", action="store_true")
    parser.add_argument("--memory", help="use in memory caching for video encoder", action="store_true")
    return parser


def parse_handle(hwnd) -> int:
    if hwnd.startswith("0x"):
        return int(hwnd[2:], 16)
    return int(hwnd)


class VideoRecorder:
    def __init__(self, output: str = "video.mp4"):
        self._thread: Thread | None = None
        self._monitor: Thread | None = None
        self._stop = False
        self._output = output
        self._video_writer: cv2.VideoWriter | None = None
        signal.signal(signal.SIGINT, self._signal_handler)

    def _signal_handler(self, sig, frame):
        self._stop = True

    def video_thread(self, x: int, y: int, w: int, h: int, fps: int, seconds_per_video: int, native: bool, memory_cache: bool):
        index = 0
        print()
        while not self._stop:
            filename = self._output
            if index > 0:
                filename = os.path.splitext(filename)[0] + f"_{index}.mp4"
            if native:
                self.native_encoder(filename, x, y, w, h, fps, seconds_per_video, index, memory_cache)
            else:
                self.record_video(filename, x, y, w, h, fps, seconds_per_video, index)

            index += 1
            if seconds_per_video == 0:
                break

    def native_encoder(self, filename: str, x: int, y: int, w: int, h: int, fps: int, max_seconds: int, index: int, memory_cache: bool):
        timer = Timer()
        with DXCamera(x, y, w, h, fps=fps) as camera:
            frame, timestamp = camera.get_bgr_frame()
            if index == 0:
                # do a 2 second warm up cycle to ensure video capture is warm
                timer.start()
                while timer.ticks() < 2:
                    frame, timestamp = camera.get_bgr_frame()
            print(f"Recording {filename}...")

            self._monitor = Thread(target=lambda: self.monitor_video(camera, max_seconds))
            self._monitor.start()

            props = EncodingProperties(frame_rate=fps, quality = VideoEncodingQuality.HD720p, seconds=max_seconds,
                                       memory_cache = memory_cache)

            camera.encode_video(filename, props)
            print(f"Encoding at bitrate {props.bit_rate} bps")

            self._monitor.join()
            print("Video saved to", filename)
            ticks = camera.get_video_ticks()
            frames = camera.get_frame_times()
            self.save_video_meta(filename, ticks, frames)
            self.report_steps(self.get_steps(ticks))

    def get_steps(self, ticks: List[float]):
        p = None
        steps: List[float] = []
        for t in ticks:
            if p is None:
                p = t
            else:
                steps.append(t - p)
                p = t
        return steps

    def save_video_meta(self, filename, ticks, frames):
        meta_file = os.path.splitext(filename)[0] + "_meta.json"
        data = {"video_ticks": ticks}
        if frames is not None:
            data["frame_times"] = frames
        with open(meta_file, "w") as f:
            json.dump(data, f)

    def monitor_video(self, camera: DXCamera, max_seconds: int):
        timer = Timer()
        timer.start()
        while timer.ticks() < max_seconds and not self._stop:
            timer.sleep(100)
        camera.stop_encoding()

    def record_video(self, filename: str, x: int, y: int, w: int, h: int, fps: int, max_seconds: int, index: int):
        self._video_writer = cv2.VideoWriter(
            filename,
            cv2.VideoWriter_fourcc(*"mp4v"),  # type: ignore
            fps,
            (w, h),
        )
        steps = []
        timer = Timer()
        ticks = []
        frame_count = 0
        with DXCamera(x, y, w, h, fps=fps) as camera:
            frame, timestamp = camera.get_bgr_frame()
            self._video_writer.write(frame)  # type: ignore
            camera.reset_throttle()

            if index == 0:
                # do a 2 second warm up cycle to ensure video capture is warm
                timer.start()
                while timer.ticks() < 2:
                    frame, timestamp = camera.get_bgr_frame()

            print(f"Recording {filename}...")
            camera.reset_throttle()
            timer.start()
            step_timer = Timer()

            while not self._stop:
                step_timer.start()
                frame, timestamp = camera.get_bgr_frame()
                self._video_writer.write(frame)
                frame_count += 1
                step_time = step_timer.ticks()
                steps.append(step_time)
                ticks.append(timer.ticks())
                if step_time > 1:
                    print(f"frame {frame_count} took {step_time:.3f} seconds??")
                if max_seconds > 0 and timer.ticks() > max_seconds:
                    break

            self._video_writer.release()

        print("Video saved to", filename)
        self.report_steps(steps)
        self.save_video_meta(filename, ticks, None)

        total = timer.ticks()
        avg_fps = frame_count / total
        print(f"Recorded {frame_count} frames at average fps {avg_fps}")

        if index == 0 and avg_fps < fps * 0.9:
            print(f"The video writer could not keep up with the target {fps} fps so the video will play too fast.")
            print("Please try a smaller window or a lower target fps.")

    def report_steps(self, ticks):
        if len(ticks) > 0:
            min_step = min(ticks)
            max_step = max(ticks)
            avg_step = sum(ticks) / len(ticks)
            print(f"frame step times, min: {min_step:.3f}, max: {max_step:.3f}, avg: {avg_step:.3f}")

    def start(self, x: int, y: int, w: int, h: int, fps: int, seconds_per_video: int, native: bool, memory_cache: bool):
        self._thread = Thread(target=lambda: self.video_thread(x, y, w, h, fps, seconds_per_video, native, memory_cache))
        self._thread.start()

    def stop(self):
        self._stop = True
        if self._thread is not None:
            self._thread.join()


def main():
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
    parser = get_argument_parser()
    args = parser.parse_args()
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

    recorder = VideoRecorder(args.output)
    recorder.start(x, y, w, h, args.fps, args.seconds_per_video, args.native, args.memory)
    input("Press ENTER to stop recording...")
    recorder.stop()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Error: {e}")
