#pragma once

#include <QRunnable>
#include <QString>
#include <atomic>
#include <chrono>
#include "WorkerSignals.hpp"

class SFTPManager;

class CrossMachineTransferWorker : public QRunnable {
public:
    CrossMachineTransferWorker(SFTPManager* srcManager,
                               SFTPManager* dstManager,
                               const QString& srcPath,
                               const QString& dstPath,
                               bool isDir = false,
                               bool isCut = false,
                               const QString& srcHostName = QString(),
                               const QString& dstHostName = QString());

    void run() override;
    void cancel();

    WorkerSignals workerSignals;

private:
    SFTPManager* m_srcManager;
    SFTPManager* m_dstManager;
    QString m_srcPath;
    QString m_dstPath;
    bool m_isDir;
    bool m_isCut;
    QString m_srcHostName;
    QString m_dstHostName;

    std::atomic<bool> m_isCancelled;
    qint64 m_bytesTransferred;
    qint64 m_totalBytes;
    std::chrono::steady_clock::time_point m_startTime;

    void streamSingleFile(const QString& src, const QString& dst);
    void transferDirectoryRecursive(const QString& src, const QString& dst);
};
