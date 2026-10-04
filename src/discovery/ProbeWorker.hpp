#pragma once

#include <QRunnable>
#include "../workers/WorkerSignals.hpp"

class ProbeWorker : public QRunnable {
public:
    explicit ProbeWorker(const QString& ip, int timeoutMs = 3000);
    void run() override;

    WorkerSignals workerSignals;

private:
    QString m_ip;
    int m_timeoutMs;
};
