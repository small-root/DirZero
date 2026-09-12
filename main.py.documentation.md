# DirZero `main.py` Documentation

## 1. Purpose and Scope

`main.py` is the complete Python implementation of DirZero, a desktop remote
file manager for machines reachable through a Tailscale network. It combines:

- Tailscale peer discovery through the `tailscale` command-line client.
- TCP probing of SSH port 22.
- SSH authentication and SFTP operations through Paramiko.
- A Qt desktop interface built with PySide6.
- Lazy remote filesystem browsing in a `QTreeView`.
- Remote copy, cut, paste, drag-and-drop, rename, create, delete, and refresh.
- Background work using `QThreadPool` and `QRunnable` so network operations do
  not block the GUI thread.
- Per-host credential persistence in a JSON file.
- Runtime stylesheet/theme selection.

The file is intentionally monolithic: data models, network services, worker
objects, widgets, window orchestration, theme loading, and process startup all
live in one module. The repository's `main.cpp` currently contains only Qt and
standard-library includes; it does not implement the application. The actual
behavior is in this file.

This document describes the code as it exists. It also explains how the same
design could be implemented in C++ with Qt and an SSH/SFTP library.

## 2. Repository Context

The relevant files are:

- [`main.py`](main.py): complete runtime implementation.
- [`main.cpp`](main.cpp): currently an include scaffold, not a port.
- [`MachinePanel.ui`](MachinePanel.ui): Qt Designer form for one machine card.
- [`Dir2Zero.ui`](Dir2Zero.ui): minimal Qt Designer main-window form. The
  current Python code builds most of the main window programmatically instead.
- [`style.qss`](style.qss): fallback/default stylesheet.
- `themes/*.qss`: selectable stylesheets loaded by `ThemeManager`.
- [`requirements.txt`](requirements.txt): `PySide6` and `paramiko`.
- [`install.py`](install.py) and [`install.sh`](install.sh): rudimentary setup
  helpers. `install.sh` only prints `$HOME`; `install.py` attempts to install
  Python using a detected Linux package manager.

Relative paths are used throughout `main.py`. Therefore the process working
directory is expected to be the project directory when loading `.ui`, `.qss`,
and theme files. Launching the script from another directory may cause theme
and UI files to be missed.

## 3. Dependencies and External Requirements

### Python packages

`requirements.txt` declares:

```text
PySide6>=6.5.0
paramiko>=3.0.0
```

PySide6 provides QtCore, QtGui, QtWidgets, and QtUiTools. Paramiko provides the
SSH client, authentication mechanisms, SFTP client, SFTP attributes, and key
loader classes.

### Operating-system tools and services

The host running DirZero needs:

1. A Python interpreter compatible with the installed PySide6 and Paramiko.
2. A graphical session or a configured Qt platform plugin.
3. The Tailscale CLI available as `tailscale` or, on Windows, `tailscale.exe`.
4. An active Tailscale session with visible peers.
5. An SSH server listening on port 22 on target machines.
6. A valid username and either a usable SSH private key, an SSH agent identity,
   or a password.

The remote SFTP subsystem must be enabled by the SSH server. SFTP is not a
separate application protocol in this program; it is opened over the Paramiko
SSH connection.

## 4. High-Level Runtime Flow

The normal application lifecycle is:

```text
main()
  -> parse_args()
  -> QApplication
  -> load_stylesheet(app)
  -> Dir2ZeroWindow(...)
       -> _setup_ui()
       -> start periodic discovery timer
       -> start_discovery()
            -> DiscoveryWorker
                 -> tailscale status --json
                 -> port 22 checks
            -> _on_discovery_completed()
                 -> create/reconcile MachinePanelWidget objects
                      -> SFTP connection if port 22 is available
                      -> list root directory
```

For a connected machine, the user can expand directories. Expansion creates an
`SFTPListWorker`, whose result is applied to `RemoteFSModel` on the GUI thread.
File operations and transfers follow the same pattern: a widget starts a
worker, the worker performs blocking I/O, and Qt signals deliver completion,
failure, and progress back to the GUI.

## 5. Imports and Constants

### Standard-library imports

- `os`: environment variables, path existence, permissions, and platform data.
- `sys`: platform detection, standard error, application arguments, and exit.
- `stat`: POSIX mode inspection and conversion to file-mode strings.
- `time`: modification-time formatting, transfer timing, and speed calculation.
- `socket`: TCP port-22 probing.
- `getpass`: default local username.
- `argparse`: command-line parsing.
- `subprocess`: execution of the Tailscale CLI.
- `json`: Tailscale JSON parsing, drag payloads, and credential storage.
- `shutil`: executable lookup and filesystem helpers.
- `posixpath`: remote path manipulation. POSIX paths are appropriate for SFTP
  even when the local machine is Windows.
- `pathlib.Path`: cross-platform local filesystem paths.
- `typing`: type annotations.

### Qt imports

The code uses Qt's object/signals system, item-model/view framework, thread
pool, timers, drag-and-drop types, widgets, actions, and UI-file loader. A few
imports (`QSize`, `QFont`) are present but are not materially used in the
current implementation.

### Constants

- `DEFAULT_SSH_PORT = 22`: SSH and SFTP port used everywhere.
- `DEFAULT_SOCKET_TIMEOUT = 3.0`: TCP connect probe timeout.
- `DEFAULT_SFTP_TIMEOUT = 8.0`: Paramiko socket timeout.
- `DEFAULT_BANNER_TIMEOUT = 15.0`: SSH banner wait timeout.
- `DEFAULT_AUTH_TIMEOUT = 10.0`: SSH authentication timeout.
- `DEFAULT_CHUNK_SIZE = 65536`: transfer buffer size, 64 KiB.
- `AUTO_REFRESH_INTERVAL_MS = 20000`: discovery refresh every 20 seconds.
- `UI_DIR2ZERO_PATH`: declared path for the main UI file; currently unused.
- `UI_MACHINEPANEL_PATH`: path checked by `MachinePanelWidget`.
- `QSS_STYLE_PATH`: fallback stylesheet path.
- `DEFAULT_SSH_KEY_FILENAMES`: standard private-key filenames searched under
  `~/.ssh`.

