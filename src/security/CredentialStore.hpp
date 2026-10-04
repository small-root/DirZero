#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <optional>

struct Credential {
    QString username;
    QString password;
    QString keyPath;

    bool isValid() const {
        return !username.isEmpty() && (!password.isEmpty() || !keyPath.isEmpty());
    }
};

class CredentialStore {
public:
    static QString configDirectory();
    static QString configFilePath();

    static QMap<QString, Credential> loadAll();
    static std::optional<Credential> get(const QString& hostIp, const QString& hostName = QString());
    static void save(const QString& hostKey, const Credential& credential);
    static void save(const QString& hostKey, const QString& username, const QString& password, const QString& keyPath);
    static void remove(const QString& hostKey);

    static QStringList defaultSshKeyFilenames();
    static QStringList getDefaultSshKeys();
    static QString getFirstAvailableSshKey();
};
