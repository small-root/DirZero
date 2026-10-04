#include "CrossMachineTransferWorker.hpp"
#include "../fs/SFTPManager.hpp"
#include <QFileInfo>
#include <QDir>
#include <vector>

CrossMachineTransferWorker::CrossMachineTransferWorker(SFTPManager* srcManager,
                                                       SFTPManager* dstManager,
                                                       const QString& srcPath,
                                                       const QString& dstPath,
                                                       bool isDir,
                                                       bool isCut,
                                                       const QString& srcHostName,
                                                       const QString& dstHostName)
    : m_srcManager(srcManager)
    , m_dstManager(dstManager)
    , m_srcPath(srcPath)
    , m_dstPath(dstPath)
    , m_isDir(isDir)
    , m_isCut(isCut)
    , m_srcHostName(srcHostName)
    , m_dstHostName(dstHostName)
    , m_isCancelled(false)
    , m_bytesTransferred(0)
    , m_totalBytes(0)
{
    setAutoDelete(true);
}

void CrossMachineTransferWorker::cancel()
{
    m_isCancelled = true;
}

void CrossMachineTransferWorker::run()
{
    emit workerSignals.started();

    if (!m_srcManager || !m_dstManager || !m_srcManager->isConnected() || !m_dstManager->isConnected()) {
        emit workerSignals.error(QStringLiteral("Source or destination SFTP session is not active."));
        emit workerSignals.finished();
        return;
    }

    bool isSameHost = (m_srcManager->host() == m_dstManager->host());

    // Same path on same host check
    if (isSameHost && m_srcPath == m_dstPath) {
        if (m_isCut) {
            emit workerSignals.transferResult(true);
            emit workerSignals.finished();
            return;
        } else {
            QFileInfo fi(m_dstPath);
            QString dir = fi.path();
            QString base = fi.completeBaseName();
            QString ext = fi.suffix();
            if (!ext.isEmpty()) ext = QStringLiteral(".") + ext;
            m_dstPath = (dir == QStringLiteral(".") || dir.isEmpty())
                ? (base + QStringLiteral("_copy") + ext)
                : (dir + QStringLiteral("/") + base + QStringLiteral("_copy") + ext);
        }
    }

    // Fast-path: Same machine rename for cut operations
    if (isSameHost && m_isCut) {
        QString err;
        if (m_srcManager->renamePath(m_srcPath, m_dstPath, err)) {
            QString displayName = QStringLiteral("[%1] %2 -> [%3] %4")
                .arg(m_srcHostName, m_srcPath, m_dstHostName, m_dstPath);
            emit workerSignals.progress(1, 1, displayName, 0.0);
            emit workerSignals.transferResult(true);
        } else {
            emit workerSignals.error(err);
        }
        emit workerSignals.finished();
        return;
    }

    // Calculate total size for accurate progress bar
    int fileCount = 0;
    m_srcManager->calculateTreeSize(m_srcPath, m_totalBytes, fileCount);
    if (m_totalBytes <= 0) {
        m_totalBytes = 1; // Prevent division by zero
    }
    m_startTime = std::chrono::steady_clock::now();

    try {
        if (!m_isDir) {
            streamSingleFile(m_srcPath, m_dstPath);
            if (m_isCut && !m_isCancelled) {
                QString delErr;
                m_srcManager->deleteFile(m_srcPath, delErr);
            }
        } else {
            transferDirectoryRecursive(m_srcPath, m_dstPath);
            if (m_isCut && !m_isCancelled) {
                QString delErr;
                m_srcManager->deleteDirectoryRecursive(m_srcPath, delErr);
            }
        }

        if (m_isCancelled) {
            emit workerSignals.error(QStringLiteral("Transfer was cancelled."));
        } else {
            emit workerSignals.transferResult(true);
        }
    } catch (const std::exception& e) {
        emit workerSignals.error(QString::fromUtf8(e.what()));
    } catch (...) {
        emit workerSignals.error(QStringLiteral("Unexpected error during transfer."));
    }

    emit workerSignals.finished();
}

void CrossMachineTransferWorker::streamSingleFile(const QString& src, const QString& dst)
{
    auto srcFile = m_srcManager->openFileRead(src);
    if (!srcFile) {
        throw std::runtime_error(QStringLiteral("Failed to open source file for reading: %1").arg(src).toStdString());
    }

    auto dstFile = m_dstManager->openFileWrite(dst);
    if (!dstFile) {
        m_srcManager->closeFile(srcFile);
        throw std::runtime_error(QStringLiteral("Failed to open destination file for writing: %1").arg(dst).toStdString());
    }

    std::vector<char> buffer(SFTPManager::DEFAULT_CHUNK_SIZE);
    QString srcAbs = m_srcManager->getAbsolutePath(src);
    QString dstAbs = m_dstManager->getAbsolutePath(dst);
    QString itemName = QStringLiteral("[%1] %2 ➔ [%3] %4").arg(m_srcHostName, srcAbs, m_dstHostName, dstAbs);

    while (!m_isCancelled) {
        qint64 bytesRead = m_srcManager->readFileChunk(srcFile, buffer.data(), buffer.size());
        if (bytesRead <= 0) {
            break;
        }

        qint64 bytesWritten = m_dstManager->writeFileChunk(dstFile, buffer.data(), static_cast<size_t>(bytesRead));
        if (bytesWritten != bytesRead) {
            m_srcManager->closeFile(srcFile);
            m_dstManager->closeFile(dstFile);
            throw std::runtime_error(QStringLiteral("Short write error on destination: %1").arg(dst).toStdString());
        }

        m_bytesTransferred += bytesRead;

        auto now = std::chrono::steady_clock::now();
        double elapsedSecs = std::chrono::duration<double>(now - m_startTime).count();
        if (elapsedSecs < 0.001) elapsedSecs = 0.001;
        double speedBps = static_cast<double>(m_bytesTransferred) / elapsedSecs;

        emit workerSignals.progress(m_bytesTransferred, std::max(m_bytesTransferred, m_totalBytes), itemName, speedBps);
    }

    m_srcManager->closeFile(srcFile);
    m_dstManager->closeFile(dstFile);
}

void CrossMachineTransferWorker::transferDirectoryRecursive(const QString& src, const QString& dst)
{
    if (m_isCancelled) return;

    QString mkdirErr;
    m_dstManager->createDirectory(dst, mkdirErr);

    QString listErr;
    QList<RemoteEntry> items = m_srcManager->listDirectory(src, listErr);
    for (const RemoteEntry& item : items) {
        if (m_isCancelled) return;

        QString subSrc = src.endsWith('/') ? (src + item.name) : (src + '/' + item.name);
        QString subDst = dst.endsWith('/') ? (dst + item.name) : (dst + '/' + item.name);

        if (item.isDirectory) {
            transferDirectoryRecursive(subSrc, subDst);
        } else {
            streamSingleFile(subSrc, subDst);
        }
    }
}
