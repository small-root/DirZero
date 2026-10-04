#include "SFTPManager.hpp"
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include "../security/SshKeyDiscovery.hpp"

SFTPManager::SFTPManager(const QString& host,
                         int port,
                         const QString& username,
                         const QString& password,
                         const QString& keyPath,
                         int timeoutSecs)
    : m_host(host)
    , m_port(port)
    , m_username(username)
    , m_password(password)
    , m_keyPath(keyPath)
    , m_timeoutSecs(timeoutSecs)
    , m_ssh(nullptr)
    , m_sftp(nullptr)
{
    if (m_username.isEmpty()) {
        m_username = QDir::home().dirName();
    }
}

SFTPManager::~SFTPManager()
{
    disconnectSession();
}

bool SFTPManager::isConnected() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_ssh != nullptr && m_sftp != nullptr && ssh_is_connected(m_ssh));
}

void SFTPManager::disconnectSession()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sftp) {
        sftp_free(m_sftp);
        m_sftp = nullptr;
    }
    if (m_ssh) {
        ssh_disconnect(m_ssh);
        ssh_free(m_ssh);
        m_ssh = nullptr;
    }
}

bool SFTPManager::authenticate(QString& errorOut)
{
    int auth = SSH_AUTH_DENIED;

    // 1. If explicit key specified, try it first
    if (!m_keyPath.isEmpty() && QFile::exists(m_keyPath)) {
        QByteArray keyBytes = m_keyPath.toUtf8();
        ssh_options_set(m_ssh, SSH_OPTIONS_IDENTITY, keyBytes.constData());
        auth = ssh_userauth_publickey_auto(m_ssh, nullptr, nullptr);
        if (auth == SSH_AUTH_SUCCESS) {
            return true;
        }
    }

    // 2. Try default keys & running SSH agent
    auth = ssh_userauth_publickey_auto(m_ssh, nullptr, nullptr);
    if (auth == SSH_AUTH_SUCCESS) {
        return true;
    }

    // Also check standard key paths explicitly
    for (const QString& stdKey : SshKeyDiscovery::defaultKeyPaths()) {
        QByteArray kBytes = stdKey.toUtf8();
        ssh_options_set(m_ssh, SSH_OPTIONS_IDENTITY, kBytes.constData());
        auth = ssh_userauth_publickey_auto(m_ssh, nullptr, nullptr);
        if (auth == SSH_AUTH_SUCCESS) {
            return true;
        }
    }

    // 3. Password authentication if password provided
    if (!m_password.isEmpty()) {
        QByteArray passBytes = m_password.toUtf8();
        auth = ssh_userauth_password(m_ssh, nullptr, passBytes.constData());
        if (auth == SSH_AUTH_SUCCESS) {
            return true;
        }
    }

    // 4. Try "none" auth method in case server allows it
    auth = ssh_userauth_none(m_ssh, nullptr);
    if (auth == SSH_AUTH_SUCCESS) {
        return true;
    }

    errorOut = QString::fromUtf8(ssh_get_error(m_ssh));
    if (errorOut.isEmpty()) {
        errorOut = QStringLiteral("Authentication failed. Please verify username, password, or SSH key.");
    }
    return false;
}

bool SFTPManager::connectSession(QString& errorOut)
{
    disconnectSession();

    std::lock_guard<std::mutex> lock(m_mutex);

    m_ssh = ssh_new();
    if (!m_ssh) {
        errorOut = QStringLiteral("Failed to allocate SSH session.");
        return false;
    }

    QByteArray hostBytes = m_host.toUtf8();
    QByteArray userBytes = m_username.toUtf8();

    ssh_options_set(m_ssh, SSH_OPTIONS_HOST, hostBytes.constData());
    ssh_options_set(m_ssh, SSH_OPTIONS_PORT, &m_port);
    ssh_options_set(m_ssh, SSH_OPTIONS_USER, userBytes.constData());

    long timeout = m_timeoutSecs;
    ssh_options_set(m_ssh, SSH_OPTIONS_TIMEOUT, &timeout);

    // Disable strict host checking prompt in background mode
    int strict = 0;
    ssh_options_set(m_ssh, SSH_OPTIONS_STRICTHOSTKEYCHECK, &strict);

    int rc = ssh_connect(m_ssh);
    if (rc != SSH_OK) {
        errorOut = QStringLiteral("SSH connect error: %1").arg(QString::fromUtf8(ssh_get_error(m_ssh)));
        ssh_free(m_ssh);
        m_ssh = nullptr;
        return false;
    }

    if (!authenticate(errorOut)) {
        ssh_disconnect(m_ssh);
        ssh_free(m_ssh);
        m_ssh = nullptr;
        return false;
    }

    m_sftp = sftp_new(m_ssh);
    if (!m_sftp) {
        errorOut = QStringLiteral("Failed to allocate SFTP session: %1").arg(QString::fromUtf8(ssh_get_error(m_ssh)));
        ssh_disconnect(m_ssh);
        ssh_free(m_ssh);
        m_ssh = nullptr;
        return false;
    }

    rc = sftp_init(m_sftp);
    if (rc != SSH_OK) {
        errorOut = QStringLiteral("Failed to initialize SFTP session: %1 (code %2)").arg(QString::fromUtf8(ssh_get_error(m_ssh))).arg(sftp_get_error(m_sftp));
        sftp_free(m_sftp);
        m_sftp = nullptr;
        ssh_disconnect(m_ssh);
        ssh_free(m_ssh);
        m_ssh = nullptr;
        return false;
    }

    return true;
}

