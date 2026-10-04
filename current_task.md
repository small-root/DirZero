# Current Task: Self-Machine Right-Click File/Folder Protection & Permission Management

> **Target Audience**: AI Coding Assistants / LLMs & Core Developers working on DirZero.  
> **Objective**: Implement a context menu feature that allows users to lock, protect, and restrict modifications to specific files or directories **strictly on their local ("Self") machine**.

---

## 1. Feature Summary & User Requirement

When viewing the machine grid in DirZero:
1. **Local Machine Scope Only**:
   - Every machine panel has an associated `MachineInfo`.
   - The option to modify/lock file permissions must **ONLY appear when right-clicking items on the "Self" machine** (i.e. `m_info.isSelf() == true`).
   - When right-clicking items on remote peer machines (`m_info.isSelf() == false`), these permission management options must **NOT** be displayed (users can only manage permissions on their own local machine).

2. **Protection Functionality (Locking / Restricting)**:
   - When the user selects `🔒 Protect / Lock (Prevent Modification)` on a file or folder:
     - **OS Level**:
       - **Linux / macOS (POSIX)**: Strip write permissions (`chmod a-w` or mode `0444` for files, `0555` for directories).
       - **Windows**: Set the `FILE_ATTRIBUTE_READONLY` attribute via `SetFileAttributesW` (or `attrib +R`).
     - **App Level (`PermissionPolicy`)**:
       - Register an `AccessRule` with `AccessPermission::ReadOnly` or `AccessPermission::DenyAll` in `PermissionPolicy`.
       - Prevent any incoming or outgoing transfer, delete, rename, or cut/move operations through DirZero against this path.
     - **UI Level**:
       - Refresh the file tree view.
       - Display a lock badge or indicator (e.g., `🔒 [filename]`) in `RemoteFSModel` or the permissions column (`r--r--r-- [LOCKED]`).

3. **Restoring Permissions (Unlocking)**:
   - When the user selects `🔓 Unlock / Restore Read-Write`:
     - **OS Level**:
       - **Linux / macOS (POSIX)**: Restore write permissions (`chmod 0644` for files, `chmod 0755` for directories).
       - **Windows**: Remove read-only attribute (`FILE_ATTRIBUTE_NORMAL` / `attrib -R`).
     - **App Level (`PermissionPolicy`)**:
       - Remove the path rule from `PermissionPolicy`.
     - **UI Level**:
       - Refresh the file tree view and update status.

---

## 2. Technical Implementation Architecture

### A. Context Menu Filtering in `MachinePanelWidget.cpp`
In `MachinePanelWidget::onCustomContextMenuRequested(const QPoint& pos)`:
```cpp
// Check if the current panel belongs to the local machine
if (m_info.isSelf() && selected && !selected->isDummy) {
    menu.addSeparator();
    
    // Check if the node is already read-only / locked
    bool isLocked = !selected->isWritable() || PermissionPolicy::defaultPolicy().isPathLocked(selected->path);
    
    if (isLocked) {
        auto unlockAct = menu.addAction(QStringLiteral("🔓 Unlock / Allow Modification"), this, [this, selected]() {
            unlockSelectedNode(selected);
        });
    } else {
        auto lockAct = menu.addAction(QStringLiteral("🔒 Protect / Prevent Modification"), this, [this, selected]() {
            lockSelectedNode(selected);
        });
    }
}
```

### B. Cross-Platform Permission Helper
Implement a utility function to adjust native OS filesystem permissions:

```cpp
bool setPathImmutable(const QString& path, bool isDirectory, bool lock, QString& errorOut)
{
#if defined(Q_OS_WIN)
    std::wstring wPath = path.toStdWString();
    DWORD attrs = GetFileAttributesW(wPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        errorOut = "Failed to get Windows file attributes.";
        return false;
    }
    if (lock) {
        attrs |= FILE_ATTRIBUTE_READONLY;
    } else {
        attrs &= ~FILE_ATTRIBUTE_READONLY;
    }
    if (!SetFileAttributesW(wPath.c_str(), attrs)) {
        errorOut = "SetFileAttributesW failed.";
        return false;
    }
    return true;
#else
    // POSIX (Linux & macOS)
    QByteArray localPath = path.toUtf8();
    mode_t newMode;
    if (lock) {
        newMode = isDirectory ? 0555 : 0444; // Read + Execute for dirs, Read-only for files
    } else {
        newMode = isDirectory ? 0755 : 0644; // Standard read/write/exec
    }
    if (chmod(localPath.constData(), newMode) != 0) {
        errorOut = QString::fromLocal8Bit(strerror(errno));
        return false;
    }
    return true;
#endif
}
```

### C. `PermissionPolicy` Registration & Persistence
- When a path is locked, add an `AccessRule` to `PermissionPolicy::defaultPolicy()`.
- Persist the list of locked paths to `~/.config/dirzero/locked_paths.json` (or `%APPDATA%\DirZero\locked_paths.json` on Windows) so protections remain active across application restarts.
- `PermissionPolicy::validateTransfer`, `validateRename`, and `validateDelete` should verify `isPathLocked(targetPath)` and reject mutations.

### D. Tree View Display (`RemoteFSModel.cpp`)
- In `RemoteFSModel::data(index, Qt::DisplayRole)`:
  - If `node` is locked, display `🔒 ` prefix in `ColName` or `ColPermissions` showing `[LOCKED]`.

---

## 3. Files to Modify

1. **[`src/core/PermissionPolicy.hpp/.cpp`](file:///home/xiaogen/code/DirZero/src/core/PermissionPolicy.hpp)**:
   - Add `isPathLocked(const QString& path)` method.
   - Add `lockPath(const QString& path)` and `unlockPath(const QString& path)`.
   - Add JSON persistence for locked paths.

2. **[`src/ui/MachinePanelWidget.hpp/.cpp`](file:///home/xiaogen/code/DirZero/src/ui/MachinePanelWidget.hpp)**:
   - In `onCustomContextMenuRequested()`, show `🔒 Protect` / `🔓 Unlock` only when `m_info.isSelf()` is true.
   - Implement slot handlers `lockSelectedNode(FSNode* node)` and `unlockSelectedNode(FSNode* node)`.
   - Trigger `reloadFilesystem()` upon permission changes.

3. **[`src/fs/RemoteFSModel.cpp`](file:///home/xiaogen/code/DirZero/src/fs/RemoteFSModel.cpp)**:
   - Render visual lock indicator for locked items.

---

## 4. Verification Checklist for the LLM

- [ ] Does the right-click menu **only** show the lock/unlock options when right-clicking on the local self machine panel (`m_info.isSelf()`)?
- [ ] Is `chmod` properly invoked on Linux/macOS, and `SetFileAttributesW` / `attrib` on Windows?
- [ ] Does `PermissionPolicy` reject remote attempts to delete, rename, or cut/move the locked file?
- [ ] Does the UI immediately refresh and show the updated lock status?
- [ ] Does the project compile cleanly with `cmake --build build` with 0 warnings/errors?
