#pragma once

#include "Types.h"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QVector>

class AdbClient;
class OcrBridge;

struct PipelineResult {
    QByteArray pngBytes;
    QImage image;
    QVector<OcrHit> hits;
};

class CapturePipeline : public QObject {
    Q_OBJECT
public:
    CapturePipeline(AdbClient *adb, OcrBridge *ocr, QString roisDir, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    void setRoisDir(const QString &dir) { m_roisDir = dir; }

public slots:
    void requestFrameOcr();

signals:
    void finished(const PipelineResult &result);
    void failed(const QString &message);

private:
    AdbClient *m_adb = nullptr;
    OcrBridge *m_ocr = nullptr;
    QString m_roisDir;
    bool m_busy = false;
};
