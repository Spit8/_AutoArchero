from __future__ import annotations

import json
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional, Tuple

import cv2

from .capture import Frame, crop_bgr


@dataclass
class TemplateMeta:
    name: str
    x: int
    y: int
    w: int
    h: int
    frame_width: int
    frame_height: int
    created_at: str


class TemplateStore:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)

    def save_crop(
        self,
        frame: Frame,
        name: str,
        box: Tuple[int, int, int, int],
    ) -> Tuple[Path, Path]:
        x, y, w, h = box
        crop = crop_bgr(frame, x, y, w, h)
        safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in name).strip("_") or "template"
        png_path = self.root / f"{safe}.png"
        json_path = self.root / f"{safe}.json"
        # Write lossless PNG from native crop (no viewer scaling)
        if not cv2.imwrite(str(png_path), crop):
            raise RuntimeError(f"failed to write {png_path}")
        meta = TemplateMeta(
            name=safe,
            x=int(x),
            y=int(y),
            w=int(crop.shape[1]),
            h=int(crop.shape[0]),
            frame_width=frame.width,
            frame_height=frame.height,
            created_at=datetime.now(timezone.utc).isoformat(),
        )
        json_path.write_text(
            json.dumps(meta.__dict__, indent=2),
            encoding="utf-8",
        )
        return png_path, json_path