QString SFTPManager::normalizeRemotePath(const QString& path) const
{
    QString p = path;
    p.replace('\\', '/');
    if (p.isEmpty()) {
        return QStringLiteral(".");
    }
    return p;
}

QString SFTPManager::getAbsolutePath(const QString& remotePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) {
        return remotePath;
    }

    QString norm = normalizeRemotePath(remotePath);
    QByteArray pBytes = norm.toUtf8();
    char* canon = sftp_canonicalize_path(m_sftp, pBytes.constData());
    if (canon) {
        QString res = QString::fromUtf8(canon);
        ssh_string_free_char(canon);
        return res;
    }

    if (norm == QStringLiteral(".")) {
        char* homeCanon = sftp_canonicalize_path(m_sftp, "");
        if (homeCanon) {
            QString res = QString::fromUtf8(homeCanon);
            ssh_string_free_char(homeCanon);
            return res;
        }
    }
    return norm;
}

QList<RemoteEntry> SFTPManager::listDirectory(const QString& remotePath, QString& errorOut)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    QList<RemoteEntry> entries;

    if (!m_sftp) {
        errorOut = QStringLiteral("SFTP session is not connected.");
        return entries;
    }

    QString normPath = normalizeRemotePath(remotePath);
    QByteArray pathBytes = normPath.toUtf8();

    sftp_dir dir = sftp_opendir(m_sftp, pathBytes.constData());
    if (!dir) {
        errorOut = QStringLiteral("Cannot open directory '%1': error %2").arg(normPath).arg(sftp_get_error(m_sftp));
        return entries;
    }

    sftp_attributes attr = nullptr;
    while ((attr = sftp_readdir(m_sftp, dir)) != nullptr) {
        QString name = QString::fromUtf8(attr->name);
        if (name == QStringLiteral(".") || name == QStringLiteral("..")) {
            sftp_attributes_free(attr);
            continue;
        }

        RemoteEntry entry;
        entry.name = name;
        if (normPath == QStringLiteral(".")) {
            entry.path = name;
        } else {
            entry.path = normPath.endsWith('/') ? (normPath + name) : (normPath + '/' + name);
        }

        entry.isDirectory = (attr->type == SSH_FILEXFER_TYPE_DIRECTORY);
        entry.size = entry.isDirectory ? 0 : static_cast<qint64>(attr->size);
        entry.mtime = static_cast<qint64>(attr->mtime);
        entry.mode = attr->permissions;
        entry.hidden = name.startsWith('.');

        entries.append(entry);
        sftp_attributes_free(attr);
    }

    sftp_closedir(dir);

    // Sort: directories first, then alphabetical case-insensitive
    std::sort(entries.begin(), entries.end(), [](const RemoteEntry& a, const RemoteEntry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory;
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return entries;
}

bool SFTPManager::createFile(const QString& remotePath, QString& errorOut)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) {
        errorOut = QStringLiteral("SFTP session is not connected.");
        return false;
    }

    QString normPath = normalizeRemotePath(remotePath);
    QByteArray pBytes = normPath.toUtf8();

    sftp_file f = sftp_open(m_sftp, pBytes.constData(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (!f) {
        errorOut = QStringLiteral("Failed to create file '%1': error %2").arg(normPath).arg(sftp_get_error(m_sftp));
        return false;
    }
    sftp_close(f);
    return true;
}

bool SFTPManager::createDirectory(const QString& remotePath, QString& errorOut)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) {
        errorOut = QStringLiteral("SFTP session is not connected.");
        return false;
    }

    QString normPath = normalizeRemotePath(remotePath);
    QByteArray pBytes = normPath.toUtf8();

    int rc = sftp_mkdir(m_sftp, pBytes.constData(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (rc != SSH_OK) {
        errorOut = QStringLiteral("Failed to create directory '%1': error %2").arg(normPath).arg(sftp_get_error(m_sftp));
        return false;
    }
    return true;
}

bool SFTPManager::deleteFile(const QString& remotePath, QString& errorOut)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) {
        errorOut = QStringLiteral("SFTP session is not connected.");
        return false;
    }

    QString normPath = normalizeRemotePath(remotePath);
    QByteArray pBytes = normPath.toUtf8();

    int rc = sftp_unlink(m_sftp, pBytes.constData());
    if (rc != SSH_OK) {
        errorOut = QStringLiteral("Failed to delete file '%1': error %2").arg(normPath).arg(sftp_get_error(m_sftp));
        return false;
    }
    return true;
}

