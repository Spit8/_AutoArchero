from __future__ import annotations

from dataclasses import dataclass
from io import BytesIO
from typing import Optional, Tuple

import cv2
import numpy as np
from PIL import Image
from PySide6.QtGui import QImage, QPixmap


@dataclass
class Frame:
    """Native-resolution frame shared by viewer / OCR / templates."""

    png_bytes: bytes
    bgr: np.ndarray  # HxWx3 uint8
    width: int
    height: int

    @property
    def size(self) -> Tuple[int, int]:
        return self.width, self.height


def png_bytes_to_frame(png_bytes: bytes) -> Frame:
    img = Image.open(BytesIO(png_bytes)).convert("RGB")
    rgb = np.array(img)
    bgr = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
    h, w = bgr.shape[:2]
    return Frame(png_bytes=png_bytes, bgr=bgr, width=w, height=h)


def frame_to_qimage(frame: Frame) -> QImage:
    rgb = cv2.cvtColor(frame.bgr, cv2.COLOR_BGR2RGB)
    h, w, ch = rgb.shape
    bytes_per_line = ch * w
    # Copy so QImage owns independent memory
    buf = np.ascontiguousarray(rgb)
    qimg = QImage(buf.data, w, h, bytes_per_line, QImage.Format.Format_RGB888)
    return qimg.copy()


def frame_to_qpixmap(frame: Frame) -> QPixmap:
    return QPixmap.fromImage(frame_to_qimage(frame))


def crop_bgr(frame: Frame, x: int, y: int, w: int, h: int) -> np.ndarray:
    x0 = max(0, int(x))
    y0 = max(0, int(y))
    x1 = min(frame.width, x0 + int(w))
    y1 = min(frame.height, y0 + int(h))
    if x1 <= x0 or y1 <= y0:
        raise ValueError("empty crop")
    return frame.bgr[y0:y1, x0:x1].copy()
