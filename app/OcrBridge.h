#pragma once

#include "Types.h"

#include <QObject>
#include <QString>

class OcrBridge : public QObject {
    Q_OBJECT
public:
    struct AnalyzeResult {
        QVector<OcrHit> ocr;
        QVector<TemplateHit> templates;
        QString warning; // soft stderr Traceback / non-fatal issues
    };

    OcrBridge(QString pythonExe, QString projectRoot, QObject *parent = nullptr);

    void setPythonExe(const QString &path) { m_pythonExe = path; }
    void setProjectRoot(const QString &root) { m_projectRoot = root; }
    bool busy() const { return m_busy; }

    AnalyzeResult runAnalyze(const QString &pngPath,
                             const QString &roisDir,
                             const QString &templatesDir,
                             bool fullScreen);

private:
    QString m_pythonExe;
    QString m_projectRoot;
    bool m_busy = false;
};
