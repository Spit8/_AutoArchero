from __future__ import annotations

from typing import Callable, List, Optional

from PySide6.QtCore import QObject, QTimer, Signal

from .adb_client import AdbClient
from .capture import Frame, png_bytes_to_frame
from .ocr_engine import OcrEngine, OcrHit, Roi, match_hits_to_rois


class PipelineResult:
    def __init__(self, frame: Frame, hits: List[OcrHit]) -> None:
        self.frame = frame
        self.hits = hits


class CaptureOcrPipeline(QObject):
    """Shared capture→OCR path used by Test and Gaming modes."""

    finished = Signal(object)  # PipelineResult
    failed = Signal(str)

    def __init__(
        self,
        adb: AdbClient,
        ocr: OcrEngine,
        rois_provider: Callable[[], List[Roi]],
        parent: Optional[QObject] = None,
    ) -> None:
        super().__init__(parent)
        self.adb = adb
        self.ocr = ocr
        self.rois_provider = rois_provider
        self._busy = False

    @property
    def busy(self) -> bool:
        return self._busy

    def request_frame_ocr(self) -> None:
        if self._busy:
            return
        self._busy = True
        try:
            png = self.adb.screencap_png()
            frame = png_bytes_to_frame(png)
            rois = self.rois_provider()
            hits: List[OcrHit] = []
            if rois:
                raw_full = self.ocr.run_full(frame)
                hits = match_hits_to_rois(raw_full, rois)
            self.finished.emit(PipelineResult(frame, hits))
        except Exception as exc:  # noqa: BLE001
            self.failed.emit(str(exc))
        finally:
            self._busy = False


class TestModeController(QObject):
    """Live detection every interval_ms (default 2000)."""

    def __init__(self, pipeline: CaptureOcrPipeline, interval_ms: int = 2000, parent=None) -> None:
        super().__init__(parent)
        self.pipeline = pipeline
        self.timer = QTimer(self)
        self.timer.setInterval(interval_ms)
        self.timer.timeout.connect(self.pipeline.request_frame_ocr)

    def start(self) -> None:
        if not self.timer.isActive():
            self.pipeline.request_frame_ocr()
            self.timer.start()

    def stop(self) -> None:
        self.timer.stop()

    @property
    def running(self) -> bool:
        return self.timer.isActive()


class GamingModeController(QObject):
    """On-demand OCR only (automate or manual)."""

    def __init__(self, pipeline: CaptureOcrPipeline, parent=None) -> None:
        super().__init__(parent)
        self.pipeline = pipeline
        self.last_request_label = "—"

    def request_frame_ocr(self, reason: str = "manual") -> None:
        self.last_request_label = reason
        self.pipeline.request_frame_ocr()
