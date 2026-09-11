"""OpenCV template matching for saved HUD / button crops."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List, Sequence, Tuple

import cv2
import numpy as np


def _box_iou(a: Sequence[int], b: Sequence[int]) -> float:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    x0 = max(ax, bx)
    y0 = max(ay, by)
    x1 = min(ax + aw, bx + bw)
    y1 = min(ay + ah, by + bh)
    inter = max(0, x1 - x0) * max(0, y1 - y0)
    if inter <= 0:
        return 0.0
    union = aw * ah + bw * bh - inter
    return inter / union if union > 0 else 0.0


def _suppress_overlapping_by_conf(
    hits: List[Dict[str, Any]],
    overlap_iou: float,
) -> List[Dict[str, Any]]:
    """Greedy NMS: keep highest-conf hit when boxes overlap (IoU >= threshold)."""
    ordered = sorted(hits, key=lambda h: float(h.get("conf", 0.0)), reverse=True)
    kept: List[Dict[str, Any]] = []
    for hit in ordered:
        box = hit.get("box")
        if not isinstance(box, (list, tuple)) or len(box) != 4:
            kept.append(hit)
            continue
        if any(_box_iou(box, k["box"]) >= overlap_iou for k in kept if "box" in k):
            continue
        kept.append(hit)
    return kept


def match_templates(
    frame_bgr: np.ndarray,
    templates_dir: Path,
    threshold: float = 0.82,
    overlap_iou: float = 0.30,
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
    return _suppress_overlapping_by_conf(hits, overlap_iou=overlap_iou)