## 6. Utility Functions

### `get_default_ssh_keys()`

Builds `Path.home() / ".ssh"`, checks the names in
`DEFAULT_SSH_KEY_FILENAMES`, and returns existing paths as strings. The search
order is Ed25519, RSA, ECDSA, then DSA.

It checks existence only. It does not validate permissions, parse the key, or
ask for a passphrase. Key parsing is deferred to `SFTPManager`.

### `get_first_available_ssh_key()`

Returns the first path from `get_default_ssh_keys()`. If none exists, it still
returns the conventional `~/.ssh/id_ed25519` path. This lets the UI display a
useful default even before a key exists.

## 7. Credential Persistence

### `CredentialStore`

`CredentialStore` is a class-method-only persistence service. It stores a JSON
object keyed by a host identifier. Each value can contain `username`,
`password`, and `key_path`.

#### `_get_config_path()`

Selects the platform-specific credential location:

- Windows: `%APPDATA%/DirZero/credentials.json`, falling back to the home
  directory if `APPDATA` is absent.
- macOS: `~/Library/Application Support/DirZero/credentials.json`.
- Linux and other platforms: `$XDG_CONFIG_HOME/dirzero/credentials.json`, or
  `~/.config/dirzero/credentials.json`.

The containing directory is created. On POSIX, the code attempts mode `0700`.
Failures are swallowed so a filesystem problem does not crash startup.

#### `load_all()`

Reads and parses the JSON file. Missing files, malformed JSON, I/O failures,
and non-dictionary JSON values become an empty dictionary.

#### `get(host_ip, host_name=None)`

Looks up the IP first and optionally the hostname second. IP priority avoids a
hostname collision when a machine is renamed.

#### `save(host_key, username, password=None, key_path=None)`

Loads the existing dictionary, replaces the selected host entry, and writes
pretty-printed JSON. Empty passwords and empty key paths are omitted. On POSIX
the file is changed to mode `0600` after writing.

#### `remove(host_key)`

Deletes an entry if present and rewrites the JSON file. It silently ignores
write failures.

### Credential implications

Passwords are stored as plaintext JSON. File permissions reduce accidental
exposure on POSIX systems but do not provide encryption. A production version
should use the operating system keychain, such as Secret Service/libsecret,
Windows Credential Manager, or macOS Keychain.

## 8. State and Data Objects

### `MachineState`

String constants used as Qt dynamic properties and UI state labels:

- `DISCOVERING`: declared but not actively assigned to machine cards.
- `ONLINE_SSH_OK`: Tailscale peer is online and port 22 responded.
- `ONLINE_SSH_UNAVAILABLE`: peer is online but port 22 did not respond.
- `AUTH_REQUIRED`: network connection exists but credentials failed or are
  missing.
- `CONNECTING`: a Paramiko connection worker is active.
- `CONNECTED`: SSH and SFTP are usable.
- `ERROR`: non-authentication connection or operation failure.
- `DISCONNECTED`: declared but not actively assigned by `set_state`.
- `OFFLINE`: declared but discovery filters offline peers out before UI use.

### `FSNode`

Represents one remote file, directory, loading placeholder, or error row.

Fields:

- `name`: display name.
- `path`: remote path used by SFTP.
- `is_dir`: whether the remote item is a directory.
- `size`: file size in bytes; directories use zero in listings.
- `mtime`: Unix modification timestamp.
- `mode`: POSIX mode bits from Paramiko.
- `parent`: owning `FSNode`, or `None` for the root.
- `children`: loaded child nodes.
- `is_loaded`: whether directory contents have been fetched.
- `is_loading`: whether a directory request is in progress.
- `is_dummy`: marks synthetic `Loading...` rows.
- `error`: optional error message for synthetic error rows.

Directories initially receive a dummy child named `Loading...`; this gives Qt a
row that makes the directory expandable before its contents are known.

#### `row()`

Returns this node's index in its parent's child list. It returns zero for a
root node or if the node is no longer present.

#### `child(row)` and `child_count()`

Safe child accessors used by the Qt model.

#### `size_formatted()`

Displays bytes, KiB, MiB, or GiB using thresholds of 1024. Directories and
synthetic rows display `-`.

#### `mtime_formatted()`

Formats the timestamp as local time using `%Y-%m-%d %H:%M`. Invalid or missing
timestamps display `-`.

#### `permissions_formatted()`

Uses `stat.filemode()` to display strings such as `-rw-r--r--`. Missing or
invalid modes display `-`.

### `MachineInfo`

A value object describing a discovered Tailscale machine:

- `name`: host name shown in the card.
- `dns_name`: Tailscale DNS name.
- `ip`: selected IPv4 Tailscale address.
- `os_type`: normalized lowercase OS identifier.
- `online`: Tailscale online status.
- `ssh_available`: result of the port-22 probe.
- `is_self`: whether the peer is the local Tailscale node.

`__repr__()` creates a concise debugging representation.

## 9. Shared Remote Clipboard

### `RemoteClipboard`

This is an in-process application clipboard, not the desktop clipboard. It is
a singleton `QObject` shared by every machine panel.

Stored state identifies the source panel and source item, plus whether the
operation is copy or cut:

- `source_panel`
- `source_path`
- `source_name`
- `is_dir`
- `is_cut`

#### `instance()`

Returns the one process-wide instance. It is created lazily without a parent.

#### `copy(panel, node)` and `cut(panel, node)`

Capture the selected remote item, set the operation mode, and emit
`clipboard_changed` so every panel can update its Paste button.

#### `clear()`

Resets all clipboard fields and emits `clipboard_changed`.

#### `has_item()`

Returns true only if a source panel, path, and name are present.

#### `summary()`

Returns a human-readable description for status/debug output.

## 10. Qt Filesystem Model

### `RemoteFSModel`

Subclass of `QAbstractItemModel` that exposes an `FSNode` tree to a
`QTreeView`. The four columns are `Name`, `Size`, `Modified`, and
`Permissions`.

#### `__init__(root_node, parent=None)`

Stores the root node and gives the model a Qt parent.

