#include "OcrBridge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <stdexcept>

OcrBridge::OcrBridge(QString pythonExe, QString projectRoot, QObject *parent)
    : QObject(parent)
    , m_pythonExe(std::move(pythonExe))
    , m_projectRoot(std::move(projectRoot))
{
}

OcrBridge::AnalyzeResult OcrBridge::runAnalyze(const QString &pngPath,
                                               const QString &roisDir,
                                               const QString &templatesDir,
                                               bool fullScreen)
{
    if (m_busy)
        return {};
    m_busy = true;
    AnalyzeResult result;
    try {
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString src = m_projectRoot + QStringLiteral("/src");
        const QString existing = env.value(QStringLiteral("PYTHONPATH"));
        env.insert(QStringLiteral("PYTHONPATH"),
                   existing.isEmpty() ? src : (src + QStringLiteral(";") + existing));
        env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
        p.setProcessEnvironment(env);
        p.setWorkingDirectory(m_projectRoot);

        QStringList args = {
            QStringLiteral("-m"),
            QStringLiteral("autoarchero.ocr_cli"),
            QStringLiteral("--image"),
            pngPath,
            QStringLiteral("--rois"),
            roisDir,
            QStringLiteral("--templates"),
            templatesDir,
        };
        if (fullScreen)
            args << QStringLiteral("--full");

        p.start(m_pythonExe, args);
        if (!p.waitForStarted(10000))
            throw std::runtime_error("cannot start python OCR");
        if (!p.waitForFinished(180000)) {
            p.kill();
            throw std::runtime_error("OCR timeout");
        }
        const QByteArray out = p.readAllStandardOutput();
        const QByteArray err = p.readAllStandardError();
        const QString errText = QString::fromUtf8(err).trimmed();

        if (p.exitCode() != 0) {
            QString msg = errText.isEmpty() ? QString::fromUtf8(out) : errText;
            if (msg.size() > 800)
                msg = msg.right(800);
            throw std::runtime_error(msg.toStdString());
        }

        if (errText.contains(QStringLiteral("Traceback"))
            || errText.contains(QStringLiteral("Error"))) {
            QString soft = errText;
            if (soft.size() > 400)
                soft = soft.right(400);
            result.warning = soft;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(out);
        QJsonArray ocrArr;
        QJsonArray tplArr;
        if (doc.isObject()) {
            const QJsonObject root = doc.object();
            ocrArr = root.value(QStringLiteral("ocr")).toArray();
            tplArr = root.value(QStringLiteral("templates")).toArray();
            const QString warn = root.value(QStringLiteral("warning")).toString();
            if (!warn.isEmpty()) {
                if (!result.warning.isEmpty())
                    result.warning += QStringLiteral(" | ");
                result.warning += warn;
            }
        } else if (doc.isArray()) {
            ocrArr = doc.array();
        } else {
            throw std::runtime_error("OCR JSON invalid");
        }

        for (const QJsonValue &v : ocrArr) {
            const QJsonObject o = v.toObject();
            OcrHit h;
            h.text = o.value(QStringLiteral("text")).toString();
            h.conf = o.value(QStringLiteral("conf")).toDouble();
            h.roiName = o.value(QStringLiteral("roi_name")).toString();
            const QJsonArray box = o.value(QStringLiteral("box")).toArray();
            if (box.size() >= 4) {
                h.box = QRect(box[0].toInt(), box[1].toInt(), box[2].toInt(), box[3].toInt());
            }
            result.ocr.push_back(h);
        }
        for (const QJsonValue &v : tplArr) {
            const QJsonObject o = v.toObject();
            TemplateHit h;
            h.name = o.value(QStringLiteral("name")).toString();
            h.conf = o.value(QStringLiteral("conf")).toDouble();
            const QJsonArray box = o.value(QStringLiteral("box")).toArray();
            if (box.size() >= 4) {
                h.box = QRect(box[0].toInt(), box[1].toInt(), box[2].toInt(), box[3].toInt());
            }
            result.templates.push_back(h);
        }
    } catch (...) {
        m_busy = false;
        throw;
    }
    m_busy = false;
    return result;
}
