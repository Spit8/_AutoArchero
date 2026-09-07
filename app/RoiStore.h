#pragma once

#include "Types.h"

#include <QDir>
#include <QVector>

class RoiStore {
public:
    explicit RoiStore(QString dir);

    QVector<Roi> load() const;
    QString save(const Roi &roi) const;
    QString rename(const QString &oldName, const QString &newName) const;
    void remove(const QString &name) const;
    QString dir() const { return m_dir; }

private:
    static QString sanitizeName(const QString &name);
    QString m_dir;
};
