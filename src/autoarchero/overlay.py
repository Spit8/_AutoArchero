from __future__ import annotations

from typing import List, Optional, Tuple

from PySide6.QtCore import QPointF, QRectF, Qt, Signal
from PySide6.QtGui import QColor, QFont, QPen, QPixmap
from PySide6.QtWidgets import (
    QGraphicsPixmapItem,
    QGraphicsRectItem,
    QGraphicsScene,
    QGraphicsSimpleTextItem,
    QGraphicsView,
)

from .ocr_engine import OcrHit, Roi


class ImageViewer(QGraphicsView):
    """Shows native frame scaled for display; maps clicks/drags back to image coords."""

    roi_drawn = Signal(int, int, int, int)  # x,y,w,h in image space

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setScene(QGraphicsScene(self))
        self.setRenderHints(self.renderHints())
        self.setDragMode(QGraphicsView.DragMode.NoDrag)
        self.setTransformationAnchor(QGraphicsView.ViewportAnchor.AnchorUnderMouse)
        self._pix_item: Optional[QGraphicsPixmapItem] = None
        self._overlay_items: List[object] = []
        self._img_w = 1
        self._img_h = 1
        self._drawing = False
        self._draw_enabled = False
        self._origin = QPointF()
        self._rubber: Optional[QGraphicsRectItem] = None

    def set_draw_roi_enabled(self, enabled: bool) -> None:
        self._draw_enabled = enabled

    def set_frame_pixmap(self, pixmap: QPixmap) -> None:
        self._img_w = max(1, pixmap.width())
        self._img_h = max(1, pixmap.height())
        if self._pix_item is None:
            self._pix_item = self.scene().addPixmap(pixmap)
        else:
            self._pix_item.setPixmap(pixmap)
        self._pix_item.setZValue(0)
        self.setSceneRect(QRectF(0, 0, self._img_w, self._img_h))
        self.fitInView(self._pix_item, Qt.AspectRatioMode.KeepAspectRatio)

    def clear_overlays(self) -> None:
        for item in self._overlay_items:
            self.scene().removeItem(item)  # type: ignore[arg-type]
        self._overlay_items.clear()

    def show_rois(self, rois: List[Roi]) -> None:
        pen = QPen(QColor(0, 180, 255, 200))
        pen.setWidth(2)
        for roi in rois:
            if not roi.enabled:
                continue
            rect = QGraphicsRectItem(QRectF(roi.x, roi.y, roi.w, roi.h))
            rect.setPen(pen)
            rect.setBrush(QColor(0, 180, 255, 30))
            rect.setZValue(10)
            self.scene().addItem(rect)
            label = QGraphicsSimpleTextItem(roi.name)
            label.setBrush(QColor(0, 200, 255))
            label.setFont(QFont("Segoe UI", 10))
            label.setPos(roi.x + 4, roi.y + 4)
            label.setZValue(11)
            self.scene().addItem(label)
            self._overlay_items.extend([rect, label])

    def show_ocr_hits(self, hits: List[OcrHit]) -> None:
        pen = QPen(QColor(50, 220, 80, 220))
        pen.setWidth(2)
        for hit in hits:
            x, y, w, h = hit.box
            rect = QGraphicsRectItem(QRectF(x, y, w, h))
            rect.setPen(pen)
            rect.setBrush(QColor(50, 220, 80, 40))
            rect.setZValue(20)
            self.scene().addItem(rect)
            text = f"{hit.text} ({hit.conf:.2f})"
            label = QGraphicsSimpleTextItem(text)
            label.setBrush(QColor(80, 255, 120))
            label.setFont(QFont("Segoe UI", 9))
            label.setPos(x, max(0, y - 16))
            label.setZValue(21)
            self.scene().addItem(label)
            self._overlay_items.extend([rect, label])

    def map_to_image(self, view_pos) -> Tuple[int, int]:
        scene_pos = self.mapToScene(view_pos)
        x = int(max(0, min(self._img_w - 1, scene_pos.x())))
        y = int(max(0, min(self._img_h - 1, scene_pos.y())))
        return x, y

    def mousePressEvent(self, event) -> None:  # noqa: N802
        if self._draw_enabled and event.button() == Qt.MouseButton.LeftButton:
            self._drawing = True
            self._origin = self.mapToScene(event.position().toPoint())
            if self._rubber:
                self.scene().removeItem(self._rubber)
            self._rubber = QGraphicsRectItem(QRectF(self._origin, self._origin))
            self._rubber.setPen(QPen(QColor(255, 200, 0), 2, Qt.PenStyle.DashLine))
            self._rubber.setZValue(50)
            self.scene().addItem(self._rubber)
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event) -> None:  # noqa: N802
        if self._drawing and self._rubber is not None:
            now = self.mapToScene(event.position().toPoint())
            self._rubber.setRect(QRectF(self._origin, now).normalized())
            event.accept()
            return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event) -> None:  # noqa: N802
        if self._drawing and event.button() == Qt.MouseButton.LeftButton:
            self._drawing = False
            if self._rubber is not None:
                r = self._rubber.rect().normalized()
                self.scene().removeItem(self._rubber)
                self._rubber = None
                x = int(max(0, r.x()))
                y = int(max(0, r.y()))
                w = int(min(self._img_w - x, r.width()))
                h = int(min(self._img_h - y, r.height()))
                if w >= 4 and h >= 4:
                    self.roi_drawn.emit(x, y, w, h)
            event.accept()
            return
        super().mouseReleaseEvent(event)

    def resizeEvent(self, event) -> None:  # noqa: N802
        super().resizeEvent(event)
        if self._pix_item is not None:
            self.fitInView(self._pix_item, Qt.AspectRatioMode.KeepAspectRatio)
