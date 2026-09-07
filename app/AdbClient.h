#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <utility>

class AdbClient {
public:
    explicit AdbClient(QString adbPath = QString());

    void setSerial(const QString &serial);
    QString serial() const { return m_serial; }
    QString adbPath() const { return m_adbPath; }

    QStringList devices() const;
    QString ensureDevice();

    QByteArray screencapPng();
    void tap(int x, int y);
    void swipe(int x1, int y1, int x2, int y2, int durationMs = 300);
    std::pair<int, int> wmSize();

private:
    QByteArray run(const QStringList &args, int timeoutMs = 30000, bool check = true) const;

    QString m_adbPath;
    QString m_serial;
};
