#pragma once

#include <QString>
#include <QStringList>

enum class AccessPermission {
    AllowAll,
    ReadOnly,
    DenyAll
};

class AccessRule {
public:
    AccessRule();
    AccessRule(const QString& pathPattern, AccessPermission permission, const QString& description = QString());

    QString pathPattern() const { return m_pathPattern; }
    void setPathPattern(const QString& pattern) { m_pathPattern = pattern; }

    AccessPermission permission() const { return m_permission; }
    void setPermission(AccessPermission perm) { m_permission = perm; }

    QString description() const { return m_description; }
    void setDescription(const QString& desc) { m_description = desc; }

    bool matches(const QString& targetPath) const;

private:
    QString m_pathPattern;
    AccessPermission m_permission;
    QString m_description;
};
