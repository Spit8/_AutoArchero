#include "AdbClient.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <stdexcept>

namespace {
QString findDefaultAdb()
{
    const QStringList candidates = {
        QStringLiteral("C:/Program Files/BlueStacks_nxt/HD-Adb.exe"),
        QStringLiteral("C:/Program Files/BlueStacks/HD-Adb.exe"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return QStringLiteral("adb");
}
} // namespace

AdbClient::AdbClient(QString adbPath)
    : m_adbPath(adbPath.isEmpty() ? findDefaultAdb() : std::move(adbPath))
{
}

void AdbClient::setSerial(const QString &serial)
{
    m_serial = serial;
}

QByteArray AdbClient::run(const QStringList &args, int timeoutMs, bool check) const
{
    QStringList full = args;
    if (!m_serial.isEmpty())
        full.prepend(m_serial), full.prepend(QStringLiteral("-s"));

    QProcess p;
    p.start(m_adbPath, full);
    if (!p.waitForStarted(5000))
        throw std::runtime_error("cannot start adb: " + m_adbPath.toStdString());
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        throw std::runtime_error("adb timeout");
    }
    if (check && p.exitCode() != 0) {
        const QString err = QString::fromUtf8(p.readAllStandardError());
        throw std::runtime_error(("adb failed: " + err).toStdString());
    }
    return p.readAllStandardOutput();
}

QStringList AdbClient::devices() const
{
    // Temporarily ignore serial for devices listing
    AdbClient tmp(m_adbPath);
    const QByteArray out = tmp.run({QStringLiteral("devices")});
    QStringList result;
    const QString text = QString::fromUtf8(out);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    for (int i = 1; i < lines.size(); ++i) {
        const QStringList parts = lines[i].split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() >= 2 && parts[1] == QLatin1String("device"))
            result.push_back(parts[0]);
    }
    return result;
}

QString AdbClient::ensureDevice()
{
    if (!m_serial.isEmpty())
        return m_serial;
    const QStringList devs = devices();
    if (devs.isEmpty())
        throw std::runtime_error("No ADB device online (start BlueStacks)");
    for (const QString &d : devs) {
        if (d.startsWith(QLatin1String("emulator-"))) {
            m_serial = d;
            return m_serial;
        }
    }
    m_serial = devs.first();
    return m_serial;
}

QByteArray AdbClient::screencapPng()
{
    ensureDevice();
    QByteArray data = run({QStringLiteral("exec-out"), QStringLiteral("screencap"), QStringLiteral("-p")}, 60000);
    if (data.size() < 100)
        throw std::runtime_error("screencap returned empty data");
    static const QByteArray kPng = QByteArray::fromHex("89504e470d0a1a0a");
    if (!data.startsWith(kPng)) {
        data.replace("\r\n", "\n");
        if (!data.startsWith(kPng))
            throw std::runtime_error("screencap did not return a PNG");
    }
    return data;
}

void AdbClient::tap(int x, int y)
{
    ensureDevice();
    run({QStringLiteral("shell"), QStringLiteral("input"), QStringLiteral("tap"),
         QString::number(x), QString::number(y)});
}

void AdbClient::swipe(int x1, int y1, int x2, int y2, int durationMs)
{
    ensureDevice();
    run({QStringLiteral("shell"), QStringLiteral("input"), QStringLiteral("swipe"),
         QString::number(x1), QString::number(y1), QString::number(x2), QString::number(y2),
         QString::number(durationMs)});
}

std::pair<int, int> AdbClient::wmSize()
{
    ensureDevice();
    const QString text = QString::fromUtf8(run({QStringLiteral("shell"), QStringLiteral("wm"), QStringLiteral("size")}));
    const QRegularExpression re(QStringLiteral("(\\d+)x(\\d+)"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        throw std::runtime_error("cannot parse wm size");
    return {m.captured(1).toInt(), m.captured(2).toInt()};
}
