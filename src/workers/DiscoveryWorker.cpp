#include "DiscoveryWorker.hpp"
#include "../discovery/TailscaleDiscovery.hpp"

DiscoveryWorker::DiscoveryWorker(bool checkSsh)
    : m_checkSsh(checkSsh)
{
    setAutoDelete(true);
}

void DiscoveryWorker::run()
{
    emit workerSignals.started();
    try {
        QList<MachineInfo> machines = TailscaleDiscovery::getOnlineMachines(m_checkSsh);
        emit workerSignals.discoveryResult(machines);
    } catch (const std::exception& e) {
        emit workerSignals.error(QString::fromUtf8(e.what()));
    } catch (...) {
        emit workerSignals.error(QStringLiteral("Unknown error during Tailscale discovery."));
    }
    emit workerSignals.finished();
}
