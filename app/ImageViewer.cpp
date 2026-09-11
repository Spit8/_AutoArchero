#include "ImageViewer.h"

#include <QContextMenuEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPen>
#include <QResizeEvent>
#include <QWheelEvent>

#include <cmath>

namespace {
constexpr qreal kZoomStep = 1.15;
constexpr qreal kMinScale = 0.1;
constexpr qreal kMaxScale = 20.0;

constexpr int kDataKind = Qt::UserRole;
constexpr int kDataBox = Qt::UserRole + 1;
constexpr int kDataName = Qt::UserRole + 2;
constexpr int kDataValue = Qt::UserRole + 3;
}

ImageViewer::ImageViewer(QWidget *parent)
    : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
}

QString ImageViewer::ocrDisplayName(const QString &roiName)
{
    if (roiName.isEmpty() || roiName == QLatin1String("full") || roiName == QLatin1String("?"))
        return QStringLiteral("?");
    return roiName;
}

QString ImageViewer::formatOcrLabel(const QString &roiName, const QString &text)
{
    return QStringLiteral("%1: %2").arg(ocrDisplayName(roiName), text);
}

QString ImageViewer::formatTemplateLabel(const QString &name, double conf)
{
    return QStringLiteral("%1: %2").arg(name).arg(conf, 0, 'f', 2);
}

void ImageViewer::tagHitItem(QGraphicsItem *item, const QString &kind, const QRect &box,
                             const QString &name, const QString &value)
{
    item->setData(kDataKind, kind);
    item->setData(kDataBox, box);
    item->setData(kDataName, name);
    item->setData(kDataValue, value);
    item->setAcceptedMouseButtons(Qt::RightButton);
}

void ImageViewer::setDrawRoiEnabled(bool enabled)
{
    m_drawEnabled = enabled;
}

void ImageViewer::setFramePixmap(const QPixmap &pixmap)
{
    m_imgW = qMax(1, pixmap.width());
    m_imgH = qMax(1, pixmap.height());
    if (!m_pix) {
        m_pix = scene()->addPixmap(pixmap);
    } else {
        m_pix->setPixmap(pixmap);
    }
    m_pix->setZValue(0);
    setSceneRect(QRectF(0, 0, m_imgW, m_imgH));
    if (!m_userZoomed)
        fitInView(m_pix, Qt::KeepAspectRatio);
}

void ImageViewer::clearOverlays()
{
    for (QGraphicsItem *item : m_overlays)
        scene()->removeItem(item);
    qDeleteAll(m_overlays);
    m_overlays.clear();
}

void ImageViewer::showRois(const QVector<Roi> &rois)
{
    QPen pen(QColor(0, 180, 255, 200));
    pen.setWidth(2);
    for (const Roi &roi : rois) {
        if (!roi.enabled)
            continue;
        const QRect box(roi.x, roi.y, roi.w, roi.h);
        auto *rect = new QGraphicsRectItem(QRectF(box));
        rect->setPen(pen);
        rect->setBrush(QColor(0, 180, 255, 30));
        rect->setZValue(10);
        tagHitItem(rect, QStringLiteral("roi"), box, roi.name, QString());
        scene()->addItem(rect);
        auto *label = new QGraphicsSimpleTextItem(roi.name);
        label->setBrush(QColor(0, 200, 255));
        label->setPos(roi.x + 4, roi.y + 4);
        label->setZValue(11);
        tagHitItem(label, QStringLiteral("roi"), box, roi.name, QString());
        scene()->addItem(label);
        m_overlays << rect << label;
    }
}

void ImageViewer::showOcrHits(const QVector<OcrHit> &hits)
{
    // Named ROI = vert ; inconnue / full = rose flashy
    const QColor knownPen(50, 220, 80, 220);
    const QColor knownBrush(50, 220, 80, 40);
    const QColor knownLabel(80, 255, 120);
    const QColor unkPen(255, 20, 180, 240);
    const QColor unkBrush(255, 40, 200, 55);
    const QColor unkLabel(255, 80, 220);

    for (const OcrHit &hit : hits) {
        const QString name = ocrDisplayName(hit.roiName);
        const bool known = name != QLatin1String("?");
        const QString labelText = formatOcrLabel(hit.roiName, hit.text);
        QPen pen(known ? knownPen : unkPen);
        pen.setWidth(known ? 2 : 3);
        auto *rect = new QGraphicsRectItem(hit.box);
        rect->setPen(pen);
        rect->setBrush(known ? knownBrush : unkBrush);
        rect->setZValue(20);
        tagHitItem(rect, QStringLiteral("ocr"), hit.box, name, hit.text);
        scene()->addItem(rect);
        auto *label = new QGraphicsSimpleTextItem(labelText);
        label->setBrush(known ? knownLabel : unkLabel);
        label->setPos(hit.box.x(), qMax(0, hit.box.y() - 16));
        label->setZValue(21);
        tagHitItem(label, QStringLiteral("ocr"), hit.box, name, hit.text);
        scene()->addItem(label);
        m_overlays << rect << label;
    }
}

