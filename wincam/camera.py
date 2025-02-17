from abc import ABC, abstractmethod
from typing import Tuple

import numpy as np

from wincam.logger import Logger

logger = Logger()
log = logger.get_root_logger()


class Camera(ABC):
    """Abstract class for a camera that captures frames from the screen.
    Some cameras may capture frames natively in the BGR format, while others may capture
    in RGB natively.  So one of the two methods will do a conversion to the format you
    require, but the native method will be the fastest."""

    def __init__(self):
        pass

    @abstractmethod
    def get_rgb_frame(self) -> Tuple[np.ndarray, float]:
        """Returns the next RGB image of the screen and the time most closely
        matching when the frame was captured (in seconds)
        starting at 0 for the first call to get_rgb_frame."""
        pass

    def get_bgr_frame(self) -> Tuple[np.ndarray, float]:
        """Returns the next BGR image of the screen and the time most closely
        matching when the frame was captured (in seconds)
        starting at 0 for the first call to get_rgb_frame."""
        pass

    @abstractmethod
    def stop(self):
        pass
