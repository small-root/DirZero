#pragma once

#include <QString>
#include <QList>
#include <QByteArray>
#include <memory>
#include <mutex>
#include "RemoteEntry.hpp"

// Forward declaration of libssh internal types
struct ssh_session_struct;
typedef struct ssh_session_struct* ssh_session;
struct sftp_session_struct;
typedef struct sftp_session_struct* sftp_session;
struct sftp_file_struct;
typedef struct sftp_file_struct* sftp_file;

class SFTPManager {
public:
    static constexpr int DEFAULT_PORT = 22;
    static constexpr int DEFAULT_TIMEOUT_SECS = 10;
    static constexpr int DEFAULT_CHUNK_SIZE = 65536; // 64 KB

    SFTPManager(const QString& host,
                int port = DEFAULT_PORT,
                const QString& username = QString(),
                const QString& password = QString(),
                const QString& keyPath = QString(),
                int timeoutSecs = DEFAULT_TIMEOUT_SECS);

    ~SFTPManager();

    // Non-copyable
    SFTPManager(const SFTPManager&) = delete;
    SFTPManager& operator=(const SFTPManager&) = delete;

    QString host() const { return m_host; }
    void setHost(const QString& host) { m_host = host; }

    int port() const { return m_port; }
    void setPort(int port) { m_port = port; }

    QString username() const { return m_username; }
    void setUsername(const QString& username) { m_username = username; }

    QString password() const { return m_password; }
    void setPassword(const QString& password) { m_password = password; }

    QString keyPath() const { return m_keyPath; }
    void setKeyPath(const QString& keyPath) { m_keyPath = keyPath; }

    bool connectSession(QString& errorOut);
    void disconnectSession();
    bool isConnected() const;

    QString getAbsolutePath(const QString& remotePath);

    QList<RemoteEntry> listDirectory(const QString& remotePath, QString& errorOut);
    bool createFile(const QString& remotePath, QString& errorOut);
    bool createDirectory(const QString& remotePath, QString& errorOut);
    bool deleteFile(const QString& remotePath, QString& errorOut);
    bool deleteDirectoryRecursive(const QString& remotePath, QString& errorOut);
    bool renamePath(const QString& oldPath, const QString& newPath, QString& errorOut);

    bool statPath(const QString& remotePath, RemoteEntry& outEntry);
    bool calculateTreeSize(const QString& remotePath, qint64& totalBytes, int& fileCount);

    // Streaming API for transfers
    sftp_file openFileRead(const QString& remotePath);
    sftp_file openFileWrite(const QString& remotePath);
    qint64 readFileChunk(sftp_file file, char* buffer, size_t maxLen);
    qint64 writeFileChunk(sftp_file file, const char* buffer, size_t len);
    void closeFile(sftp_file file);

private:
    QString m_host;
    int m_port;
    QString m_username;
    QString m_password;
    QString m_keyPath;
    int m_timeoutSecs;

    ssh_session m_ssh;
    sftp_session m_sftp;
    mutable std::mutex m_mutex;

    bool authenticate(QString& errorOut);
    QString normalizeRemotePath(const QString& path) const;
};
