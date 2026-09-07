"""CLI: RapidOCR (+ optional full-screen) and template match → JSON on stdout."""

from __future__ import annotations

import argparse
import json
import sys
import traceback
from pathlib import Path

import cv2
import numpy as np

from autoarchero.ocr_engine import (
    OcrEngine,
    drop_full_hits_inside_rois,
    keep_richest_hit_per_roi,
    load_rois,
    merge_nearby_hits,
)
from autoarchero.template_match import match_templates


def _force_utf8_stdio() -> None:
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
        except Exception:  # noqa: BLE001
            pass


def _write_stdout_utf8(text: str) -> None:
    data = text.encode("utf-8", errors="replace")
    try:
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
    except Exception:  # noqa: BLE001
        sys.stdout.write(text)
        sys.stdout.flush()


def _write_stderr_utf8(text: str) -> None:
    data = (text.rstrip() + "\n").encode("utf-8", errors="replace")
    try:
        sys.stderr.buffer.write(data)
        sys.stderr.buffer.flush()
    except Exception:  # noqa: BLE001
        print(text, file=sys.stderr)


def _log_error(project_hint: Path, message: str) -> None:
    try:
        out_dir = project_hint / "outputs"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "ocr_cli_last_error.txt").write_text(message, encoding="utf-8")
    except OSError:
        pass
    first = message.strip().splitlines()[-1] if message.strip() else "unknown error"
    _write_stderr_utf8(first)


def main() -> int:
    _force_utf8_stdio()

    parser = argparse.ArgumentParser(description="AutoArchero OCR + template CLI")
    parser.add_argument("--image", required=True, help="PNG path")
    parser.add_argument("--rois", required=True, help="ROI json directory")
    parser.add_argument("--templates", default="", help="Template PNG/JSON directory")
    parser.add_argument("--full", action="store_true", help="OCR full frame in addition to ROIs")
    args = parser.parse_args()

    image_path = Path(args.image)
    rois_dir = Path(args.rois)
    project_hint = rois_dir.parent.parent if rois_dir.parent.name == "assets" else Path.cwd()

    warning_parts: list[str] = []

    try:
        if not image_path.is_file():
            _write_stderr_utf8(f"image not found: {image_path}")
            return 2

        bgr = cv2.imdecode(np.fromfile(str(image_path), dtype=np.uint8), cv2.IMREAD_COLOR)
        if bgr is None:
            _write_stderr_utf8("failed to decode image")
            return 3

        from autoarchero.capture import Frame

        ok, png = cv2.imencode(".png", bgr)
        if not ok:
            _write_stderr_utf8("encode failed")
            return 4
        frame = Frame(png_bytes=png.tobytes(), bgr=bgr, width=bgr.shape[1], height=bgr.shape[0])

        engine = OcrEngine(lang="fr")
        if not engine.available:
            _write_stderr_utf8(f"OCR unavailable: {engine.error}")
            return 5

        rois = load_rois(rois_dir)
        hits = []
        try:
            if rois:
                hits.extend(engine.run_rois(frame, rois))
                hits = merge_nearby_hits(hits)
                hits = keep_richest_hit_per_roi(hits)
            if args.full:
                hits.extend(engine.run_full(frame))
                hits = drop_full_hits_inside_rois(hits, rois)
                hits = merge_nearby_hits(hits)
            if not rois and not args.full:
                hits = []
        except Exception as exc:  # noqa: BLE001
            warning_parts.append(f"ocr_failed: {exc}")
            _log_error(project_hint, traceback.format_exc())

        ocr_payload = [
            {
                "text": h.text,
                "conf": h.conf,
                "box": [h.box[0], h.box[1], h.box[2], h.box[3]],
                "roi_name": h.roi_name,
            }
            for h in hits
        ]

        templates_payload: list[dict] = []
        if args.templates:
            templates_dir = Path(args.templates)
            if templates_dir.is_dir():
                try:
                    templates_payload = match_templates(bgr, templates_dir, threshold=0.82)
                except Exception as exc:  # noqa: BLE001
                    warning_parts.append(f"templates_failed: {exc}")
                    _log_error(project_hint, traceback.format_exc())

        payload = {
            "ocr": ocr_payload,
            "templates": templates_payload,
        }
        if warning_parts:
            payload["warning"] = " | ".join(warning_parts)

        _write_stdout_utf8(json.dumps(payload, ensure_ascii=False) + "\n")
        return 0
    except Exception:  # noqa: BLE001
        _log_error(project_hint, traceback.format_exc())
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
