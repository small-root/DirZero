#include "ProbeWorker.hpp"
#include "TailscaleDiscovery.hpp"

ProbeWorker::ProbeWorker(const QString& ip, int timeoutMs)
    : m_ip(ip)
    , m_timeoutMs(timeoutMs)
{
    setAutoDelete(true);
}

void ProbeWorker::run()
{
    emit workerSignals.started();
    bool open = TailscaleDiscovery::checkPort22(m_ip, m_timeoutMs);
    emit workerSignals.portCheckResult(open);
    emit workerSignals.finished();
}
