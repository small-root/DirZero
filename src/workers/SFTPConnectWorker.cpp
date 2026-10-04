#include "SFTPConnectWorker.hpp"
#include "../fs/SFTPManager.hpp"

SFTPConnectWorker::SFTPConnectWorker(SFTPManager* manager)
    : m_manager(manager)
{
    setAutoDelete(true);
}

void SFTPConnectWorker::run()
{
    emit workerSignals.started();
    QString error;
    bool ok = m_manager->connectSession(error);
    if (ok) {
        emit workerSignals.connectResult(true);
    } else {
        emit workerSignals.error(error);
    }
    emit workerSignals.finished();
}
