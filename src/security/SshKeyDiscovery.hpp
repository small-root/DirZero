#pragma once

#include <QStringList>

class SshKeyDiscovery {
public:
    static QStringList defaultKeyFilenames();
    static QStringList defaultKeyPaths();
    static QString firstAvailableKeyPath();
};
