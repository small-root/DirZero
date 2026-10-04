#pragma once

#include <QString>
#include <QMetaType>

class MachineInfo {
public:
    MachineInfo();
    MachineInfo(const QString& name,
                const QString& dnsName,
                const QString& ip,
                const QString& osType = QStringLiteral("linux"),
                bool online = true,
                bool sshAvailable = false,
                bool isSelf = false);

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    QString dnsName() const { return m_dnsName; }
    void setDnsName(const QString& dnsName) { m_dnsName = dnsName; }

    QString ip() const { return m_ip; }
    void setIp(const QString& ip) { m_ip = ip; }

    QString osType() const { return m_osType; }
    void setOsType(const QString& osType) { m_osType = osType.toLower(); }

    bool online() const { return m_online; }
    void setOnline(bool online) { m_online = online; }

    bool sshAvailable() const { return m_sshAvailable; }
    void setSshAvailable(bool available) { m_sshAvailable = available; }

    bool isSelf() const { return m_isSelf; }
    void setIsSelf(bool isSelf) { m_isSelf = isSelf; }

    QString osIcon() const;
    QString displayName() const;
    QString toString() const;

private:
    QString m_name;
    QString m_dnsName;
    QString m_ip;
    QString m_osType;
    bool m_online;
    bool m_sshAvailable;
    bool m_isSelf;
};

Q_DECLARE_METATYPE(MachineInfo)