#### `columnCount(parent)`

Always returns four, independent of the parent index.

#### `headerData(section, orientation, role)`

Returns a horizontal display header from `HEADERS`; other orientations and
roles return `None`.

#### `rowCount(parent)`

Maps an invalid model index to the root node and a valid index to its internal
`FSNode`. It returns the selected node's child count.

#### `index(row, column, parent)`

Uses `hasIndex`, resolves the parent node, obtains its child, and creates a Qt
model index whose internal pointer is the child `FSNode`.

#### `parent(index)`

Resolves the node's parent. A child of the root has an invalid Qt parent index,
which makes the root's direct children appear at the top level.

#### `data(index, role)`

For `DisplayRole`, column zero adds an icon-like Unicode prefix for files,
directories, loading rows, and error rows. Other columns use the formatting
helpers on `FSNode`. `ToolTipRole` exposes path, size, modified time, and
permissions, or the error text.

#### `populate_node(parent_node, entries, parent_index)`

Removes all current children with `beginRemoveRows`/`endRemoveRows`, creates
new `FSNode` objects from SFTP listing dictionaries, inserts them with
`beginInsertRows`/`endInsertRows`, and marks the directory loaded.

This method must run on the GUI thread because it mutates a Qt model.

#### `set_node_error(parent_node, error_msg, parent_index)`

Replaces current children with one synthetic error node and marks the directory
loaded. It allows connection and listing errors to be visible in the tree
instead of only in a status bar.

## 11. SFTP Service Layer

### `SFTPManager`

Owns one Paramiko `SSHClient` and one associated `SFTPClient`. Each machine
panel has its own manager. The manager is used by worker threads for blocking
network operations.

#### `__init__(...)`

Stores host, port, username, optional key path, optional password, and timeout
values. It initializes `client` and `sftp` to `None`.

#### `_try_load_pkey(filepath)`

Expands `~`, verifies existence, then tries Paramiko's Ed25519, RSA, and ECDSA
private-key loaders. It returns the first parsed key or `None`. DSA is searched
as a filename but is not explicitly loaded here.

#### `connect()`

1. Closes any previous session.
2. Creates `paramiko.SSHClient`.
3. Uses `AutoAddPolicy`, which accepts unknown host keys automatically.
4. Builds connection parameters and chooses authentication:
   - Explicit key first.
   - Default local keys if no password was supplied.
   - Password if available.
   - Agent/discovered keys otherwise.
5. Connects with socket, banner, and auth timeouts.
6. If key authentication raises `AuthenticationException` and a password was
   supplied, retries with password authentication.
7. Opens SFTP.

The use of `AutoAddPolicy` is convenient but weakens host authenticity. A
production client should verify known host keys.

#### `get_absolute_path(remote_path=".")`

Normalizes backslashes to forward slashes and asks the SFTP server to resolve
the path. If normalization fails, it joins the server's normalized current
directory with the requested path. This is used for display and for creation
operations.

#### `list_directory(remote_path=".")`

Calls `listdir_attr`, converts each `SFTPAttributes` record into a dictionary,
detects directories with `stat.S_ISDIR`, includes a hidden-file flag, and sorts
directories before files, case-insensitively by name.

When listing `.`, the path is represented as the filename. For other paths it
is joined with `posixpath.join`.

#### `create_file(remote_path)`

Resolves the path, opens it in write-binary mode, and immediately closes it.
This creates an empty file or truncates an existing file, depending on the
server's semantics.

#### `create_directory(remote_path)`

Resolves the path and calls SFTP `mkdir`.

#### `delete_file(remote_path)`

Normalizes the path and calls SFTP `remove`.

#### `delete_directory_recursive(remote_path)`

Recursively lists a directory. Real subdirectories are recursively deleted;
symlinks are treated as removable entries rather than traversed. It then
removes the directory itself. This is destructive and is called only after a
GUI confirmation.

#### `rename(old_path, new_path)`

Normalizes both paths and calls SFTP `rename`.

#### `stat_path(remote_path)`

Attempts SFTP `stat`, returning attributes or `None` on any failure. It is a
small convenience method and is not currently used by the UI flow.

#### `calculate_tree_size(remote_path)`

Recursively computes total bytes and file count. A regular file contributes its
size and one file; a directory recursively contributes its descendants. Any
exception causes that subtree call to return `(0, 0)`, which makes transfer
progress best-effort rather than authoritative.

#### `disconnect()`

Closes the SFTP client, then the SSH client, swallowing close errors, and sets
both references to `None`.

#### `is_connected()`

Requires both client objects and an active Paramiko transport.

## 12. Tailscale Discovery

### `TailscaleDiscovery`

Static/class methods isolate local Tailscale process execution from the UI.

#### `find_tailscale_binary()`

Checks `shutil.which` first, then platform-specific fallback paths for macOS,
Windows, and Linux. It returns the first existing path or `None`.

#### `check_port_22(ip, timeout)`

Attempts a TCP connection to port 22 with `socket.create_connection`. Any
socket, timeout, or OS failure returns `False`; success returns `True`.

This proves reachability of a TCP listener only. It does not prove that SSH
authentication or SFTP will succeed.

#### `get_online_machines(check_ssh=True)`

The primary path executes `tailscale status --json` with captured text output
and a six-second process timeout. It parses:

- `Self` into one local `MachineInfo`.
- `Peer` values into peer `MachineInfo` objects.
- The first IPv4-looking Tailscale address in `TailscaleIPs`.

Offline machines are filtered out. If `check_ssh` is true, every remaining
machine is probed sequentially on port 22.

If JSON execution or parsing fails, it falls back to `tailscale status`. The
fallback treats the first two whitespace-separated fields as IP and name and
uses a simple `offline` substring check. This parser is intentionally loose and
may not handle all CLI formatting variants.

All exceptions are swallowed and result in an empty list. That keeps discovery
from crashing the UI, but it also makes diagnosis harder.

## 13. Worker Signals and Background Workers

### `WorkerSignals`

A reusable `QObject` containing:

