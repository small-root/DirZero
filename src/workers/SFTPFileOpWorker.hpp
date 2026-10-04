#pragma once

#include <QRunnable>
#include <QString>
#include "WorkerSignals.hpp"

class SFTPManager;

class SFTPFileOpWorker : public QRunnable {
public:
    SFTPFileOpWorker(const QString& opType,
                     SFTPManager* manager,
                     const QString& path1,
                     const QString& path2 = QString());

    void run() override;

    WorkerSignals workerSignals;

private:
    QString m_opType;
    SFTPManager* m_manager;
    QString m_path1;
    QString m_path2;
};
