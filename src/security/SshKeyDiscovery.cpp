#include "SshKeyDiscovery.hpp"

#include <QDir>
#include <QFile>

QStringList SshKeyDiscovery::defaultKeyFilenames()
{
    return {
        QStringLiteral("id_ed25519"),
        QStringLiteral("id_rsa"),
        QStringLiteral("id_ecdsa"),
        QStringLiteral("id_dsa")
    };
}

QStringList SshKeyDiscovery::defaultKeyPaths()
{
    QStringList keys;
    const QString sshDir = QDir::homePath() + QStringLiteral("/.ssh");
    for (const QString& name : defaultKeyFilenames()) {
        const QString candidate = sshDir + QLatin1Char('/') + name;
        if (QFile::exists(candidate)) {
            keys.append(candidate);
        }
    }
    return keys;
}

QString SshKeyDiscovery::firstAvailableKeyPath()
{
    const QStringList keys = defaultKeyPaths();
    return keys.isEmpty() ? QString() : keys.first();
}