- `started`: no arguments.
- `finished`: no arguments.
- `error`: one string.
- `result`: one object.
- `progress`: bytes done, total bytes, item label, and speed in bytes/sec.

### Worker lifecycle pattern

Every worker subclasses `QRunnable`, creates `WorkerSignals`, and implements a
`run()` slot. The pattern is:

1. Emit `started`.
2. Execute blocking work.
3. Emit `result` on success.
4. Emit `error` on failure.
5. Always emit `finished`.

Signals are wrapped in `try/except RuntimeError` because the receiver or Qt
object may have been destroyed while a worker is finishing.

### `DiscoveryWorker`

Stores `check_ssh`, calls `TailscaleDiscovery.get_online_machines`, and emits
the resulting `MachineInfo` list.

### `PortCheckWorker`

Stores an IP and timeout, runs `check_port_22`, and emits a boolean.

### `SFTPConnectWorker`

Stores an `SFTPManager`, calls `connect`, and emits that manager after success.

### `SFTPListWorker`

Stores a manager, remote path, target `FSNode`, and target `QModelIndex`. It
lists the directory and emits `(parent_node, entries, parent_index)`. Errors
are packed into a pipe-delimited string containing the path, exception text,
and Python object ID. The object ID is currently not consumed by the receiver.

### `SFTPFileOpWorker`

Uses the string `op_type` dispatch table embedded in `run()`:

- `create_file`
- `create_dir`
- `delete_file`
- `delete_dir`
- `rename`

Arguments are stored as a tuple and passed positionally to the manager.
Unknown operation types currently emit success without performing an operation;
an enum or explicit validation would be safer.

### `CrossMachineTransferWorker`

Copies or moves one remote file/directory between two SFTP managers. It stores
source/destination paths, directory and cut flags, display host names, byte
counters, start time, and a cancellation flag.

#### `cancel()`

Sets `_is_cancelled`. There is currently no visible Cancel button, but the
method provides the worker-side hook.

#### `run()`

1. Verifies both SFTP sessions are active.
2. Detects whether the managers point at the same host.
3. If source and destination are identical, a cut is treated as a no-op; a copy
   is renamed to a sibling with `_copy` before its extension.
4. Calculates total size from the source.
5. For same-host cuts, uses remote `rename`, which is efficient.
6. For files, streams the source to the destination and optionally deletes the
   source afterward.
7. For directories, recursively creates destination directories and streams
   each file, then optionally deletes the source tree.

Errors emit `error`; successful operations emit `True`; all paths emit
`finished`.

#### `_stream_single_file(src, dst)`

Opens source read-binary and destination write-binary SFTP handles. It copies
64 KiB chunks until EOF or cancellation. After each chunk it calculates elapsed
time, transfer speed, and emits a progress signal. The display label includes
absolute source and destination paths.

#### `_transfer_dir_recursive(s_src, s_dst, src, dst)`

Creates the destination directory, ignores a mkdir failure, lists source
entries, recursively handles directories, and delegates files to
`_stream_single_file`.

## 14. Authentication Widget

### `MachineAuthBar`

`MachineAuthBar` is a collapsible `QFrame` containing username, password,
private-key, remember, browse, show/hide, and connect controls.

Signals:

- `auth_requested(username, password, key_path, remember)`.
- `dismissed()`.

#### `__init__(host_ip, host_name, parent)`

Builds the form entirely in Python, applies local stylesheet rules, connects the
close, password, browse, and submit actions, and defaults the username to the
current local user.

#### `load_credentials(creds)`

Copies saved username, password, and key path values into the form if present.

#### `show_alert(message)` and `clear_alert()`

Show or hide an error banner. `show_alert` also makes the form visible.

#### `_toggle_password_visibility()`

Switches the password line edit between masked and normal echo modes and
changes the button glyph.

#### `_browse_key_file()`

Opens a file dialog rooted at `~/.ssh` and places the selected path into the key
field.

#### `_on_submit()`

Collects trimmed username/key text, leaves password unchanged, resolves an
empty username to the local user, reads the remember checkbox, and emits
`auth_requested`.

## 15. Responsive Machine Grid

### `ResponsiveGridContainer`

Owns a `QGridLayout` and a list of panels. It keeps machine cards arranged in a
responsive grid.

#### `add_panel(panel)`

Adds a panel once and forces layout recalculation.

#### `remove_panel(panel)`

Removes the panel from the list and layout, detaches its parent, and rearranges.

#### `clear_panels()`

Removes every widget from the layout, detaches it, clears the list, and resets
the cached column count.

#### `resizeEvent(event)`

Calls the base implementation and recalculates columns.

#### `_calculate_columns()`

Computes `width // min_col_width`, clamps to at least one and at most four.

#### `_rearrange_layout(force=False)`

Avoids work when the column count and widget count are unchanged. Otherwise it
removes all layout items and adds panels row by row, then applies equal stretch
to each column.

## 16. Remote Tree View and Drag-and-Drop

### `RemoteFileTreeView`

Subclass of `QTreeView` that owns a reference to its `MachinePanelWidget` and
adds file-manager interaction.

The constructor enables dragging, accepting drops, drop indicators, copy/move
actions, single selection, and whole-row selection.

#### `startDrag(supportedActions)`

Gets the selected `FSNode`, refuses dummy/disconnected nodes, and serializes a
JSON payload into the custom MIME type `application/x-dirzero-item`. The payload
contains source IP, host, path, name, and directory status. It also provides a
human-readable text MIME value and builds a small painted preview pixmap.

The drag advertises copy and move, defaulting to copy.

#### `dragEnterEvent(event)` and `dragMoveEvent(event)`

Accept only the custom MIME type when the destination SFTP session is active.

#### `dropEvent(event)`

Parses the JSON payload, identifies the directory under the pointer (or the
root), interprets Move or Shift as a cut, locates the source panel by IP, and
calls `MachinePanelWidget._handle_drop_transfer`. Invalid payloads or unknown
source panels are rejected with a status message.

#### `keyPressEvent(event)`

Maps copy, cut, paste, delete/backspace, F2, and F5 to the owning panel's
actions. Other keys are passed to `QTreeView`.

## 17. Machine Panel

### `MachinePanelWidget`

