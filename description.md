# DirZero — C++ Technical Specification & Architecture Manual

> **Purpose of this document**: This document serves as a comprehensive technical blueprint and guide for developers and AI coding agents working on, extending, or maintaining the **DirZero** C++ codebase.

---

## 1. Project Overview & Philosophy

**DirZero** is a cross-platform desktop remote file manager tailored for private mesh networks powered by [Tailscale](https://tailscale.com/). It allows users to visualize, browse, manage, and transfer files across all reachable machines in their Tailscale tailnet with zero centralized cloud dependencies.

### Core Capabilities
- **Zero-Config Mesh Discovery**: Interrogates local Tailscale daemon (`tailscale status --json`) to detect online peers, map hostnames, IPv4 addresses, and OS types.
- **This PC Drive Browsing**: Shows mounted local drives and provides in-app folder navigation alongside the Tailnet machine view.
- **Session-Only SSH Authentication**: Does not persist passwords or connection credentials; users initiate a fresh connection each app session.
- **Port 22 SSH Reachability Probing**: Asynchronously probes TCP port 22 to separate general online nodes from SSH/SFTP-enabled hosts.
- **Multi-Host Concurrent Browsing**: Renders individual machine cards in a responsive grid, allowing concurrent navigation across distinct remote file trees.
- **Cross-Machine File Transfers**: Stream-based file and recursive directory copying/moving between any two machines on the mesh using buffered 64 KB chunks, without caching entire files to local disk.
- **Safety & Permission System**: Enforces security policies to prevent destructive operations on system root directories (`/etc`, `/bin`, `C:\Windows`, etc.).
- **Glassmorphic Cyber UI**: Hardware-accelerated Qt6 desktop interface featuring glass styling, dynamic state glows, custom drag-and-drop pill previews, and runtime theme switching.

---

## 2. Technology Stack & Prerequisites

| Layer | Technology / Library | Purpose |
| :--- | :--- | :--- |
| **Language** | C++17 / C++20 | Core logic, memory safety, concurrency |
| **GUI Framework** | Qt6 (`Core`, `Gui`, `Widgets`, `Network`) | UI shell, models, event loop, networking |
| **SSH / SFTP Engine** | `libssh` (>= 0.9.0) | Secure SSH session management & SFTP I/O |
| **Build System** | CMake (>= 3.21) | Cross-platform build orchestration |
| **Network Discovery** | Tailscale CLI (`tailscale status --json`) | Local peer status extraction |

---

## 3. Directory Layout & Module Structure

```text
DirZero/
├── CMakeLists.txt                  # Root CMake definition
├── status.md                       # High-level task & transition status log
├── description.md                  # Comprehensive architectural guide (this document)
├── src/
│   ├── CMakeLists.txt              # Target compilation, dependencies & flags
│   ├── main.cpp                    # Application entry point & CLI parser
│   │
│   ├── core/                       # Shared domain models & security policies
│   │   ├── MachineInfo.hpp/.cpp    # Machine metadata model (IP, name, OS, status)
│   │   ├── MachineState.hpp        # Enumeration of connection & operational states
│   │   ├── AuthState.hpp           # Auth credential states
│   │   ├── AccessRule.hpp/.cpp     # Path matching & access permissions
│   │   └── PermissionPolicy.hpp/.cpp # Pre-execution validation safety layer
│   │
│   ├── security/                   # SSH key discovery
│   │   ├── SshKeyDiscovery.hpp/.cpp# Optional discovery of standard ~/.ssh key paths
│   │
│   ├── fs/                         # Remote filesystem modeling & libssh backend
│   │   ├── RemoteEntry.hpp         # Plain metadata struct for remote files/directories
│   │   ├── FSNode.hpp/.cpp         # Hierarchical tree node structure (lazy-loadable)
│   │   ├── SFTPManager.hpp/.cpp    # libssh session, directory listing, streaming I/O
│   │   └── RemoteFSModel.hpp/.cpp  # QAbstractItemModel binding FSNode tree to QTreeView
│   │
│   ├── discovery/                  # Tailnet & SSH probing
│   │   ├── TailscaleDiscovery.hpp/.cpp # Tailscale JSON CLI runner & IP parser
│   │   └── ProbeWorker.hpp/.cpp    # TCP socket port 22 checker worker
│   │
│   ├── workers/                    # Asynchronous non-blocking QRunnable jobs
│   │   ├── WorkerSignals.hpp       # QObject signal broker for worker threads
│   │   ├── DiscoveryWorker.hpp/.cpp# Background peer discovery runner
│   │   ├── SFTPConnectWorker.hpp/.cpp # Async SSH authentication & session initialization
│   │   ├── SFTPListWorker.hpp/.cpp # Async directory listing
│   │   ├── SFTPFileOpWorker.hpp/.cpp # Async file CRUD (create, delete, rename)
│   │   └── CrossMachineTransferWorker.hpp/.cpp # Buffered chunk streaming between endpoints
│   │
│   ├── helpers/                    # Shared UI utilities & layout management
│   │   ├── RemoteClipboard.hpp/.cpp# Cross-panel copy/cut clipboard state singleton
│   │   └── ResponsiveGridContainer.hpp/.cpp # Width-aware dynamic column layout container
│   │
│   ├── ui/                         # Presentation layer & widgets
│   │   ├── ThemeManager.hpp/.cpp   # Embedded glass theme engine & persistence
│   │   ├── MachineAuthBar.hpp/.cpp # Inline authentication bar (user, password, key picker)
│   │   ├── RemoteFileTreeView.hpp/.cpp # QTreeView subclass with custom drag preview & hotkeys
│   │   ├── MachinePanelWidget.hpp/.cpp # Complete machine card controller & view
│   │   └── Dir2ZeroWindow.hpp/.cpp # This PC drive browser, Tailnet view switcher, header & transfer dock
│   │
│   └── themes/                     # External fallback/reference stylesheets (.qss)
```

---

## 4. Key Architectural Subsystems

```
                                  ┌────────────────────────┐
                                  │      main.cpp          │
                                  └───────────┬────────────┘
                                              │
                                  ┌───────────▼────────────┐
                                  │    Dir2ZeroWindow      │
                                  └─────┬────────────┬─────┘
                                        │            │
            ┌───────────────────────────┘            └──────────────────────────┐
            ▼                                                                   ▼
┌───────────────────────┐                                           ┌───────────────────────┐
│   DiscoveryWorker     │                                           │  ThemeManager         │
│ (Tailscale Discovery) │                                           │  (Glassmorphic Styles)│
└───────────┬───────────┘                                           └───────────────────────┘
            │
            │ Reconciles Peer List
            ▼
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│                               ResponsiveGridContainer                                     │
│  ┌───────────────────────────────┐                 ┌───────────────────────────────┐      │
│  │     MachinePanelWidget        │                 │     MachinePanelWidget        │      │
│  │  ┌─────────────────────────┐  │                 │  ┌─────────────────────────┐  │      │
│  │  │    MachineAuthBar       │  │                 │  │    MachineAuthBar       │  │      │
│  │  └─────────────────────────┘  │                 │  └─────────────────────────┘  │      │
│  │  ┌─────────────────────────┐  │                 │  ┌─────────────────────────┐  │      │
│  │  │   RemoteFileTreeView    │  │  ◄────────────► │  │   RemoteFileTreeView    │  │      │
│  │  │    (RemoteFSModel)      │  │    Drag & Drop  │  │    (RemoteFSModel)      │  │      │
│  │  └───────────┬─────────────┘  │   Cross-Transfer│  └───────────┬─────────────┘  │      │
│  └──────────────┼────────────────┘                 └──────────────┼────────────────┘      │
└─────────────────┼─────────────────────────────────────────────────┼───────────────────────┘
                  │                                                 │
                  ▼                                                 ▼
        ┌───────────────────┐                             ┌───────────────────┐
        │    SFTPManager    │                             │    SFTPManager    │
        │     (libssh)      │                             │     (libssh)      │
        └───────────────────┘                             └───────────────────┘
```

### A. Discovery & Network Probing
- `TailscaleDiscovery`: Finds the `tailscale` binary across Linux, macOS, and Windows default paths, then executes `tailscale status --json`.
- It processes both the `Self` node and the `Peer` map, filtering for currently online nodes.
- `ProbeWorker`: Establishes a lightweight non-blocking `QTcpSocket` connection to port 22 with a 3-second timeout to determine if SSH is reachable.

### B. SSH & SFTP Engine (`SFTPManager`)
- Implemented with `libssh` (C API wrapped in a clean, RAII C++ class).
- Supports multiple authentication cascades:
  1. Explicit private key file path (e.g. `~/.ssh/id_ed25519`).
  2. Running SSH Agent identities.
  3. Default system SSH keys (`id_ed25519`, `id_rsa`, `id_ecdsa`, `id_dsa`).
  4. Interactive password authentication.
  5. `none` fallback.
- Thread safety is guaranteed via internal `std::mutex` locking during I/O transactions.

### C. Tree View & Lazy-Loading Model (`FSNode` & `RemoteFSModel`)
- `RemoteFSModel` inherits `QAbstractItemModel`.
- Direct folder expansion triggers an asynchronous `SFTPListWorker`.
- Unloaded child folders contain a placeholder dummy node (`"Loading..."`). Upon worker completion, rows are dynamically inserted via `beginInsertRows()` / `endInsertRows()`.

### D. Cross-Machine Stream Transfers (`CrossMachineTransferWorker`)
- Transfers between two machines do not save intermediate files to local disk.
- Worker opens `openFileRead` on source `SFTPManager` and `openFileWrite` on destination `SFTPManager`.
- Reads and writes through a `64 KB` buffer while computing transfer speed, elapsed time, and percentage.
- Progress updates are emitted via `WorkerSignals::progress` directly to the `Dir2ZeroWindow` dock.
- If source and destination are on the same machine and the operation is a `Cut`, it optimizes into a direct atomic remote rename.

### E. Permission & Safety Engine (`PermissionPolicy`)
- Blocks accidental destructive actions against system root paths (`/`, `/bin`, `/sbin`, `/etc`, `/usr`, `/lib`, `C:\Windows`, `C:\Program Files`).
- Sanitizes file/folder names to prevent directory traversal (`/` or `\` in input).
- Checked before all `create_file`, `create_dir`, `rename`, `delete`, and `transfer` calls.

### F. Theme System & Glass Styling (`ThemeManager`)
- 4 built-in glass themes:
  1. `Cyber Glass Dark` (Flagship glowing cyan/sapphire theme)
  2. `Midnight Glass` (Minimalist slate/dark theme)
  3. `Emerald Matrix` (Neon green terminal glass aesthetic)
  4. `Glass Frost Light` (High-contrast clean frosted light mode)
- Theme preference is remembered across runs in `.active_theme`.

---

## 5. Build, Test, and Execution Instructions

### Build Prerequisites
```bash
# Ubuntu / Debian
sudo apt-get install build-essential cmake qt6-base-dev libssh-dev pkg-config

# Fedora
sudo dnf install gcc-c++ cmake qt6-qtbase-devel libssh-devel pkgconfig

# Arch Linux
sudo pacman -S base-devel cmake qt6-base libssh pkgconf
```

### Compiling & Running
```bash
# 1. Generate CMake build directory
cmake -B build -S .

# 2. Compile binary
cmake --build build -j$(nproc)

# 3. Launch application
./build/src/DirZero
```

### Command Line Arguments
```bash
./build/src/DirZero --user <username> --key <path-to-private-key>
```

---

## 6. Guide for Future Agents & Developers

When making modifications or adding features, adhere to these guidelines:

1. **Keep Worker Signals Qt-Safe**:
   - Never name a member variable or method `signals` in Qt classes (it conflicts with the `#define signals Q_SIGNALS` preprocessor macro). Always use `workerSignals` or specific names.
2. **Never Perform Network I/O on the GUI Thread**:
   - Any operation touching `SFTPManager`, `TailscaleDiscovery`, or TCP sockets must run inside a `QRunnable` on `QThreadPool::globalInstance()`, reporting back to the UI via `WorkerSignals`.
3. **Always Validate with `PermissionPolicy`**:
   - Before executing remote mutations, validate the target paths through `PermissionPolicy::defaultPolicy()`.
4. **Maintain `status.md`**:
   - Update `status.md` whenever adding new files, refactoring subsystems, or completing roadmap milestones so subsequent agents have immediate situational context.
