# Recreating DirZero in C++ (Qt6 + libssh2): A Complete Step-by-Step Blueprint

Welcome! You have just completed a C++ programming course covering variables, control structures, functions, pointers, classes, inheritance, polymorphism, templates, and the C++ Standard Library (`std::vector`, `std::string`, `std::map`).

Now, you are ready to apply your knowledge to build a real-world software product: **DirZero C++**, a multi-threaded, cross-platform GUI remote file manager and network diagnostic tool.

This document is your **Master Teacher's Guide**. Instead of just giving you pre-written code or vague summaries, this guide breaks down the exact **mental model**, **file structure**, **header definitions**, **logic flow**, and **step-by-step method** required to build every single part of the application in C++.

---

## Table of Contents

1. [The Software Architecture Mindset](#1-the-software-architecture-mindset)
2. [Project File Hierarchy](#2-project-file-hierarchy)
3. [Lesson 1: Build System & Dependencies (`CMakeLists.txt`)](#lesson-1-build-system--dependencies-cmakelists-txt)
4. [Lesson 2: Core Data Types & Enums (`MachineState.h` & `MachineInfo.h`)](#lesson-2-core-data-types--enums-machinestate-h--machineinfo-h)
5. [Lesson 3: Tree Data Hierarchy & Formatting (`FSNode.h` & `FSNode.cpp`)](#lesson-3-tree-data-hierarchy--formatting-fsnode-h--fsnode-cpp)
6. [Lesson 4: Secure Local Credential Storage (`CredentialStore`)](#lesson-4-secure-local-credential-storage-credentialstore)
7. [Lesson 5: Tailscale Discovery & Socket Probing (`TailscaleDiscovery`)](#lesson-5-tailscale-discovery--socket-probing-tailscalediscovery)
8. [Lesson 6: Remote SFTP Network Engine (`SFTPManager` via `libssh2`)](#lesson-6-remote-sftp-network-engine-sftpmanager-via-libssh2)
9. [Lesson 7: Multithreading Architecture & Signals (`WorkerSignals` & `QRunnable`)](#lesson-7-multithreading-architecture--signals-workersignals--qrunnable)
10. [Lesson 8: Custom Qt Model-View System (`RemoteFSModel` & `RemoteFileTreeView`)](#lesson-8-custom-qt-model-view-system-remotefsmodel--remotefiletreeview)
11. [Lesson 9: Cross-Panel Clipboard Singleton (`RemoteClipboard`)](#lesson-9-cross-panel-clipboard-singleton-remoteclipboard)
12. [Lesson 10: Card UI Components & Responsive Layouts](#lesson-10-card-ui-components--responsive-layouts)
13. [Lesson 11: Main Window, Smart Reconciliation & Entry Point (`main.cpp`)](#lesson-11-main-window-smart-reconciliation--entry-point-main-cpp)
14. [Step-by-Step Student Milestone Checklist](#14-step-by-step-student-milestone-checklist)

---

## 1. The Software Architecture Mindset

Building a desktop app in C++ requires separating concerns into five distinct layers:

```
+-------------------------------------------------------------------------------+
| LAYER 5: GUI Presentation Layer (Qt Widgets, Layouts, Windows, Themes)       |
|          Dir2ZeroWindow, MachinePanelWidget, ResponsiveGridContainer, AuthBar |
+-------------------------------------------------------------------------------+
                                    | Signals / Slots
+-------------------------------------------------------------------------------+
| LAYER 4: Qt Item Model & Drag-and-Drop View (Model-View Framework)            |
|          RemoteFSModel (QAbstractItemModel), RemoteFileTreeView (QTreeView)   |
+-------------------------------------------------------------------------------+
                                    | Qt Signals & QThreadPool
+-------------------------------------------------------------------------------+
| LAYER 3: Asynchronous Thread Pool & Workers (Non-Blocking GUI Multithreading) |
|          DiscoveryWorker, PortCheckWorker, SFTPConnectWorker, TransferWorker  |
+-------------------------------------------------------------------------------+
                                    | Direct C++ API Calls
+-------------------------------------------------------------------------------+
| LAYER 2: Core Protocol Engine & Subprocess CLI Tools                          |
|          SFTPManager (libssh2), TailscaleDiscovery (QProcess & QTcpSocket)    |
+-------------------------------------------------------------------------------+
                                    | Raw Data & Disk Access
+-------------------------------------------------------------------------------+
| LAYER 1: Data Structures, Enums & Security Persistence                        |
|          FSNode (Tree Model), MachineInfo, MachineState, CredentialStore      |
+-------------------------------------------------------------------------------+
```

---

## 2. Project File Hierarchy

Create the following folder and file structure in your project workspace:

```
DirZeroCpp/
├── CMakeLists.txt
├── include/
│   ├── FSNode.h
│   ├── MachineInfo.h
│   ├── MachineState.h
│   ├── CredentialStore.h
│   ├── TailscaleDiscovery.h
│   ├── SFTPManager.h
│   ├── WorkerSignals.h
│   ├── Workers.h
│   ├── RemoteFSModel.h
│   ├── RemoteFileTreeView.h
│   ├── RemoteClipboard.h
│   ├── MachineAuthBar.h
│   ├── ResponsiveGridContainer.h
│   ├── MachinePanelWidget.h
│   ├── Dir2ZeroWindow.h
│   └── ThemeManager.h
└── src/
    ├── main.cpp
    ├── FSNode.cpp
    ├── CredentialStore.cpp
    ├── TailscaleDiscovery.cpp
    ├── SFTPManager.cpp
    ├── Workers.cpp
    ├── RemoteFSModel.cpp
    ├── RemoteFileTreeView.cpp
    ├── RemoteClipboard.cpp
    ├── MachineAuthBar.cpp
    ├── ResponsiveGridContainer.cpp
    ├── MachinePanelWidget.cpp
    ├── Dir2ZeroWindow.cpp
    └── ThemeManager.cpp
```

---

## Lesson 1: Build System & Dependencies (`CMakeLists.txt`)

### The Method
Unlike Python where you run `import paramiko`, C++ requires explicit compilation and linking using **CMake**.

### What to Create: `CMakeLists.txt`
In C++, Qt uses a special tool called **MOC (Meta-Object Compiler)** to process `Q_OBJECT` macros, signals, and slots. You must set `CMAKE_AUTOMOC ON` in CMake.

```cmake
cmake_minimum_required(VERSION 3.16)
project(DirZero LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt Meta-Object Compiler (MOC) for Signals and Slots
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 Modules
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network Gui)

# Find Header-Only JSON Library
find_package(nlohmann_json REQUIRED)

# Find libssh2 C Library
find_path(LIBSSH2_INCLUDE_DIR libssh2.h)
find_library(LIBSSH2_LIBRARY NAMES ssh2 libssh2)

include_directories(include ${LIBSSH2_INCLUDE_DIR})

# Source files list
file(GLOB SOURCES "src/*.cpp")

add_executable(DirZero ${SOURCES})

target_link_libraries(DirZero PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Network
    Qt6::Gui
    ${LIBSSH2_LIBRARY}
    nlohmann_json::nlohmann_json
)
```

---

## Lesson 2: Core Data Types & Enums (`MachineState.h` & `MachineInfo.h`)

### The Method
Always define your primitive data structures before writing complex GUI components.

### 1. `include/MachineState.h`
Use a strongly typed `enum class` to prevent accidental integer assignment bugs:

```cpp
#pragma once

enum class MachineState {
    DISCOVERING,
    ONLINE_SSH_OK,
    ONLINE_SSH_UNAVAILABLE,
    AUTH_REQUIRED,
    CONNECTING,
    CONNECTED,
    ERROR,
    DISCONNECTED,
    OFFLINE
};
```

### 2. `include/MachineInfo.h`
Define a struct storing metadata for a Tailscale node:

```cpp
#pragma once
#include <string>

struct MachineInfo {
    std::string name;
    std::string dnsName;
    std::string ip;
    std::string osType = "linux";
    bool online = true;
    bool sshAvailable = false;
    bool isSelf = false;
};
```

---

## Lesson 3: Tree Data Hierarchy & Formatting (`FSNode.h` & `FSNode.cpp`)

### The Method
Qt's `QTreeView` displays hierarchical data (folders inside folders). To supply data to Qt, you must write a tree node class (`FSNode`) where each node holds a raw pointer to its parent and a `std::vector` of pointers to its children.

### 1. Header Definition: `include/FSNode.h`
```cpp
#pragma once
#include <string>
#include <vector>
#include <sys/stat.h>
#include <ctime>

class FSNode {
public:
    std::string name;
    std::string path;
    bool isDir = false;
    uint64_t size = 0;
    time_t mtime = 0;
    mode_t mode = 0;

    FSNode* parent = nullptr;
    std::vector<FSNode*> children;

    bool isLoaded = false;
    bool isLoading = false;
    bool isDummy = false;
    std::string error;

    FSNode(const std::string& name, const std::string& path, bool isDir = false,
           uint64_t size = 0, time_t mtime = 0, mode_t mode = 0,
           FSNode* parent = nullptr, bool isDummy = false);

    ~FSNode(); // Destructor must delete all allocated child pointers!

    int row() const;
    FSNode* child(int row);
    int childCount() const;

    std::string sizeFormatted() const;
    std::string mtimeFormatted() const;
    std::string permissionsFormatted() const;
};
```

### 2. Implementation Logic: `src/FSNode.cpp`

- **Destructor Memory Cleanup**:
  ```cpp
  FSNode::~FSNode() {
      for (FSNode* child : children) {
          delete child; // Prevents memory leaks when tree is deleted
      }
      children.clear();
  }
  ```

- **Row Lookup (`row()`)**:
  Finds this node's index inside `parent->children`:
  ```cpp
  int FSNode::row() const {
      if (parent) {
          auto it = std::find(parent->children.begin(), parent->children.end(), this);
          if (it != parent->children.end()) {
              return std::distance(parent->children.begin(), it);
          }
      }
      return 0;
  }
  ```

- **The Lazy Loading Dummy Node Pattern**:
  In the `FSNode` constructor, if `isDir == true` and `isDummy == false`, automatically append a dummy child:
  ```cpp
  if (isDir && !isDummy) {
      children.push_back(new FSNode("Loading...", "", false, 0, 0, 0, this, true));
  }
  ```
  *Why?* Qt's `QTreeView` checks `childCount() > 0` to decide whether to show an expand arrow (`>`). When the user clicks expand, you remove the dummy node and fetch real remote directory entries via SFTP!

- **String Formatting Helpers**:
  - `sizeFormatted()`: If `size < 1024` return `"X B"`; if `< 1048576` return `"X KB"`, etc.
  - `permissionsFormatted()`: Convert POSIX mode bits (`mode & S_IRUSR`, `S_IWUSR`, `S_IXUSR`) to `-rw-r--r--` string.

---

## Lesson 4: Secure Local Credential Storage (`CredentialStore`)

### The Method
Store saved login credentials (username, password, private key path) locally in JSON format, locked with OS-level permission attributes.

### Step-by-Step Implementation (`CredentialStore.h` / `.cpp`):
1. **Cross-Platform Directory Lookup**: Use `QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)`.
   - Windows: `%APPDATA%\DirZero\credentials.json`
   - Linux: `~/.config/dirzero/credentials.json`
   - macOS: `~/Library/Application Support/DirZero/credentials.json`
2. **File Permission Lockdown**:
   After writing `credentials.json`, invoke POSIX permissions call on Linux/macOS:
   ```cpp
   #ifndef _WIN32
   chmod(configDir.c_str(), 0700);  // Read/Write/Execute by owner only
   chmod(filePath.c_str(), 0600);   // Read/Write by owner only
   #endif
   ```
3. **JSON Methods**:
   Implement `save(hostKey, username, password, keyPath)`, `loadAll()`, and `remove(hostKey)` using `nlohmann::json`.

---

## Lesson 5: Tailscale Discovery & Socket Probing (`TailscaleDiscovery`)

### The Method
Discover nodes on your Tailscale overlay network by running the `tailscale` CLI command asynchronously and parsing its JSON status.

### 1. `include/TailscaleDiscovery.h`
```cpp
#pragma once
#include "MachineInfo.h"
#include <vector>
#include <string>

class TailscaleDiscovery {
public:
    static std::string findTailscaleBinary();
    static bool checkPort22(const std::string& ip, int timeoutMs = 3000);
    static std::vector<MachineInfo> getOnlineMachines(bool checkSsh = true);
};
```

### 2. Implementation Details (`src/TailscaleDiscovery.cpp`):

- **Running CLI Command (`getOnlineMachines`)**:
  Use Qt's `QProcess`:
  ```cpp
  QProcess proc;
  proc.start(QString::fromStdString(tsBinary), QStringList() << "status" << "--json");
  if (proc.waitForFinished(6000)) {
      QByteArray data = proc.readAllStandardOutput();
      auto json = nlohmann::json::parse(data.toStdString());

      // 1. Read json["Self"] -> Extract local host IP, name, OS
      // 2. Loop json["Peer"] -> Extract remote node IPs, names, online state
  }
  ```

- **Socket Probing (`checkPort22`)**:
  Test if SSH is accessible without freezing the UI:
  ```cpp
  QTcpSocket socket;
  socket.connectToHost(QString::fromStdString(ip), 22);
  return socket.waitForConnected(timeoutMs);
  ```

---

## Lesson 6: Remote SFTP Network Engine (`SFTPManager` via `libssh2`)

### The Method
Wrap `libssh2` C functions inside an object-oriented C++ class.

### 1. `include/SFTPManager.h`
```cpp
#pragma once
#include <string>
#include <vector>
#include <libssh2.h>
#include <libssh2_sftp.h>

struct FileEntry {
    std::string name;
    std::string path;
    bool isDir;
    uint64_t size;
    time_t mtime;
    uint32_t mode;
};

class SFTPManager {
private:
    std::string host;
    int port;
    std::string username;
    std::string keyFilePath;
    std::string password;

    int sock = -1;
    LIBSSH2_SESSION* session = nullptr;
    LIBSSH2_SFTP* sftp = nullptr;

public:
    SFTPManager(std::string host, int port = 22, std::string username = "",
                std::string keyFilePath = "", std::string password = "");
    ~SFTPManager();

    bool connect();
    void disconnect();
    bool isConnected() const;

    std::vector<FileEntry> listDirectory(const std::string& remotePath);
    void createFile(const std::string& remotePath);
    void createDirectory(const std::string& remotePath);
    void deleteFile(const std::string& remotePath);
    void deleteDirectoryRecursive(const std::string& remotePath);
    void rename(const std::string& oldPath, const std::string& newPath);
    std::pair<uint64_t, uint64_t> calculateTreeSize(const std::string& remotePath);
};
```

### 2. Implementation Steps (`src/SFTPManager.cpp`):

1. **`connect()`**:
   - Create socket: `sock = socket(AF_INET, SOCK_STREAM, 0);`.
   - Connect socket to remote IP on port 22.
   - Init session: `session = libssh2_session_init();`.
   - Handshake: `libssh2_session_handshake(session, sock);`.
   - Authenticate via public key (`libssh2_userauth_publickey_fromfile`) or password (`libssh2_userauth_password`).
   - Open SFTP stream: `sftp = libssh2_sftp_init(session);`.

2. **`listDirectory()`**:
   - Open directory stream: `LIBSSH2_SFTP_HANDLE* h = libssh2_sftp_opendir(sftp, path.c_str());`.
   - Read entries loop: `libssh2_sftp_readdir(h, mem, sizeof(mem), &attrs);`.
   - Fill `FileEntry` structs and close handle: `libssh2_sftp_closedir(h);`.

---

## Lesson 7: Multithreading Architecture & Signals (`WorkerSignals` & `QRunnable`)

### The Method
**Never block Qt's main GUI event loop with network calls!**
Create thread workers inheriting from `QRunnable` and send results to the GUI using a Qt `QObject` signal bridge.

### 1. The Signal Bridge (`include/WorkerSignals.h`)
```cpp
#pragma once
#include <QObject>
#include <QVariant>

class WorkerSignals : public QObject {
    Q_OBJECT
signals:
    void started();
    void finished();
    void error(QString errorMsg);
    void resultReady(QVariant result);
    void progress(qint64 bytesTransferred, qint64 totalBytes, QString itemName, double bytesPerSec);
};
```

### 2. Thread Workers (`include/Workers.h`)
Define workers for async tasks:
- `DiscoveryWorker : public QRunnable` -> runs `TailscaleDiscovery::getOnlineMachines()`.
- `PortCheckWorker : public QRunnable` -> runs `checkPort22()`.
- `SFTPConnectWorker : public QRunnable` -> runs `sftpManager->connect()`.
- `SFTPListWorker : public QRunnable` -> runs `sftpManager->listDirectory()`.
- `SFTPFileOpWorker : public QRunnable` -> runs create/delete/rename operations.
- `CrossMachineTransferWorker : public QRunnable` -> streams files between hosts.

#### High-Performance Streaming Transfer Loop:
Inside `CrossMachineTransferWorker::run()`:
```cpp
char buffer[65536]; // 64 KB Buffer
qint64 totalTransferred = 0;
auto startTime = std::chrono::steady_clock::now();

while (bytesRemaining > 0 && !isCancelled) {
    int bytesRead = srcSFTP->read(buffer, sizeof(buffer));
    dstSFTP->write(buffer, bytesRead);

    totalTransferred += bytesRead;
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
    double speed = totalTransferred / std::max(0.001, elapsed);

    // Emit progress signal to UI thread
    emit signals.progress(totalTransferred, totalBytes, fileName, speed);
}
```

Dispatch workers to Qt's thread pool: `QThreadPool::globalInstance()->start(worker);`.

---

## Lesson 8: Custom Qt Model-View System (`RemoteFSModel` & `RemoteFileTreeView`)

### The Method
Connect your `FSNode` C++ tree to Qt's visual `QTreeView` widget by inheriting from `QAbstractItemModel`.

### 1. `include/RemoteFSModel.h`
Override the 6 required methods:

```cpp
#pragma once
#include <QAbstractItemModel>
#include "FSNode.h"

class RemoteFSModel : public QAbstractItemModel {
    Q_OBJECT
private:
    FSNode* rootNode;

public:
    explicit RemoteFSModel(FSNode* root, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex index(int row, int col, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void populateNode(FSNode* parentNode, const std::vector<FileEntry>& entries, const QModelIndex& parentIndex);
};
```

### 2. Drag-and-Drop Tree View (`include/RemoteFileTreeView.h`)
Subclass `QTreeView` to enable cross-machine drag and drop:
- `startDrag()`: Serialize item metadata (source IP, host name, remote path) into JSON bytes. Set MIME type `application/x-dirzero-item`. Render a floating drag pixmap badge using `QPixmap` and `QPainter`.
- `dragEnterEvent()` & `dragMoveEvent()`: Accept events containing `application/x-dirzero-item`.
- `dropEvent()`: Extract JSON payload, determine drop target `FSNode`, and trigger `CrossMachineTransferWorker`!

---

## Lesson 9: Cross-Panel Clipboard Singleton (`RemoteClipboard`)

### The Method
Implement a Thread-Safe Singleton class to share copied/cut files across separate machine card panels.

### `include/RemoteClipboard.h`
```cpp
#pragma once
#include <QObject>
#include <string>

class MachinePanelWidget; // Forward declaration

class RemoteClipboard : public QObject {
    Q_OBJECT
private:
    static RemoteClipboard* instancePtr;
    RemoteClipboard() = default;

public:
    static RemoteClipboard* instance();

    MachinePanelWidget* sourcePanel = nullptr;
    std::string sourcePath;
    std::string sourceName;
    bool isDir = false;
    bool isCut = false;

    void copy(MachinePanelWidget* panel, const std::string& path, const std::string& name, bool isDir);
    void cut(MachinePanelWidget* panel, const std::string& path, const std::string& name, bool isDir);
    void clear();
    bool hasItem() const;

signals:
    void clipboardChanged();
};
```

---

## Lesson 10: Card UI Components & Responsive Layouts

### 1. In-App Auth Bar (`MachineAuthBar`)
Build a collapsible `QFrame` widget containing:
- Username input (`QLineEdit`).
- Password input (`QLineEdit` with `EchoMode::Password`) + Show/Hide eye button (`QPushButton`).
- Key file path field + `QFileDialog` browser button.
- "Connect" button.

### 2. Responsive Grid Container (`ResponsiveGridContainer`)
Build a custom `QWidget` wrapping a `QGridLayout`. Override `resizeEvent()`:
```cpp
void ResponsiveGridContainer::resizeEvent(QResizeEvent* event) {
    int containerWidth = width();
    int cols = std::max(1, containerWidth / 380); // Min column width 380px
    cols = std::min(cols, 4);                     // Max 4 columns

    if (cols != currentCols) {
        rearrangeGrid(cols);
    }
}
```

### 3. Machine Panel Host Card (`MachinePanelWidget`)
Build host card `QFrame` widget containing:
- Heading label (OS Icon + Host Name + Self badge).
- Status label (`CONNECTED`, `AUTH_REQUIRED`, etc.).
- Action toolbar (`+ File`, `+ Folder`, `Paste`, `Auth`, `Refresh`, `Disconnect`).
- Collapsible `MachineAuthBar`.
- `RemoteFileTreeView`.

---

## Lesson 11: Main Window, Smart Reconciliation & Entry Point (`main.cpp`)

### 1. Main Window (`Dir2ZeroWindow`) Structure
- **Header Bar**: Title, stats badges (`Online`, `Reachable`, `Unavailable`), Theme selector dropdown, Scan button.
- **Scroll Area**: Embeds `ResponsiveGridContainer`.
- **Bottom Transfer Progress Dock**: `QProgressBar` + live transfer speed label (KB/s or MB/s).
- **Auto-Discovery Timer (`QTimer`)**: Fires every 20,000 ms (20 seconds).

### 2. Smart Reconciliation Algorithm
When background discovery finishes:
```cpp
void Dir2ZeroWindow::onDiscoveryCompleted(const std::vector<MachineInfo>& machines) {
    // 1. Update existing card reachability states WITHOUT re-instantiating widgets
    // 2. Add new MachinePanelWidget cards for newly discovered hosts
    // 3. Remove MachinePanelWidget cards for hosts that went offline
}
```
*Why?* Smart reconciliation keeps active SFTP streams open and preserves expanded tree view node states while refreshing network reachability!

### 3. Application Entry Point (`src/main.cpp`)
```cpp
#include <QApplication>
#include "Dir2ZeroWindow.h"
#include "ThemeManager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("DirZero");

    // Apply QSS theme
    ThemeManager::instance()->applyCurrentTheme(&app);

    Dir2ZeroWindow window;
    window.show();

    return app.exec(); // Enter Qt Event Loop
}
```

---

## 14. Step-by-Step Student Milestone Checklist

Follow this exact sequence to build and verify your project:

- [ ] **Milestone 1**: Write `CMakeLists.txt` and verify project compiles with Qt6 and libssh2.
- [ ] **Milestone 2**: Write `FSNode` and test tree node memory creation/deletion in console.
- [ ] **Milestone 3**: Implement `TailscaleDiscovery` and verify it prints online Tailscale IPs in console.
- [ ] **Milestone 4**: Implement `checkPort22()` socket probe and print reachable SSH nodes.
- [ ] **Milestone 5**: Implement `SFTPManager` and test connecting to your local SSH server.
- [ ] **Milestone 6**: Implement `RemoteFSModel` (`QAbstractItemModel`) and render files in a standard `QTreeView`.
- [ ] **Milestone 7**: Create `MachineAuthBar` and `MachinePanelWidget` host card UI.
- [ ] **Milestone 8**: Implement `ResponsiveGridContainer` and verify grid column rearrangement on window resize.
- [ ] **Milestone 9**: Create `CrossMachineTransferWorker` and test background file streaming with live progress bar!

---

### Teacher's Final Advice
> "Building a real desktop software product in C++ is like building a house. Lay down the foundation (Data Models & SFTP network functions) before painting the walls (Qt Widgets & Stylesheets). Build one milestone at a time, test in console, and then connect your thread workers to your GUI controls!"
