#include "CaptureWorker.h"

#include "AdbClient.h"
#include "OcrBridge.h"

#include <QDir>
#include <QTemporaryFile>
#include <stdexcept>

CaptureWorker::CaptureWorker(QString adbPath,
                             QString pythonExe,
                             QString projectRoot,
                             QString roisDir,
                             QString templatesDir,
                             QObject *parent)
    : QObject(parent)
    , m_adb(std::make_unique<AdbClient>(std::move(adbPath)))
    , m_ocr(std::make_unique<OcrBridge>(std::move(pythonExe), std::move(projectRoot)))
    , m_roisDir(std::move(roisDir))
    , m_templatesDir(std::move(templatesDir))
{
}

CaptureWorker::~CaptureWorker() = default;

void CaptureWorker::runFrameOcr(bool fullScreen)
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

        const auto analyzed = m_ocr->runAnalyze(tmp.fileName(), m_roisDir, m_templatesDir, fullScreen);
        result.hits = analyzed.ocr;
        result.templateHits = analyzed.templates;
        result.warning = analyzed.warning;
        emit finished(result);
    } catch (const std::exception &ex) {
        emit failed(QString::fromUtf8(ex.what()));
    } catch (...) {
        emit failed(QStringLiteral("unknown capture/OCR error"));
    }
    m_busy = false;
}
