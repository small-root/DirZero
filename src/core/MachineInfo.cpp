#include "MachineInfo.hpp"

MachineInfo::MachineInfo()
    : m_online(false)
    , m_sshAvailable(false)
    , m_isSelf(false)
{
}

MachineInfo::MachineInfo(const QString& name,
                         const QString& dnsName,
                         const QString& ip,
                         const QString& osType,
                         bool online,
                         bool sshAvailable,
                         bool isSelf)
    : m_name(name)
    , m_dnsName(dnsName)
    , m_ip(ip)
    , m_osType(osType.toLower())
    , m_online(online)
    , m_sshAvailable(sshAvailable)
    , m_isSelf(isSelf)
{
}

QString MachineInfo::osIcon() const
{
    const QString os = m_osType.toLower();
    if (os.contains(QStringLiteral("linux"))) {
        return QStringLiteral("🐧");
    } else if (os.contains(QStringLiteral("windows"))) {
        return QStringLiteral("🪟");
    } else if (os.contains(QStringLiteral("darwin")) || os.contains(QStringLiteral("mac") ) || os.contains(QStringLiteral("apple"))) {
        return QStringLiteral("🍎");
    } else if (os.contains(QStringLiteral("android"))) {
        return QStringLiteral("🤖");
    } else if (os.contains(QStringLiteral("freebsd")) || os.contains(QStringLiteral("bsd"))) {
        return QStringLiteral("😈");
    }
    return QStringLiteral("💻");
}

QString MachineInfo::displayName() const
{
    QString title = m_name.toUpper();
    if (m_isSelf) {
        title += QStringLiteral(" (This Machine)");
    }
    return title;
}

QString MachineInfo::toString() const
{
    return QStringLiteral("<MachineInfo %1 (%2) online=%3 ssh=%4 self=%5>")
        .arg(m_name, m_ip)
        .arg(m_online ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_sshAvailable ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_isSelf ? QStringLiteral("true") : QStringLiteral("false"));
}