void ImageViewer::showTemplateHits(const QVector<TemplateHit> &hits)
{
    QPen pen(QColor(255, 140, 40, 230));
    pen.setWidth(2);
    for (const TemplateHit &hit : hits) {
        const QString labelText = formatTemplateLabel(hit.name, hit.conf);
        auto *rect = new QGraphicsRectItem(hit.box);
        rect->setPen(pen);
        rect->setBrush(QColor(255, 140, 40, 45));
        rect->setZValue(25);
        tagHitItem(rect, QStringLiteral("tpl"), hit.box, hit.name,
                   QString::number(hit.conf, 'f', 2));
        scene()->addItem(rect);
        auto *label = new QGraphicsSimpleTextItem(labelText);
        label->setBrush(QColor(255, 180, 80));
        label->setPos(hit.box.x(), qMax(0, hit.box.y() - 16));
        label->setZValue(26);
        tagHitItem(label, QStringLiteral("tpl"), hit.box, hit.name,
                   QString::number(hit.conf, 'f', 2));
        scene()->addItem(label);
        m_overlays << rect << label;
    }
}

void ImageViewer::zoomIn()
{
    applyZoomFactor(kZoomStep);
}

void ImageViewer::zoomOut()
{
    applyZoomFactor(1.0 / kZoomStep);
}

void ImageViewer::resetZoom()
{
    m_userZoomed = false;
    if (m_pix)
        fitInView(m_pix, Qt::KeepAspectRatio);
}

void ImageViewer::applyZoomFactor(qreal factor)
{
    if (!m_pix)
        return;
    const qreal current = transform().m11();
    const qreal next = current * factor;
    if (next < kMinScale || next > kMaxScale)
        return;
    m_userZoomed = true;
    scale(factor, factor);
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    if (!m_pix) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    applyZoomFactor(delta > 0 ? kZoomStep : (1.0 / kZoomStep));
    event->accept();
}

void ImageViewer::contextMenuEvent(QContextMenuEvent *event)
{
    QGraphicsItem *item = itemAt(event->pos());
    while (item && item->data(kDataKind).toString().isEmpty())
        item = item->parentItem();
    if (!item) {
        event->ignore();
        return;
    }
    const QString kind = item->data(kDataKind).toString();
    if (kind != QLatin1String("ocr") && kind != QLatin1String("roi")) {
        event->ignore();
        return;
    }
    const QRect box = item->data(kDataBox).toRect();
    const QString name = item->data(kDataName).toString();
    const QString value = item->data(kDataValue).toString();
    QMenu menu(this);
    QAction *rename = menu.addAction(QStringLiteral("Renommer…"));
    if (menu.exec(event->globalPos()) == rename)
        emit hitRenameRequested(box, name, value);
    event->accept();
}

void ImageViewer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QMouseEvent fake(event->type(), event->position(), event->globalPosition(),
                         Qt::LeftButton, Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&fake);
        event->accept();
        return;
    }
    if (m_drawEnabled && event->button() == Qt::LeftButton) {
        m_drawing = true;
        m_origin = mapToScene(event->pos());
        if (m_rubber) {
            scene()->removeItem(m_rubber);
            delete m_rubber;
            m_rubber = nullptr;
        }
        m_rubber = new QGraphicsRectItem(QRectF(m_origin, m_origin));
        QPen pen(QColor(255, 200, 0));
        pen.setStyle(Qt::DashLine);
        pen.setWidth(2);
        m_rubber->setPen(pen);
        m_rubber->setZValue(50);
        scene()->addItem(m_rubber);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (dragMode() == QGraphicsView::ScrollHandDrag
        && (event->buttons() & (Qt::MiddleButton | Qt::LeftButton))) {
        QMouseEvent fake(event->type(), event->position(), event->globalPosition(),
                         Qt::LeftButton, Qt::LeftButton, event->modifiers());
        QGraphicsView::mouseMoveEvent(&fake);
        event->accept();
        return;
    }
    if (m_drawing && m_rubber) {
        const QPointF now = mapToScene(event->pos());
        m_rubber->setRect(QRectF(m_origin, now).normalized());
        event->accept();
        // still report coords while drawing
    }

    {
        const QPointF scenePos = mapToScene(event->pos());
        const int x = int(std::floor(scenePos.x()));
        const int y = int(std::floor(scenePos.y()));
        const bool inside = m_pix && x >= 0 && y >= 0 && x < m_imgW && y < m_imgH;
        emit cursorPosChanged(x, y, inside);
    }

    if (m_drawing && m_rubber) {
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        QMouseEvent fake(event->type(), event->position(), event->globalPosition(),
                         Qt::LeftButton, Qt::NoButton, event->modifiers());
        QGraphicsView::mouseReleaseEvent(&fake);
        setDragMode(QGraphicsView::NoDrag);
        event->accept();
        return;
    }
    if (m_drawing && event->button() == Qt::LeftButton) {
        m_drawing = false;
        if (m_rubber) {
            const QRectF r = m_rubber->rect().normalized();
            scene()->removeItem(m_rubber);
            delete m_rubber;
            m_rubber = nullptr;
            const int x = qBound(0, int(r.x()), m_imgW - 1);
            const int y = qBound(0, int(r.y()), m_imgH - 1);
            const int w = qBound(1, int(r.width()), m_imgW - x);
            const int h = qBound(1, int(r.height()), m_imgH - y);
            if (w >= 4 && h >= 4)
                emit roiDrawn(x, y, w, h);
        }
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_pix && !m_userZoomed)
        fitInView(m_pix, Qt::KeepAspectRatio);
}
