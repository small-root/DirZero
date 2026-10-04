#include "CredentialStore.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QProcessEnvironment>

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#include <unistd.h>
#endif

QString CredentialStore::configDirectory()
{
    QString baseDir;
#if defined(Q_OS_WIN)
    QString appdata = QProcessEnvironment::systemEnvironment().value(QStringLiteral("APPDATA"));
    if (!appdata.isEmpty()) {
        baseDir = appdata + QStringLiteral("/DirZero");
    } else {
        baseDir = QDir::homePath() + QStringLiteral("/DirZero");
    }
#elif defined(Q_OS_MACOS)
    baseDir = QDir::homePath() + QStringLiteral("/Library/Application Support/DirZero");
#else
    QString xdg = QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_CONFIG_HOME"));
    if (!xdg.isEmpty()) {
        baseDir = xdg + QStringLiteral("/dirzero");
    } else {
        baseDir = QDir::homePath() + QStringLiteral("/.config/dirzero");
    }
#endif

    QDir dir(baseDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
#if defined(Q_OS_UNIX)
        chmod(baseDir.toLocal8Bit().constData(), 0700);
#endif
    }
    return baseDir;
}

QString CredentialStore::configFilePath()
{
    return configDirectory() + QStringLiteral("/credentials.json");
}

QMap<QString, Credential> CredentialStore::loadAll()
{
    QMap<QString, Credential> creds;
    const QString filePath = configFilePath();
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return creds;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        return creds;
    }

    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.value().isObject()) {
            QJsonObject entry = it.value().toObject();
            Credential c;
            c.username = entry.value(QStringLiteral("username")).toString();
            c.password = entry.value(QStringLiteral("password")).toString();
            c.keyPath = entry.value(QStringLiteral("key_path")).toString();
            creds.insert(it.key(), c);
        }
    }
    return creds;
}

std::optional<Credential> CredentialStore::get(const QString& hostIp, const QString& hostName)
{
    QMap<QString, Credential> creds = loadAll();
    if (!hostIp.isEmpty() && creds.contains(hostIp)) {
        return creds.value(hostIp);
    }
    if (!hostName.isEmpty() && creds.contains(hostName)) {
        return creds.value(hostName);
    }
    return std::nullopt;
}

void CredentialStore::save(const QString& hostKey, const Credential& credential)
{
    if (hostKey.isEmpty()) {
        return;
    }
    QMap<QString, Credential> creds = loadAll();
    creds.insert(hostKey, credential);

    QJsonObject root;
    for (auto it = creds.begin(); it != creds.end(); ++it) {
        QJsonObject entry;
        entry.insert(QStringLiteral("username"), it.value().username);
        if (!it.value().password.isEmpty()) {
            entry.insert(QStringLiteral("password"), it.value().password);
        }
        if (!it.value().keyPath.isEmpty()) {
            entry.insert(QStringLiteral("key_path"), it.value().keyPath);
        }
        root.insert(it.key(), entry);
    }

    const QString filePath = configFilePath();
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
#if defined(Q_OS_UNIX)
        chmod(filePath.toLocal8Bit().constData(), 0600);
#endif
    }
}

void CredentialStore::save(const QString& hostKey, const QString& username, const QString& password, const QString& keyPath)
{
    Credential c;
    c.username = username;
    c.password = password;
    c.keyPath = keyPath;
    save(hostKey, c);
}

void CredentialStore::remove(const QString& hostKey)
{
    if (hostKey.isEmpty()) {
        return;
    }
    QMap<QString, Credential> creds = loadAll();
    if (!creds.contains(hostKey)) {
        return;
    }
    creds.remove(hostKey);

    QJsonObject root;
    for (auto it = creds.begin(); it != creds.end(); ++it) {
        QJsonObject entry;
        entry.insert(QStringLiteral("username"), it.value().username);
        if (!it.value().password.isEmpty()) {
            entry.insert(QStringLiteral("password"), it.value().password);
        }
        if (!it.value().keyPath.isEmpty()) {
            entry.insert(QStringLiteral("key_path"), it.value().keyPath);
        }
        root.insert(it.key(), entry);
    }

    const QString filePath = configFilePath();
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
#if defined(Q_OS_UNIX)
        chmod(filePath.toLocal8Bit().constData(), 0600);
#endif
    }
}

QStringList CredentialStore::defaultSshKeyFilenames()
{
    return {
        QStringLiteral("id_ed25519"),
        QStringLiteral("id_rsa"),
        QStringLiteral("id_ecdsa"),
        QStringLiteral("id_dsa")
    };
}

QStringList CredentialStore::getDefaultSshKeys()
{
    QStringList keys;
    const QString sshDir = QDir::homePath() + QStringLiteral("/.ssh");
    for (const QString& name : defaultSshKeyFilenames()) {
        QString candidate = sshDir + QStringLiteral("/") + name;
        if (QFile::exists(candidate)) {
            keys.append(candidate);
        }
    }
    return keys;
}

QString CredentialStore::getFirstAvailableSshKey()
{
    QStringList keys = getDefaultSshKeys();
    if (!keys.isEmpty()) {
        return keys.first();
    }
    return QDir::homePath() + QStringLiteral("/.ssh/id_ed25519");
}
