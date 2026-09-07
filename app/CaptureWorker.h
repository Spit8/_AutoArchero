#pragma once

#include "Types.h"

#include <QObject>
#include <QString>
#include <memory>

class AdbClient;
class OcrBridge;

class CaptureWorker : public QObject {
    Q_OBJECT
public:
    CaptureWorker(QString adbPath,
                  QString pythonExe,
                  QString projectRoot,
                  QString roisDir,
                  QString templatesDir,
                  QObject *parent = nullptr);
    ~CaptureWorker() override;

    bool busy() const { return m_busy; }

public slots:
    void runFrameOcr(bool fullScreen);

signals:
    void finished(const PipelineResult &result);
    void failed(const QString &message);

private:
    std::unique_ptr<AdbClient> m_adb;
    std::unique_ptr<OcrBridge> m_ocr;
    QString m_roisDir;
    QString m_templatesDir;
    bool m_busy = false;
};
