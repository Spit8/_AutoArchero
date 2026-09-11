#pragma once

#include "Types.h"

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>
#include <utility>

class TemplateStore {
public:
    explicit TemplateStore(QString dir);

    std::pair<QString, QString> saveCrop(const QImage &frame, const QString &name, const QRect &box) const;
    int countEntries() const;
    QVector<TemplateEntry> listEntries() const;
    void remove(const QString &name) const;
    QString dir() const { return m_dir; }

private:
    static QString sanitizeName(const QString &name);
    QString m_dir;
};
