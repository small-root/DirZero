#pragma once

#include <QRunnable>
#include <QModelIndex>
#include "WorkerSignals.hpp"

class SFTPManager;
class FSNode;

class SFTPListWorker : public QRunnable {
public:
    SFTPListWorker(SFTPManager* manager,
                   const QString& remotePath,
                   FSNode* parentNode,
                   const QModelIndex& parentIndex);

    void run() override;

    WorkerSignals workerSignals;

private:
    SFTPManager* m_manager;
    QString m_remotePath;
    FSNode* m_parentNode;
    QModelIndex m_parentIndex;
};
