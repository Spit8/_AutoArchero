#include "TemplateStore.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIODevice>
#include <QSaveFile>
#include <QSet>
#include <stdexcept>

TemplateStore::TemplateStore(QString dir)
    : m_dir(std::move(dir))
{
    QDir().mkpath(m_dir);
}

QString TemplateStore::sanitizeName(const QString &name)
{
    QString safe;
    for (QChar c : name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            safe += c;
        else
            safe += QLatin1Char('_');
    }
    if (safe.isEmpty())
        safe = QStringLiteral("template");
    return safe;
}

std::pair<QString, QString> TemplateStore::saveCrop(const QImage &frame, const QString &name, const QRect &box) const
{
    QDir().mkpath(m_dir);
    const QString safe = sanitizeName(name);

    const QRect clipped = box.intersected(frame.rect());
    if (!clipped.isValid() || clipped.width() < 1 || clipped.height() < 1)
        throw std::runtime_error("empty template crop");

    const QImage crop = frame.copy(clipped).convertToFormat(QImage::Format_RGBA8888);
    const QString pngPath = QDir(m_dir).filePath(safe + QStringLiteral(".png"));
    const QString jsonPath = QDir(m_dir).filePath(safe + QStringLiteral(".json"));

    {
        QSaveFile pngFile(pngPath);
        if (!pngFile.open(QIODevice::WriteOnly))
            throw std::runtime_error("cannot open template png for write");
        if (!crop.save(&pngFile, "PNG"))
            throw std::runtime_error("failed to encode template png");
        if (!pngFile.commit())
            throw std::runtime_error("cannot commit template png");
    }
    if (!QFileInfo::exists(pngPath) || QFileInfo(pngPath).size() < 8)
        throw std::runtime_error("template png missing after save");

    QJsonObject o;
    o.insert(QStringLiteral("name"), safe);
    o.insert(QStringLiteral("x"), clipped.x());
    o.insert(QStringLiteral("y"), clipped.y());
    o.insert(QStringLiteral("w"), clipped.width());
    o.insert(QStringLiteral("h"), clipped.height());
    o.insert(QStringLiteral("frame_width"), frame.width());
    o.insert(QStringLiteral("frame_height"), frame.height());
    o.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    QSaveFile f(jsonPath);
    if (!f.open(QIODevice::WriteOnly))
        throw std::runtime_error("cannot write template json");
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!f.commit())
        throw std::runtime_error("cannot commit template json");
    return {pngPath, jsonPath};
}

int TemplateStore::countEntries() const
{
    int n = 0;
    for (const TemplateEntry &e : listEntries()) {
        if (e.hasPng)
            ++n;
    }
    return n;
}

QVector<TemplateEntry> TemplateStore::listEntries() const
{
    QDir d(m_dir);
    QSet<QString> names;
    const QStringList pngs = d.entryList({QStringLiteral("*.png")}, QDir::Files);
    const QStringList jsons = d.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &p : pngs)
        names.insert(QFileInfo(p).completeBaseName());
    for (const QString &j : jsons)
        names.insert(QFileInfo(j).completeBaseName());

    QVector<TemplateEntry> out;
    QStringList sorted = names.values();
    sorted.sort(Qt::CaseInsensitive);
    for (const QString &name : sorted) {
        TemplateEntry e;
        e.name = name;
        e.hasPng = QFileInfo::exists(d.filePath(name + QStringLiteral(".png")));
        e.hasJson = QFileInfo::exists(d.filePath(name + QStringLiteral(".json")));
        out.push_back(e);
    }
    return out;
}
