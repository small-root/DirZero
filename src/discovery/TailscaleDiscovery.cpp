#include "TailscaleDiscovery.hpp"
#include <QProcess>
#include <QTcpSocket>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

QString TailscaleDiscovery::findTailscaleBinary()
{
    QString found = QStandardPaths::findExecutable(QStringLiteral("tailscale"));
    if (!found.isEmpty()) {
        return found;
    }

#if defined(Q_OS_WIN)
    found = QStandardPaths::findExecutable(QStringLiteral("tailscale.exe"));
    if (!found.isEmpty()) {
        return found;
    }
    QStringList candidates = {
        QStringLiteral("C:\\Program Files\\Tailscale\\tailscale.exe"),
        QStringLiteral("C:\\Program Files (x86)\\Tailscale\\tailscale.exe")
    };
#elif defined(Q_OS_MACOS)
    QStringList candidates = {
        QStringLiteral("/Applications/Tailscale.app/Contents/MacOS/Tailscale"),
        QStringLiteral("/usr/local/bin/tailscale"),
        QStringLiteral("/opt/homebrew/bin/tailscale")
    };
#else
    QStringList candidates = {
        QStringLiteral("/usr/bin/tailscale"),
        QStringLiteral("/usr/local/bin/tailscale"),
        QStringLiteral("/bin/tailscale"),
        QStringLiteral("/usr/sbin/tailscale")
    };
#endif

    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}

bool TailscaleDiscovery::checkPort22(const QString& ip, int timeoutMs)
{
    if (ip.isEmpty()) {
        return false;
    }
    QTcpSocket socket;
    socket.connectToHost(ip, DEFAULT_SSH_PORT);
    if (socket.waitForConnected(timeoutMs)) {
        socket.disconnectFromHost();
        return true;
    }
    return false;
}

QList<MachineInfo> TailscaleDiscovery::getOnlineMachines(bool checkSsh)
{
    QList<MachineInfo> machines;
    const QString tsBin = findTailscaleBinary();
    if (tsBin.isEmpty()) {
        return machines;
    }

    QProcess proc;
    proc.start(tsBin, QStringList() << QStringLiteral("status") << QStringLiteral("--json"));
    if (!proc.waitForFinished(6000) || proc.exitCode() != 0) {
        // Fallback: try plain status text
        QProcess textProc;
        textProc.start(tsBin, QStringList() << QStringLiteral("status"));
        if (textProc.waitForFinished(4000) && textProc.exitCode() == 0) {
            QString out = QString::fromUtf8(textProc.readAllStandardOutput());
            QStringList lines = out.split('\n', Qt::SkipEmptyParts);
            for (int idx = 0; idx < lines.size(); ++idx) {
                QString line = lines.at(idx).trimmed();
                if (line.isEmpty()) continue;
                QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    QString ip = parts.at(0);
                    QString name = parts.at(1);
                    bool online = !line.toLower().contains(QStringLiteral("offline"));
                    if (online) {
                        MachineInfo m(name, name, ip, QStringLiteral("linux"), true, false, idx == 0);
                        if (checkSsh) {
                            m.setSshAvailable(checkPort22(ip));
                        }
                        machines.append(m);
                    }
                }
            }
        }
        return machines;
    }

    QByteArray rawJson = proc.readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(rawJson);
    if (!doc.isObject()) {
        return machines;
    }

    QJsonObject root = doc.object();

    // 1. Process "Self" machine
    if (root.contains(QStringLiteral("Self")) && root.value(QStringLiteral("Self")).isObject()) {
        QJsonObject selfObj = root.value(QStringLiteral("Self")).toObject();
        QString name = selfObj.value(QStringLiteral("HostName")).toString(QStringLiteral("Localhost"));
        QString dnsName = selfObj.value(QStringLiteral("DNSName")).toString();
        if (dnsName.endsWith('.')) {
            dnsName.chop(1);
        }
        QString osType = selfObj.value(QStringLiteral("OS")).toString(QStringLiteral("linux"));
        bool online = selfObj.value(QStringLiteral("Online")).toBool(true);

        QString primaryIp;
        QJsonArray ips = selfObj.value(QStringLiteral("TailscaleIPs")).toArray();
        for (const QJsonValue& val : ips) {
            QString ipStr = val.toString();
            if (ipStr.contains('.')) {
                primaryIp = ipStr;
                break;
            }
        }

        if (!primaryIp.isEmpty()) {
            MachineInfo selfInfo(name, dnsName, primaryIp, osType, online, false, true);
            machines.append(selfInfo);
        }
    }

    // 2. Process "Peer" machines
    if (root.contains(QStringLiteral("Peer")) && root.value(QStringLiteral("Peer")).isObject()) {
        QJsonObject peerMap = root.value(QStringLiteral("Peer")).toObject();
        for (auto it = peerMap.begin(); it != peerMap.end(); ++it) {
            if (it.value().isObject()) {
                QJsonObject peer = it.value().toObject();
                bool online = peer.value(QStringLiteral("Online")).toBool(false);
                if (!online) {
                    continue; // Skip offline peers
                }

                QString name = peer.value(QStringLiteral("HostName")).toString(QStringLiteral("Unknown"));
                QString dnsName = peer.value(QStringLiteral("DNSName")).toString();
                if (dnsName.endsWith('.')) {
                    dnsName.chop(1);
                }
                QString osType = peer.value(QStringLiteral("OS")).toString(QStringLiteral("unknown"));

                QString primaryIp;
                QJsonArray ips = peer.value(QStringLiteral("TailscaleIPs")).toArray();
                for (const QJsonValue& val : ips) {
                    QString ipStr = val.toString();
                    if (ipStr.contains('.')) {
                        primaryIp = ipStr;
                        break;
                    }
                }

                if (!primaryIp.isEmpty()) {
                    MachineInfo peerInfo(name, dnsName, primaryIp, osType, true, false, false);
                    machines.append(peerInfo);
                }
            }
        }
    }

    // 3. Probe SSH Port 22 if requested
    if (checkSsh) {
        for (MachineInfo& m : machines) {
            m.setSshAvailable(checkPort22(m.ip()));
        }
    }

    return machines;
}
