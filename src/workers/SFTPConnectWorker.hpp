#pragma once

#include <QRunnable>
#include "WorkerSignals.hpp"

class SFTPManager;

class SFTPConnectWorker : public QRunnable {
public:
    explicit SFTPConnectWorker(SFTPManager* manager);
    void run() override;

    WorkerSignals workerSignals;

private:
    SFTPManager* m_manager;
};
