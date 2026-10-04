# DirZero C++ Port Status & Transition Log

## 1. Project Overview & Progress Summary
DirZero has been completely rewritten from Python (PySide6 / Paramiko) to a high-performance, modular C++20/C++17 Qt6 desktop application utilizing `libssh` for secure, multi-threaded remote file operations and Tailscale peer networking.

- **Target Architecture**: C++17 / Qt6 (`Core`, `Gui`, `Widgets`, `Network`) + `libssh`
- **Build System**: CMake (tested and building cleanly with GCC/Clang on Linux)
- **UI Design System**: Glassmorphic Cyber Dark theme with real-time glows, custom drag-preview pills, interactive authentication bar, and multi-machine responsive grid layout.

---

## 2. Completed Modules & Files

### Core & Architecture (`src/core/`)
- `MachineInfo.hpp / .cpp`: Data model representing remote Tailscale machines, OS type, IP addresses, DNS names, and SSH reachability.
- `MachineState.hpp`: State enum for remote endpoints (`Offline`, `OnlineSshUnavailable`, `AuthRequired`, `Connecting`, `Connected`, `Error`).
- `AuthState.hpp`: Authentication state tracking.
- `AccessRule.hpp / .cpp`: Path-based permission rules (`AllowAll`, `ReadOnly`, `DenyAll`).
- `PermissionPolicy.hpp / .cpp`: Security enforcement engine preventing destructive operations on critical system directories (`/etc`, `/bin`, `C:\Windows`, etc.) and validating file creation, rename, delete, and copy/move operations.

### Credential & Security Layer (`src/security/`)
- `CredentialStore.hpp / .cpp`: Multi-platform persistent credential storage (`~/.config/dirzero/credentials.json` on Linux/POSIX, `%APPDATA%\DirZero` on Windows), standard SSH key discovery (`~/.ssh/id_ed25519`, `~/.ssh/id_rsa`, etc.), and file permission enforcement (`0700` / `0600`).

### Filesystem & Remote Protocol (`src/fs/`)
- `RemoteEntry.hpp`: Metadata container for remote items (name, path, isDirectory, size, mtime, POSIX mode, permissions).
- `FSNode.hpp / .cpp`: Hierarchical tree node model with lazy loading child support and formatted metadata.
- `SFTPManager.hpp / .cpp`: Robust `libssh` wrapper managing SSH/SFTP connections, directory listing, recursive size calculation, file CRUD operations, and stream-based 64KB chunk transfers with mutex thread-safety.
- `RemoteFSModel.hpp / .cpp`: Qt `QAbstractItemModel` powering the tree view with async node population and dynamic error reporting.

### Discovery & Background Workers (`src/discovery/`, `src/workers/`)
- `TailscaleDiscovery.hpp / .cpp`: Auto-discovery engine parsing `tailscale status --json` with fallback CLI parsing, IPv4 extraction, and TCP port 22 reachability checks.
- `WorkerSignals.hpp`: Qt signal hub for communicating between background `QRunnable` workers and UI thread.
- `ProbeWorker.hpp / .cpp`: TCP port 22 reachability probing worker.
- `DiscoveryWorker.hpp / .cpp`: Background discovery worker preventing UI freezes.
- `SFTPConnectWorker.hpp / .cpp`: Asynchronous SSH/SFTP connection worker.
- `SFTPListWorker.hpp / .cpp`: Asynchronous directory listing worker.
- `SFTPFileOpWorker.hpp / .cpp`: Asynchronous file operation worker (create file/dir, rename, delete).
- `CrossMachineTransferWorker.hpp / .cpp`: Stream-based cross-machine and local machine copy/cut/move worker with progress reporting and cancellation support.

### UI & Glassmorphic Styling (`src/ui/`, `src/helpers/`)
- `ThemeManager.hpp / .cpp`: Dynamic theme engine featuring 4 built-in glass themes (`Cyber Glass Dark`, `Midnight Glass`, `Emerald Matrix`, `Glass Frost Light`) with active theme persistence.
- `ResponsiveGridContainer.hpp / .cpp`: Auto-fitting responsive grid for machine cards.
- `RemoteClipboard.hpp / .cpp`: Inter-panel copy/cut/paste clipboard service.
- `RemoteFileTreeView.hpp / .cpp`: Drag & Drop enabled tree view with custom frosted glass preview rendering and full keyboard shortcuts (`Ctrl+C`, `Ctrl+X`, `Ctrl+V`, `F2`, `F5`, `Delete`).
- `MachineAuthBar.hpp / .cpp`: Collapsible glassmorphic authentication bar supporting username, password, private key file selection, and credential persistence.
- `MachinePanelWidget.hpp / .cpp`: Machine card component with dynamic state glow borders, context menus, directory navigation, inline retry, and file operations.
- `Dir2ZeroWindow.hpp / .cpp`: Main window shell featuring header badges (Online / Reachable / Unavailable), scan button, theme switcher menu, and bottom dock for real-time transfer progress and transfer speed metrics.
- `main.cpp`: Entry point with CLI arguments (`--user`, `--key`) and theme initialization.

---

## 3. What is Currently Being Done
- Refactored `MachinePanelWidget` toolbar into a dedicated dual-row layout:
  - **Row 1**: Current directory path display (`📁 ...`) and session buttons (`🔑 Auth`, `↻ Refresh`, `⟳ Connect`, `Disconnect`).
  - **Row 2**: Distinct file action buttons (`📄 + New File`, `📁 + New Folder`, `📥 Paste`) with auto-expanding width, 32px height, and clear text/icons.
- Enhanced `MachineAuthBar` inputs and buttons with 30px height, balanced padding, and responsive sizing.
- Refined `QPushButton` typography and vertical padding across all 4 glassmorphic themes in `ThemeManager.cpp` to prevent text truncation/clipping.
- Successfully built with 0 errors and 0 warnings (`cmake --build build`).

---

## 4. What is to be Done Next (Roadmap for Next Agents / Developers)
1. **Packaging & Deployment**:
   - Create AppImage, Flatpak, or Debian package configurations for distribution.
   - Configure Windows / macOS cross-compilation builds in CMake if required.
2. **Optional Extended Features**:
   - Double-click to download & open remote files with system default application.
   - Terminal launcher integration (e.g. launching system terminal with `ssh user@machine`).
   - Breadcrumb navigation bar above each machine's tree view for direct path jumping.
   - File search / filter input inside each machine panel.
