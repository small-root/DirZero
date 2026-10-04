#pragma once

#include <QString>
#include <QList>
#include "AccessRule.hpp"

enum class OperationType {
    Read,
    CreateFile,
    CreateDirectory,
    Rename,
    DeleteFile,
    DeleteDirectory,
    Copy,
    Move,
    Paste
};

struct PermissionCheckResult {
    bool allowed;
    QString reason;

    static PermissionCheckResult allow() {
        return {true, QString()};
    }

    static PermissionCheckResult deny(const QString& reason) {
        return {false, reason};
    }
};

class PermissionPolicy {
public:
    PermissionPolicy();

    static PermissionPolicy& defaultPolicy();

    void addRule(const AccessRule& rule);
    void clearRules();

    PermissionCheckResult checkOperation(OperationType op,
                                         const QString& targetPath,
                                         const QString& secondaryPath = QString()) const;

    PermissionCheckResult validateCreateFile(const QString& targetDir, const QString& fileName) const;
    PermissionCheckResult validateCreateDirectory(const QString& targetDir, const QString& dirName) const;
    PermissionCheckResult validateRename(const QString& oldPath, const QString& newPath) const;
    PermissionCheckResult validateDelete(const QString& targetPath, bool isDirectory) const;
    PermissionCheckResult validateTransfer(const QString& srcPath, const QString& dstPath, bool isCut) const;

    bool isSystemProtectedPath(const QString& path) const;

private:
    QList<AccessRule> m_rules;
    QStringList m_protectedSystemPaths;
};