bool SFTPManager::deleteDirectoryRecursive(const QString& remotePath, QString& errorOut)
{
    // List directory first
    QString listErr;
    QList<RemoteEntry> items = listDirectory(remotePath, listErr);
    for (const RemoteEntry& item : items) {
        if (item.isDirectory) {
            if (!deleteDirectoryRecursive(item.path, errorOut)) {
                return false;
            }
        } else {
            if (!deleteFile(item.path, errorOut)) {
                return false;
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) {
        errorOut = QStringLiteral("SFTP session is not connected.");
        return false;
    }

    QString normPath = normalizeRemotePath(remotePath);
    QByteArray pBytes = normPath.toUtf8();

    int rc = sftp_rmdir(m_sftp, pBytes.constData());
    if (rc != SSH_OK) {
        errorOut = QStringLiteral("Failed to remove directory '%1': error %2").arg(normPath).arg(sftp_get_error(m_sftp));
        return false;
    }
    return true;
}

bool SFTPManager::renamePath(const QString& oldPath, const QString& newPath, QString& errorOut)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) {
        errorOut = QStringLiteral("SFTP session is not connected.");
        return false;
    }

    QString normOld = normalizeRemotePath(oldPath);
    QString normNew = normalizeRemotePath(newPath);

    QByteArray oldBytes = normOld.toUtf8();
    QByteArray newBytes = normNew.toUtf8();

    int rc = sftp_rename(m_sftp, oldBytes.constData(), newBytes.constData());
    if (rc != SSH_OK) {
        errorOut = QStringLiteral("Failed to rename '%1' to '%2': error %3").arg(normOld, normNew).arg(sftp_get_error(m_sftp));
        return false;
    }
    return true;
}

bool SFTPManager::statPath(const QString& remotePath, RemoteEntry& outEntry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) {
        return false;
    }

    QString normPath = normalizeRemotePath(remotePath);
    QByteArray pBytes = normPath.toUtf8();

    sftp_attributes attr = sftp_stat(m_sftp, pBytes.constData());
    if (!attr) {
        return false;
    }

    outEntry.name = QFileInfo(normPath).fileName();
    outEntry.path = normPath;
    outEntry.isDirectory = (attr->type == SSH_FILEXFER_TYPE_DIRECTORY);
    outEntry.size = outEntry.isDirectory ? 0 : static_cast<qint64>(attr->size);
    outEntry.mtime = static_cast<qint64>(attr->mtime);
    outEntry.mode = attr->permissions;
    outEntry.hidden = outEntry.name.startsWith('.');

    sftp_attributes_free(attr);
    return true;
}

bool SFTPManager::calculateTreeSize(const QString& remotePath, qint64& totalBytes, int& fileCount)
{
    RemoteEntry info;
    if (!statPath(remotePath, info)) {
        return false;
    }

    if (!info.isDirectory) {
        totalBytes += info.size;
        fileCount += 1;
        return true;
    }

    QString err;
    QList<RemoteEntry> items = listDirectory(remotePath, err);
    for (const RemoteEntry& item : items) {
        if (item.isDirectory) {
            calculateTreeSize(item.path, totalBytes, fileCount);
        } else {
            totalBytes += item.size;
            fileCount += 1;
        }
    }
    return true;
}

sftp_file SFTPManager::openFileRead(const QString& remotePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) return nullptr;
    QString norm = normalizeRemotePath(remotePath);
    QByteArray bytes = norm.toUtf8();
    return sftp_open(m_sftp, bytes.constData(), O_RDONLY, 0);
}

sftp_file SFTPManager::openFileWrite(const QString& remotePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sftp) return nullptr;
    QString norm = normalizeRemotePath(remotePath);
    QByteArray bytes = norm.toUtf8();
    return sftp_open(m_sftp, bytes.constData(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

qint64 SFTPManager::readFileChunk(sftp_file file, char* buffer, size_t maxLen)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!file) return -1;
    ssize_t bytesRead = sftp_read(file, buffer, maxLen);
    return static_cast<qint64>(bytesRead);
}

qint64 SFTPManager::writeFileChunk(sftp_file file, const char* buffer, size_t len)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!file) return -1;
    ssize_t bytesWritten = sftp_write(file, buffer, len);
    return static_cast<qint64>(bytesWritten);
}

void SFTPManager::closeFile(sftp_file file)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (file) {
        sftp_close(file);
    }
}
