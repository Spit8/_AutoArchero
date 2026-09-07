#include "RoiStore.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <stdexcept>

RoiStore::RoiStore(QString dir)
    : m_dir(std::move(dir))
{
    QDir().mkpath(m_dir);
}

QString RoiStore::sanitizeName(const QString &name)
{
    QString safe;
    for (QChar c : name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            safe += c;
        else
            safe += QLatin1Char('_');
    }
    if (safe.isEmpty())
        safe = QStringLiteral("roi");
    return safe;
}

QVector<Roi> RoiStore::load() const
{
    QVector<Roi> out;
    QDir d(m_dir);
    const QFileInfoList files = d.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : files) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        Roi r;
        r.name = o.value(QStringLiteral("name")).toString(fi.baseName());
        r.x = o.value(QStringLiteral("x")).toInt();
        r.y = o.value(QStringLiteral("y")).toInt();
        r.w = o.value(QStringLiteral("w")).toInt();
        r.h = o.value(QStringLiteral("h")).toInt();
        r.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        out.push_back(r);
    }
    return out;
}

QString RoiStore::save(const Roi &roi) const
{
    QDir().mkpath(m_dir);
    Roi r = roi;
    r.name = sanitizeName(r.name);
    const QString path = QDir(m_dir).filePath(r.name + QStringLiteral(".json"));
    QJsonObject o;
    o.insert(QStringLiteral("name"), r.name);
    o.insert(QStringLiteral("x"), r.x);
    o.insert(QStringLiteral("y"), r.y);
    o.insert(QStringLiteral("w"), r.w);
    o.insert(QStringLiteral("h"), r.h);
    o.insert(QStringLiteral("enabled"), r.enabled);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        throw std::runtime_error("cannot write ROI json");
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!f.commit())
        throw std::runtime_error("cannot commit ROI json");
    return path;
}

QString RoiStore::rename(const QString &oldName, const QString &newName) const
{
    const QString oldSafe = sanitizeName(oldName);
    const QString newSafe = sanitizeName(newName);
    if (oldSafe == newSafe)
        return QDir(m_dir).filePath(oldSafe + QStringLiteral(".json"));

    const QString oldPath = QDir(m_dir).filePath(oldSafe + QStringLiteral(".json"));
    QFile f(oldPath);
    if (!f.open(QIODevice::ReadOnly))
        throw std::runtime_error("ROI not found: " + oldSafe.toStdString());
    QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    Roi r;
    r.name = newSafe;
    r.x = o.value(QStringLiteral("x")).toInt();
    r.y = o.value(QStringLiteral("y")).toInt();
    r.w = o.value(QStringLiteral("w")).toInt();
    r.h = o.value(QStringLiteral("h")).toInt();
    r.enabled = o.value(QStringLiteral("enabled")).toBool(true);
    const QString newPath = save(r);
    QFile::remove(oldPath);
    return newPath;
}

void RoiStore::remove(const QString &name) const
{
    const QString safe = sanitizeName(name);
    const QString path = QDir(m_dir).filePath(safe + QStringLiteral(".json"));
    if (!QFileInfo::exists(path))
        throw std::runtime_error("ROI not found: " + safe.toStdString());
    if (!QFile::remove(path))
        throw std::runtime_error("cannot delete ROI: " + safe.toStdString());
}