The central per-machine controller. It is both a styled `QFrame` and the owner
of one `SFTPManager`, one root `FSNode`, one `RemoteFSModel`, one tree view,
the authentication form, and all machine-level actions.

### Construction and setup

#### `__init__(machine_info, username=None, key_path=None, parent=None)`

Initializes credentials and worker tracking, overlays saved credentials from
`CredentialStore`, constructs the SFTP manager and model, builds the UI, applies
the initial machine state, and subscribes to the shared clipboard signal.

#### `_setup_ui()`

Attempts to load `MachinePanel.ui` with `QUiLoader`. If successful, it locates
named labels/buttons/layouts, replaces the Designer `QTreeView` with the custom
tree view, and embeds the loaded form. If loading fails, it constructs a small
fallback layout programmatically.

It then creates the authentication bar, toolbar buttons, path label, retry
button, and fallback Disconnect button. It inserts the toolbar and auth bar
near the end of the card layout, configures the model/header, enables movable
and interactive columns, connects expansion and context-menu signals, and
installs keyboard actions.

#### `_setup_shortcuts()`

Creates widget-scoped `QAction` objects for Copy, Cut, Paste, F2, Delete, and
F5. Actions are added to both the panel and tree view so they work when child
widgets have focus.

#### `_apply_machine_data()`

Updates the heading with an OS glyph, uppercase name, and local-machine badge;
updates the IP label; then chooses initial state based on `ssh_available`.
Reachable machines immediately start SFTP connection; unreachable ones show a
probe action.

### State management

#### `set_state(state, detail=None)`

Sets the Qt dynamic property `machineState`, repolishes the widget, enables or
disables file controls, updates the status label, and changes visibility of
refresh/retry/auth controls.

Important transitions:

- `ONLINE_SSH_OK`: network probe passed; connection is about to start.
- `CONNECTING`: disables refresh and indicates active connection.
- `CONNECTED`: enables the tree and file creation/paste controls, hides auth.
- `AUTH_REQUIRED`: disables the tree, shows an authentication error row/form.
- `ONLINE_SSH_UNAVAILABLE`: disables the tree and shows a port-22 error row.
- `ERROR`: displays the exception and allows retry.

The stylesheet matches the dynamic property to change card appearance.

#### `_toggle_auth_bar()`

Toggles visibility of the authentication form.

#### `_on_auth_form_submit(username, password, key_path, remember)`

Updates in-memory credentials, optionally persists them under the machine IP,
updates the existing manager, and starts a new connection worker.

#### `disconnect_machine()`

Closes the SFTP session, removes saved entries under both IP and name, clears
the in-memory password and password field, resets the root model, sets an error
row, changes the path label, enters `AUTH_REQUIRED`, shows the auth form, and
posts a status-bar message.

#### `_on_clipboard_changed()`

Enables Paste only when this panel is connected and the shared clipboard has an
item.

#### `_on_action_button_clicked()`

Dispatches the retry button to port probing, authentication-form display, or a
fresh connection depending on the current dynamic state.

### Connection and browsing

#### `_probe_port_22()` and `_on_port_check_result(is_open)`

Start a `PortCheckWorker`, disable retry while it runs, and either transition
to SSH-ready plus connection or leave a port-unreachable status.

#### `start_connection()`

Sets `CONNECTING`, creates `SFTPConnectWorker`, connects result/error/finished
signals, tracks the worker, and submits it to the global thread pool.

#### `_on_sftp_connected(manager)`

Sets `CONNECTED` and refreshes the root directory.

#### `_on_sftp_error(err_msg)`

Classifies an error as authentication-related if its lowercase text contains
`auth`, `permission denied`, `publickey`, `password`, or `session`; it chooses
`AUTH_REQUIRED` for those and `ERROR` otherwise.

This is a heuristic string classifier, not a typed exception classifier.

#### `reload_filesystem()`

Reconnects if necessary. Otherwise it marks the root loading, starts an
`SFTPListWorker` for `.`, and temporarily disables Refresh.

#### `_on_tree_expanded(index)`

Starts lazy listing only for a valid, unloaded, non-loading directory.

#### `_on_dir_listed(result_tuple)`

Applies worker results to the model, re-enables Refresh, and updates the path
label with the server-normalized home/current path for root listings.

#### `_on_dir_list_error(err_payload)`

Splits the worker's pipe-delimited error payload, re-enables Refresh, and shows
an error on the root when the failed path is root or root has not loaded.
Nested-directory errors are currently not rendered on their specific node.

### Selection and target resolution

#### `_get_selected_node()`

Returns the internal `FSNode` from selected indexes, falling back to the current
index.

#### `_get_target_dir(node)`

Uses a selected directory as the destination, otherwise uses the selected
node's parent unless that parent is the hidden root, otherwise returns `.`.

### Context menu and file operations

#### `_show_context_menu(pos)`

Creates a context menu for a selected item or blank area. It provides Copy,
Cut, Paste, Rename, Delete, New File, New Folder, and Refresh actions as
appropriate, then executes the menu at the global cursor position.

#### `_copy_selected()`, `_cut_selected()`, `_rename_selected()`,
`_delete_selected()`

Resolve the selected node, reject missing/dummy rows with a status message, and
delegate to the corresponding node-level method.

#### `_copy_node(node)` and `_cut_node(node)`

Write to the shared `RemoteClipboard` and post a status message.

#### `_paste_clipboard()`

Resolves the selected target directory and delegates to `_paste_into_dir`.

#### `_paste_into_dir(dest_dir)`

Validates the clipboard, computes destination path, starts the main-window
progress monitor, constructs `CrossMachineTransferWorker`, connects result,
error, progress, and finished signals, and submits the worker.

The destination path is the source name under the target directory. For root,
that means a sibling named like the source. Same-host cuts are optimized by the
worker into a rename.

#### `_handle_drop_transfer(...)`

Performs the same transfer setup for drag-and-drop, including a status message
that says Copying or Moving.

#### `_on_paste_finished(src_panel, is_cut, name)`

Refreshes the destination. For a cut, it refreshes the source and clears the
clipboard. It then reports success.

