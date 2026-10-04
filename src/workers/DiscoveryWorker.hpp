#pragma once

#include <QRunnable>
#include "WorkerSignals.hpp"

class DiscoveryWorker : public QRunnable {
public:
    explicit DiscoveryWorker(bool checkSsh = true);
    void run() override;

    WorkerSignals workerSignals;

private:
    bool m_checkSsh;
};
