# Rebuilding DirZero in C++: A general Step-by-Step approach how we gonna do this shit.

## What This Document Is

This is a learning path for recreating the Python application in C++ with Qt 6.
It assumes that you have completed a basic C++ course: variables, functions,
classes, inheritance, pointers, references, STL containers, file I/O, and basic
error handling.

The goal is not to hand you a finished C++ translation. The goal is to teach you
how to think through the rebuild, what to create first, why the parts depend on
each other, and what to test after each step. Treat the application like a large
math problem: first identify the quantities and relationships, then solve the
small subproblems, then combine the answers.

The Python file is already a working reference. Keep it open while building,
but do not translate it mechanically line by line. Python hides many things that
C++ makes explicit:

- ownership and object lifetime;
- types and conversions;
- thread safety;
- build configuration and linking;
- error representation;
- UI object ownership;
- third-party library setup.

Those hidden details are the actual lessons in this project.

---

## 1. First Understand the Problem

DirZero is not one problem. It is several smaller systems connected together:

1. **Discovery:** find online machines from Tailscale.
2. **Reachability:** test whether SSH port 22 accepts a TCP connection.
3. **Authentication:** establish an SSH connection with a key, agent, or password.
4. **Remote filesystem:** list and modify files through SFTP.
5. **Data model:** represent remote folders and files as a tree.
6. **GUI:** show the tree and machine status in Qt widgets.
7. **Concurrency:** perform slow network operations away from the GUI thread.
8. **Transfers:** stream bytes between two remote SFTP sessions.
9. **Persistence:** remember credentials and the selected theme.
10. **Reconciliation:** update machine cards when the Tailscale network changes.

The most important architectural rule is this:

> The GUI should describe and request work. Service classes should perform work.
> Worker classes should move blocking work away from the GUI thread. Models should
> expose data to views. Widgets should react to results.

If you put SSH code directly in a button-click handler, the window will freeze.
If a worker directly edits a `QTreeView`, you will create thread bugs. If every
widget parses Tailscale JSON itself, the program becomes difficult to test.

---

## 2. The Order You Should Build It

Build the application in this order:

```text
A. Create a tiny Qt window
B. Make CMake build it reliably
C. Create plain data types
D. Create and test the filesystem tree in memory
E. Add credential persistence
F. Add Tailscale discovery
G. Add SSH/SFTP behind a service interface
H. Add one worker and prove the GUI stays responsive
I. Add the Qt tree model
J. Add one machine panel
K. Add authentication and connection states
L. Add file operations
M. Add transfers and progress
N. Add multiple panels and reconciliation
O. Add themes, drag/drop, shortcuts, and cleanup
```

Do not begin with drag-and-drop. Do not begin with SFTP. Do not begin by
copying every Python class into one giant `main.cpp`. Each later stage depends on
an earlier stage being understandable and testable.

The suggested C++ project layout is:

```text
DirZero/
  CMakeLists.txt
  CMakePresets.json                  optional
  resources.qrc                      optional
  ui/
    Dir2Zero.ui
    MachinePanel.ui
  themes/
    cybersecurity_dark.qss
    classic_dark.qss
    contrast.qss
    light_grayscale.qss
    midnight_cyber.qss
  src/
    main.cpp
    core/
      MachineState.hpp
      MachineInfo.hpp
      FSNode.hpp
      FSNode.cpp
      RemoteClipboard.hpp
      RemoteClipboard.cpp
      RemoteEntry.hpp
    persistence/
      CredentialStore.hpp
      CredentialStore.cpp
    discovery/
      TailscaleDiscovery.hpp
      TailscaleDiscovery.cpp
    network/
      ISftpManager.hpp
      Libssh2SftpManager.hpp
      Libssh2SftpManager.cpp
    workers/
      WorkerSignals.hpp
      DiscoveryWorker.hpp
      DiscoveryWorker.cpp
      SftpConnectWorker.hpp
      SftpConnectWorker.cpp
      SftpListWorker.hpp
      SftpListWorker.cpp
      FileOperationWorker.hpp
      FileOperationWorker.cpp
      TransferWorker.hpp
      TransferWorker.cpp
    model/
      RemoteFSModel.hpp
      RemoteFSModel.cpp
    widgets/
      MachineAuthBar.hpp
      MachineAuthBar.cpp
      RemoteFileTreeView.hpp
      RemoteFileTreeView.cpp
      ResponsiveGridContainer.hpp
      ResponsiveGridContainer.cpp
      MachinePanelWidget.hpp
      MachinePanelWidget.cpp
      Dir2ZeroWindow.hpp
      Dir2ZeroWindow.cpp
    theme/
      ThemeManager.hpp
      ThemeManager.cpp
  tests/
    test_fsnodes.cpp
    test_paths.cpp
    test_json.cpp
```

You do not need to create all these files at once. The list is the destination,
not your first task.

---

## 3. Phase A: Set Up a Small C++/Qt Program

### 3.1 Install the tools