#### `_create_new_file(target_dir=None)`

Requires an active connection, resolves the target from selection when absent,
prompts for a filename, starts `SFTPFileOpWorker("create_file", ...)`, and
refreshes through `_on_file_op_success`.

#### `_create_new_folder(target_dir=None)`

Same flow as file creation, using `create_dir` and `mkdir`.

#### `_rename_node(node)`

Prompts for a replacement name, derives the parent path with `posixpath`, and
starts a rename worker.

#### `_delete_node(node)`

Prompts for confirmation, chooses file removal or recursive directory removal,
and starts the operation worker.

#### `_on_file_op_success(msg)`

Refreshes the filesystem and reports success.

#### `_on_file_op_error(err_msg)`

Reports the error in the status bar and displays a warning dialog.

#### `_notify_status(message)`

Finds the parent `Dir2ZeroWindow` and shows a six-second message in its status
bar.

#### `close()`

Disconnects the machine's SFTP session and calls the base widget close method.

## 18. Main Window

### `Dir2ZeroWindow`

The application-level `QMainWindow`. It owns discovery, the machine-panel list,
the responsive grid, global transfer progress, theme controls, and status bar.

#### `__init__(username=None, key_path=None, parent=None)`

Initializes worker tracking and panel state, builds the UI, starts a 20-second
discovery timer, and performs initial foreground discovery.

#### `_setup_ui()`

Builds the main window programmatically:

- Window title and minimum/initial sizes.
- Header with title and subtitle.
- Online, reachable, and unavailable badges.
- Theme menu and cycle action.
- Refresh Tailnet button.
- Resizable scroll area containing `ResponsiveGridContainer`.
- Hidden transfer progress dock with path label, progress label, and bar.
- Status bar.

Although `Dir2Zero.ui` exists, this method does not load it. The form is a
minimal Designer artifact and the Python layout is the active main-window UI.

### Discovery and reconciliation

#### `start_discovery(background=False)`

Prevents overlapping scans with `_is_discovering`, updates foreground button and
status text, creates a `DiscoveryWorker`, and connects result/error/finished
signals. Background timer scans reuse the same logic without replacing the
button text.

#### `_on_discovery_completed(machines, background)`

Updates the three summary badges, indexes existing panels by IP, creates panels
for new IPs, updates changed SSH reachability, and closes/removes panels whose
IP disappeared from discovery.

Active panels are retained by IP so a background discovery does not destroy an
open SFTP session. If no machines exist, a placeholder label is added.

#### `_on_discovery_error(err)`

Shows a discovery failure in the status bar.

#### `_on_discovery_finished()`

Clears the discovery guard and restores the scan button.

### Transfer progress

#### `start_transfer_monitor(src_path, dst_path, src_host, dst_host)`

Shows the progress dock, resets the bar, displays source/destination paths,
and sets a preparing message.

#### `update_transfer_progress(bytes_done, total_bytes, item_name, speed_bps)`

Calculates a bounded integer percentage, updates source/destination labels when
the worker includes an arrow separator, formats byte values, and displays
progress plus speed.

Its nested `format_size(b)` helper formats a byte count using B, KB, MB, or GB
units with the same 1024-based thresholds used by `FSNode.size_formatted()`.

#### `finish_transfer_monitor()`

Sets the bar to 100 and hides the dock after 2.5 seconds.

#### `closeEvent(event)`

Stops discovery, closes all panels, waits up to one second for the global thread
pool, and delegates to the base close event.

### Theme callbacks

- `_select_theme(theme_id)`: switches the selected theme.
- `_cycle_theme()`: advances to the next theme.
- `_on_theme_changed(theme_id, theme_name)`: refreshes the theme button and
  reports the selected theme.
- `_update_theme_button_label()`: displays the current theme name and arrow.

## 19. Stylesheet and Theme Loading

### `FALLBACK_STYLESHEET`

An embedded QSS string used only when the selected theme and `style.qss` cannot
be read. It defines the base dark UI, machine state colors, tree view, headers,
scrollbars, and status bar.

### `DEFAULT_THEMES`

Registers four themes by ID, display name, and relative file path:

- `cybersecurity_dark`
- `light_grayscale`
- `midnight_cyber`
- `classic_dark`

Additional `.qss` files in `themes/` are discovered dynamically and named from
their filenames. `contrast.qss` is therefore available even though it is not in
the initial list.

### `ThemeManager`

Singleton `QObject` with `theme_changed(theme_id, theme_name)`.

#### `instance()`

Lazily returns the process-wide manager.

#### `__init__()`

Initializes theme storage and calls `load_available_themes()`.

#### `load_available_themes()`

Copies defaults, scans the theme directory for extra `.qss` files, and restores
the saved theme index from `.active_theme`.

#### `get_saved_theme_id()` and `save_theme_id(theme_id)`

Read/write the selected ID in the project working directory. Errors are ignored.

#### `get_current_theme()`

Returns the indexed theme, or an embedded default descriptor if the list is
empty.

#### `apply_current_theme(app=None)`

Reads the selected file, falls back to `style.qss`, then to
`FALLBACK_STYLESHEET`, applies the resulting QSS to the QApplication, persists
the selected ID, emits `theme_changed`, and returns success.

#### `switch_to_theme(theme_id, app=None)`

Finds an ID, changes the index, and applies it. Unknown IDs return `False`.

#### `cycle_next_theme(app=None)`

Moves to the next theme modulo the list length, applies it, and returns the new
ID/name pair.

### `load_stylesheet(app)`

Convenience wrapper that applies the current theme through the singleton.

## 20. Command-Line Entry Point

### `parse_args()`

Defines:

- `--user` / `-u`: SSH username, defaulting to `SSH_USER` or the local user.
- `--key` / `-k`: key path, defaulting to `SSH_KEY` or the first discovered
  standard private key.

### `main()`

Parses arguments, creates `QApplication`, names it `DirZero`, loads the theme,
constructs and shows `Dir2ZeroWindow`, and enters `app.exec()` until exit.

The `if __name__ == "__main__"` guard makes direct execution the normal entry
point.

## 21. Qt Threading Model

