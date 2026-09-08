from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

import cv2
import numpy as np

from .capture import Frame, crop_bgr


@dataclass
class Roi:
    name: str
    x: int
    y: int
    w: int
    h: int
    enabled: bool = True

    def clamp(self, frame_w: int, frame_h: int) -> "Roi":
        x = max(0, min(self.x, frame_w - 1))
        y = max(0, min(self.y, frame_h - 1))
        w = max(1, min(self.w, frame_w - x))
        h = max(1, min(self.h, frame_h - y))
        return Roi(self.name, x, y, w, h, self.enabled)


@dataclass
class OcrHit:
    text: str
    conf: float
    box: tuple[int, int, int, int]  # x, y, w, h in full-frame coords
    roi_name: str


def load_rois(rois_dir: Path) -> List[Roi]:
    rois: List[Roi] = []
    if not rois_dir.is_dir():
        return rois
    for path in sorted(rois_dir.glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        rois.append(
            Roi(
                name=str(data.get("name") or path.stem),
                x=int(data["x"]),
                y=int(data["y"]),
                w=int(data["w"]),
                h=int(data["h"]),
                enabled=bool(data.get("enabled", True)),
            )
        )
    return rois


def save_roi(rois_dir: Path, roi: Roi) -> Path:
    rois_dir.mkdir(parents=True, exist_ok=True)
    path = rois_dir / f"{roi.name}.json"
    payload = {
        "name": roi.name,
        "x": roi.x,
        "y": roi.y,
        "w": roi.w,
        "h": roi.h,
        "enabled": roi.enabled,
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return path


def preprocess_roi(bgr: np.ndarray) -> np.ndarray:
    """Upscale + adaptive threshold for game HUD text."""
    if bgr.size == 0:
        return bgr
    scale = 3
    up = cv2.resize(bgr, None, fx=scale, fy=scale, interpolation=cv2.INTER_CUBIC)
    return _adaptive_prep(up)


def _adaptive_prep(bgr: np.ndarray) -> np.ndarray:
    """Same binary prep as full-frame OCR (no upscale)."""
    if bgr.size == 0:
        return bgr
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (3, 3), 0)
    thr = cv2.adaptiveThreshold(
        blur, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY, 31, 8
    )
    return cv2.cvtColor(thr, cv2.COLOR_GRAY2BGR)


def _box_iou(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> float:
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


# Max pixel gap for multiline / same-line fragment merge (HUD text).
_MAX_GAP_PX = 16


def _normalize_roi_name(name: str) -> str:
    if not name or name in ("full", "?"):
        return "full"
    return name


def _same_merge_group(a: str, b: str) -> bool:
    """Only merge hits that share a ROI name, or are both untagged full/?."""
    return _normalize_roi_name(a) == _normalize_roi_name(b)


def _boxes_nearby(
    a: tuple[int, int, int, int],
    b: tuple[int, int, int, int],
    max_gap_px: int = _MAX_GAP_PX,
) -> bool:
    """True if boxes overlap lightly or sit on adjacent text lines (≤ max_gap_px)."""
    if _box_iou(a, b) > 0.05:
        return True
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    # vertical gap between boxes
    if ay + ah <= by:
        vgap = by - (ay + ah)
    elif by + bh <= ay:
        vgap = ay - (by + bh)
    else:
        vgap = 0
    max_h = max(ah, bh)
    if vgap > min(0.6 * max_h, max_gap_px):
        return False
    # horizontal: overlap OR small gap on the same line
    x0 = max(ax, bx)
    x1 = min(ax + aw, bx + bw)
    hoverlap = max(0, x1 - x0)
    min_w = max(1, min(aw, bw))
    if (hoverlap / min_w) >= 0.30:
        return True
    if ax + aw <= bx:
        hgap = bx - (ax + aw)
    elif bx + bw <= ax:
        hgap = ax - (bx + bw)
    else:
        hgap = 0
    return hgap <= max_gap_px and vgap <= max_gap_px


def _pick_roi_name(names: List[str]) -> str:
    for n in names:
        if n and n not in ("full", "?"):
            return n
    return names[0] if names else "full"


def _join_cluster_text(members: List[OcrHit]) -> str:
    """Rebuild readable Value: space within a line, newline between lines."""
    if not members:
        return ""
    ordered = sorted(members, key=lambda h: (h.box[1] + h.box[3] / 2.0, h.box[0]))
    lines: List[List[OcrHit]] = []
    for m in ordered:
        if not m.text:
            continue
        mcy = m.box[1] + m.box[3] / 2.0
        mh = max(1, m.box[3])
        placed = False
        for line in lines:
            ref = line[0]
            rcy = ref.box[1] + ref.box[3] / 2.0
            thr = 0.5 * max(mh, ref.box[3])
            # same line if centers close or boxes overlap vertically
            my0, my1 = m.box[1], m.box[1] + m.box[3]
            ry0, ry1 = ref.box[1], ref.box[1] + ref.box[3]
            v_overlap = max(0, min(my1, ry1) - max(my0, ry0))
            if abs(mcy - rcy) <= thr or v_overlap >= 0.35 * min(mh, ref.box[3]):
                line.append(m)
                placed = True
                break
        if not placed:
            lines.append([m])
    line_texts: List[str] = []
    for line in lines:
        line.sort(key=lambda h: h.box[0])
        line_texts.append(" ".join(h.text.strip() for h in line if h.text.strip()))
    return "\n".join(t for t in line_texts if t)


def merge_nearby_hits(hits: List[OcrHit]) -> List[OcrHit]:
    """Cluster nearby/overlapping OCR boxes into multi-line readings."""
    if len(hits) <= 1:
        return hits
    n = len(hits)
    parent = list(range(n))

    def find(i: int) -> int:
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    def union(i: int, j: int) -> None:
        ri, rj = find(i), find(j)
        if ri != rj:
            parent[rj] = ri

    for i in range(n):
        for j in range(i + 1, n):
            if not _same_merge_group(hits[i].roi_name, hits[j].roi_name):
                continue
            if _boxes_nearby(hits[i].box, hits[j].box):
                union(i, j)

    clusters: dict[int, List[int]] = {}
    for i in range(n):
        clusters.setdefault(find(i), []).append(i)

    merged: List[OcrHit] = []
    for idxs in clusters.values():
        members = [hits[i] for i in idxs]
        text = _join_cluster_text(members)
        xs = [m.box[0] for m in members]
        ys = [m.box[1] for m in members]
        x2 = [m.box[0] + m.box[2] for m in members]
        y2 = [m.box[1] + m.box[3] for m in members]
        x0, y0 = min(xs), min(ys)
        box = (x0, y0, max(1, max(x2) - x0), max(1, max(y2) - y0))
        conf = max(m.conf for m in members)
        name = _pick_roi_name([m.roi_name for m in members])
        merged.append(OcrHit(text=text, conf=conf, box=box, roi_name=name))
    return merged


def _char_count(text: str) -> int:
    """Nombre de caractères hors espaces / sauts."""
    return len("".join(text.split()))


def keep_richest_hit_per_roi(hits: List[OcrHit]) -> List[OcrHit]:
    """Une seule lecture par ROI nommée : celle avec le plus de caractères.

    Hits full / ? inchangés. À égalité de longueur → conf max, puis premier.
    """
    named: dict[str, List[OcrHit]] = {}
    other: List[OcrHit] = []
    for h in hits:
        if h.roi_name and h.roi_name not in ("full", "?", ""):
            named.setdefault(h.roi_name, []).append(h)
        else:
            other.append(h)
    out: List[OcrHit] = list(other)
    for group in named.values():
        if len(group) == 1:
            out.append(group[0])
            continue
        winner = max(group, key=lambda h: (_char_count(h.text), h.conf))
        out.append(winner)
    return out


# IoU min + fraction de la BB OCR qui doit etre dans la ROI (anti hors-scope).
_MIN_MATCH_IOU = 0.15
_MIN_HIT_COVERAGE = 0.45


def _intersection_box(
    a: tuple[int, int, int, int], b: tuple[int, int, int, int]
) -> Optional[tuple[int, int, int, int]]:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    x0 = max(ax, bx)
    y0 = max(ay, by)
    x1 = min(ax + aw, bx + bw)
    y1 = min(ay + ah, by + bh)
    if x1 <= x0 or y1 <= y0:
        return None
    return (x0, y0, x1 - x0, y1 - y0)


def _hit_coverage_in_roi(h: OcrHit, r: Roi) -> float:
    """Fraction de l'aire de la BB OCR a l'interieur de la ROI (0..1)."""
    inter = _intersection_box(h.box, (r.x, r.y, r.w, r.h))
    if inter is None:
        return 0.0
    hit_area = float(max(1, h.box[2] * h.box[3]))
    return float(inter[2] * inter[3]) / hit_area


def _hit_center(h: OcrHit) -> tuple[int, int]:
    return h.box[0] + h.box[2] // 2, h.box[1] + h.box[3] // 2


def _point_in_roi(cx: int, cy: int, r: Roi) -> bool:
    return r.x <= cx < r.x + r.w and r.y <= cy < r.y + r.h


def _hit_eligible_for_roi(h: OcrHit, r: Roi) -> bool:
    """Hit full utilisable pour la zone (pas de re-OCR crop)."""
    roi_box = (r.x, r.y, r.w, r.h)
    if _box_iou(h.box, roi_box) < _MIN_MATCH_IOU:
        return False
    # Centre dans le guide, ou majorite de la BB dedans (ROI un peu plus basse que la BB).
    cx, cy = _hit_center(h)
    if _point_in_roi(cx, cy, r):
        return True
    return _hit_coverage_in_roi(h, r) >= _MIN_HIT_COVERAGE


def match_hits_to_rois(full_hits: List[OcrHit], rois: List[Roi]) -> List[OcrHit]:
    """Value = texte des hits plein ecran dans la zone (qualite full, pas de crop).

    Attend des hits non fusionnes. Par ROI : hits eligibles → merge_nearby local →
    cluster au meilleur IoU. Box clippee au guide.
    """
    enabled = [r for r in rois if r.enabled]
    if not enabled or not full_hits:
        return []
    out: List[OcrHit] = []
    for r in enabled:
        roi_box = (r.x, r.y, r.w, r.h)
        members = [
            OcrHit(text=h.text, conf=h.conf, box=h.box, roi_name=r.name)
            for h in full_hits
            if _hit_eligible_for_roi(h, r)
        ]
        if not members:
            continue
        clusters = merge_nearby_hits(members)
        winner = max(
            clusters,
            key=lambda h: (_box_iou(h.box, roi_box), _char_count(h.text), h.conf),
        )
        clipped = _intersection_box(winner.box, roi_box) or winner.box
        out.append(
            OcrHit(text=winner.text, conf=winner.conf, box=clipped, roi_name=r.name)
        )
    return out


def outside_full_hits_as_question(full_hits: List[OcrHit], rois: List[Roi]) -> List[OcrHit]:
    """Hits full hors des guides ROI (IoU < seuil) -> roi_name '?'."""
    enabled = [r for r in rois if r.enabled]
    out: List[OcrHit] = []
    for h in full_hits:
        if enabled and any(
            _box_iou(h.box, (r.x, r.y, r.w, r.h)) >= _MIN_MATCH_IOU for r in enabled
        ):
            continue
        out.append(OcrHit(text=h.text, conf=h.conf, box=h.box, roi_name="?"))
    return out


def drop_full_hits_inside_rois(hits: List[OcrHit], rois: List[Roi]) -> List[OcrHit]:
    """Garde les hits crop nommes ; jette les hits full/? dont le centre est dans une ROI."""
    enabled = [r for r in rois if r.enabled]
    if not enabled:
        return hits
    out: List[OcrHit] = []
    for h in hits:
        if h.roi_name not in ("full", "?", ""):
            out.append(h)
            continue
        cx = h.box[0] + h.box[2] // 2
        cy = h.box[1] + h.box[3] // 2
        inside = any(
            r.x <= cx < r.x + r.w and r.y <= cy < r.y + r.h for r in enabled
        )
        if not inside:
            out.append(h)
    return out


class OcrEngine:
    """ROI OCR via RapidOCR (ONNX). PaddleOCR 3.x currently breaks on Win+Paddle3."""

    def __init__(self, lang: str = "fr") -> None:
        self.lang = lang
        self._ocr = None
        self._init_error: Optional[str] = None
        self.backend = "rapidocr"

    def _ensure(self) -> None:
        if self._ocr is not None or self._init_error:
            return
        try:
            from rapidocr_onnxruntime import RapidOCR

            self._ocr = RapidOCR()
        except Exception as exc:  # noqa: BLE001
            self._init_error = str(exc)

    @property
    def available(self) -> bool:
        self._ensure()
        return self._ocr is not None

    @property
    def error(self) -> Optional[str]:
        self._ensure()
        return self._init_error

    def _parse_result(
        self,
        result,
        to_native: float,
        offset_x: int,
        offset_y: int,
        roi_name: str,
    ) -> List[OcrHit]:
        """Map OCR-image coords → native frame: multiply by to_native, then add offset."""
        hits: List[OcrHit] = []
        if not result:
            return hits
        for item in result:
            # item: [box_points, text, score]
            if not item or len(item) < 3:
                continue
            pts, text, conf = item[0], item[1], float(item[2])
            xs = [p[0] * to_native for p in pts]
            ys = [p[1] * to_native for p in pts]
            x0, x1 = int(min(xs)), int(max(xs))
            y0, y1 = int(min(ys)), int(max(ys))
            hits.append(
                OcrHit(
                    text=str(text),
                    conf=conf,
                    box=(offset_x + x0, offset_y + y0, max(1, x1 - x0), max(1, y1 - y0)),
                    roi_name=roi_name,
                )
            )
        return hits

    def run_rois(self, frame: Frame, rois: List[Roi]) -> List[OcrHit]:
        self._ensure()
        if self._ocr is None:
            return []
        hits: List[OcrHit] = []
        # preprocess_roi upscales ×3 → OCR coords must be scaled back by 1/3
        to_native = 1.0 / 3.0
        for roi in rois:
            if not roi.enabled:
                continue
            try:
                r = roi.clamp(frame.width, frame.height)
                crop = crop_bgr(frame, r.x, r.y, r.w, r.h)
                prep = preprocess_roi(crop)
                result, _elapsed = self._ocr(prep)
                hits.extend(self._parse_result(result, to_native, r.x, r.y, r.name))
            except Exception:  # noqa: BLE001
                continue
        return hits

    def read_rois_scoped(self, frame: Frame, rois: List[Roi]) -> List[OcrHit]:
        """OCR chaque guide ROI : Value = texte lu dans le crop uniquement.

        Meme prep que le plein ecran (pas d'upscale x3). Box emise = rectangle du guide.
        """
        self._ensure()
        if self._ocr is None:
            return []
        out: List[OcrHit] = []
        for roi in rois:
            if not roi.enabled:
                continue
            try:
                r = roi.clamp(frame.width, frame.height)
                crop = crop_bgr(frame, r.x, r.y, r.w, r.h)
                if crop.size == 0:
                    continue
                prep = _adaptive_prep(crop)
                result, _elapsed = self._ocr(prep)
                local = self._parse_result(result, 1.0, r.x, r.y, r.name)
                if not local:
                    continue
                text = _join_cluster_text(local)
                if not text:
                    continue
                conf = max(h.conf for h in local)
                out.append(
                    OcrHit(
                        text=text,
                        conf=conf,
                        box=(r.x, r.y, r.w, r.h),
                        roi_name=r.name,
                    )
                )
            except Exception:  # noqa: BLE001
                continue
        return out

    def run_full(self, frame: Frame, max_long_side: int = 1280) -> List[OcrHit]:
        """Full-frame OCR with optional downscale for speed; boxes remapped to native."""
        self._ensure()
        if self._ocr is None:
            return []
        bgr = frame.bgr
        h, w = bgr.shape[:2]
        work = bgr
        long_side = max(h, w)
        if long_side > max_long_side:
            scale = long_side / float(max_long_side)
            nw = max(1, int(round(w / scale)))
            nh = max(1, int(round(h / scale)))
            work = cv2.resize(bgr, (nw, nh), interpolation=cv2.INTER_AREA)
        to_native = float(w) / float(work.shape[1])
        prep = _adaptive_prep(work)
        result, _elapsed = self._ocr(prep)
        return self._parse_result(result, to_native, 0, 0, "full")
