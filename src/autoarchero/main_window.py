from __future__ import annotations

from pathlib import Path
from typing import List, Optional, Tuple

from PySide6.QtCore import QFile, Qt
from PySide6.QtUiTools import QUiLoader
from PySide6.QtWidgets import QMainWindow, QMessageBox, QVBoxLayout, QWidget

from .adb_client import AdbClient, AdbError
from .capture import Frame, frame_to_qpixmap
from .modes import CaptureOcrPipeline, GamingModeController, PipelineResult, TestModeController
from .ocr_engine import OcrEngine, Roi, load_rois, save_roi
from .overlay import ImageViewer
from .template_store import TemplateStore


def project_root() -> Path:
    # src/autoarchero/main_window.py → parents[2] = repo root
    return Path(__file__).resolve().parents[2]


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.root = project_root()
        self.rois_dir = self.root / "assets" / "rois"
        self.templates_dir = self.root / "assets" / "templates"
        self.ui_path = self.root / "ui" / "mainwindow.ui"

        self._frame: Optional[Frame] = None
        self._last_box: Optional[Tuple[int, int, int, int]] = None
        self._rois: List[Roi] = load_rois(self.rois_dir)

        self.adb = AdbClient()
        self.ocr = OcrEngine(lang="fr")
        self.templates = TemplateStore(self.templates_dir)

        self._load_ui()
        self.viewer = ImageViewer(self.ui.viewerHost)
        layout = self.ui.viewerHostLayout
        if not isinstance(layout, QVBoxLayout):
            layout = QVBoxLayout(self.ui.viewerHost)
            self.ui.viewerHostLayout = layout
        # Clear placeholder layout children if any
        while layout.count():
            item = layout.takeAt(0)
            w = item.widget()
            if w is not None:
                w.deleteLater()
        layout.addWidget(self.viewer)

        self.pipeline = CaptureOcrPipeline(
            adb=self.adb,
            ocr=self.ocr,
            rois_provider=lambda: self._rois,
            parent=self,
        )
        self.pipeline.finished.connect(self._on_pipeline_result)
        self.pipeline.failed.connect(self._on_pipeline_failed)

        self.test_mode = TestModeController(self.pipeline, interval_ms=2000, parent=self)
        self.gaming_mode = GamingModeController(self.pipeline, parent=self)

        self._wire()
        self._update_mode_ui()
        self._set_status("Prêt — lance BlueStacks puis Start live / OCR now")

    def _load_ui(self) -> None:
        from PySide6.QtWidgets import (
            QCheckBox,
            QComboBox,
            QLabel,
            QLineEdit,
            QListWidget,
            QPushButton,
        )

        loader = QUiLoader()
        ui_file = QFile(str(self.ui_path))
        if not ui_file.open(QFile.OpenModeFlag.ReadOnly):
            raise RuntimeError(f"Cannot open UI: {self.ui_path}")
        try:
            loaded = loader.load(ui_file)
        finally:
            ui_file.close()
        if loaded is None or not isinstance(loaded, QMainWindow):
            raise RuntimeError("QUiLoader failed to load mainwindow.ui as QMainWindow")

        self.setWindowTitle(loaded.windowTitle())
        self.resize(loaded.size())
        central = loaded.centralWidget()
        if central is None:
            raise RuntimeError("mainwindow.ui has no central widget")
        central.setParent(self)
        self.setCentralWidget(central)

        class Ns:
            pass

        self.ui = Ns()
        self.ui.comboMode = central.findChild(QComboBox, "comboMode")
        self.ui.btnTestStart = central.findChild(QPushButton, "btnTestStart")
        self.ui.btnTestStop = central.findChild(QPushButton, "btnTestStop")
        self.ui.btnOcrNow = central.findChild(QPushButton, "btnOcrNow")
        self.ui.labelGamingLast = central.findChild(QLabel, "labelGamingLast")
        self.ui.labelStatus = central.findChild(QLabel, "labelStatus")
        self.ui.chkDrawRoi = central.findChild(QCheckBox, "chkDrawRoi")
        self.ui.editRoiName = central.findChild(QLineEdit, "editRoiName")
        self.ui.btnSaveRoi = central.findChild(QPushButton, "btnSaveRoi")
        self.ui.btnSaveTemplate = central.findChild(QPushButton, "btnSaveTemplate")
        self.ui.labelLastBox = central.findChild(QLabel, "labelLastBox")
        self.ui.btnTapCenter = central.findChild(QPushButton, "btnTapCenter")
        self.ui.listHits = central.findChild(QListWidget, "listHits")
        self.ui.viewerHost = central.findChild(QWidget, "viewerHost")
        missing = [
            n
            for n, v in {
                "comboMode": self.ui.comboMode,
                "btnTestStart": self.ui.btnTestStart,
                "btnTestStop": self.ui.btnTestStop,
                "btnOcrNow": self.ui.btnOcrNow,
                "labelGamingLast": self.ui.labelGamingLast,
                "labelStatus": self.ui.labelStatus,
                "chkDrawRoi": self.ui.chkDrawRoi,
                "editRoiName": self.ui.editRoiName,
                "btnSaveRoi": self.ui.btnSaveRoi,
                "btnSaveTemplate": self.ui.btnSaveTemplate,
                "labelLastBox": self.ui.labelLastBox,
                "btnTapCenter": self.ui.btnTapCenter,
                "listHits": self.ui.listHits,
                "viewerHost": self.ui.viewerHost,
            }.items()
            if v is None
        ]
        if missing:
            raise RuntimeError(f"UI widgets missing: {missing}")

        layout = self.ui.viewerHost.layout()
        if layout is None:
            layout = QVBoxLayout(self.ui.viewerHost)
        self.ui.viewerHostLayout = layout

    def _wire(self) -> None:
        self.ui.comboMode.currentIndexChanged.connect(self._on_mode_changed)
        self.ui.btnTestStart.clicked.connect(self._on_test_start)
        self.ui.btnTestStop.clicked.connect(self._on_test_stop)
        self.ui.btnOcrNow.clicked.connect(self._on_ocr_now)
        self.ui.chkDrawRoi.toggled.connect(self.viewer.set_draw_roi_enabled)
        self.viewer.roi_drawn.connect(self._on_roi_drawn)
        self.ui.btnSaveRoi.clicked.connect(self._on_save_roi)
        self.ui.btnSaveTemplate.clicked.connect(self._on_save_template)
        self.ui.btnTapCenter.clicked.connect(self._on_tap_center)

    def _mode_name(self) -> str:
        return self.ui.comboMode.currentText()

    def _update_mode_ui(self) -> None:
        is_test = self._mode_name() == "Test"
        self.ui.btnTestStart.setEnabled(is_test)
        self.ui.btnTestStop.setEnabled(is_test)
        self.ui.btnOcrNow.setEnabled(not is_test)
        if not is_test:
            self.test_mode.stop()

    def _on_mode_changed(self, _index: int) -> None:
        self._update_mode_ui()
        self._set_status(f"Mode {self._mode_name()}")

    def _on_test_start(self) -> None:
        if self._mode_name() != "Test":
            return
        if self.ocr.error and not self.ocr.available:
            QMessageBox.warning(self, "OCR", f"PaddleOCR indisponible:\n{self.ocr.error}")
        self.test_mode.start()
        self._set_status("Test live démarré (2s)")

    def _on_test_stop(self) -> None:
        self.test_mode.stop()
        self._set_status("Test live stoppé")

    def _on_ocr_now(self) -> None:
        self.gaming_mode.request_frame_ocr("manual")
        self.ui.labelGamingLast.setText(f"Dernière demande: {self.gaming_mode.last_request_label}")
        self._set_status("Gaming OCR demandé…")

    def request_frame_ocr(self, reason: str = "automate") -> None:
        """Public hook for future automate (Gaming mode)."""
        if self._mode_name() != "Gaming":
            self.ui.comboMode.setCurrentText("Gaming")
        self.gaming_mode.request_frame_ocr(reason)
        self.ui.labelGamingLast.setText(f"Dernière demande: {reason}")

    def _on_pipeline_result(self, result: PipelineResult) -> None:
        self._frame = result.frame
        pix = frame_to_qpixmap(result.frame)
        self.viewer.set_frame_pixmap(pix)
        self.viewer.clear_overlays()
        self.viewer.show_rois(self._rois)
        self.viewer.show_ocr_hits(result.hits)
        self.ui.listHits.clear()
        for hit in result.hits:
            self.ui.listHits.addItem(f"[{hit.roi_name}] {hit.text}  conf={hit.conf:.2f}")
        self._set_status(
            f"Frame {result.frame.width}x{result.frame.height} — {len(result.hits)} hits"
            + (f" — OCR err: {self.ocr.error}" if self.ocr.error and not result.hits else "")
        )

    def _on_pipeline_failed(self, message: str) -> None:
        self._set_status(f"Erreur: {message}")

    def _on_roi_drawn(self, x: int, y: int, w: int, h: int) -> None:
        self._last_box = (x, y, w, h)
        self.ui.labelLastBox.setText(f"Dernière box: {x},{y} {w}x{h}")

    def _on_save_roi(self) -> None:
        if not self._last_box:
            QMessageBox.information(self, "ROI", "Dessine d'abord une ROI.")
            return
        name = (self.ui.editRoiName.text() or "roi_custom").strip()
        x, y, w, h = self._last_box
        roi = Roi(name=name, x=x, y=y, w=w, h=h, enabled=True)
        path = save_roi(self.rois_dir, roi)
        # replace or append
        self._rois = [r for r in self._rois if r.name != name] + [roi]
        if self._frame is not None:
            self.viewer.clear_overlays()
            self.viewer.show_rois(self._rois)
        self._set_status(f"ROI sauvée: {path.name}")

    def _on_save_template(self) -> None:
        if self._frame is None:
            QMessageBox.information(self, "Template", "Capture une frame d'abord.")
            return
        if not self._last_box:
            QMessageBox.information(self, "Template", "Dessine d'abord une box.")
            return
        name = (self.ui.editRoiName.text() or "template").strip()
        png_path, json_path = self.templates.save_crop(self._frame, name, self._last_box)
        self._set_status(f"Template: {png_path.name} + {json_path.name} (crop natif)")

    def _on_tap_center(self) -> None:
        try:
            if self._frame is not None:
                x, y = self._frame.width // 2, self._frame.height // 2
            else:
                w, h = self.adb.wm_size()
                x, y = w // 2, h // 2
            self.adb.tap(x, y)
            self._set_status(f"Tap fantôme ADB ({x},{y})")
        except AdbError as exc:
            QMessageBox.warning(self, "ADB", str(exc))

    def _set_status(self, text: str) -> None:
        self.ui.labelStatus.setText(f"Status: {text}")
