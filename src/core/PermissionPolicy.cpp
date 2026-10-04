#include "PermissionPolicy.hpp"

PermissionPolicy::PermissionPolicy()
{
    // Define critical system protected paths that should never be accidentally deleted or overwritten
    m_protectedSystemPaths << QStringLiteral("/")
                           << QStringLiteral("/bin")
                           << QStringLiteral("/sbin")
                           << QStringLiteral("/boot")
                           << QStringLiteral("/dev")
                           << QStringLiteral("/etc")
                           << QStringLiteral("/proc")
                           << QStringLiteral("/sys")
                           << QStringLiteral("/usr")
                           << QStringLiteral("/lib")
                           << QStringLiteral("/lib64")
                           << QStringLiteral("C:\\Windows")
                           << QStringLiteral("C:\\Program Files");

    // Add default security access rules
    m_rules.append(AccessRule(QStringLiteral("/etc/shadow"), AccessPermission::DenyAll, QStringLiteral("Shadow password file is protected")));
    m_rules.append(AccessRule(QStringLiteral("/etc/sudoers"), AccessPermission::DenyAll, QStringLiteral("Sudoers configuration is protected")));
    m_rules.append(AccessRule(QStringLiteral("/etc/master.passwd"), AccessPermission::DenyAll, QStringLiteral("Master password file is protected")));
}

PermissionPolicy& PermissionPolicy::defaultPolicy()
{
    static PermissionPolicy instance;
    return instance;
}

void PermissionPolicy::addRule(const AccessRule& rule)
{
    m_rules.append(rule);
}

void PermissionPolicy::clearRules()
{
    m_rules.clear();
}

bool PermissionPolicy::isSystemProtectedPath(const QString& path) const
{
    QString p = path.trimmed();
    while (p.endsWith('/') && p.length() > 1) {
        p.chop(1);
    }

    for (const QString& sysPath : m_protectedSystemPaths) {
        if (p == sysPath) {
            return true;
        }
    }
    return false;
}

PermissionCheckResult PermissionPolicy::checkOperation(OperationType op,
                                                       const QString& targetPath,
                                                       const QString& secondaryPath) const
{
    const QString cleanTarget = targetPath.trimmed();

    if (cleanTarget.isEmpty()) {
        return PermissionCheckResult::deny(QStringLiteral("Target path cannot be empty."));
    }

    // Check custom access rules
    for (const AccessRule& rule : m_rules) {
        if (rule.matches(cleanTarget)) {
            if (rule.permission() == AccessPermission::DenyAll) {
                return PermissionCheckResult::deny(QStringLiteral("Access denied by security rule: %1 (%2)")
                    .arg(cleanTarget, rule.description()));
            }
            if (rule.permission() == AccessPermission::ReadOnly && op != OperationType::Read) {
                return PermissionCheckResult::deny(QStringLiteral("Write operation blocked: '%1' is in a read-only path.")
                    .arg(cleanTarget));
            }
        }
    }

    // Check protected paths for destructive or modifying operations
    if (op == OperationType::DeleteFile ||
        op == OperationType::DeleteDirectory ||
        op == OperationType::Rename ||
        op == OperationType::Move)
    {
        if (isSystemProtectedPath(cleanTarget)) {
            return PermissionCheckResult::deny(QStringLiteral("Operation prohibited on critical system directory: '%1'").arg(cleanTarget));
        }
    }

    if (op == OperationType::Rename || op == OperationType::Move || op == OperationType::Copy || op == OperationType::Paste) {
        if (!secondaryPath.isEmpty()) {
            for (const AccessRule& rule : m_rules) {
                if (rule.matches(secondaryPath) && rule.permission() != AccessPermission::AllowAll) {
                    return PermissionCheckResult::deny(QStringLiteral("Destination access restricted by rule: %1").arg(secondaryPath));
                }
            }
        }
    }

    return PermissionCheckResult::allow();
}

PermissionCheckResult PermissionPolicy::validateCreateFile(const QString& targetDir, const QString& fileName) const
{
    if (fileName.trimmed().isEmpty()) {
        return PermissionCheckResult::deny(QStringLiteral("File name cannot be empty."));
    }
    if (fileName.contains('/') || fileName.contains('\\')) {
        return PermissionCheckResult::deny(QStringLiteral("File name cannot contain path separators ('/' or '\\')."));
    }
    if (isSystemProtectedPath(targetDir)) {
        return PermissionCheckResult::deny(QStringLiteral("Cannot create files in protected system directory: %1").arg(targetDir));
    }
    return checkOperation(OperationType::CreateFile, targetDir + QStringLiteral("/") + fileName);
}

PermissionCheckResult PermissionPolicy::validateCreateDirectory(const QString& targetDir, const QString& dirName) const
{
    if (dirName.trimmed().isEmpty()) {
        return PermissionCheckResult::deny(QStringLiteral("Directory name cannot be empty."));
    }
    if (dirName.contains('/') || dirName.contains('\\')) {
        return PermissionCheckResult::deny(QStringLiteral("Directory name cannot contain path separators ('/' or '\\')."));
    }
    if (isSystemProtectedPath(targetDir)) {
        return PermissionCheckResult::deny(QStringLiteral("Cannot create directories in protected system directory: %1").arg(targetDir));
    }
    return checkOperation(OperationType::CreateDirectory, targetDir + QStringLiteral("/") + dirName);
}

PermissionCheckResult PermissionPolicy::validateRename(const QString& oldPath, const QString& newPath) const
{
    if (oldPath.trimmed().isEmpty() || newPath.trimmed().isEmpty()) {
        return PermissionCheckResult::deny(QStringLiteral("Source and target paths must be specified for rename."));
    }
    if (oldPath == newPath) {
        return PermissionCheckResult::deny(QStringLiteral("New name is identical to the current name."));
    }
    if (isSystemProtectedPath(oldPath)) {
        return PermissionCheckResult::deny(QStringLiteral("Cannot rename critical system directory: %1").arg(oldPath));
    }
    return checkOperation(OperationType::Rename, oldPath, newPath);
}

PermissionCheckResult PermissionPolicy::validateDelete(const QString& targetPath, bool isDirectory) const
{
    if (isSystemProtectedPath(targetPath)) {
        return PermissionCheckResult::deny(QStringLiteral("Critical system path cannot be deleted: %1").arg(targetPath));
    }
    OperationType op = isDirectory ? OperationType::DeleteDirectory : OperationType::DeleteFile;
    return checkOperation(op, targetPath);
}

PermissionCheckResult PermissionPolicy::validateTransfer(const QString& srcPath, const QString& dstPath, bool isCut) const
{
    if (srcPath.trimmed().isEmpty() || dstPath.trimmed().isEmpty()) {
        return PermissionCheckResult::deny(QStringLiteral("Source and destination paths cannot be empty."));
    }
    if (isCut && isSystemProtectedPath(srcPath)) {
        return PermissionCheckResult::deny(QStringLiteral("Cannot move/cut protected system path: %1").arg(srcPath));
    }
    OperationType op = isCut ? OperationType::Move : OperationType::Copy;
    return checkOperation(op, srcPath, dstPath);
}