The GUI thread owns widgets and the item model. Worker threads perform:

- Tailscale subprocess calls.
- TCP socket probes.
- Paramiko connection and SFTP calls.
- File transfer loops.

Qt queued signal delivery returns results to slots on the receiver's thread,
which is normally the GUI thread because the panels and main window were
created there. Model mutations happen in `_on_dir_listed` and related slots,
not in the worker itself.

`_active_workers` sets keep Python references alive while tasks run. Finished
signals remove those references. The global Qt thread pool controls actual
concurrency.

Important ownership rule: a given `SFTPManager` is used by its panel's workers,
but Paramiko clients are not generally safe to use concurrently without care.
Concurrent refresh, file operation, and transfer tasks against the same manager
should be serialized or guarded in a more robust implementation.

## 22. End-to-End User Workflows

### Startup

1. Parse username/key options.
2. Apply the saved theme.
3. Construct the main window.
4. Discover online Tailscale peers.
5. Probe SSH port 22.
6. Create one card per online peer.
7. Automatically connect reachable peers.
8. Load each root listing.

### Authentication

1. A peer is reachable but key/agent authentication fails.
2. The panel enters `AUTH_REQUIRED`.
3. The tree displays an error row and the auth bar becomes available.
4. The user enters a password/key and optionally remembers it.
5. The panel updates the manager and starts a new connection worker.

### Copy or cut

1. Select an item.
2. Press Ctrl+C/Ctrl+X or use the context menu.
3. `RemoteClipboard` stores source panel/path/name/type.
4. Select a destination folder and paste, or drag the item to another panel.
5. A transfer worker streams/copies or renames the remote object.
6. Destination and, for cuts, source listings refresh.

### File operations

New file, new folder, rename, and delete all prompt or confirm on the GUI
thread, then perform the blocking SFTP request in a worker and refresh on
completion.

## 23. Python-to-C++ Implementation Plan

`main.cpp` already includes many of the corresponding Qt headers, but it needs
actual classes and implementations. A clean C++ port should split the current
single file into headers and source files rather than reproducing another
monolith.

### Suggested C++ structure

```text
src/
  main.cpp
  CredentialStore.hpp/.cpp
  Models.hpp/.cpp
  SftpManager.hpp/.cpp
  TailscaleDiscovery.hpp/.cpp
  Workers.hpp/.cpp
  MachineAuthBar.hpp/.cpp
  RemoteFileTreeView.hpp/.cpp
  MachinePanelWidget.hpp/.cpp
  Dir2ZeroWindow.hpp/.cpp
  ThemeManager.hpp/.cpp
```

### Type mapping

| Python | C++/Qt equivalent |
|---|---|
| `Optional[str]` | `std::optional<QString>` or nullable `QString` |
| `List[T]` | `QVector<T>` or `std::vector<T>` |
| `Dict[str, Any]` | typed struct, `QVariantMap`, or `QJsonObject` |
| `Set[Any]` | `QSet<T*>` |
| `Path` | `QFileInfo`, `QDir`, or `std::filesystem::path` |
| `QObject` | `QObject` with `Q_OBJECT` |
| `Signal(...)` | Qt `signals:` declarations |
| `@Slot` | Qt `slots:` declarations or `Q_INVOKABLE` where appropriate |
| `QRunnable` | `QRunnable` subclass |
| `QThreadPool.globalInstance()` | `QThreadPool::globalInstance()` |
| Paramiko `SSHClient` | libssh2, libssh, Botan-based SSH, or another SSH library |
| Paramiko SFTP | the selected SSH library's SFTP API |
| Python `subprocess.run` | `QProcess` |
| Python `json` | `QJsonDocument`, `QJsonObject`, `QJsonArray` |
| `posixpath` | `QDir::cleanPath` with explicit POSIX handling, or `std::filesystem` carefully constrained to `/` |
| `QMimeData` payload | `QByteArray` plus `QJsonDocument` |

### `FSNode` in C++

Use a class or struct with `QString name/path`, numeric metadata, a raw or smart
parent pointer, and `QVector<std::unique_ptr<FSNode>> children`. A model index
can store `FSNode*` with `createIndex`. Keep ownership stable while indexes are
alive; replacing children must follow Qt model begin/end notifications exactly.

### `RemoteFSModel` in C++

Subclass `QAbstractItemModel` and implement:

```cpp
int columnCount(const QModelIndex& parent) const override;
int rowCount(const QModelIndex& parent) const override;
QModelIndex index(int row, int column,
                  const QModelIndex& parent) const override;
QModelIndex parent(const QModelIndex& child) const override;
QVariant data(const QModelIndex& index, int role) const override;
QVariant headerData(int section, Qt::Orientation orientation,
                    int role) const override;
```

The Python `internalPointer()` approach maps directly to `index.internalPointer()`.
Use `beginRemoveRows`, `beginInsertRows`, and matching end calls exactly as the
Python model does.

### SSH/SFTP layer in C++

Qt itself does not provide a complete SSH/SFTP client. Choose and wrap one
library behind an interface such as:

```cpp
class ISftpManager {
public:
    virtual ~ISftpManager() = default;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual QVector<RemoteEntry> listDirectory(const QString& path) = 0;
    virtual void createFile(const QString& path) = 0;
    virtual void createDirectory(const QString& path) = 0;
    virtual void removeFile(const QString& path) = 0;
    virtual void removeDirectoryRecursive(const QString& path) = 0;
    virtual void rename(const QString& oldPath, const QString& newPath) = 0;
};
```

This interface makes the UI testable with a fake SFTP backend and avoids
coupling every widget to a third-party API.

### Workers in C++

Declare worker signals explicitly:

```cpp
class SftpListWorker final : public QObject, public QRunnable {
    Q_OBJECT
public:
    void run() override;
signals:
    void result(RemoteNode* parent, QVector<RemoteEntry> entries);
    void error(const QString& message);
    void finished();
};
```

For queued delivery, construct workers in a way that keeps the receiver on the
GUI thread and avoid touching QWidget or model objects inside `run()`. Protect
shared managers with a mutex or serialize operations per host.

