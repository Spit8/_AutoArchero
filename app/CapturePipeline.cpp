#include "CapturePipeline.h"

#include "AdbClient.h"
#include "OcrBridge.h"

#include <QDir>
#include <QTemporaryFile>
#include <stdexcept>

CapturePipeline::CapturePipeline(AdbClient *adb, OcrBridge *ocr, QString roisDir, QObject *parent)
    : QObject(parent)
    , m_adb(adb)
    , m_ocr(ocr)
    , m_roisDir(std::move(roisDir))
{
}

void CapturePipeline::requestFrameOcr()
{
    if (m_busy)
        return;
    m_busy = true;
    try {
        PipelineResult result;
        result.pngBytes = m_adb->screencapPng();
        if (!result.image.loadFromData(result.pngBytes, "PNG"))
            throw std::runtime_error("cannot decode screencap PNG");

        QTemporaryFile tmp(QDir::temp().filePath(QStringLiteral("autoarchero_frame_XXXXXX.png")));
        tmp.setAutoRemove(true);
        if (!tmp.open())
            throw std::runtime_error("cannot create temp png");
        tmp.write(result.pngBytes);
        tmp.flush();

        result.hits = m_ocr->runOcr(tmp.fileName(), m_roisDir);
        emit finished(result);
    } catch (const std::exception &ex) {
        emit failed(QString::fromUtf8(ex.what()));
    } catch (...) {
        emit failed(QStringLiteral("unknown capture/OCR error"));
    }
    m_busy = false;
}