On Debian or Ubuntu, install the basic build tools and Qt:

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-tools-dev
```

For SFTP later, also install the development package for the SSH library you
choose. A practical choice is `libssh2`:

```bash
sudo apt install libssh2-1-dev
```

You will also need a JSON library. You can use Qt's built-in JSON classes for
this project, which avoids adding another dependency:

- `QJsonDocument`
- `QJsonObject`
- `QJsonArray`
- `QJsonValue`

This guide uses Qt JSON instead of `nlohmann::json` so there are fewer things
to configure while you are learning.

### 3.2 Create the first `CMakeLists.txt`

Create `CMakeLists.txt` in the project root. At first, include only Qt Widgets.
Do not add libssh2 until a basic window builds.

Your first CMake file needs to communicate five facts:

1. The minimum CMake version.
2. The project name.
3. The C++ standard.
4. Which Qt package components are needed.
5. Which source files form the executable.

Conceptual shape:

```cmake
cmake_minimum_required(VERSION 3.21)
project(DirZero LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(DirZero
    src/main.cpp
)

target_link_libraries(DirZero PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)
```

What each line means:

- `CMAKE_CXX_STANDARD 17` enables `std::optional`, structured bindings, and
  other useful modern C++ features.
- `CMAKE_AUTOMOC` lets Qt generate meta-object code for classes using `Q_OBJECT`.
- `find_package` locates installed Qt modules.
- `add_executable` lists your own source files.
- `target_link_libraries` connects your program to Qt implementations.

### 3.3 Write the smallest `main.cpp`

Create a `QApplication`, create a `QMainWindow`, give it a title, show it, and
return `app.exec()`.

Do not add discovery or networking yet. Your first checkpoint is simply:

```text
CMake configures successfully.
The executable compiles.
A window appears.
Closing the window exits the program.
```

If this fails, stop and fix CMake or Qt installation first. Every later failure
will be harder to understand if the foundation is not working.

### 3.4 Build out of source

Use a separate build directory:

```bash
cmake -S . -B build
cmake --build build
./build/DirZero
```

The mental model is:

- `cmake -S . -B build` prepares the build plan.
- `cmake --build build` compiles according to that plan.
- `./build/DirZero` runs the result.

---

## 4. Phase B: Create the Plain Data Types

Before making widgets, create the objects that describe the world. These types
should not know anything about `QPushButton` or `QTreeView`.

### 4.1 Create `MachineState.hpp`

The Python code uses strings such as `CONNECTED`. In C++, use an enum class:

```cpp
enum class MachineState {
    Discovering,
    OnlineSshOk,
    OnlineSshUnavailable,
    AuthRequired,
    Connecting,
    Connected,
    Error,
    Disconnected,
    Offline
};
```

Why use `enum class`?

- It prevents accidental comparison with unrelated integers.
- The compiler can catch misspelled states.
- It tells the reader that only a fixed set of values is valid.

Later, write a separate function that converts the enum to display text. Do not
use the display text as your internal state.

### 4.2 Create `MachineInfo.hpp`

Make a simple struct with:

- `QString name`;
- `QString dnsName`;
- `QString ip`;
- `QString osType`;
- `bool online`;
- `bool sshAvailable`;
- `bool isSelf`.

Use Qt strings consistently in the GUI layer. Mixing `std::string` and
`QString` everywhere creates conversion noise. You may use standard strings
inside a third-party network wrapper, but convert at the boundary.

Add a small constructor with sensible defaults. Add a debug formatter later if
you need it; do not overbuild the type now.

### 4.3 Create `RemoteEntry.hpp`

`SFTPManager::listDirectory` in Python returns dictionaries. C++ should replace
those anonymous dictionaries with a named type:

```text
RemoteEntry
  name
  path
  isDirectory
  size
  modifiedTime
  mode
  hidden
```

This is a major C++ lesson: when a dictionary always has the same fields, turn it
into a struct. The compiler can then help you use it correctly.

### 4.4 Create `FSNode.hpp` and `FSNode.cpp`

An `FSNode` is the in-memory tree used by the file view. Give each node:

```text
name: QString
path: QString
isDirectory: bool
size: qint64
modifiedTime: qint64 or time value
mode: uint
parent: FSNode*
children: container of child nodes
isLoaded: bool
isLoading: bool
isDummy: bool
error: QString
```

#### Decide ownership before writing code

The parent owns the children. Prefer:

```cpp
std::vector<std::unique_ptr<FSNode>> children;
```

Then each child has a non-owning `FSNode* parent` pointer.

This means:

- deleting a parent deletes its children;
- the parent pointer must never be deleted separately;
- a Qt model index must not outlive the node it points to;
- replacing children requires care because old pointers become invalid.

You could use `QVector<FSNode*>` and manually delete nodes, but that makes
lifetime mistakes easier. Learn `unique_ptr` here.

#### Implement the methods in this order

1. Constructor.
2. `row()`: search the parent's children for this node.
3. `child(row)`: bounds-check and return a child pointer.
4. `childCount()`.
5. `sizeFormatted()`.
6. `modifiedFormatted()`.
7. `permissionsFormatted()`.

The formatting methods are pure calculations. Test them without Qt widgets.

#### Add the loading placeholder

When constructing a real directory, add one dummy child named `Loading...`.
The reason is a tree-view detail: a view shows an expansion indicator when it
believes a node has children. The dummy child advertises that the directory can
be expanded before the network request completes.

The sequence later becomes:

```text
Create directory node
  -> add dummy child
User expands node
  -> worker lists remote directory
  -> model removes dummy child
  -> model inserts real children
```

### 4.5 Checkpoint: test the tree without a GUI

Write a temporary test or a small console function that:

1. creates a root directory;
2. creates two children;
3. attaches a folder with a dummy child;
4. checks the row numbers;
5. checks formatted sizes and permissions;
6. removes and replaces the children.

Do not continue until you understand who owns each node.

---

## 5. Phase C: Create Credential Persistence

### 5.1 Why persistence is a separate class

The UI should not know where credentials are stored. The UI should ask:

```text
credentials = CredentialStore::get(machineIp, machineName)
```

The storage class should know the path, JSON format, and file permissions.

### 5.2 Create `CredentialStore.hpp`

Decide on a typed value:

```text
Credential
  username
  password
  keyPath
```

Then provide functions conceptually equivalent to:

```text
configPath()
loadAll()
get(hostIp, optional hostName)
save(hostKey, credential)
remove(hostKey)
```

Use `std::optional<Credential>` for a missing entry.

### 5.3 Choose the config path

Use:

```cpp
QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
```

Append `credentials.json`. Create the directory with `QDir::mkpath`.

Do not rely on the current working directory for credentials. The Python
implementation does, but an installed C++ desktop application should use an OS
configuration path.

### 5.4 Read JSON with Qt

The algorithm is:

1. Open the file with `QFile`.
2. Read all bytes.
3. Parse with `QJsonDocument::fromJson`.
4. Confirm the root is a JSON object.
5. Look up the host key.
6. Convert fields to strings.
7. Return an optional credential.

When saving:

1. Load the entire object.
2. Build a new `QJsonObject` entry.
3. Insert it under the host key.
4. Serialize with `QJsonDocument::toJson`.
5. Write atomically if possible using `QSaveFile`.

### 5.5 Be honest about security

Do not call plaintext password storage secure just because the file is mode 0600.
For a real application, use an OS keychain. You can first implement JSON to
learn the flow, then replace the backend without changing the UI.

Checkpoint: save a fake credential, load it in a new process, remove it, and
confirm malformed JSON returns a controlled error instead of crashing.

---

## 6. Phase D: Add Tailscale Discovery

This phase teaches process execution, JSON parsing, and network probing.

### 6.1 Create `TailscaleDiscovery.hpp`

Keep this class independent from widgets. It should return values and errors,
not update labels.

Plan these operations:

```text
findBinary() -> optional path
getOnlineMachines(checkSsh) -> result containing MachineInfo list or error
checkPort22(ip, timeout) -> bool or result
```

You can initially make them synchronous because they will be called by worker
objects later. The important separation is that the discovery code does not
know which button started it.

### 6.2 Find the executable

Use `QStandardPaths::findExecutable("tailscale")` first. Add fallback paths for
Linux, macOS, and Windows only if needed.

Think of this as input validation:

```text
Can I locate the program?
If no, return a clear error.
If yes, continue.
```

Do not silently turn every failure into an empty machine list while learning.
An empty list can mean "no peers" or "Tailscale is missing". Preserve that
difference in your result type.

### 6.3 Execute `tailscale status --json`

Use `QProcess`:

1. Create the process.
2. Set the executable.
3. Set arguments.
4. Start it.
5. Wait with a timeout in the worker thread.
6. Read standard output and standard error.
7. Check the exit code.
8. Parse standard output.

A useful result design is:

```text
DiscoveryResult
  machines: vector of MachineInfo
  errorMessage: optional QString
```

Or use a small `Result<T>` type later. At first, returning a boolean plus an
output parameter is acceptable while you learn.

### 6.4 Parse the JSON in stages

Do not write one enormous expression. Parse in this order:

1. Get the root object.
2. Read `Self`.
3. Extract the first IPv4-looking address from `TailscaleIPs`.
4. Create a `MachineInfo` for self.
5. Read `Peer`.
6. Iterate through peer object values.
7. Extract host name, DNS name, OS, online status, and IP.
8. Filter offline machines.
9. Optionally probe port 22.

At each stage, check whether the JSON value has the expected type.

### 6.5 Probe TCP port 22

Use `QTcpSocket` in the worker thread:

```text
create socket
connect to host(ip, 22)
wait for connected(timeout)
return success or failure
```

A successful TCP connection only means that something accepted port 22. It does
not mean the username is valid or that SFTP is available.

Checkpoint: run discovery and print machine names from a console test before
connecting any GUI code. Test the missing-binary and malformed-JSON cases too.

---

## 7. Phase E: Design the SFTP Boundary Before Implementing It

This is the hardest part because Qt does not include an SSH/SFTP client.

### 7.1 Choose a library deliberately

Possible choices include:

- `libssh2`: widely used C library, lower-level API.
- `libssh`: another C library with broader SSH functionality.
- A commercial or platform-specific library.

This guide assumes `libssh2`, but the architectural lesson is more important
than the choice: hide the library behind your own interface.

### 7.2 Create `ISftpManager.hpp`

Before touching libssh2, define what the rest of your application needs:

```text
connect()
disconnect()
isConnected()
absolutePath(path)
listDirectory(path)
createFile(path)
createDirectory(path)
deleteFile(path)
deleteDirectoryRecursive(path)
rename(oldPath, newPath)
statPath(path)
calculateTreeSize(path)
openForRead(path)
openForWrite(path)
```

Use typed `RemoteEntry` values rather than JSON or library-specific structs.

Why create the interface first? Because now you can write a fake implementation
that returns sample files. The GUI can be developed before SSH works.

### 7.3 Create a fake SFTP manager

The fake manager should store an in-memory tree and implement the interface.
Use it to test:

- listing;
- creating files and directories;
- renaming;
- deletion;
- copying a file;
- error cases.

This is not wasted work. It separates GUI bugs from authentication bugs.

### 7.4 Add `Libssh2SftpManager`

Only after the fake manager works, add the real implementation. It must own:

```text
socket handle
LIBSSH2_SESSION*
LIBSSH2_SFTP*
username
password
private key path
host and port
timeout settings
```

The destructor must call `disconnect`. Make the type non-copyable because two
objects must not accidentally own the same raw libssh2 pointers.

### 7.5 Connection algorithm

Implement one step and test it before adding the next:

1. Create a TCP socket.
2. Connect to the host and port.
3. Initialize libssh2 globally once for the process.
4. Create an SSH session.
5. Configure timeouts.
6. Perform the SSH handshake.
7. Authenticate with the configured method.
8. Create the SFTP subsystem.
9. Store the live handles.

At every failure, clean up everything created so far. For example, if the
handshake succeeds but SFTP initialization fails, the session and socket still
need closing.

A useful cleanup mental model is:

```text
socket owns transport
session owns SSH state
sftp session depends on SSH state
```

Close in reverse order: SFTP, SSH session, socket.

### 7.6 Authentication order

Match the Python behavior conceptually:

1. Explicit private key if supplied.
2. Default key paths if no password was supplied.
3. SSH agent if supported.
4. Password fallback when available.

Do not guess silently. Return a typed or clearly categorized authentication
failure so the panel can show `AUTH_REQUIRED` instead of generic `ERROR`.

### 7.7 SFTP directory listing

The algorithm is:

1. Open the remote directory.
2. Read each directory entry.
3. Convert its name and attributes into `RemoteEntry`.
4. Detect directory/file from mode bits.
5. Build a POSIX remote path.
6. Close the directory handle.
7. Sort directories before files.

Never use local `std::filesystem` to interpret remote paths. Remote paths use `/`
even when the local computer runs Windows.

### 7.8 File operations

Implement and test in this order:

1. `stat`.
2. `createFile`.
3. `createDirectory`.
4. `rename`.
5. `deleteFile`.
6. recursive directory deletion.
7. streaming read/write.

Recursive deletion needs a deliberate symlink policy. Do not follow a symlink
as if it were a real directory unless you explicitly want that behavior.

### 7.9 Add libssh2 to CMake

Only now update CMake. Add include and library discovery, then link the target.
Keep the interface headers independent of libssh2 so most of the application
still compiles and tests without it.

Checkpoint: write a console program that connects to one known host, lists one
directory, and disconnects. Do not add Qt widgets until this works.

---

## 8. Phase F: Learn Qt Threading with One Worker

The GUI thread must not wait for SSH. This is where `QRunnable`, `QThreadPool`,
signals, and slots become necessary.

### 8.1 The rule

A worker may perform blocking I/O. A worker must not directly modify widgets or
Qt models owned by the GUI thread.

The pattern is:

```text
GUI creates worker
GUI connects worker signals to GUI slots
GUI submits worker to QThreadPool
worker performs blocking work
worker emits result/error/finished
GUI slot updates widgets/model
```

### 8.2 Create a worker signal object

Define a `WorkerSignals : public QObject` with signals such as:

```text
started()
finished()
error(QString)
```

Each specialized worker can add its own result signal. This is clearer in C++
than using one `object` signal for everything.

Every class using signals or slots needs `Q_OBJECT`, and CMake's `AUTOMOC` must
be enabled.

### 8.3 Build `DiscoveryWorker`

The worker stores a discovery service and a `checkSsh` flag. Its `run()` method:

1. emits started;
2. calls discovery;
3. emits a machine list on success;
4. emits an error string on failure;
5. emits finished in all cases.

Use RAII and exceptions only if your project consistently uses exceptions. It is
also valid to return result objects. The important thing is that every path
emits `finished`.

### 8.4 Test responsiveness

Make the worker sleep for several seconds before returning. Add a button or
label that still responds while the worker runs. This proves that the task is
actually off the GUI thread.

Do not pass raw references to stack objects into a worker that may outlive them.
Either use shared ownership, a stable service object, or ensure the manager's
lifetime exceeds the worker.

### 8.5 Add workers one at a time

Create these in order:

1. `DiscoveryWorker`.
2. `PortCheckWorker`.
3. `SftpConnectWorker`.
4. `SftpListWorker`.
5. `FileOperationWorker`.
6. `TransferWorker`.

After each one, make one UI action that starts it and one slot that displays its
result.

---

## 9. Phase G: Implement the Qt Tree Model

### 9.1 Why the view needs a model

`QTreeView` does not own your file data. It asks a model questions:

- How many columns exist?
- How many children does this parent have?
- What is at row 2, column 1?
- What is the parent of this index?
- What text should I display?

Your `RemoteFSModel` answers those questions using `FSNode` objects.

### 9.2 Create `RemoteFSModel.hpp`

Subclass `QAbstractItemModel`, store a root `FSNode*`, and declare overrides:

```text
rowCount
columnCount
index
parent
data
headerData
```

Declare helper methods:

```text
populateNode(parentNode, entries, parentIndex)
setNodeError(parentNode, message, parentIndex)
```

### 9.3 Implement the model in this order

1. `columnCount`: return four.
2. `headerData`: return Name, Size, Modified, Permissions.
3. `rowCount`: return the selected node's child count.
4. `index`: convert a child pointer into `createIndex`.
5. `parent`: convert a node's parent pointer into a model index.
6. `data`: handle display and tooltip roles.
7. `populateNode`: use model notifications correctly.
8. `setNodeError`.

The parent algorithm is the key idea:

```text
invalid QModelIndex means root node
valid QModelIndex means index.internalPointer()
child index stores a pointer to its FSNode
```

### 9.4 Model notifications are not optional

When removing children:

```text
beginRemoveRows(parentIndex, first, last)
modify the vector
endRemoveRows()
```

When inserting children:

```text
beginInsertRows(parentIndex, first, last)
modify the vector
endInsertRows()
```

Qt uses these notifications to keep the view's indexes and selection state
consistent. Mutating the vector without notifications causes subtle bugs.

### 9.5 Test with fake data

Attach the model to a `QTreeView` using only the fake SFTP manager. Confirm:

- files and directories appear;
- four columns show correct values;
- directories have expansion arrows;
- expanding replaces `Loading...` with children;
- errors appear as visible rows.

Do not connect real SSH yet if the fake model still behaves incorrectly.

---

## 10. Phase H: Build the Authentication Widget

### 10.1 Decide whether to use `.ui` files

You have two options:

- Use Qt Designer `.ui` files and generated `ui_*.h` files.
- Build the widget with C++ layouts and controls.

For learning, use the existing `MachinePanel.ui` for the static machine card,
but build the authentication bar programmatically so you learn layouts,
signals, and input widgets.

### 10.2 Create `MachineAuthBar`

Subclass `QFrame`. Add:

- username `QLineEdit`;
- password `QLineEdit` with password echo mode;
- key path `QLineEdit`;
- browse `QPushButton`;
- remember `QCheckBox`;
- connect `QPushButton`;
- close `QToolButton`;
- alert `QLabel`.

Declare signals:

```text
authRequested(username, password, keyPath, remember)
dismissed()
```

Connect button signals to private slots. The widget should collect input and
emit a signal; it should not call libssh2 directly.

### 10.3 Implement the input flow

When Connect is clicked:

1. trim username and key path;
2. use the local username if blank;
3. read the password without logging it;
4. read the checkbox;
5. emit `authRequested`.

The machine panel receives this signal and decides whether to save credentials
and start a connection worker.

---

## 11. Phase I: Build One `MachinePanelWidget`

This is the first class that combines model, service, workers, and widgets.
Do not add multiple machines yet.

### 11.1 Members to plan

The panel needs:

```text
MachineInfo machineInfo
MachineState state
std::unique_ptr<ISftpManager> sftpManager
std::unique_ptr<FSNode> rootNode
RemoteFSModel* model
RemoteFileTreeView* treeView
MachineAuthBar* authBar
QLabel* headingLabel
QLabel* statusLabel
QLabel* ipLabel
QPushButton* refreshButton
QPushButton* retryButton
QPushButton* pasteButton
QSet<QObject*> activeWorkers or another lifetime strategy
RemoteClipboard* clipboard
```

Do not create all controls in one giant constructor body. Divide UI creation:

```text
createBaseCard()
createToolbar()
createAuthBar()
configureTreeView()
connectSignals()
```

### 11.2 Load the Designer form

If using `MachinePanel.ui`:

1. Generate its C++ UI header with `uic` or let CMake handle it.
2. Create a container widget.
3. Call `ui.setupUi(this)`.
4. Find or directly use named controls.
5. Replace the Designer tree view with your subclass if needed.

The important lesson is that a `.ui` file describes widgets and layouts; it does
not implement application behavior.

### 11.3 Implement `setState`

Create one function responsible for state transitions. It should:

1. store the enum;
2. update a dynamic Qt property such as `machineState`;
3. repolish the widget;
4. enable or disable buttons;
5. update status text;
6. show the correct error/auth row;
7. start or stop the relevant action.

Do not spread state decisions randomly across button handlers. The state
function is your single source of truth for what a panel looks like.

### 11.4 Implement the connection flow

The panel's algorithm is:

```text
if machine says SSH is reachable:
    setState(Connecting)
    create SftpConnectWorker
    connect result/error/finished
    submit to QThreadPool

on connection result:
    setState(Connected)
    reloadFilesystem()

on connection error:
    classify authentication versus general failure
    setState(AuthRequired) or setState(Error)
```

### 11.5 Implement root listing

`reloadFilesystem()` should:

1. check `isConnected`;
2. mark the root node as loading;
3. create an `SftpListWorker` for `.`;
4. connect its result to a GUI-thread slot;
5. submit it.

The result slot should call `model->populateNode(...)`. The worker must not call
that method itself.

### 11.6 Implement lazy directory loading

Connect `QTreeView::expanded` to a panel slot. That slot should:

1. read the `FSNode*` from the model index;
2. ignore files, dummy rows, loaded nodes, and already-loading nodes;
3. mark the node loading;
4. create a list worker for that path;
5. on result, populate that node.

This is the same algorithm as root listing, except the target model index is a
real directory index.

Checkpoint: one panel can connect to a real or fake SFTP service, show a root,
and lazily expand folders.

---

## 12. Phase J: Add File Operations

Every operation follows the same five-part pattern:

```text
1. Validate current state.
2. Ask the user for input or confirmation.
3. Create a worker containing the operation.
4. Run it in QThreadPool.
5. Refresh and report success/error in the GUI slot.
```

### 12.1 New file

The GUI determines the target directory, opens a `QInputDialog`, validates a
non-empty name, then starts a worker that calls `createFile`.

Do not perform the SFTP call before the dialog returns. Do not let the dialog
run from a worker thread.

### 12.2 New folder

Use the same path as new file, calling `createDirectory` instead.

### 12.3 Rename

The selected node provides:

```text
oldPath = node.path
parentPath = parent directory of oldPath
newPath = parentPath plus newName
```

Use a path helper that always uses remote POSIX separators. Do not concatenate
paths with local Windows separators.

### 12.4 Delete

Before deleting, show a `QMessageBox` confirmation. For a directory, make the
message explicitly say that contents will be deleted. Then select either
`deleteFile` or `deleteDirectoryRecursive`.

### 12.5 Clipboard operations

Create `RemoteClipboard` as an application-owned `QObject`. It stores:

```text
source panel
source path
source name
is directory
is cut
```

It emits `clipboardChanged`. Each panel listens and updates its Paste button.
This is not the operating system clipboard; it is an internal transfer plan.

---

## 13. Phase K: Implement Transfers

Transfers are a separate problem from ordinary file operations because they
may involve two remote machines.

### 13.1 Define the transfer inputs

A transfer worker needs:

```text
source SFTP manager
source path
destination SFTP manager
destination path
isDirectory
isCut
source host name
destination host name
cancel flag
bytes transferred
total bytes
start time
```

Do not pass whole widgets into the worker. Pass only the service objects and
plain data needed to do the job.

### 13.2 Decide the operation before opening files

The worker should calculate:

1. Are both sessions connected?
2. Is this the same host?
3. Is source and destination the exact same path?
4. Is this a file or directory?
5. Is this copy or cut?

Same-host cut can use remote rename. Cross-host cut requires copy first and
source deletion only after successful completion.

### 13.3 Stream a single file

The file loop is conceptually:

```text
open source for binary read
open destination for binary write
while not cancelled:
    read a bounded chunk
    if no bytes remain: break
    write the chunk
    add chunk size to bytes transferred
    calculate speed
    emit progress
```

Never read an entire large file into memory. The chunk size is a performance
tradeoff: too small creates overhead; too large increases memory use and delays
progress updates.

### 13.4 Transfer a directory recursively

The recursive method should:

1. create the destination directory;
2. list source children;
3. recurse into directories;
4. stream files;
5. stop if cancelled.

Decide what to do with symlinks, permissions, timestamps, and destination
collisions. The Python implementation mostly handles the basic file tree; your
C++ version should document any stronger policy.

### 13.5 Progress signals

Define a typed progress signal such as:

```text
progress(bytesDone, totalBytes, currentItem, speedBytesPerSecond)
```

The main window receives it and updates:

- progress bar percentage;
- source/destination labels;
- bytes done and total;
- transfer speed.

The worker emits signals; it does not find or modify `QProgressBar`.

### 13.6 Cancellation and cleanup

Use an atomic cancellation flag if the worker can be requested from another
thread. On cancellation:

- stop reading;
- close both file handles through RAII;
- decide whether to remove a partial destination file;
- do not delete the source for a cut.

This is a good place to learn why a plain `bool` can be unsafe across threads.

---

## 14. Phase L: Build the Main Window

### 14.1 Create `Dir2ZeroWindow`

The main window owns application-wide things:

```text
machine panel collection
responsive grid
discovery timer
scan button
online/reachable/unavailable badges
progress dock
status bar
theme manager reference
```

It should not own the details of one machine's SFTP operations. Those remain in
`MachinePanelWidget`.

### 14.2 Build the layout in layers

Create the UI top to bottom:

1. central widget;
2. vertical base layout;
3. header row;
4. title and subtitle;
5. statistics labels;
6. theme menu;
7. refresh button;
8. scroll area;
9. responsive grid container;
10. progress dock;
11. status bar.

After each layer, run the application and check the geometry. Building a UI
incrementally makes layout errors local and understandable.

### 14.3 Create `ResponsiveGridContainer`

Subclass `QWidget` and own a `QGridLayout`. Store panel pointers in a vector.
When resized:

1. calculate `width / minimumColumnWidth`;
2. clamp the number of columns;
3. remove/reinsert panels row by row;
4. set column stretches.

Do not create a new panel every time the window resizes. Only rearrange existing
widgets.

### 14.4 Add discovery reconciliation

The main window starts a `QTimer` for periodic discovery and also responds to a
manual refresh.

When results arrive:

1. count online and SSH-reachable machines;
2. index existing panels by IP;
3. update existing panels without destroying active sessions;
4. create panels for new IPs;
5. close and remove panels whose IP disappeared;
6. update badges;
7. show a placeholder only when the list is empty.

Use IP as the stable identity because host names can change.

Be careful with the placeholder: keep a separate pointer or a separate empty
state instead of treating the placeholder as a machine panel.

### 14.5 Add `closeEvent`

When the window closes:

1. stop the discovery timer;
2. request worker cancellation where supported;
3. disconnect SFTP managers;
4. wait for workers only for a bounded time;
5. let Qt finish closing.

Do not block forever during application shutdown.

---

## 15. Phase M: Add the Custom Tree View

Only after the normal tree works should you add interaction enhancements.

### 15.1 Keyboard shortcuts

Create `QAction` objects with widget-with-children shortcut context:

```text
Ctrl+C -> copy
Ctrl+X -> cut
Ctrl+V -> paste
F2 -> rename
Delete -> delete
F5 -> refresh
```

The action should call the panel method, not duplicate the file logic.

### 15.2 Context menu

On right click:

1. find the index at the cursor;
2. set it current if valid;
3. create a temporary `QMenu`;
4. add actions according to selection and clipboard state;
5. execute it at the global position.

The menu is a view interaction. The actual operation remains a panel method.

### 15.3 Drag payload

Use a custom MIME type:

```text
application/x-dirzero-item
```

Serialize only the minimum transfer plan:

```text
source IP
source host
source path
source name
is directory
```

At drop time:

1. validate MIME type;
2. parse JSON;
3. find destination directory from the drop index;
4. find the source panel by IP;
5. interpret Move or Shift as cut;
6. start the transfer worker.

Never trust arbitrary drag data. Validate every required field before using it.

---

## 16. Phase N: Add Themes and Resource Handling

### 16.1 Create `ThemeManager`

The manager stores:

```text
theme ID
friendly name
stylesheet file path
current index
```

It provides:

```text
loadAvailableThemes()
getCurrentTheme()
applyCurrentTheme()
switchToTheme(id)
cycleNextTheme()
```

Emit a `themeChanged` signal after successful application.

### 16.2 Use stable resource paths

Do not assume the current directory is the project root. During development,
that assumption may appear to work. After installation, it often fails.

Choose one strategy:

- compile QSS into a Qt resource file (`.qrc`);
- locate files relative to `QCoreApplication::applicationDirPath()`;
- install themes to a known data directory and query that directory.

For learning, external QSS files are easy to edit. For distribution, Qt
resources are often simpler.

### 16.3 Apply styles

Read with `QFile`, fall back to the default stylesheet, and call
`QApplication::setStyleSheet`. Keep theme selection separate from the widgets
that display the menu.

---

## 17. The Correct C++/Qt Header Pattern

As your classes grow, use a predictable header shape:

```cpp
#pragma once

#include <QWidget>

class MachinePanelWidget final : public QFrame {
    Q_OBJECT

public:
    explicit MachinePanelWidget(const MachineInfo& info,
                                QWidget* parent = nullptr);
    ~MachinePanelWidget() override;

signals:
    void statusMessage(const QString& text);

private slots:
    void onSftpConnected();
    void onSftpError(const QString& message);

private:
    void setupUi();
    void setState(MachineState state);

    MachineInfo machineInfo_;
    MachineState state_;
};
```

What to learn from this pattern:

- `explicit` prevents unwanted one-argument conversions.
- `override` asks the compiler to verify a base-class override.
- `private slots` are functions called by Qt signals.
- the trailing underscore convention distinguishes members from parameters.
- a forward declaration can reduce header dependencies.
- `Q_OBJECT` is required for signals, slots, and properties.

Do not expose every member publicly. Give each class a small public interface.

---

## 18. Error Handling Strategy

The Python version catches broad exceptions. In C++, choose a consistent policy.
A beginner-friendly first policy is:

```text
service methods return bool or a result struct
service methods fill an error QString
workers convert failures into error signals
GUI slots decide how to display errors
```

For example:

```text
OperationResult
  succeeded: bool
  message: QString
```

Avoid returning an empty vector for every error, because an empty directory and
a failed directory listing are different states.

Define categories where useful:

```text
AuthenticationFailure
ConnectionFailure
PermissionFailure
NotFoundFailure
TransferCancelled
ProtocolFailure
```

The UI can then choose authentication controls for an authentication failure
and a retry button for a connection failure.

---

## 19. Ownership and Lifetime Rules to Memorize

These rules prevent many Qt crashes:

1. A Qt parent owns its child QObject and deletes it automatically.
2. Do not manually delete a QObject with a parent unless you understand the
   ownership change.
3. A `QModelIndex` is only valid while its referenced model item remains stable.
4. A worker must not reference a widget that may be destroyed while it runs.
5. A raw pointer is acceptable as a non-owning pointer only when its owner is
   obvious and longer-lived.
6. Use `std::unique_ptr` for exclusive ownership of non-QObject objects.
7. Use `std::shared_ptr` only when multiple owners are genuinely required.
8. Make network manager classes non-copyable.
9. Close network handles in destructors and explicit disconnect methods.
10. Keep GUI objects on the GUI thread.

Draw an ownership diagram before adding a new pointer:

```text
Dir2ZeroWindow owns grid and panels
MachinePanelWidget owns model and tree root
RemoteFSModel observes tree root
Worker temporarily observes service
Worker emits result to panel
Panel updates model on GUI thread
```

If you cannot say who deletes an object, stop and fix the design before coding.

---

## 20. Testing Plan for a Beginner

Do not wait until the entire application exists to test it. Use milestones.

### Test 1: build test

The empty Qt window builds and runs.

### Test 2: data test

`FSNode` creates children, formats data, and removes children correctly.

### Test 3: JSON test

Credential objects save, load, and survive a new process.

### Test 4: discovery test

A sample Tailscale JSON fixture produces expected `MachineInfo` values.

### Test 5: fake SFTP test

The fake service lists, creates, renames, and deletes entries.

### Test 6: model test

The tree view displays fake entries and expands a fake directory.

### Test 7: worker test

A delayed worker completes while the UI remains interactive.

### Test 8: real SFTP test

One known host connects, lists a harmless directory, and disconnects.

### Test 9: operation test

Create and delete a temporary remote file. Rename it. Use a temporary folder
for recursive deletion.

### Test 10: transfer test

Copy a small file, then a directory, then test cut. Test same-host and
cross-host cases separately.

### Test 11: failure test

Check missing Tailscale, offline host, wrong password, missing key, permission
failure, destination collision, and cancellation.

### Test 12: shutdown test

Start a slow operation and close the window. Confirm the process exits without
crashing or hanging forever.

---

## 21. Suggested Commit Order

If you use Git, make one commit per learning milestone:

```text
01 create minimal Qt application
02 add CMake and out-of-source build
03 add MachineInfo and MachineState
04 add FSNode and unit tests
05 add CredentialStore
06 add Tailscale discovery
07 add fake SFTP manager
08 add real SFTP manager
09 add worker infrastructure
10 add RemoteFSModel
11 add one MachinePanelWidget
12 add authentication states
13 add file operations
14 add transfer worker and progress
15 add main-window reconciliation
16 add drag/drop and shortcuts
17 add themes and shutdown cleanup
```

A small commit gives you a known-good checkpoint. If the transfer worker breaks
the UI, you can inspect only that milestone instead of debugging the entire
history.

---

## 22. What You Should Write First, Literally

When you sit down to begin, do this exact sequence:

1. Create `src/main.cpp` with a minimal `QApplication` and window.
2. Create `CMakeLists.txt` that builds only that file.
3. Configure and build from `build/`.
4. Create `MachineState.hpp`.
5. Create `MachineInfo.hpp`.
6. Create `RemoteEntry.hpp`.
7. Create `FSNode.hpp` and `FSNode.cpp`.
8. Add a temporary console/test function that creates a root and children.
9. Add `CredentialStore.hpp` and `.cpp`.
10. Test JSON save/load with fake values.
11. Add `TailscaleDiscovery.hpp` and `.cpp`.
12. Test the Tailscale parser with saved JSON output.
13. Add `ISftpManager.hpp`.
14. Add a fake SFTP implementation.
15. Add `RemoteFSModel.hpp` and `.cpp`.
16. Display fake entries in a tree view.
17. Add `WorkerSignals` and a delayed test worker.
18. Replace the fake listing call with an `SftpListWorker`.
19. Add the real libssh2 implementation.
20. Add `MachineAuthBar`.
21. Add `MachinePanelWidget` for one machine.
22. Add create, rename, delete, copy, and cut one operation at a time.
23. Add transfer streaming and progress.
24. Add `Dir2ZeroWindow` and multiple panels.
25. Add discovery reconciliation.
26. Add themes, keyboard shortcuts, and drag/drop last.

At every numbered step, compile. After every feature that touches the GUI,
run the program. After every network feature, test one success and one failure.

---

## 23. How to Read the Python Reference While Porting

Use the Python method as a question, not as a line-by-line transcription.

For each method, ask:

1. What information does this method receive?
2. What information does it produce?
3. Is it a pure calculation, a file operation, a network operation, or a UI
   operation?
4. Which class should own that responsibility in C++?
5. What must remain alive while it runs?
6. Can it block?
7. Which thread should run it?
8. What error cases exist?
9. What signal or return value tells the caller it is done?
10. What test can prove it works without the whole application?

Examples:

### Python `FSNode.size_formatted`

This is a pure calculation. Port it to a C++ member function and unit test it.
It does not need Qt widgets or a worker.

### Python `SFTPManager.list_directory`

This is blocking network work. Port it behind `ISftpManager`, call it from a
worker, and return typed `RemoteEntry` values.

### Python `RemoteFSModel.populate_node`

This mutates model data and sends Qt model notifications. Run it on the GUI
thread and keep it in the model class.

### Python `MachinePanelWidget._paste_into_dir`

This coordinates UI selection, clipboard state, progress, and a worker. Keep
the coordination in the panel, but move byte copying into `TransferWorker`.

### Python `Dir2ZeroWindow._on_discovery_completed`

This reconciles application state. It belongs in the main window because the
main window owns the collection of machine panels.

This method of reading code is how you learn architecture instead of merely
rewriting syntax.

---

## 24. Common Beginner Mistakes

### Mistake: putting everything in `main.cpp`

It feels easier initially, but it makes compilation errors and ownership harder
to understand. Split once a class has a clear responsibility.

### Mistake: doing network work in a button slot

The GUI freezes because the event loop cannot repaint or process input while
waiting. Put blocking work in a worker.

### Mistake: updating a widget from a worker

Qt widgets belong to the GUI thread. Emit a signal and update the widget in a
slot on the GUI thread.

### Mistake: using `std::filesystem` for remote paths

The remote system's path rules are not the local machine's path rules. Use a
POSIX remote-path helper.

### Mistake: copying raw pointers without ownership rules

A pointer in a model index can become dangling when children are replaced. Use
stable ownership and model notifications.

### Mistake: catching every error and returning an empty list

An empty folder, an offline host, invalid credentials, and a missing executable
are not the same thing. Preserve the error category.

### Mistake: adding UI polish before the model works

Icons, themes, and drag previews cannot fix a broken data flow. Build the data
path first.

### Mistake: trusting `AutoAddPolicy` equivalent behavior

Automatically accepting unknown SSH host keys is convenient but vulnerable to
man-in-the-middle attacks. Implement known-host verification for serious use.

### Mistake: storing passwords casually

A JSON file is a teaching implementation, not a secure credential vault. Plan
the keychain replacement early.

---

## 25. Final Mental Model

When the finished C++ application runs, the flow should feel like this:

```text
QApplication starts
  -> Dir2ZeroWindow creates its layout
  -> discovery worker asks Tailscale for machines
  -> main window reconciles MachineInfo values into panels
  -> reachable panel starts an SFTP worker
  -> connection result changes panel state
  -> list worker obtains RemoteEntry values
  -> GUI slot converts entries into FSNode children
  -> RemoteFSModel exposes children to QTreeView
  -> user action creates another worker
  -> worker emits result/progress/error
  -> GUI updates model, labels, and progress bar
```

The application is not one giant algorithm. It is a set of contracts:

- discovery returns machines;
- SFTP returns remote entries;
- workers return asynchronous results;
- models expose structured data;
- widgets request work and display state;
- the main window coordinates many panels.

If you keep those contracts clear, you can replace the fake SFTP backend with
libssh2, replace libssh2 with another SSH library, replace JSON credentials with
a keychain, or replace the Python version with C++ without redesigning the user
experience from scratch.

That is the real method for recreating this app: identify responsibilities,
create the smallest typed contract for each responsibility, test each contract,
and only then connect the contracts through Qt signals, slots, and ownership.
