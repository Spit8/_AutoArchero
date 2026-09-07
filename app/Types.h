#pragma once

#include <QByteArray>
#include <QImage>
#include <QMetaType>
#include <QRect>
#include <QString>
#include <QVector>

struct Roi {
    QString name;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool enabled = true;

    QRect rect() const { return QRect(x, y, w, h); }
};

struct OcrHit {
    QString text;
    double conf = 0.0;
    QRect box; // full-frame coords
    QString roiName;
};

struct TemplateHit {
    QString name;
    double conf = 0.0;
    QRect box;
};

struct TemplateEntry {
    QString name;
    bool hasPng = false;
    bool hasJson = false;
};

struct PipelineResult {
    QByteArray pngBytes;
    QImage image;
    QVector<OcrHit> hits;
    QVector<TemplateHit> templateHits;
    QString warning;
};

Q_DECLARE_METATYPE(PipelineResult)
