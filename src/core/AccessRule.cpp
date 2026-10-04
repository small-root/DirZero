#include "AccessRule.hpp"
#include <QRegularExpression>

AccessRule::AccessRule()
    : m_permission(AccessPermission::AllowAll)
{
}

AccessRule::AccessRule(const QString& pathPattern, AccessPermission permission, const QString& description)
    : m_pathPattern(pathPattern)
    , m_permission(permission)
    , m_description(description)
{
}

bool AccessRule::matches(const QString& targetPath) const
{
    if (m_pathPattern.isEmpty()) {
        return false;
    }

    if (m_pathPattern == QStringLiteral("*")) {
        return true;
    }

    // Exact match or prefix match for directories
    if (targetPath == m_pathPattern) {
        return true;
    }

    QString normalizedTarget = targetPath;
    QString normalizedPattern = m_pathPattern;

    if (!normalizedPattern.endsWith('/')) {
        normalizedPattern += '/';
    }
    if (!normalizedTarget.endsWith('/')) {
        normalizedTarget += '/';
    }

    if (normalizedTarget.startsWith(normalizedPattern)) {
        return true;
    }

    // Wildcard matching using QRegularExpression
    if (m_pathPattern.contains('*') || m_pathPattern.contains('?')) {
        QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(m_pathPattern));
        return regex.match(targetPath).hasMatch();
    }

    return false;
}
