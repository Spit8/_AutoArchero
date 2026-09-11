#pragma once

#include "Types.h"

#include <QGraphicsView>
#include <QPixmap>

class QContextMenuEvent;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QWheelEvent;

class ImageViewer : public QGraphicsView {
    Q_OBJECT
public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void setDrawRoiEnabled(bool enabled);
    void setFramePixmap(const QPixmap &pixmap);
    void clearOverlays();
    void showRois(const QVector<Roi> &rois);
    void showOcrHits(const QVector<OcrHit> &hits);
    void showTemplateHits(const QVector<TemplateHit> &hits);

    void zoomIn();
    void zoomOut();
    void resetZoom();

    static QString ocrDisplayName(const QString &roiName);
    static QString formatOcrLabel(const QString &roiName, const QString &text);
    static QString formatTemplateLabel(const QString &name, double conf);

signals:
    void roiDrawn(int x, int y, int w, int h);
    void hitRenameRequested(QRect box, QString currentName, QString currentValue);
    void cursorPosChanged(int x, int y, bool insideFrame);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void applyZoomFactor(qreal factor);
    void tagHitItem(QGraphicsItem *item, const QString &kind, const QRect &box,
                    const QString &name, const QString &value);

    QGraphicsPixmapItem *m_pix = nullptr;
    QList<QGraphicsItem *> m_overlays;
    int m_imgW = 1;
    int m_imgH = 1;
    bool m_drawEnabled = false;
    bool m_drawing = false;
    bool m_userZoomed = false;
    QPointF m_origin;
    QGraphicsRectItem *m_rubber = nullptr;
};