### Tailscale discovery in C++

Use `QProcess`:

1. Set program to `tailscale`.
2. Set arguments `status`, `--json`.
3. Connect `finished` and `errorOccurred`.
4. Parse `readAllStandardOutput()` with `QJsonDocument::fromJson`.
5. Convert peer records to typed `MachineInfo` objects.

For TCP probing, `QTcpSocket::connectToHost` with a `QTimer` gives an
asynchronous equivalent to `socket.create_connection`.

### Credential storage in C++

Use `QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)` for
the config directory and `QSaveFile` for atomic writes. Do not store plaintext
passwords for a production port; use a platform keychain library or a Qt
keychain integration.

### UI and `.ui` files in C++

`MachinePanel.ui` can be compiled with `uic` and loaded through a generated
`Ui::MachinePanel` class. `Dir2Zero.ui` can similarly generate a main-window
form, although the current Python window does not use it. Replace the Designer
tree with a subclassed `RemoteFileTreeView` after `setupUi`, as the Python code
does after `QUiLoader` loading.

### Drag-and-drop in C++

Override `startDrag`, `dragEnterEvent`, `dragMoveEvent`, and `dropEvent`. Store
the same JSON fields in a `QMimeData` format named
`application/x-dirzero-item`. Use `QJsonDocument` rather than manual string
parsing.

### Styles and themes in C++

Read QSS with `QFile`, apply it using `QApplication::setStyleSheet`, and store
the selected ID with `QSettings`. The Python `.active_theme` text file maps
naturally to `QSettings`, which also avoids dependence on the process working
directory.

## 24. Current Implementation Notes and Risks

These are important facts for anyone modifying the program:

1. **Plaintext credentials:** saved passwords are JSON, even though POSIX mode
   restrictions are attempted.
2. **Host-key trust:** `AutoAddPolicy` accepts unknown SSH host keys. Use a
   known-hosts policy for security-sensitive deployments.
3. **Working-directory paths:** UI, QSS, theme, and `.active_theme` paths are
   relative to the process current directory.
4. **Placeholder lifecycle:** when discovery finds no machines, a placeholder is
   added to the grid, but the reconciliation path does not explicitly remove
   that placeholder when machines later appear.
5. **Panel removal:** discovery removes panels by IP and calls `close`, but
   already-running workers are not explicitly cancelled before the panel is
   detached.
6. **Transfer cancellation:** `_is_cancelled` exists, but no user-facing
   cancellation control calls `cancel()`.
7. **Concurrent SFTP use:** refreshes and operations can overlap on the same
   Paramiko manager. Serialize per-manager operations if stability requires it.
8. **Transfer collisions:** destination existence is generally left to the
   remote server. Only the exact same source/destination case gets a `_copy`
   name for same-host copies.
9. **Directory progress:** total size is calculated recursively, but directory
   transfer progress is still based on aggregate bytes and does not show a file
   count or per-file completion model.
10. **Error routing:** nested directory listing errors are not assigned to the
    exact expanded node; root error presentation is more complete.
11. **Error classification:** authentication detection searches exception text,
    which can misclassify a network error containing words such as `session`.
12. **Unknown file operation:** `SFTPFileOpWorker` emits success for an unknown
    operation string instead of rejecting it.
13. **Symlink policy:** recursive deletion avoids traversing symlinks, but tree
    size calculation and directory transfer do not apply an equally explicit
    symlink policy.
14. **Theme validity:** theme files are loaded as-is. A malformed QSS file may
    lead to missing or partially applied styling; there is no syntax validation.
15. **Theme portability:** some QSS files contain visibly incomplete or invalid
    rules, so each selectable theme should be tested after loading.
16. **Unused artifacts:** `main.cpp` is not a functioning alternate frontend,
    `Dir2Zero.ui` is not used by the active main-window construction, and some
    imports/constants are currently unused.

These notes are documentation of existing behavior, not a claim that each item
must be fixed before normal experimentation.

## 25. Recommended Extension Order

For a safe next phase of development:

1. Add unit tests for path construction, `FSNode` formatting, discovery JSON
   conversion, and transfer destination calculation.
2. Replace pipe-delimited worker errors with typed result/error objects.
3. Add an explicit per-host operation queue or mutex around each SFTP manager.
4. Introduce a real cancellation control and cancellation-aware cleanup.
5. Remove the placeholder explicitly during reconciliation.
6. Move credentials into the OS keychain.
7. Replace `AutoAddPolicy` with known-host verification.
8. Resolve resource paths relative to the executable or a configured project
   root instead of the current working directory.
9. Split the monolith into testable modules.
10. Only then complete the C++ port, reusing the model/view and worker design
    but placing network code behind an interface.

## 26. Quick Run Instructions

From the project directory:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
python3 main.py --user YOUR_SSH_USER --key ~/.ssh/id_ed25519
```

Alternatively, set `SSH_USER` and `SSH_KEY` before launching. Tailscale must be
running and the target machines must expose SSH/SFTP.

The program is GUI-based, so a headless environment needs an appropriate Qt
platform configuration for testing. The discovery command can be checked
independently with:

```bash
tailscale status --json
```

## 27. Definition Checklist

For completeness, the definitions in `main.py` are:

### Top-level functions

`get_default_ssh_keys`, `get_first_available_ssh_key`, `load_stylesheet`,
`parse_args`, `main`.

### Classes

`CredentialStore`, `MachineState`, `FSNode`, `MachineInfo`, `RemoteClipboard`,
`RemoteFSModel`, `SFTPManager`, `TailscaleDiscovery`, `WorkerSignals`,
`DiscoveryWorker`, `PortCheckWorker`, `SFTPConnectWorker`, `SFTPListWorker`,
`SFTPFileOpWorker`, `CrossMachineTransferWorker`, `MachineAuthBar`,
`ResponsiveGridContainer`, `RemoteFileTreeView`, `MachinePanelWidget`,
`Dir2ZeroWindow`, and `ThemeManager`.

Every method on those classes is described in the relevant section above,
including constructors, Qt event handlers, worker entry points, model methods,
filesystem methods, UI callbacks, and theme callbacks.
