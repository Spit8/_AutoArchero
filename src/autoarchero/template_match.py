"""OpenCV template matching for saved HUD / button crops."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List

import cv2
import numpy as np


def match_templates(
    frame_bgr: np.ndarray,
    templates_dir: Path,
    threshold: float = 0.82,
) -> List[Dict[str, Any]]:
    if frame_bgr is None or frame_bgr.size == 0:
        return []
    if not templates_dir.is_dir():
        return []

    gray = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2GRAY)
    fh, fw = gray.shape[:2]
    hits: List[Dict[str, Any]] = []

    for png_path in sorted(templates_dir.glob("*.png")):
        try:
            tmpl = cv2.imdecode(np.fromfile(str(png_path), dtype=np.uint8), cv2.IMREAD_COLOR)
            if tmpl is None:
                continue
            tgray = cv2.cvtColor(tmpl, cv2.COLOR_BGR2GRAY)
            th, tw = tgray.shape[:2]
            if th < 4 or tw < 4 or th > fh or tw > fw:
                continue

            res = cv2.matchTemplate(gray, tgray, cv2.TM_CCOEFF_NORMED)
            _min_val, max_val, _min_loc, max_loc = cv2.minMaxLoc(res)
            if float(max_val) < threshold:
                continue
            x, y = int(max_loc[0]), int(max_loc[1])
            hits.append(
                {
                    "name": png_path.stem,
                    "conf": float(max_val),
                    "box": [x, y, tw, th],
                }
            )
        except Exception:  # noqa: BLE001
            # Skip corrupt / incompatible template; do not abort whole batch
            continue
    return hits
