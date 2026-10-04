#pragma once

#include <QString>
#include <QList>
#include "../core/MachineInfo.hpp"

class TailscaleDiscovery {
public:
    static constexpr int DEFAULT_SSH_PORT = 22;
    static constexpr int DEFAULT_SOCKET_TIMEOUT_MS = 3000;

    static QString findTailscaleBinary();
    static bool checkPort22(const QString& ip, int timeoutMs = DEFAULT_SOCKET_TIMEOUT_MS);
    static QList<MachineInfo> getOnlineMachines(bool checkSsh = true);
};
