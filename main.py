import os
import sys
import stat
import time
import socket
import getpass
import argparse
import subprocess
import json
import shutil
import posixpath
from pathlib import Path
from typing import Optional, List, Dict, Any, Tuple, Set

import paramiko

from PySide6.QtCore import (
    Qt,
    QObject,
    Signal,
    Slot,
    QAbstractItemModel,
    QModelIndex,
    QThreadPool,
    QRunnable,
    QFile,
    QPoint,
    QTimer,
    QSize,
    QMimeData,
    QByteArray,
)
from PySide6.QtWidgets import (
    QApplication,
    QMainWindow,
    QWidget,
    QLabel,
    QPushButton,
    QLineEdit,
    QCheckBox,
    QFileDialog,
    QTreeView,
    QHeaderView,
    QVBoxLayout,
    QHBoxLayout,
    QGridLayout,
    QScrollArea,
    QFrame,
    QStatusBar,
    QProgressBar,
    QSizePolicy,
    QAbstractItemView,
    QMenu,
    QInputDialog,
    QMessageBox,
    QToolButton,
)
from PySide6.QtGui import (
    QAction,
    QKeySequence,
    QFont,
    QColor,
    QDrag,
    QPixmap,
    QPainter,
    QKeyEvent,
    QDragEnterEvent,
    QDragMoveEvent,
    QDropEvent,
)
from PySide6.QtUiTools import QUiLoader
DEFAULT_SSH_PORT = 22
DEFAULT_SOCKET_TIMEOUT = 3.0
DEFAULT_SFTP_TIMEOUT = 8.0
DEFAULT_BANNER_TIMEOUT = 15.0
DEFAULT_AUTH_TIMEOUT = 10.0
DEFAULT_CHUNK_SIZE = 65536  # 64 KB streaming chunks
AUTO_REFRESH_INTERVAL_MS = 20000  # 20 seconds periodic discovery check

UI_DIR2ZERO_PATH = "Dir2Zero.ui"
UI_MACHINEPANEL_PATH = "MachinePanel.ui"
QSS_STYLE_PATH = "style.qss"

# Standard SSH private key search paths
DEFAULT_SSH_KEY_FILENAMES = [
    "id_ed25519",
    "id_rsa",
    "id_ecdsa",
    "id_dsa",
]


def get_default_ssh_keys() -> List[str]:
    """Returns list of standard SSH private key paths across Linux, macOS, and Windows."""
    ssh_dir = Path.home() / ".ssh"
    keys: List[str] = []
    for key_name in DEFAULT_SSH_KEY_FILENAMES:
        p = ssh_dir / key_name
        if p.exists():
            keys.append(str(p))
    return keys


def get_first_available_ssh_key() -> str:
    keys = get_default_ssh_keys()
    if keys:
        return keys[0]
    return str(Path.home() / ".ssh" / "id_ed25519")

class CredentialStore:
    @classmethod
    def _get_config_path(cls) -> Path:
        if sys.platform == "win32":
            appdata = os.environ.get("APPDATA")
            base = Path(appdata) if appdata else Path.home()
            config_dir = base / "DirZero"
        elif sys.platform == "darwin":
            config_dir = Path.home() / "Library" / "Application Support" / "DirZero"
        else:
            xdg_config = os.environ.get("XDG_CONFIG_HOME")
            base = Path(xdg_config) if xdg_config else (Path.home() / ".config")
            config_dir = base / "dirzero"

        try:
            config_dir.mkdir(parents=True, exist_ok=True)
            # Apply restricted permissions on POSIX systems
            if hasattr(os, "chmod") and sys.platform != "win32":
                try:
                    os.chmod(config_dir, 0o700)
                except Exception:
                    pass
        except Exception:
            pass

        return config_dir / "credentials.json"

    @classmethod
    def load_all(cls) -> Dict[str, Dict[str, Any]]:
        path = cls._get_config_path()
        if not path.exists():
            return {}
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
                return data if isinstance(data, dict) else {}
        except Exception:
            return {}

    @classmethod
    def get(cls, host_ip: str, host_name: Optional[str] = None) -> Optional[Dict[str, Any]]:
        creds = cls.load_all()
        if host_ip in creds:
            return creds[host_ip]
        if host_name and host_name in creds:
            return creds[host_name]
        return None

    @classmethod
    def save(
        cls,
        host_key: str,
        username: str,
        password: Optional[str] = None,
        key_path: Optional[str] = None,
    ):
        creds = cls.load_all()
        entry: Dict[str, Any] = {"username": username}
        if password:
            entry["password"] = password
        if key_path:
            entry["key_path"] = key_path

        creds[host_key] = entry
        path = cls._get_config_path()
        try:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(creds, f, indent=2)
            if hasattr(os, "chmod") and sys.platform != "win32":
                try:
                    os.chmod(path, 0o600)
                except Exception:
                    pass
        except Exception as e:
            print(f"[CredentialStore] Warning: Could not save credentials: {e}", file=sys.stderr)

    @classmethod
    def remove(cls, host_key: str):
        creds = cls.load_all()
        if host_key in creds:
            del creds[host_key]
            path = cls._get_config_path()
            try:
                with open(path, "w", encoding="utf-8") as f:
                    json.dump(creds, f, indent=2)
            except Exception:
                pass

class MachineState:
    DISCOVERING = "DISCOVERING"
    ONLINE_SSH_OK = "ONLINE_SSH_OK"
    ONLINE_SSH_UNAVAILABLE = "ONLINE_SSH_UNAVAILABLE"
    AUTH_REQUIRED = "AUTH_REQUIRED"
    CONNECTING = "CONNECTING"
    CONNECTED = "CONNECTED"
    ERROR = "ERROR"
    DISCONNECTED = "DISCONNECTED"
    OFFLINE = "OFFLINE"


class FSNode:
    def __init__(
        self,
        name: str,
        path: str,
        is_dir: bool = False,
        size: int = 0,
        mtime: float = 0.0,
        mode: int = 0,
        parent: Optional["FSNode"] = None,
        is_dummy: bool = False,
        error: Optional[str] = None,
    ):
        self.name = name
        self.path = path
        self.is_dir = is_dir
        self.size = size
        self.mtime = mtime
        self.mode = mode
        self.parent = parent
        self.children: List["FSNode"] = []
        self.is_loaded = False
        self.is_loading = False
        self.is_dummy = is_dummy
        self.error = error
        if self.is_dir and not self.is_dummy:
            self.children.append(
                FSNode("Loading...", "", is_dir=False, parent=self, is_dummy=True)
            )

    def row(self) -> int:
        if self.parent:
            try:
                return self.parent.children.index(self)
            except ValueError:
                return 0
        return 0

    def child(self, row: int) -> Optional["FSNode"]:
        if 0 <= row < len(self.children):
            return self.children[row]
        return None

    def child_count(self) -> int:
        return len(self.children)

    def size_formatted(self) -> str:
        if self.is_dir or self.is_dummy:
            return "-"
        if self.size < 1024:
            return f"{self.size} B"
        elif self.size < 1024 * 1024:
            return f"{self.size / 1024:.1f} KB"
        elif self.size < 1024 * 1024 * 1024:
            return f"{self.size / (1024 * 1024):.1f} MB"
        else:
            return f"{self.size / (1024 * 1024 * 1024):.2f} GB"

    def mtime_formatted(self) -> str:
        if self.is_dummy or not self.mtime:
            return "-"
        try:
            return time.strftime("%Y-%m-%d %H:%M", time.localtime(self.mtime))
        except Exception:
            return "-"

    def permissions_formatted(self) -> str:
        if self.is_dummy or not self.mode:
            return "-"
        try:
            return stat.filemode(self.mode)
        except Exception:
            return "-"


class MachineInfo:
    def __init__(
        self,
        name: str,
        dns_name: str,
        ip: str,
        os_type: str = "linux",
        online: bool = True,
        ssh_available: bool = False,
        is_self: bool = False,
    ):
        self.name = name
        self.dns_name = dns_name
        self.ip = ip
        self.os_type = os_type.lower()
        self.online = online
        self.ssh_available = ssh_available
        self.is_self = is_self

    def __repr__(self) -> str:
        return f"<MachineInfo {self.name} ({self.ip}) online={self.online} ssh={self.ssh_available}>"

class RemoteClipboard(QObject):
    clipboard_changed = Signal()
    _instance = None
    @classmethod
    def instance(cls) -> "RemoteClipboard":
        if cls._instance is None:
            cls._instance = RemoteClipboard()
        return cls._instance
    def __init__(self):
        super().__init__()
        self.source_panel: Optional["MachinePanelWidget"] = None
        self.source_path: str = ""
        self.source_name: str = ""
        self.is_dir: bool = False
        self.is_cut: bool = False

    def copy(self, panel: "MachinePanelWidget", node: FSNode):
        self.source_panel = panel
        self.source_path = node.path
        self.source_name = node.name
        self.is_dir = node.is_dir
        self.is_cut = False
        self.clipboard_changed.emit()

    def cut(self, panel: "MachinePanelWidget", node: FSNode):
        self.source_panel = panel
        self.source_path = node.path
        self.source_name = node.name
        self.is_dir = node.is_dir
        self.is_cut = True
        self.clipboard_changed.emit()

    def clear(self):
        self.source_panel = None
        self.source_path = ""
        self.source_name = ""
        self.is_dir = False
        self.is_cut = False
        self.clipboard_changed.emit()

    def has_item(self) -> bool:
        return bool(self.source_panel and self.source_path and self.source_name)

    def summary(self) -> str:
        if not self.has_item():
            return "Clipboard is empty"
        action = "Cut" if self.is_cut else "Copy"
        machine = self.source_panel.machine_info.name if self.source_panel else "Unknown"
        return f"{action} '{self.source_name}' from [{machine}]"

class RemoteFSModel(QAbstractItemModel):
    HEADERS = ["Name", "Size", "Modified", "Permissions"]
    def __init__(self, root_node: FSNode, parent: Optional[QObject] = None):
        super().__init__(parent)
        self.root_node = root_node

    def columnCount(self, parent: QModelIndex = QModelIndex()) -> int:
        return len(self.HEADERS)

    def headerData(
        self, section: int, orientation: Qt.Orientation, role: int = Qt.DisplayRole
    ) -> Any:
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            if 0 <= section < len(self.HEADERS):
                return self.HEADERS[section]
        return None

    def rowCount(self, parent: QModelIndex = QModelIndex()) -> int:
        if not parent.isValid():
            parent_node = self.root_node
        else:
            parent_node = parent.internalPointer()

        if parent_node is None:
            return 0
        return parent_node.child_count()

    def index(
        self, row: int, column: int, parent: QModelIndex = QModelIndex()
    ) -> QModelIndex:
        if not self.hasIndex(row, column, parent):
            return QModelIndex()

        if not parent.isValid():
            parent_node = self.root_node
        else:
            parent_node = parent.internalPointer()

        if parent_node is None:
            return QModelIndex()

        child_node = parent_node.child(row)
        if child_node:
            return self.createIndex(row, column, child_node)
        return QModelIndex()

    def parent(self, index: QModelIndex) -> QModelIndex:
        if not index.isValid():
            return QModelIndex()

        child_node = index.internalPointer()
        if child_node is None:
            return QModelIndex()

        parent_node = child_node.parent
        if parent_node is None or parent_node == self.root_node:
            return QModelIndex()

        return self.createIndex(parent_node.row(), 0, parent_node)

    def data(self, index: QModelIndex, role: int = Qt.DisplayRole) -> Any:
        if not index.isValid():
            return None

        node: FSNode = index.internalPointer()
        if node is None:
            return None

        col = index.column()

        if role == Qt.DisplayRole:
            if col == 0:
                if node.error:
                    return f"⚠️ {node.name}"
                elif node.is_dummy:
                    return f"⏳ {node.name}"
                elif node.is_dir:
                    return f"📁 {node.name}"
                else:
                    return f"📄 {node.name}"
            elif col == 1:
                return node.size_formatted()
            elif col == 2:
                return node.mtime_formatted()
            elif col == 3:
                return node.permissions_formatted()

        elif role == Qt.ToolTipRole:
            if node.error:
                return f"Error: {node.error}"
            return f"Path: {node.path}\nSize: {node.size_formatted()}\nModified: {node.mtime_formatted()}\nPermissions: {node.permissions_formatted()}"

        return None

    def populate_node(
        self, parent_node: FSNode, entries: List[Dict[str, Any]], parent_index: QModelIndex = QModelIndex()
    ):
        if parent_node.child_count() > 0:
            self.beginRemoveRows(parent_index, 0, parent_node.child_count() - 1)
            parent_node.children.clear()
            self.endRemoveRows()

        if entries:
            self.beginInsertRows(parent_index, 0, len(entries) - 1)
            for item in entries:
                child = FSNode(
                    name=item["name"],
                    path=item["path"],
                    is_dir=item["is_dir"],
                    size=item["size"],
                    mtime=item["mtime"],
                    mode=item["mode"],
                    parent=parent_node,
                )
                parent_node.children.append(child)
            self.endInsertRows()

        parent_node.is_loaded = True
        parent_node.is_loading = False

    def set_node_error(
        self, parent_node: FSNode, error_msg: str, parent_index: QModelIndex = QModelIndex()
    ):
        if parent_node.child_count() > 0:
            self.beginRemoveRows(parent_index, 0, parent_node.child_count() - 1)
            parent_node.children.clear()
            self.endRemoveRows()

        self.beginInsertRows(parent_index, 0, 0)
        err_child = FSNode(
            name=f"[{error_msg}]",
            path=parent_node.path,
            is_dir=False,
            parent=parent_node,
            error=error_msg,
        )
        parent_node.children.append(err_child)
        self.endInsertRows()

        parent_node.is_loaded = True
        parent_node.is_loading = False

class SFTPManager:
    def __init__(
        self,
        host: str,
        port: int = DEFAULT_SSH_PORT,
        username: Optional[str] = None,
        key_filepath: Optional[str] = None,
        password: Optional[str] = None,
        timeout: float = DEFAULT_SFTP_TIMEOUT,
        banner_timeout: float = DEFAULT_BANNER_TIMEOUT,
        auth_timeout: float = DEFAULT_AUTH_TIMEOUT,
    ):
        self.host = host
        self.port = port
        self.username = username or getpass.getuser()
        self.key_filepath = key_filepath
        self.password = password
        self.timeout = timeout
        self.banner_timeout = banner_timeout
        self.auth_timeout = auth_timeout
        self.client: Optional[paramiko.SSHClient] = None
        self.sftp: Optional[paramiko.SFTPClient] = None

    def _try_load_pkey(self, filepath: str) -> Optional[paramiko.PKey]:
        expanded = os.path.expanduser(filepath)
        if not os.path.exists(expanded):
            return None

        loaders = [
            paramiko.Ed25519Key.from_private_key_file,
            paramiko.RSAKey.from_private_key_file,
            paramiko.ECDSAKey.from_private_key_file,
        ]
        for loader in loaders:
            try:
                return loader(expanded)
            except Exception:
                continue
        return None

    def connect(self):
        self.disconnect()
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        connect_kwargs: Dict[str, Any] = {
            "hostname": self.host,
            "port": self.port,
            "username": self.username,
            "timeout": self.timeout,
            "banner_timeout": self.banner_timeout,
            "auth_timeout": self.auth_timeout,
        }
        pkey = None
        if self.key_filepath:
            pkey = self._try_load_pkey(self.key_filepath)
        if not pkey and not self.password:
            for default_key in get_default_ssh_keys():
                pkey = self._try_load_pkey(default_key)
                if pkey:
                    break

        if pkey:
            connect_kwargs["pkey"] = pkey
            connect_kwargs["look_for_keys"] = True
            connect_kwargs["allow_agent"] = True
        elif self.password:
            connect_kwargs["password"] = self.password
            connect_kwargs["look_for_keys"] = False
            connect_kwargs["allow_agent"] = False
        else:
            connect_kwargs["look_for_keys"] = True
            connect_kwargs["allow_agent"] = True

        try:
            self.client.connect(**connect_kwargs)
        except paramiko.AuthenticationException as e:
            if self.password and "pkey" in connect_kwargs:
                del connect_kwargs["pkey"]
                connect_kwargs["password"] = self.password
                connect_kwargs["look_for_keys"] = False
                connect_kwargs["allow_agent"] = False
                self.client.connect(**connect_kwargs)
            else:
                raise e

        self.sftp = self.client.open_sftp()

    def get_absolute_path(self, remote_path: str = ".") -> str:
        if not self.sftp:
            return remote_path
        clean_path = remote_path.replace("\\", "/")
        if clean_path == ".":
            try:
                return self.sftp.normalize(".")
            except Exception:
                return "."
        try:
            return self.sftp.normalize(clean_path)
        except Exception:
            try:
                base = self.sftp.normalize(".")
                return posixpath.join(base, clean_path)
            except Exception:
                return clean_path

    def list_directory(self, remote_path: str = ".") -> List[Dict[str, Any]]:
        if not self.sftp:
            raise RuntimeError("SFTP session is not active. Call connect() first.")

        attrs = self.sftp.listdir_attr(remote_path)
        entries: List[Dict[str, Any]] = []

        for a in attrs:
            is_dir = stat.S_ISDIR(a.st_mode) if a.st_mode else False
            if remote_path == ".":
                full_path = a.filename
            else:
                full_path = posixpath.join(remote_path, a.filename)

            entry = {
                "name": a.filename,
                "path": full_path,
                "is_dir": is_dir,
                "size": a.st_size if not is_dir else 0,
                "mtime": a.st_mtime or 0.0,
                "mode": a.st_mode or 0,
                "hidden": a.filename.startswith("."),
            }
            entries.append(entry)
        entries.sort(key=lambda x: (not x["is_dir"], x["name"].lower()))
        return entries

    def create_file(self, remote_path: str):
        if not self.sftp:
            raise RuntimeError("SFTP session is not active.")
        abs_path = self.get_absolute_path(remote_path)
        f = self.sftp.open(abs_path, "wb")
        f.close()

    def create_directory(self, remote_path: str):
        if not self.sftp:
            raise RuntimeError("SFTP session is not active.")
        abs_path = self.get_absolute_path(remote_path)
        self.sftp.mkdir(abs_path)

    def delete_file(self, remote_path: str):
        if not self.sftp:
            raise RuntimeError("SFTP session is not active.")
        remote_path = posixpath.normpath(remote_path)
        self.sftp.remove(remote_path)

    def delete_directory_recursive(self, remote_path: str):
        if not self.sftp:
            raise RuntimeError("SFTP session is not active.")
        remote_path = posixpath.normpath(remote_path)
        for item in self.sftp.listdir_attr(remote_path):
            sub_path = posixpath.join(remote_path, item.filename)
            is_dir = stat.S_ISDIR(item.st_mode) if item.st_mode else False
            is_link = stat.S_ISLNK(item.st_mode) if item.st_mode else False
            if is_dir and not is_link:
                self.delete_directory_recursive(sub_path)
            else:
                self.sftp.remove(sub_path)
        self.sftp.rmdir(remote_path)

    def rename(self, old_path: str, new_path: str):
        if not self.sftp:
            raise RuntimeError("SFTP session is not active.")
        self.sftp.rename(posixpath.normpath(old_path), posixpath.normpath(new_path))

    def stat_path(self, remote_path: str) -> Optional[paramiko.SFTPAttributes]:
        if not self.sftp:
            return None
        try:
            return self.sftp.stat(remote_path)
        except Exception:
            return None

    def calculate_tree_size(self, remote_path: str) -> Tuple[int, int]:
        if not self.sftp:
            return 0, 0
        try:
            attrs = self.sftp.stat(remote_path)
            if not stat.S_ISDIR(attrs.st_mode):
                return attrs.st_size, 1

            total_bytes = 0
            file_count = 0
            for item in self.sftp.listdir_attr(remote_path):
                sub = posixpath.join(remote_path, item.filename)
                if stat.S_ISDIR(item.st_mode):
                    b, c = self.calculate_tree_size(sub)
                    total_bytes += b
                    file_count += c
                else:
                    total_bytes += item.st_size or 0
                    file_count += 1
            return total_bytes, file_count
        except Exception:
            return 0, 0

    def disconnect(self):
        if self.sftp:
            try:
                self.sftp.close()
            except Exception:
                pass
            self.sftp = None

        if self.client:
            try:
                self.client.close()
            except Exception:
                pass
            self.client = None

    def is_connected(self) -> bool:
        if not self.client or not self.sftp:
            return False
        transport = self.client.get_transport()
        return transport is not None and transport.is_active()

class TailscaleDiscovery:
    @staticmethod
    def find_tailscale_binary() -> Optional[str]:
        bin_name = "tailscale.exe" if sys.platform == "win32" else "tailscale"
        which_path = shutil.which(bin_name) or shutil.which("tailscale")
        if which_path:
            return which_path
        candidates: List[str] = []
        if sys.platform == "darwin":
            candidates = [
                "/Applications/Tailscale.app/Contents/MacOS/Tailscale",
                "/usr/local/bin/tailscale",
                "/opt/homebrew/bin/tailscale",
            ]
        elif sys.platform == "win32":
            candidates = [
                r"C:\Program Files\Tailscale\tailscale.exe",
                r"C:\Program Files (x86)\Tailscale\tailscale.exe",
                os.path.expandvars(r"%LOCALAPPDATA%\Tailscale\tailscale.exe"),
                os.path.expandvars(r"%PROGRAMFILES%\Tailscale\tailscale.exe"),
            ]
        else:
            candidates = [
                "/usr/bin/tailscale",
                "/usr/local/bin/tailscale",
                "/bin/tailscale",
            ]

        for p in candidates:
            if os.path.exists(p):
                return p
        return None

    @staticmethod
    def check_port_22(ip: str, timeout: float = DEFAULT_SOCKET_TIMEOUT) -> bool:
        try:
            with socket.create_connection((ip, DEFAULT_SSH_PORT), timeout=timeout):
                return True
        except (socket.timeout, socket.error, OSError):
            return False

    @classmethod
    def get_online_machines(cls, check_ssh: bool = True) -> List[MachineInfo]:
        machines: List[MachineInfo] = []
        ts_bin = cls.find_tailscale_binary()
        if not ts_bin:
            return machines
        subprocess_kwargs: Dict[str, Any] = {
            "capture_output": True,
            "text": True,
            "timeout": 6.0,
        }
        if sys.platform == "win32" and hasattr(subprocess, "CREATE_NO_WINDOW"):
            subprocess_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
        try:
            res = subprocess.run([ts_bin, "status", "--json"], **subprocess_kwargs)
            if res.returncode == 0:
                data = json.loads(res.stdout)
                self_node = data.get("Self", {})
                if self_node:
                    ips = [ip for ip in self_node.get("TailscaleIPs", []) if "." in ip]
                    if ips:
                        m_self = MachineInfo(
                            name=self_node.get("HostName", "Localhost"),
                            dns_name=self_node.get("DNSName", "").rstrip("."),
                            ip=ips[0],
                            os_type=self_node.get("OS", "linux"),
                            online=self_node.get("Online", True),
                            is_self=True,
                        )
                        machines.append(m_self)

                for peer in data.get("Peer", {}).values():
                    ips = [ip for ip in peer.get("TailscaleIPs", []) if "." in ip]
                    if ips:
                        m_peer = MachineInfo(
                            name=peer.get("HostName", "Unknown"),
                            dns_name=peer.get("DNSName", "").rstrip("."),
                            ip=ips[0],
                            os_type=peer.get("OS", "unknown"),
                            online=peer.get("Online", False),
                            is_self=False,
                        )
                        machines.append(m_peer)

                online_machines = [m for m in machines if m.online]

                if check_ssh:
                    for m in online_machines:
                        m.ssh_available = cls.check_port_22(m.ip)

                return online_machines
        except Exception:
            pass
        try:
            res = subprocess.run([ts_bin, "status"], **subprocess_kwargs)
            if res.returncode == 0:
                lines = res.stdout.splitlines()
                for idx, line in enumerate(lines):
                    line = line.strip()
                    if not line:
                        continue
                    parts = line.split()
                    if len(parts) >= 2:
                        ip = parts[0]
                        name = parts[1]
                        online = "offline" not in line.lower()
                        if online:
                            m = MachineInfo(
                                name=name,
                                dns_name=name,
                                ip=ip,
                                os_type="linux",
                                online=True,
                                is_self=(idx == 0),
                            )
                            if check_ssh:
                                m.ssh_available = cls.check_port_22(ip)
                            machines.append(m)
                return machines
        except Exception:
            pass

        return machines

class WorkerSignals(QObject):
    started = Signal()
    finished = Signal()
    error = Signal(str)
    result = Signal(object)
    progress = Signal(int, int, str, float)

class DiscoveryWorker(QRunnable):
    def __init__(self, check_ssh: bool = True):
        super().__init__()
        self.signals = WorkerSignals()
        self.check_ssh = check_ssh

    @Slot()
    def run(self):
        try:
            try:
                self.signals.started.emit()
            except RuntimeError:
                pass
            machines = TailscaleDiscovery.get_online_machines(check_ssh=self.check_ssh)
            try:
                self.signals.result.emit(machines)
            except RuntimeError:
                pass
        except Exception as e:
            try:
                self.signals.error.emit(str(e))
            except RuntimeError:
                pass
        finally:
            try:
                self.signals.finished.emit()
            except RuntimeError:
                pass


class PortCheckWorker(QRunnable):
    def __init__(self, ip: str, timeout: float = DEFAULT_SOCKET_TIMEOUT):
        super().__init__()
        self.signals = WorkerSignals()
        self.ip = ip
        self.timeout = timeout

    @Slot()
    def run(self):
        try:
            try:
                self.signals.started.emit()
            except RuntimeError:
                pass
            is_open = TailscaleDiscovery.check_port_22(self.ip, timeout=self.timeout)
            try:
                self.signals.result.emit(is_open)
            except RuntimeError:
                pass
        except Exception as e:
            try:
                self.signals.error.emit(str(e))
            except RuntimeError:
                pass
        finally:
            try:
                self.signals.finished.emit()
            except RuntimeError:
                pass


class SFTPConnectWorker(QRunnable):
    def __init__(self, sftp_manager: SFTPManager):
        super().__init__()
        self.signals = WorkerSignals()
        self.sftp_manager = sftp_manager

    @Slot()
    def run(self):
        try:
            try:
                self.signals.started.emit()
            except RuntimeError:
                pass
            self.sftp_manager.connect()
            try:
                self.signals.result.emit(self.sftp_manager)
            except RuntimeError:
                pass
        except Exception as e:
            try:
                self.signals.error.emit(str(e))
            except RuntimeError:
                pass
        finally:
            try:
                self.signals.finished.emit()
            except RuntimeError:
                pass


class SFTPListWorker(QRunnable):
    def __init__(
        self,
        sftp_manager: SFTPManager,
        remote_path: str,
        parent_node: FSNode,
        parent_index: QModelIndex,
    ):
        super().__init__()
        self.signals = WorkerSignals()
        self.sftp_manager = sftp_manager
        self.remote_path = remote_path
        self.parent_node = parent_node
        self.parent_index = parent_index

    @Slot()
    def run(self):
        try:
            try:
                self.signals.started.emit()
            except RuntimeError:
                pass
            entries = self.sftp_manager.list_directory(self.remote_path)
            try:
                self.signals.result.emit((self.parent_node, entries, self.parent_index))
            except RuntimeError:
                pass
        except Exception as e:
            try:
                self.signals.error.emit(
                    f"{self.remote_path}|{e}|{id(self.parent_node)}"
                )
            except RuntimeError:
                pass
        finally:
            try:
                self.signals.finished.emit()
            except RuntimeError:
                pass


class SFTPFileOpWorker(QRunnable):
    def __init__(self, op_type: str, sftp_manager: SFTPManager, *args):
        super().__init__()
        self.signals = WorkerSignals()
        self.op_type = op_type
        self.sftp_manager = sftp_manager
        self.args = args

    @Slot()
    def run(self):
        try:
            try:
                self.signals.started.emit()
            except RuntimeError:
                pass

            if self.op_type == "create_file":
                self.sftp_manager.create_file(self.args[0])
            elif self.op_type == "create_dir":
                self.sftp_manager.create_directory(self.args[0])
            elif self.op_type == "delete_file":
                self.sftp_manager.delete_file(self.args[0])
            elif self.op_type == "delete_dir":
                self.sftp_manager.delete_directory_recursive(self.args[0])
            elif self.op_type == "rename":
                self.sftp_manager.rename(self.args[0], self.args[1])

            try:
                self.signals.result.emit(self.op_type)
            except RuntimeError:
                pass
        except Exception as e:
            try:
                self.signals.error.emit(str(e))
            except RuntimeError:
                pass
        finally:
            try:
                self.signals.finished.emit()
            except RuntimeError:
                pass


class CrossMachineTransferWorker(QRunnable):
    def __init__(
        self,
        src_manager: SFTPManager,
        dst_manager: SFTPManager,
        src_path: str,
        dst_path: str,
        is_dir: bool = False,
        is_cut: bool = False,
        src_host_name: str = "",
        dst_host_name: str = "",
    ):
        super().__init__()
        self.signals = WorkerSignals()
        self.src_manager = src_manager
        self.dst_manager = dst_manager
        self.src_path = src_path
        self.dst_path = dst_path
        self.is_dir = is_dir
        self.is_cut = is_cut
        self.src_host_name = src_host_name
        self.dst_host_name = dst_host_name

        self._bytes_transferred = 0
        self._total_bytes = 0
        self._start_time = time.time()
        self._is_cancelled = False

    def cancel(self):
        self._is_cancelled = True

    @Slot()
    def run(self):
        try:
            try:
                self.signals.started.emit()
            except RuntimeError:
                pass

            if not self.src_manager.is_connected() or not self.dst_manager.is_connected():
                raise RuntimeError("Source or destination SFTP session is not active.")

            is_same_host = (self.src_manager.host == self.dst_manager.host)

            if is_same_host and self.src_path == self.dst_path:
                if self.is_cut:
                    try:
                        self.signals.result.emit(True)
                    except RuntimeError:
                        pass
                    return
                else:
                    p_dir = posixpath.dirname(self.dst_path)
                    p_base = posixpath.basename(self.dst_path)
                    name, ext = posixpath.splitext(p_base)
                    self.dst_path = posixpath.join(p_dir, f"{name}_copy{ext}") if p_dir else f"{name}_copy{ext}"
            self._total_bytes, _ = self.src_manager.calculate_tree_size(self.src_path)
            self._start_time = time.time()

            if is_same_host and self.is_cut:
                self.src_manager.rename(self.src_path, self.dst_path)
                try:
                    p_name = f"[{self.src_host_name}] {self.src_path} -> [{self.dst_host_name}] {self.dst_path}"
                    self.signals.progress.emit(self._total_bytes, self._total_bytes, p_name, 0.0)
                    self.signals.result.emit(True)
                except RuntimeError:
                    pass
                return

            if not self.is_dir:
                self._stream_single_file(self.src_path, self.dst_path)
                if self.is_cut and not self._is_cancelled:
                    self.src_manager.delete_file(self.src_path)
            else:
                self._transfer_dir_recursive(
                    self.src_manager.sftp,
                    self.dst_manager.sftp,
                    self.src_path,
                    self.dst_path,
                )
                if self.is_cut and not self._is_cancelled:
                    self.src_manager.delete_directory_recursive(self.src_path)

            try:
                self.signals.result.emit(True)
            except RuntimeError:
                pass
        except Exception as e:
            try:
                self.signals.error.emit(str(e))
            except RuntimeError:
                pass
        finally:
            try:
                self.signals.finished.emit()
            except RuntimeError:
                pass

    def _stream_single_file(self, src: str, dst: str):
        src_abs = self.src_manager.get_absolute_path(src)
        dst_abs = self.dst_manager.get_absolute_path(dst)
        item_name = f"[{self.src_host_name}] {src_abs} ➔ [{self.dst_host_name}] {dst_abs}"
        with self.src_manager.sftp.open(src, "rb") as src_f:
            with self.dst_manager.sftp.open(dst, "wb") as dst_f:
                while not self._is_cancelled:
                    chunk = src_f.read(DEFAULT_CHUNK_SIZE)
                    if not chunk:
                        break
                    dst_f.write(chunk)
                    self._bytes_transferred += len(chunk)

                    elapsed = max(0.001, time.time() - self._start_time)
                    speed = self._bytes_transferred / elapsed
                    try:
                        self.signals.progress.emit(
                            self._bytes_transferred,
                            max(self._bytes_transferred, self._total_bytes),
                            item_name,
                            speed,
                        )
                    except RuntimeError:
                        pass

    def _transfer_dir_recursive(self, s_src, s_dst, src: str, dst: str):
        if self._is_cancelled:
            return

        try:
            s_dst.mkdir(dst)
        except Exception:
            pass

        for item in s_src.listdir_attr(src):
            if self._is_cancelled:
                return

            s_sub = posixpath.join(src, item.filename)
            d_sub = posixpath.join(dst, item.filename)

            if stat.S_ISDIR(item.st_mode):
                self._transfer_dir_recursive(s_src, s_dst, s_sub, d_sub)
            else:
                self._stream_single_file(s_sub, d_sub)

class MachineAuthBar(QFrame):
    auth_requested = Signal(str, str, str, bool)  # (username, password, key_path, remember)
    dismissed = Signal()

    def __init__(self, host_ip: str, host_name: str, parent: Optional[QWidget] = None):
        super().__init__(parent)
        self.host_ip = host_ip
        self.host_name = host_name
        self._is_password_visible = False

        self.setObjectName("authFrame")
        self.setFrameShape(QFrame.StyledPanel)
        self.setStyleSheet(
            "QFrame#authFrame { background: #131b28; border: 1px solid #334155; border-radius: 8px; padding: 8px; margin: 4px 0px; }"
            "QLabel { color: #cbd5e1; font-size: 11px; font-weight: 600; }"
            "QLineEdit { background: #0b0f16; color: #f8fafc; border: 1px solid #28354b; border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
            "QLineEdit:focus { border-color: #3b82f6; }"
            "QCheckBox { color: #94a3b8; font-size: 11px; }"
            "QCheckBox::indicator { width: 14px; height: 14px; }"
        )

        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(6)
        title_box = QHBoxLayout()
        self.title_label = QLabel("🔑 SSH Authentication")
        self.title_label.setStyleSheet("color: #60a5fa; font-weight: 700; font-size: 11px;")
        title_box.addWidget(self.title_label)
        title_box.addStretch()

        self.btn_close = QToolButton(self)
        self.btn_close.setText("✕")
        self.btn_close.setStyleSheet("QToolButton { background: transparent; color: #94a3b8; border: none; font-weight: 700; } QToolButton:hover { color: #ef4444; }")
        self.btn_close.clicked.connect(self.dismissed.emit)
        title_box.addWidget(self.btn_close)
        layout.addLayout(title_box)

        # Alert banner for auth errors
        self.alert_label = QLabel("")
        self.alert_label.setStyleSheet("color: #f87171; font-size: 10px; font-weight: 600;")
        self.alert_label.setWordWrap(True)
        self.alert_label.setVisible(False)
        layout.addWidget(self.alert_label)

        # Row 1: Username & Password with Show/Hide Toggle
        row1 = QHBoxLayout()
        row1.setSpacing(6)

        self.input_user = QLineEdit(self)
        self.input_user.setPlaceholderText("Username")
        self.input_user.setText(getpass.getuser())
        row1.addWidget(self.input_user, 1)

        pass_box = QHBoxLayout()
        pass_box.setSpacing(0)
        self.input_pass = QLineEdit(self)
        self.input_pass.setPlaceholderText("Password")
        self.input_pass.setEchoMode(QLineEdit.Password)
        self.input_pass.returnPressed.connect(self._on_submit)
        pass_box.addWidget(self.input_pass)

        self.btn_toggle_pass = QPushButton("👁", self)
        self.btn_toggle_pass.setFixedSize(26, 26)
        self.btn_toggle_pass.setStyleSheet(
            "QPushButton { background: #1e293b; color: #94a3b8; border: 1px solid #28354b; border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px; font-size: 11px; }"
            "QPushButton:hover { background: #334155; color: #ffffff; }"
        )
        self.btn_toggle_pass.clicked.connect(self._toggle_password_visibility)
        pass_box.addWidget(self.btn_toggle_pass)
        row1.addLayout(pass_box, 1)

        layout.addLayout(row1)
        row2 = QHBoxLayout()
        row2.setSpacing(6)

        self.input_key = QLineEdit(self)
        self.input_key.setPlaceholderText("Private Key (optional, e.g. ~/.ssh/id_ed25519)")
        row2.addWidget(self.input_key, 1)

        self.btn_browse_key = QPushButton("📂 Browse...", self)
        self.btn_browse_key.setFixedHeight(26)
        self.btn_browse_key.setStyleSheet(
            "QPushButton { background: #1e293b; color: #cbd5e1; border: 1px solid #334155; border-radius: 4px; padding: 2px 8px; font-size: 10px; font-weight: 600; }"
            "QPushButton:hover { background: #334155; color: #ffffff; }"
        )
        self.btn_browse_key.clicked.connect(self._browse_key_file)
        row2.addWidget(self.btn_browse_key)

        layout.addLayout(row2)
        row3 = QHBoxLayout()
        row3.setSpacing(8)

        self.chk_remember = QCheckBox("Save credentials", self)
        self.chk_remember.setChecked(True)
        row3.addWidget(self.chk_remember)

        row3.addStretch()

        self.btn_connect = QPushButton("🔑 Connect", self)
        self.btn_connect.setFixedHeight(26)
        self.btn_connect.setStyleSheet(
            "QPushButton { background: #2563eb; color: #ffffff; border: none; border-radius: 4px; padding: 2px 12px; font-size: 11px; font-weight: 700; }"
            "QPushButton:hover { background: #1d4ed8; }"
        )
        self.btn_connect.clicked.connect(self._on_submit)
        row3.addWidget(self.btn_connect)

        layout.addLayout(row3)

    def load_credentials(self, creds: Optional[Dict[str, Any]]):
        if creds:
            if "username" in creds:
                self.input_user.setText(creds["username"])
            if "password" in creds:
                self.input_pass.setText(creds["password"])
            if "key_path" in creds:
                self.input_key.setText(creds["key_path"])

    def show_alert(self, message: str):
        self.alert_label.setText(f"⚠️ {message}")
        self.alert_label.setVisible(True)
        self.setVisible(True)

    def clear_alert(self):
        self.alert_label.setVisible(False)

    def _toggle_password_visibility(self):
        self._is_password_visible = not self._is_password_visible
        if self._is_password_visible:
            self.input_pass.setEchoMode(QLineEdit.Normal)
            self.btn_toggle_pass.setText("🔒")
        else:
            self.input_pass.setEchoMode(QLineEdit.Password)
            self.btn_toggle_pass.setText("👁")

    def _browse_key_file(self):
        home_ssh = str(Path.home() / ".ssh")
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select SSH Private Key File",
            home_ssh,
            "All Files (*);;Key Files (*.pem *.key id_*)",
        )
        if file_path:
            self.input_key.setText(file_path)

    def _on_submit(self):
        username = self.input_user.text().strip() or getpass.getuser()
        password = self.input_pass.text()
        key_path = self.input_key.text().strip()
        remember = self.chk_remember.isChecked()
        self.auth_requested.emit(username, password, key_path, remember)


class ResponsiveGridContainer(QWidget):
    def __init__(self, parent: Optional[QWidget] = None, min_col_width: int = 380):
        super().__init__(parent)
        self.min_col_width = min_col_width
        self.panels: List[QWidget] = []
        self.current_cols = -1

        self.grid_layout = QGridLayout(self)
        self.grid_layout.setContentsMargins(12, 12, 12, 12)
        self.grid_layout.setSpacing(16)
        self.setLayout(self.grid_layout)

    def add_panel(self, panel: QWidget):
        if panel not in self.panels:
            self.panels.append(panel)
            self._rearrange_layout(force=True)

    def remove_panel(self, panel: QWidget):
        if panel in self.panels:
            self.panels.remove(panel)
            self.grid_layout.removeWidget(panel)
            panel.setParent(None)
            self._rearrange_layout(force=True)

    def clear_panels(self):
        while self.grid_layout.count():
            item = self.grid_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.setParent(None)
        self.panels.clear()
        self.current_cols = -1

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._rearrange_layout()

    def _calculate_columns(self) -> int:
        w = self.width()
        if w <= 0:
            return 1
        cols = max(1, w // self.min_col_width)
        return min(cols, 4)  # Cap at 4 columns for clean presentation

    def _rearrange_layout(self, force: bool = False):
        if not self.panels:
            return

        cols = self._calculate_columns()
        if not force and cols == self.current_cols and self.grid_layout.count() == len(self.panels):
            return

        self.current_cols = cols
        while self.grid_layout.count():
            self.grid_layout.takeAt(0)

        for i, panel in enumerate(self.panels):
            row = i // cols
            col = i % cols
            self.grid_layout.addWidget(panel, row, col)
        for c in range(cols):
            self.grid_layout.setColumnStretch(c, 1)


class RemoteFileTreeView(QTreeView):
    def __init__(self, panel: "MachinePanelWidget", parent: Optional[QWidget] = None):
        super().__init__(parent)
        self.panel = panel

        # Drag and Drop Configuration
        self.setDragEnabled(True)
        self.setAcceptDrops(True)
        self.setDropIndicatorShown(True)
        self.setDragDropMode(QAbstractItemView.DragDrop)
        self.setDefaultDropAction(Qt.CopyAction)
        self.setSelectionMode(QAbstractItemView.SingleSelection)
        self.setSelectionBehavior(QAbstractItemView.SelectRows)

    def startDrag(self, supportedActions):
        node = self.panel._get_selected_node()
        if not node or node.is_dummy or not self.panel.sftp_manager.is_connected():
            return

        payload = {
            "source_ip": self.panel.machine_info.ip,
            "source_host": self.panel.machine_info.name,
            "source_path": node.path,
            "source_name": node.name,
            "is_dir": node.is_dir,
        }

        mime_data = QMimeData()
        mime_data.setData("application/x-dirzero-item", QByteArray(json.dumps(payload).encode("utf-8")))
        mime_data.setText(f"{self.panel.machine_info.name}:{node.path}")

        drag = QDrag(self)
        drag.setMimeData(mime_data)

        # Create visual drag preview badge
        pixmap = QPixmap(190, 28)
        pixmap.fill(Qt.transparent)
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setBrush(QColor("#1e3a8a"))
        painter.setPen(QColor("#60a5fa"))
        painter.drawRoundedRect(1, 1, 188, 26, 4, 4)
        painter.setPen(QColor("#ffffff"))
        icon = "📁" if node.is_dir else "📄"
        display_text = f"{icon} {node.name}"
        if len(display_text) > 22:
            display_text = display_text[:20] + "…"
        painter.drawText(8, 18, display_text)
        painter.end()

        drag.setPixmap(pixmap)
        drag.setHotSpot(QPoint(15, 14))

        drag.exec(Qt.CopyAction | Qt.MoveAction, Qt.CopyAction)

    def dragEnterEvent(self, event: QDragEnterEvent):
        if event.mimeData().hasFormat("application/x-dirzero-item") and self.panel.sftp_manager.is_connected():
            event.acceptProposedAction()
        else:
            event.ignore()

    def dragMoveEvent(self, event: QDragMoveEvent):
        if event.mimeData().hasFormat("application/x-dirzero-item") and self.panel.sftp_manager.is_connected():
            event.acceptProposedAction()
        else:
            event.ignore()

    def dropEvent(self, event: QDropEvent):
        if not event.mimeData().hasFormat("application/x-dirzero-item") or not self.panel.sftp_manager.is_connected():
            event.ignore()
            return

        try:
            raw_bytes = bytes(event.mimeData().data("application/x-dirzero-item"))
            data = json.loads(raw_bytes.decode("utf-8"))
        except Exception:
            event.ignore()
            return

        src_ip = data.get("source_ip", "")
        src_path = data.get("source_path", "")
        src_name = data.get("source_name", "")
        is_dir = data.get("is_dir", False)

        if not src_path or not src_name:
            event.ignore()
            return

        pos = event.position().toPoint()
        drop_idx = self.indexAt(pos)
        if drop_idx.isValid():
            target_node: FSNode = drop_idx.internalPointer()
            if target_node and target_node.is_dir and not target_node.is_dummy:
                dest_dir = target_node.path
            elif target_node and target_node.parent and target_node.parent != self.panel.root_node:
                dest_dir = target_node.parent.path
            else:
                dest_dir = "."
        else:
            dest_dir = "."

        is_cut = (event.dropAction() == Qt.MoveAction) or bool(event.modifiers() & Qt.ShiftModifier)

        main_win = self.window()
        src_panel = None
        if isinstance(main_win, Dir2ZeroWindow):
            for p in main_win.machine_panels:
                if p.machine_info.ip == src_ip:
                    src_panel = p
                    break

        if not src_panel:
            self.panel._notify_status("Drop failed: Could not locate source machine panel.")
            event.ignore()
            return

        self.panel._handle_drop_transfer(
            src_panel=src_panel,
            src_path=src_path,
            src_name=src_name,
            is_dir=is_dir,
            dest_dir=dest_dir,
            is_cut=is_cut,
        )
        event.acceptProposedAction()

    def keyPressEvent(self, event: QKeyEvent):
        """Interprets keyboard shortcuts when tree view has focus."""
        # Copy: Ctrl+C
        if (event.key() == Qt.Key_C and event.modifiers() & Qt.ControlModifier) or event.matches(QKeySequence.Copy):
            self.panel._copy_selected()
            event.accept()
            return

        # Cut: Ctrl+X
        if (event.key() == Qt.Key_X and event.modifiers() & Qt.ControlModifier) or event.matches(QKeySequence.Cut):
            self.panel._cut_selected()
            event.accept()
            return

        # Paste: Ctrl+V
        if (event.key() == Qt.Key_V and event.modifiers() & Qt.ControlModifier) or event.matches(QKeySequence.Paste):
            self.panel._paste_clipboard()
            event.accept()
            return

        # Delete: Delete or Backspace
        if event.key() in (Qt.Key_Delete, Qt.Key_Backspace):
            self.panel._delete_selected()
            event.accept()
            return

        # Rename: F2
        if event.key() == Qt.Key_F2:
            self.panel._rename_selected()
            event.accept()
            return

        # Refresh: F5
        if event.key() == Qt.Key_F5:
            self.panel.reload_filesystem()
            event.accept()
            return

        super().keyPressEvent(event)


class MachinePanelWidget(QFrame):
    """
    Reusable card representing a single Tailscale machine and its full remote SFTP filesystem browser.
    Includes in-app authentication, column dragging, drag & drop, file operations, and context menus.
    """

    def __init__(
        self,
        machine_info: MachineInfo,
        username: Optional[str] = None,
        key_path: Optional[str] = None,
        parent: Optional[QWidget] = None,
    ):
        super().__init__(parent)
        self.machine_info = machine_info
        self.username = username or getpass.getuser()
        self.key_path = key_path or get_first_available_ssh_key()
        self.password: Optional[str] = None
        self.thread_pool = QThreadPool.globalInstance()
        self._active_workers: Set[Any] = set()

        # Check for saved credentials in CredentialStore
        saved_creds = CredentialStore.get(self.machine_info.ip, self.machine_info.name)
        if saved_creds:
            if "username" in saved_creds:
                self.username = saved_creds["username"]
            if "password" in saved_creds:
                self.password = saved_creds["password"]
            if "key_path" in saved_creds:
                self.key_path = saved_creds["key_path"]

        self.sftp_manager = SFTPManager(
            host=self.machine_info.ip,
            port=DEFAULT_SSH_PORT,
            username=self.username,
            key_filepath=self.key_path,
            password=self.password,
        )

        self.root_node = FSNode("root", ".", is_dir=True)
        self.fs_model = RemoteFSModel(self.root_node, self)
        self.clipboard = RemoteClipboard.instance()

        self._setup_ui()
        self._apply_machine_data()

        # Connect clipboard change listener
        self.clipboard.clipboard_changed.connect(self._on_clipboard_changed)

    def _setup_ui(self):
        """Loads MachinePanel.ui or constructs fallback layout with full toolbar and auth bar."""
        self.setObjectName("machinePanel")
        self.setFrameShape(QFrame.StyledPanel)
        self.setFrameShadow(QFrame.Raised)
        self.setMinimumSize(360, 440)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        loaded_ui = None
        if os.path.exists(UI_MACHINEPANEL_PATH):
            try:
                loader = QUiLoader()
                qfile = QFile(UI_MACHINEPANEL_PATH)
                if qfile.open(QFile.ReadOnly):
                    loaded_ui = loader.load(qfile, self)
                    qfile.close()
            except Exception:
                loaded_ui = None

        self.btn_disconnect = None
        if loaded_ui:
            self.heading_label = loaded_ui.findChild(QLabel, "machineHeading")
            self.status_label = loaded_ui.findChild(QLabel, "statusLabel")
            self.ip_heading = loaded_ui.findChild(QLabel, "ipHeading")
            self.ip_label = loaded_ui.findChild(QLabel, "ipAddress")
            self.btn_disconnect = loaded_ui.findChild(QPushButton, "disconnect")
            if self.btn_disconnect:
                self.btn_disconnect.setToolTip("Disconnect SSH/SFTP session and forget saved credentials")
                self.btn_disconnect.clicked.connect(self.disconnect_machine)
            old_tree = loaded_ui.findChild(QTreeView, "treeView")

            # Create enhanced RemoteFileTreeView
            self.tree_view = RemoteFileTreeView(self, loaded_ui)
            self.tree_view.setObjectName("treeView")

            card_layout = loaded_ui.findChild(QVBoxLayout, "cardLayout")
            if card_layout and old_tree:
                card_layout.replaceWidget(old_tree, self.tree_view)
                old_tree.deleteLater()

            self.main_layout = QVBoxLayout(self)
            self.main_layout.setContentsMargins(0, 0, 0, 0)
            self.main_layout.addWidget(loaded_ui)
        else:
            self.main_layout = QVBoxLayout(self)
            self.main_layout.setContentsMargins(16, 16, 16, 16)
            self.main_layout.setSpacing(8)

            self.heading_label = QLabel(self.machine_info.name.upper())
            self.heading_label.setObjectName("machineHeading")
            self.main_layout.addWidget(self.heading_label)

            self.status_label = QLabel("● INITIALIZING")
            self.status_label.setObjectName("statusLabel")
            self.main_layout.addWidget(self.status_label)

            self.ip_heading = QLabel("IP ADDRESS")
            self.ip_heading.setObjectName("ipHeading")
            self.main_layout.addWidget(self.ip_heading)

            self.ip_label = QLabel(self.machine_info.ip)
            self.ip_label.setObjectName("ipAddress")
            self.main_layout.addWidget(self.ip_label)

            self.tree_view = RemoteFileTreeView(self)
            self.tree_view.setObjectName("treeView")
            self.main_layout.addWidget(self.tree_view)

        # In-App Authentication Bar (Collapsible)
        self.auth_bar = MachineAuthBar(self.machine_info.ip, self.machine_info.name, self)
        self.auth_bar.auth_requested.connect(self._on_auth_form_submit)
        self.auth_bar.dismissed.connect(lambda: self.auth_bar.setVisible(False))
        self.auth_bar.setVisible(False)

        # Pre-populate saved credentials if any
        saved_creds = CredentialStore.get(self.machine_info.ip, self.machine_info.name)
        self.auth_bar.load_credentials(saved_creds)

        # Action Toolbar for File Operations & Auth
        self.toolbar_layout = QHBoxLayout()
        self.toolbar_layout.setContentsMargins(0, 4, 0, 4)
        self.toolbar_layout.setSpacing(6)

        self.path_label = QLabel("📁 Path: (Connecting...)")
        self.path_label.setStyleSheet("color: #94a3b8; font-size: 11px; font-weight: 500;")
        self.toolbar_layout.addWidget(self.path_label)

        self.toolbar_layout.addStretch()

        # Toolbar: New File Button
        self.btn_new_file = QPushButton("+ File")
        self.btn_new_file.setFixedHeight(26)
        self.btn_new_file.setStyleSheet(
            "QPushButton { background: #1f293d; color: #e2e8f0; border: 1px solid #334155; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #334155; border-color: #60a5fa; color: #ffffff; }"
            "QPushButton:disabled { background: #131822; color: #475569; border-color: #1e293b; }"
        )
        self.btn_new_file.setToolTip("Create a new file on this machine")
        self.btn_new_file.clicked.connect(lambda: self._create_new_file(None))
        self.toolbar_layout.addWidget(self.btn_new_file)

        # Toolbar: New Folder Button
        self.btn_new_folder = QPushButton("+ Folder")
        self.btn_new_folder.setFixedHeight(26)
        self.btn_new_folder.setStyleSheet(
            "QPushButton { background: #1f293d; color: #e2e8f0; border: 1px solid #334155; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #334155; border-color: #60a5fa; color: #ffffff; }"
            "QPushButton:disabled { background: #131822; color: #475569; border-color: #1e293b; }"
        )
        self.btn_new_folder.setToolTip("Create a new folder on this machine")
        self.btn_new_folder.clicked.connect(lambda: self._create_new_folder(None))
        self.toolbar_layout.addWidget(self.btn_new_folder)

        # Toolbar: Paste Button
        self.btn_paste = QPushButton("📥 Paste")
        self.btn_paste.setFixedHeight(26)
        self.btn_paste.setStyleSheet(
            "QPushButton { background: #065f46; color: #34d399; border: 1px solid #059669; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #047857; color: #ffffff; }"
            "QPushButton:disabled { background: #131822; color: #475569; border-color: #1e293b; }"
        )
        self.btn_paste.setEnabled(False)
        self.btn_paste.setToolTip("Paste copied/cut item into this directory")
        self.btn_paste.clicked.connect(self._paste_clipboard)
        self.toolbar_layout.addWidget(self.btn_paste)

        # Toolbar: Auth Toggle Button
        self.btn_auth_toggle = QPushButton("🔑 Auth")
        self.btn_auth_toggle.setFixedHeight(26)
        self.btn_auth_toggle.setStyleSheet(
            "QPushButton { background: #1e293b; color: #93c5fd; border: 1px solid #334155; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #334155; border-color: #3b82f6; }"
        )
        self.btn_auth_toggle.setToolTip("Open in-app authentication form to enter password or SSH key")
        self.btn_auth_toggle.clicked.connect(self._toggle_auth_bar)
        self.toolbar_layout.addWidget(self.btn_auth_toggle)

        # Toolbar: Refresh Button
        self.btn_refresh = QPushButton("↻ Refresh")
        self.btn_refresh.setFixedHeight(26)
        self.btn_refresh.setStyleSheet(
            "QPushButton { background: #252d3d; color: #e2e8f0; border: 1px solid #3b4861; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #323d52; border-color: #60a5fa; }"
        )
        self.btn_refresh.clicked.connect(self.reload_filesystem)
        self.toolbar_layout.addWidget(self.btn_refresh)

        # Toolbar: Probe / Retry Button
        self.btn_retry = QPushButton("⟳ Connect")
        self.btn_retry.setFixedHeight(26)
        self.btn_retry.setStyleSheet(
            "QPushButton { background: #1e3a8a; color: #ffffff; border: 1px solid #3b82f6; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: #2563eb; }"
        )
        self.btn_retry.clicked.connect(self._on_action_button_clicked)
        self.btn_retry.setVisible(False)
        self.toolbar_layout.addWidget(self.btn_retry)

        # Toolbar: Fallback Disconnect Button (if not already loaded from MachinePanel.ui)
        if not self.btn_disconnect:
            self.btn_disconnect = QPushButton("Disconnect")
            self.btn_disconnect.setObjectName("disconnect")
            self.btn_disconnect.setFixedHeight(26)
            self.btn_disconnect.setToolTip("Disconnect SSH/SFTP session and forget saved credentials")
            self.btn_disconnect.clicked.connect(self.disconnect_machine)
            self.toolbar_layout.addWidget(self.btn_disconnect)

        # Insert toolbar and auth_bar into layout above tree view
        card_layout = self.findChild(QVBoxLayout, "cardLayout")
        target_layout = card_layout if card_layout else self.main_layout

        # Insert toolbar
        insert_idx = max(0, target_layout.count() - 1)
        target_layout.insertLayout(insert_idx, self.toolbar_layout)
        # Insert auth_bar
        target_layout.insertWidget(insert_idx + 1, self.auth_bar)

        # Configure QTreeView with Interactive Draggable Column Headers & Context Menu
        if self.tree_view:
            self.tree_view.setModel(self.fs_model)
            self.tree_view.setAlternatingRowColors(True)
            self.tree_view.setUniformRowHeights(True)
            self.tree_view.setHeaderHidden(False)
            self.tree_view.setEditTriggers(QAbstractItemView.NoEditTriggers)
            self.tree_view.setSelectionMode(QAbstractItemView.SingleSelection)

            # Interactive draggable column dividers
            header = self.tree_view.header()
            header.setSectionResizeMode(QHeaderView.Interactive)
            header.setStretchLastSection(True)
            header.setSectionsMovable(True)
            header.setSectionsClickable(True)
            header.resizeSection(0, 180)  # Name
            header.resizeSection(1, 75)   # Size
            header.resizeSection(2, 125)  # Modified
            header.resizeSection(3, 90)   # Permissions

            self.tree_view.expanded.connect(self._on_tree_expanded)

            # Enable Context Menu
            self.tree_view.setContextMenuPolicy(Qt.CustomContextMenu)
            self.tree_view.customContextMenuRequested.connect(self._show_context_menu)

            # Setup keyboard shortcuts
            self._setup_shortcuts()

    def _setup_shortcuts(self):
        """Attaches standard file manager keyboard shortcuts as widget-level shortcuts."""
        for key, slot in [
            (QKeySequence.Copy, self._copy_selected),
            (QKeySequence.Cut, self._cut_selected),
            (QKeySequence.Paste, self._paste_clipboard),
            (QKeySequence(Qt.Key_F2), self._rename_selected),
            (QKeySequence.Delete, self._delete_selected),
            (QKeySequence(Qt.Key_F5), self.reload_filesystem),
        ]:
            act = QAction(self)
            act.setShortcut(key)
            act.setShortcutContext(Qt.WidgetWithChildrenShortcut)
            act.triggered.connect(slot)
            self.addAction(act)
            if self.tree_view:
                self.tree_view.addAction(act)

    def _apply_machine_data(self):
        """Initializes labels and state based on machine reachability."""
        self_badge = " (This Machine)" if self.machine_info.is_self else ""
        os_icon = "🐧" if self.machine_info.os_type == "linux" else ("🪟" if self.machine_info.os_type == "windows" else "💻")

        if self.heading_label:
            self.heading_label.setText(f"{os_icon} {self.machine_info.name.upper()}{self_badge}")

        if self.ip_label:
            self.ip_label.setText(self.machine_info.ip)

        if self.machine_info.ssh_available:
            self.set_state(MachineState.ONLINE_SSH_OK)
            self.start_connection()
        else:
            self.set_state(MachineState.ONLINE_SSH_UNAVAILABLE)

    def set_state(self, state: str, detail: Optional[str] = None):
        """Updates visual state of card according to connection status."""
        self.setProperty("machineState", state)
        self.style().unpolish(self)
        self.style().polish(self)

        is_connected = (state == MachineState.CONNECTED)

        self.btn_new_file.setEnabled(is_connected)
        self.btn_new_folder.setEnabled(is_connected)
        self.btn_paste.setEnabled(is_connected and self.clipboard.has_item())
        if hasattr(self, "btn_disconnect") and self.btn_disconnect:
            self.btn_disconnect.setEnabled(is_connected or state == MachineState.CONNECTING)

        if state == MachineState.ONLINE_SSH_OK:
            if self.status_label:
                self.status_label.setText("● ONLINE — SSH READY")
                self.status_label.setStyleSheet("color: #10b981; font-weight: 700;")
            self.btn_refresh.setVisible(True)
            self.btn_retry.setVisible(False)
            if self.tree_view:
                self.tree_view.setEnabled(True)

        elif state == MachineState.CONNECTING:
            if self.status_label:
                self.status_label.setText("● CONNECTING...")
                self.status_label.setStyleSheet("color: #38bdf8; font-weight: 700;")
            self.btn_refresh.setEnabled(False)

        elif state == MachineState.CONNECTED:
            if self.status_label:
                self.status_label.setText("● CONNECTED")
                self.status_label.setStyleSheet("color: #10b981; font-weight: 700;")
            self.btn_refresh.setVisible(True)
            self.btn_refresh.setEnabled(True)
            self.btn_retry.setVisible(False)
            self.auth_bar.setVisible(False)
            self.auth_bar.clear_alert()
            if self.tree_view:
                self.tree_view.setEnabled(True)

        elif state == MachineState.AUTH_REQUIRED or (state == MachineState.ERROR and detail and "auth" in detail.lower()):
            if self.status_label:
                self.status_label.setText("🔑 AUTHENTICATION REQUIRED")
                self.status_label.setStyleSheet("color: #f59e0b; font-weight: 700;")
            self.btn_refresh.setVisible(False)
            self.btn_retry.setText("🔑 Authenticate")
            self.btn_retry.setVisible(True)
            self.btn_retry.setEnabled(True)
            if self.tree_view:
                self.tree_view.setEnabled(False)
            self.fs_model.set_node_error(
                self.root_node, "Authentication required. Enter password or select private key."
            )
            self.auth_bar.show_alert(detail or "Authentication failed. Enter password or select private key.")

        elif state == MachineState.ONLINE_SSH_UNAVAILABLE:
            if self.status_label:
                self.status_label.setText("⚠ ONLINE — SSH UNAVAILABLE")
                self.status_label.setStyleSheet("color: #f59e0b; font-weight: 700;")
            self.btn_refresh.setVisible(False)
            self.btn_retry.setText("Probe Port 22")
            self.btn_retry.setVisible(True)
            self.btn_retry.setEnabled(True)
            if self.tree_view:
                self.tree_view.setEnabled(False)
            self.fs_model.set_node_error(
                self.root_node, "Port 22 unreachable. SSH/SFTP service not running."
            )

        elif state == MachineState.ERROR:
            err_text = f"✕ FAILED: {detail}" if detail else "✕ CONNECTION FAILED"
            if self.status_label:
                self.status_label.setText(err_text)
                self.status_label.setStyleSheet("color: #ef4444; font-weight: 700;")
            self.btn_refresh.setVisible(False)
            self.btn_retry.setText("⟳ Retry")
            self.btn_retry.setVisible(True)
            self.btn_retry.setEnabled(True)
            if self.tree_view:
                self.tree_view.setEnabled(True)
            if detail:
                self.fs_model.set_node_error(self.root_node, detail)

    def _toggle_auth_bar(self):
        """Toggles the in-app authentication form."""
        self.auth_bar.setVisible(not self.auth_bar.isVisible())

    def _on_auth_form_submit(self, username: str, password: str, key_path: str, remember: bool):
        """Handles user submitting authentication form."""
        self.username = username
        self.password = password if password else None
        self.key_path = key_path if key_path else None

        if remember:
            CredentialStore.save(
                self.machine_info.ip,
                username=self.username,
                password=self.password,
                key_path=self.key_path,
            )

        # Update SFTP manager with new credentials and connect
        self.sftp_manager.username = self.username
        self.sftp_manager.password = self.password
        self.sftp_manager.key_filepath = self.key_path

        self.start_connection()

    def disconnect_machine(self):
        """
        Disconnects active SSH/SFTP session for this machine,
        forgets saved credentials from disk and memory, resets the file tree,
        and prompts the user for authentication again.
        """
        # 1. Disconnect SSH/SFTP session
        self.sftp_manager.disconnect()

        # 2. Forget saved credentials from CredentialStore
        CredentialStore.remove(self.machine_info.ip)
        if self.machine_info.name:
            CredentialStore.remove(self.machine_info.name)

        # 3. Clear memory credentials
        self.password = None
        self.sftp_manager.password = None

        # 4. Reset authentication form password input
        if hasattr(self, "auth_bar"):
            self.auth_bar.input_pass.clear()

        # 5. Reset remote file system model and root node
        self.root_node.children.clear()
        self.root_node.is_loaded = False
        self.root_node.is_loading = False
        self.fs_model.set_node_error(
            self.root_node, "Disconnected. Enter password to authenticate."
        )

        # 6. Reset path label
        if hasattr(self, "path_label"):
            self.path_label.setText("📁 Path: (Disconnected)")

        # 7. Transition card state to AUTH_REQUIRED and show auth bar
        self.set_state(
            MachineState.AUTH_REQUIRED,
            detail="Disconnected. Please enter password to re-authenticate."
        )
        if hasattr(self, "auth_bar"):
            self.auth_bar.setVisible(True)

        # 8. Notify status bar
        self._notify_status(f"[{self.machine_info.name}] Disconnected & credentials forgotten.")

    def _on_clipboard_changed(self):
        """Updates Paste button state when clipboard changes."""
        is_connected = (self.property("machineState") == MachineState.CONNECTED)
        self.btn_paste.setEnabled(is_connected and self.clipboard.has_item())

    def _on_action_button_clicked(self):
        """Handles button click for Probe Port 22 or Retry/Auth."""
        current_state = self.property("machineState")
        if current_state == MachineState.ONLINE_SSH_UNAVAILABLE:
            self._probe_port_22()
        elif current_state == MachineState.AUTH_REQUIRED:
            self.auth_bar.setVisible(True)
        else:
            self.start_connection()

    def _probe_port_22(self):
        """Asynchronously tests port 22 on this machine."""
        if self.status_label:
            self.status_label.setText("● PROBING PORT 22...")
            self.status_label.setStyleSheet("color: #38bdf8; font-weight: 700;")
        self.btn_retry.setEnabled(False)

        worker = PortCheckWorker(self.machine_info.ip, timeout=DEFAULT_SOCKET_TIMEOUT)
        self._active_workers.add(worker)
        worker.signals.result.connect(self._on_port_check_result)
        worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
        self.thread_pool.start(worker)

    @Slot(object)
    def _on_port_check_result(self, is_open: bool):
        self.btn_retry.setEnabled(True)
        if is_open:
            self.machine_info.ssh_available = True
            self.set_state(MachineState.ONLINE_SSH_OK)
            self.start_connection()
        else:
            if self.status_label:
                self.status_label.setText("⚠ PORT 22 STILL UNREACHABLE")
                self.status_label.setStyleSheet("color: #f59e0b; font-weight: 700;")

    def start_connection(self):
        """Asynchronously connects SFTP and populates root directory."""
        self.set_state(MachineState.CONNECTING)

        worker = SFTPConnectWorker(self.sftp_manager)
        self._active_workers.add(worker)
        worker.signals.result.connect(self._on_sftp_connected)
        worker.signals.error.connect(self._on_sftp_error)
        worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
        self.thread_pool.start(worker)

    @Slot(object)
    def _on_sftp_connected(self, manager: SFTPManager):
        self.set_state(MachineState.CONNECTED)
        self.reload_filesystem()

    @Slot(str)
    def _on_sftp_error(self, err_msg: str):
        # Detect authentication failures specifically
        is_auth_error = any(
            x in err_msg.lower()
            for x in ["auth", "permission denied", "publickey", "password", "session"]
        )
        if is_auth_error:
            self.set_state(MachineState.AUTH_REQUIRED, detail=err_msg)
        else:
            self.set_state(MachineState.ERROR, detail=err_msg)

    def reload_filesystem(self):
        """Refreshes the root directory listing asynchronously."""
        if not self.sftp_manager.is_connected():
            self.start_connection()
            return

        self.btn_refresh.setEnabled(False)
        self.root_node.is_loaded = False
        self.root_node.is_loading = True

        worker = SFTPListWorker(
            self.sftp_manager, ".", self.root_node, QModelIndex()
        )
        self._active_workers.add(worker)
        worker.signals.result.connect(self._on_dir_listed)
        worker.signals.error.connect(self._on_dir_list_error)
        worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
        self.thread_pool.start(worker)

    @Slot(QModelIndex)
    def _on_tree_expanded(self, index: QModelIndex):
        """Lazy loads child directories when expanded in QTreeView."""
        if not index.isValid():
            return
        node: FSNode = index.internalPointer()
        if node and node.is_dir and not node.is_loaded and not node.is_loading:
            node.is_loading = True
            worker = SFTPListWorker(
                self.sftp_manager, node.path, node, index
            )
            self._active_workers.add(worker)
            worker.signals.result.connect(self._on_dir_listed)
            worker.signals.error.connect(self._on_dir_list_error)
            worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
            self.thread_pool.start(worker)

    @Slot(object)
    def _on_dir_listed(self, result_tuple):
        parent_node, entries, parent_index = result_tuple
        self.fs_model.populate_node(parent_node, entries, parent_index)
        self.btn_refresh.setEnabled(True)
        if parent_node == self.root_node:
            abs_path = self.sftp_manager.get_absolute_path(".")
            self.path_label.setText(f"📁 {abs_path}")

    @Slot(str)
    def _on_dir_list_error(self, err_payload: str):
        parts = err_payload.split("|", 2)
        path = parts[0]
        err_msg = parts[1] if len(parts) > 1 else "Read error"
        self.btn_refresh.setEnabled(True)

        if path == "." or not self.root_node.is_loaded:
            self.fs_model.set_node_error(self.root_node, err_msg, QModelIndex())

    # ==========================================================================
    # FILE MANAGER ACTIONS: CONTEXT MENU, CREATE, DELETE, RENAME, COPY, CUT, PASTE, DROP
    # ==========================================================================

    def _get_selected_node(self) -> Optional[FSNode]:
        """Returns the currently selected or active FSNode from the tree view."""
        if not self.tree_view:
            return None
        indexes = self.tree_view.selectedIndexes()
        if indexes:
            return indexes[0].internalPointer()
        current = self.tree_view.currentIndex()
        if current.isValid():
            return current.internalPointer()
        return None

    def _get_target_dir(self, node: Optional[FSNode]) -> str:
        """Returns target directory path based on node selection using POSIX conventions."""
        if node and node.is_dir and not node.is_dummy:
            return node.path
        elif node and node.parent and node.parent != self.root_node:
            return node.parent.path
        return "."

    def _show_context_menu(self, pos: QPoint):
        """Displays right-click context menu for filesystem operations."""
        if not self.sftp_manager.is_connected():
            return

        index = self.tree_view.indexAt(pos)
        if index.isValid():
            self.tree_view.setCurrentIndex(index)
            selected_node = index.internalPointer()
        else:
            selected_node = None

        menu = QMenu(self)

        if selected_node and not selected_node.is_dummy:
            # Item Actions
            act_copy = menu.addAction("📋 Copy")
            act_copy.setShortcut(QKeySequence.Copy)
            act_copy.triggered.connect(lambda: self._copy_node(selected_node))

            act_cut = menu.addAction("✂️ Cut")
            act_cut.setShortcut(QKeySequence.Cut)
            act_cut.triggered.connect(lambda: self._cut_node(selected_node))

            if self.clipboard.has_item():
                target_dir = self._get_target_dir(selected_node)
                act_paste = menu.addAction(f"📥 Paste '{self.clipboard.source_name}'")
                act_paste.setShortcut(QKeySequence.Paste)
                act_paste.triggered.connect(lambda: self._paste_into_dir(target_dir))

            menu.addSeparator()

            act_rename = menu.addAction("✏️ Rename...")
            act_rename.setShortcut(QKeySequence(Qt.Key_F2))
            act_rename.triggered.connect(lambda: self._rename_node(selected_node))

            act_delete = menu.addAction("🗑️ Delete")
            act_delete.setShortcut(QKeySequence.Delete)
            act_delete.triggered.connect(lambda: self._delete_node(selected_node))

            menu.addSeparator()

        else:
            # Blank Area Actions
            if self.clipboard.has_item():
                act_paste = menu.addAction(f"📥 Paste '{self.clipboard.source_name}'")
                act_paste.setShortcut(QKeySequence.Paste)
                act_paste.triggered.connect(lambda: self._paste_into_dir("."))
                menu.addSeparator()

        # General Create Actions
        act_new_file = menu.addAction("+ 📄 New File...")
        target_dir = self._get_target_dir(selected_node)
        act_new_file.triggered.connect(lambda: self._create_new_file(target_dir))

        act_new_folder = menu.addAction("+ 📁 New Folder...")
        act_new_folder.triggered.connect(lambda: self._create_new_folder(target_dir))

        menu.addSeparator()
        act_refresh = menu.addAction("↻ Refresh")
        act_refresh.setShortcut(QKeySequence(Qt.Key_F5))
        act_refresh.triggered.connect(self.reload_filesystem)

        menu.exec(self.tree_view.viewport().mapToGlobal(pos))

    def _copy_selected(self):
        """Copies currently selected item."""
        node = self._get_selected_node()
        if node and not node.is_dummy:
            self._copy_node(node)
        else:
            self._notify_status(f"[{self.machine_info.name}] Please select a file or folder to copy.")

    def _cut_selected(self):
        """Cuts currently selected item."""
        node = self._get_selected_node()
        if node and not node.is_dummy:
            self._cut_node(node)
        else:
            self._notify_status(f"[{self.machine_info.name}] Please select a file or folder to cut.")

    def _rename_selected(self):
        """Renames currently selected item."""
        node = self._get_selected_node()
        if node and not node.is_dummy:
            self._rename_node(node)
        else:
            self._notify_status(f"[{self.machine_info.name}] Please select a file or folder to rename.")

    def _delete_selected(self):
        """Deletes currently selected item."""
        node = self._get_selected_node()
        if node and not node.is_dummy:
            self._delete_node(node)
        else:
            self._notify_status(f"[{self.machine_info.name}] Please select a file or folder to delete.")

    def _copy_node(self, node: FSNode):
        """Copies node to global clipboard."""
        self.clipboard.copy(self, node)
        self._notify_status(f"Copied '{node.name}' from [{self.machine_info.name}]. Ready to paste.")

    def _cut_node(self, node: FSNode):
        """Cuts node to global clipboard."""
        self.clipboard.cut(self, node)
        self._notify_status(f"Cut '{node.name}' from [{self.machine_info.name}]. Ready to paste.")

    def _paste_clipboard(self):
        """Pastes clipboard item into currently selected folder or root."""
        selected = self._get_selected_node()
        target_dir = self._get_target_dir(selected)
        self._paste_into_dir(target_dir)

    def _paste_into_dir(self, dest_dir: str):
        if not self.clipboard.has_item():
            self._notify_status("Clipboard is empty.")
            return

        src_panel = self.clipboard.source_panel
        src_path = self.clipboard.source_path
        src_name = self.clipboard.source_name
        is_dir = self.clipboard.is_dir
        is_cut = self.clipboard.is_cut

        if dest_dir == ".":
            dest_path = src_name
        else:
            dest_path = posixpath.join(dest_dir, src_name)

        main_win = self.window()
        if isinstance(main_win, Dir2ZeroWindow):
            src_abs = src_panel.sftp_manager.get_absolute_path(src_path)
            dst_abs = self.sftp_manager.get_absolute_path(dest_path)
            main_win.start_transfer_monitor(
                src_path=src_abs,
                dst_path=dst_abs,
                src_host=src_panel.machine_info.name,
                dst_host=self.machine_info.name,
            )

        worker = CrossMachineTransferWorker(
            src_manager=src_panel.sftp_manager,
            dst_manager=self.sftp_manager,
            src_path=src_path,
            dst_path=dest_path,
            is_dir=is_dir,
            is_cut=is_cut,
            src_host_name=src_panel.machine_info.name,
            dst_host_name=self.machine_info.name,
        )
        self._active_workers.add(worker)

        if isinstance(main_win, Dir2ZeroWindow):
            worker.signals.progress.connect(main_win.update_transfer_progress)

        worker.signals.result.connect(lambda res: self._on_paste_finished(src_panel, is_cut, src_name))
        worker.signals.error.connect(self._on_file_op_error)
        worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
        if isinstance(main_win, Dir2ZeroWindow):
            worker.signals.finished.connect(main_win.finish_transfer_monitor)

        self.thread_pool.start(worker)

    def _handle_drop_transfer(
        self,
        src_panel: "MachinePanelWidget",
        src_path: str,
        src_name: str,
        is_dir: bool,
        dest_dir: str,
        is_cut: bool,
    ):
        """Executes drag-and-drop file/directory transfer between panels."""
        if dest_dir == ".":
            dest_path = src_name
        else:
            dest_path = posixpath.join(dest_dir, src_name)

        action_label = "Moving" if is_cut else "Copying"
        self._notify_status(
            f"{action_label} '{src_name}' from [{src_panel.machine_info.name}] to [{self.machine_info.name}]..."
        )

        main_win = self.window()
        if isinstance(main_win, Dir2ZeroWindow):
            src_abs = src_panel.sftp_manager.get_absolute_path(src_path)
            dst_abs = self.sftp_manager.get_absolute_path(dest_path)
            main_win.start_transfer_monitor(
                src_path=src_abs,
                dst_path=dst_abs,
                src_host=src_panel.machine_info.name,
                dst_host=self.machine_info.name,
            )

        worker = CrossMachineTransferWorker(
            src_manager=src_panel.sftp_manager,
            dst_manager=self.sftp_manager,
            src_path=src_path,
            dst_path=dest_path,
            is_dir=is_dir,
            is_cut=is_cut,
            src_host_name=src_panel.machine_info.name,
            dst_host_name=self.machine_info.name,
        )
        self._active_workers.add(worker)

        if isinstance(main_win, Dir2ZeroWindow):
            worker.signals.progress.connect(main_win.update_transfer_progress)

        worker.signals.result.connect(lambda res: self._on_paste_finished(src_panel, is_cut, src_name))
        worker.signals.error.connect(self._on_file_op_error)
        worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
        if isinstance(main_win, Dir2ZeroWindow):
            worker.signals.finished.connect(main_win.finish_transfer_monitor)

        self.thread_pool.start(worker)

    def _on_paste_finished(self, src_panel: "MachinePanelWidget", is_cut: bool, name: str):
        """Refreshes panels and clears clipboard if cut operation."""
        self.reload_filesystem()
        if is_cut and src_panel:
            src_panel.reload_filesystem()
            self.clipboard.clear()
        self._notify_status(f"Successfully transferred '{name}' to [{self.machine_info.name}]!")

    def _create_new_file(self, target_dir: Optional[str] = None):
        """Prompts for filename and creates an empty file on the remote machine."""
        if not isinstance(target_dir, str):
            target_dir = None

        if not self.sftp_manager.is_connected():
            self._notify_status(f"[{self.machine_info.name}] Cannot create file: not connected.")
            return

        if target_dir is None:
            selected = self._get_selected_node()
            target_dir = self._get_target_dir(selected)

        abs_target = self.sftp_manager.get_absolute_path(target_dir)
        name, ok = QInputDialog.getText(
            self, "Create New File", f"Target directory:\n{abs_target}\n\nEnter new filename:"
        )
        if ok and name.strip():
            name = name.strip()
            dest_path = name if target_dir == "." else posixpath.join(target_dir, name)
            self._notify_status(f"Creating file '{name}' on [{self.machine_info.name}]...")

            worker = SFTPFileOpWorker("create_file", self.sftp_manager, dest_path)
            self._active_workers.add(worker)
            worker.signals.result.connect(lambda _: self._on_file_op_success(f"Created file '{name}'"))
            worker.signals.error.connect(self._on_file_op_error)
            worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
            self.thread_pool.start(worker)

    def _create_new_folder(self, target_dir: Optional[str] = None):
        """Prompts for folder name and creates directory on remote machine."""
        if not isinstance(target_dir, str):
            target_dir = None

        if not self.sftp_manager.is_connected():
            self._notify_status(f"[{self.machine_info.name}] Cannot create folder: not connected.")
            return

        if target_dir is None:
            selected = self._get_selected_node()
            target_dir = self._get_target_dir(selected)

        abs_target = self.sftp_manager.get_absolute_path(target_dir)
        name, ok = QInputDialog.getText(
            self, "Create New Folder", f"Target directory:\n{abs_target}\n\nEnter new folder name:"
        )
        if ok and name.strip():
            name = name.strip()
            dest_path = name if target_dir == "." else posixpath.join(target_dir, name)
            self._notify_status(f"Creating folder '{name}' on [{self.machine_info.name}]...")

            worker = SFTPFileOpWorker("create_dir", self.sftp_manager, dest_path)
            self._active_workers.add(worker)
            worker.signals.result.connect(lambda _: self._on_file_op_success(f"Created folder '{name}'"))
            worker.signals.error.connect(self._on_file_op_error)
            worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
            self.thread_pool.start(worker)

    def _rename_node(self, node: FSNode):
        """Prompts for new name and renames file/folder."""
        if not node or node.is_dummy:
            return

        new_name, ok = QInputDialog.getText(
            self, "Rename", f"Enter new name for '{node.name}':", text=node.name
        )
        if ok and new_name.strip() and new_name.strip() != node.name:
            new_name = new_name.strip()
            parent_dir = posixpath.dirname(node.path)
            new_path = new_name if not parent_dir else posixpath.join(parent_dir, new_name)

            self._notify_status(f"Renaming '{node.name}' to '{new_name}'...")
            worker = SFTPFileOpWorker("rename", self.sftp_manager, node.path, new_path)
            self._active_workers.add(worker)
            worker.signals.result.connect(lambda _: self._on_file_op_success(f"Renamed to '{new_name}'"))
            worker.signals.error.connect(self._on_file_op_error)
            worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
            self.thread_pool.start(worker)

    def _delete_node(self, node: FSNode):
        """Confirms and deletes remote file or recursive directory."""
        if not node or node.is_dummy:
            return

        kind = "directory and all its contents" if node.is_dir else "file"
        reply = QMessageBox.question(
            self,
            "Confirm Delete",
            f"Are you sure you want to delete the {kind}:\n\n'{node.name}'\n\nfrom {self.machine_info.name}?",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply == QMessageBox.Yes:
            op_type = "delete_dir" if node.is_dir else "delete_file"
            self._notify_status(f"Deleting '{node.name}' from [{self.machine_info.name}]...")

            worker = SFTPFileOpWorker(op_type, self.sftp_manager, node.path)
            self._active_workers.add(worker)
            worker.signals.result.connect(lambda _: self._on_file_op_success(f"Deleted '{node.name}'"))
            worker.signals.error.connect(self._on_file_op_error)
            worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
            self.thread_pool.start(worker)

    def _on_file_op_success(self, msg: str):
        """Refreshes directory and displays success status message."""
        self.reload_filesystem()
        self._notify_status(f"[{self.machine_info.name}] {msg}")

    def _on_file_op_error(self, err_msg: str):
        """Displays error message on failure."""
        self._notify_status(f"[{self.machine_info.name}] Operation failed: {err_msg}")
        QMessageBox.warning(self, "Operation Error", f"Operation failed:\n\n{err_msg}")

    def _notify_status(self, message: str):
        """Emits message to main window status bar."""
        main_win = self.window()
        if isinstance(main_win, Dir2ZeroWindow):
            main_win.status_bar.showMessage(message, 6000)

    def close(self):
        """Disconnects SFTP cleanly."""
        self.sftp_manager.disconnect()
        super().close()


# ==============================================================================
# 12. MAIN APPLICATION WINDOW (DIR2ZERO)
# ==============================================================================

class Dir2ZeroWindow(QMainWindow):
    """
    Main application window for DirZero / Dir2Zero.
    Includes dynamic Tailscale monitoring, in-app progress bar, and card grid.
    """

    def __init__(
        self,
        username: Optional[str] = None,
        key_path: Optional[str] = None,
        parent: Optional[QWidget] = None,
    ):
        super().__init__(parent)
        self.username = username
        self.key_path = key_path
        self.thread_pool = QThreadPool.globalInstance()
        self._active_workers: Set[Any] = set()
        self.machine_panels: List[MachinePanelWidget] = []
        self._is_discovering = False

        self._setup_ui()

        # Dynamic Auto-Discovery Periodic Timer
        self.discovery_timer = QTimer(self)
        self.discovery_timer.setInterval(AUTO_REFRESH_INTERVAL_MS)
        self.discovery_timer.timeout.connect(lambda: self.start_discovery(background=True))
        self.discovery_timer.start()

        # Initial discovery
        self.start_discovery(background=False)

    def _setup_ui(self):
        """Configures main application window structure, header, grid, and progress dock."""
        self.setWindowTitle("DirZero — Tailscale Remote File Manager")
        self.resize(1180, 800)
        self.setMinimumSize(880, 620)

        # Central Widget & Base Layout
        central_widget = QWidget(self)
        self.setCentralWidget(central_widget)
        base_layout = QVBoxLayout(central_widget)
        base_layout.setContentsMargins(16, 16, 16, 16)
        base_layout.setSpacing(12)

        # Header Bar
        header_widget = QWidget(self)
        header_widget.setObjectName("headerWidget")
        header_layout = QHBoxLayout(header_widget)
        header_layout.setContentsMargins(8, 6, 8, 8)

        title_vbox = QVBoxLayout()
        title_label = QLabel("DIRZERO")
        title_label.setStyleSheet("color: #60a5fa; font-size: 22px; font-weight: 800; letter-spacing: 1px;")
        subtitle_label = QLabel("Dynamic Tailscale Remote File Manager & Network Diagnostics")
        subtitle_label.setStyleSheet("color: #94a3b8; font-size: 12px; font-weight: 500;")
        title_vbox.addWidget(title_label)
        title_vbox.addWidget(subtitle_label)
        header_layout.addLayout(title_vbox)

        header_layout.addStretch()

        # Stats Badges
        self.badge_online = QLabel("● 0 Online")
        self.badge_online.setStyleSheet("background: #064e3b; color: #34d399; padding: 6px 12px; border-radius: 6px; font-size: 12px; font-weight: 700;")
        header_layout.addWidget(self.badge_online)

        self.badge_ssh_ok = QLabel("● 0 Reachable")
        self.badge_ssh_ok.setStyleSheet("background: #1e3a8a; color: #60a5fa; padding: 6px 12px; border-radius: 6px; font-size: 12px; font-weight: 700;")
        header_layout.addWidget(self.badge_ssh_ok)

        self.badge_unavailable = QLabel("⚠ 0 Unavailable")
        self.badge_unavailable.setStyleSheet("background: #78350f; color: #fbbf24; padding: 6px 12px; border-radius: 6px; font-size: 12px; font-weight: 700;")
        header_layout.addWidget(self.badge_unavailable)

        # Theme Switcher Button & Dropdown Menu
        self.btn_theme = QPushButton()
        self.btn_theme.setFixedHeight(34)
        self.btn_theme.setToolTip("Click to choose or cycle themes extracted from git branches")
        self.btn_theme.setStyleSheet(
            "QPushButton { background: #1e293b; color: #38bdf8; border: 1px solid #334155; border-radius: 6px; padding: 0 12px; font-size: 12px; font-weight: 700; }"
            "QPushButton:hover { background: #334155; color: #ffffff; border-color: #60a5fa; }"
        )

        theme_menu = QMenu(self)
        theme_menu.setObjectName("themeMenu")

        theme_mgr = ThemeManager.instance()
        for theme in theme_mgr.themes:
            action = QAction(theme["name"], self)
            tid = theme["id"]
            action.triggered.connect(lambda checked=False, t_id=tid: self._select_theme(t_id))
            theme_menu.addAction(action)

        theme_menu.addSeparator()
        action_cycle = QAction("🔄 Cycle Next Theme", self)
        action_cycle.triggered.connect(self._cycle_theme)
        theme_menu.addAction(action_cycle)

        self.btn_theme.setMenu(theme_menu)
        header_layout.addWidget(self.btn_theme)

        theme_mgr.theme_changed.connect(self._on_theme_changed)
        self._update_theme_button_label()

        # Global Refresh Button
        self.btn_scan = QPushButton("↻ Refresh Tailnet")
        self.btn_scan.setFixedHeight(34)
        self.btn_scan.setStyleSheet(
            "QPushButton { background: #3b82f6; color: #ffffff; border: none; border-radius: 6px; padding: 0 16px; font-size: 13px; font-weight: 700; }"
            "QPushButton:hover { background: #2563eb; }"
            "QPushButton:disabled { background: #475569; color: #94a3b8; }"
        )
        self.btn_scan.clicked.connect(lambda: self.start_discovery(background=False))
        header_layout.addWidget(self.btn_scan)

        base_layout.addWidget(header_widget)

        # Scroll Area for Responsive Machine Grid
        self.scroll_area = QScrollArea(self)
        self.scroll_area.setWidgetResizable(True)
        self.scroll_area.setFrameShape(QFrame.NoFrame)
        self.scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.scroll_area.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)

        self.grid_container = ResponsiveGridContainer(self.scroll_area, min_col_width=400)
        self.scroll_area.setWidget(self.grid_container)
        base_layout.addWidget(self.scroll_area, 1)

        # In-App Bottom Transfer Progress Bar Dock
        self.progress_dock = QFrame(self)
        self.progress_dock.setObjectName("progressDock")
        self.progress_dock.setFrameShape(QFrame.StyledPanel)
        self.progress_dock.setStyleSheet(
            "QFrame#progressDock { background: #131b29; border: 1px solid #1e293b; border-radius: 8px; padding: 8px 12px; margin-top: 4px; }"
            "QLabel#progressLabel { color: #93c5fd; font-size: 12px; font-weight: 600; }"
            "QProgressBar { background: #0b0f16; border: 1px solid #28354b; border-radius: 5px; text-align: center; color: #f8fafc; font-size: 11px; font-weight: 700; height: 16px; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563eb, stop:1 #38bdf8); border-radius: 4px; }"
        )
        progress_layout = QVBoxLayout(self.progress_dock)
        progress_layout.setContentsMargins(6, 6, 6, 6)
        progress_layout.setSpacing(4)

        self.progress_path_label = QLabel("", self.progress_dock)
        self.progress_path_label.setStyleSheet("color: #cbd5e1; font-size: 11px; font-weight: 500;")
        self.progress_path_label.setWordWrap(True)
        progress_layout.addWidget(self.progress_path_label)

        self.progress_label = QLabel("🚀 Ready", self.progress_dock)
        self.progress_label.setObjectName("progressLabel")
        progress_layout.addWidget(self.progress_label)

        self.progress_bar = QProgressBar(self.progress_dock)
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        progress_layout.addWidget(self.progress_bar)

        base_layout.addWidget(self.progress_dock)
        self.progress_dock.setVisible(False)  # Hidden when idle

        # Status Bar
        self.status_bar = QStatusBar(self)
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Ready. Initializing dynamic Tailscale discovery...")

    # ==========================================================================
    # DYNAMIC AUTO-DISCOVERY & SMART RECONCILIATION
    # ==========================================================================

    def start_discovery(self, background: bool = False):
        """Launches background Tailscale network scan and updates machine cards."""
        if self._is_discovering:
            return
        self._is_discovering = True

        if not background:
            self.btn_scan.setEnabled(False)
            self.btn_scan.setText("Scanning...")
            self.status_bar.showMessage("Scanning Tailscale network and probing port 22...")

        worker = DiscoveryWorker(check_ssh=True)
        self._active_workers.add(worker)
        worker.signals.result.connect(lambda machines: self._on_discovery_completed(machines, background))
        worker.signals.error.connect(self._on_discovery_error)
        worker.signals.finished.connect(self._on_discovery_finished)
        worker.signals.finished.connect(lambda: self._active_workers.discard(worker))
        self.thread_pool.start(worker)

    @Slot(object, bool)
    def _on_discovery_completed(self, machines: List[MachineInfo], background: bool):
        """Smart reconciliation: updates cards without destroying active sessions."""
        online_count = len(machines)
        ssh_ok_count = sum(1 for m in machines if m.ssh_available)
        ssh_unavail_count = online_count - ssh_ok_count

        self.badge_online.setText(f"● {online_count} Online")
        self.badge_ssh_ok.setText(f"● {ssh_ok_count} Reachable")
        self.badge_unavailable.setText(f"⚠ {ssh_unavail_count} Unavailable")

        existing_by_ip: Dict[str, MachinePanelWidget] = {
            p.machine_info.ip: p for p in self.machine_panels
        }
        discovered_ips = {m.ip for m in machines}

        # 1. Update existing panels or add new ones
        for machine in machines:
            if machine.ip in existing_by_ip:
                panel = existing_by_ip[machine.ip]
                # Update SSH reachability state if changed
                if panel.machine_info.ssh_available != machine.ssh_available:
                    panel.machine_info.ssh_available = machine.ssh_available
                    if machine.ssh_available and panel.property("machineState") == MachineState.ONLINE_SSH_UNAVAILABLE:
                        panel.set_state(MachineState.ONLINE_SSH_OK)
                        panel.start_connection()
                    elif not machine.ssh_available and not panel.sftp_manager.is_connected():
                        panel.set_state(MachineState.ONLINE_SSH_UNAVAILABLE)
            else:
                # New machine discovered -> create panel
                new_panel = MachinePanelWidget(
                    machine_info=machine,
                    username=self.username,
                    key_path=self.key_path,
                    parent=self.grid_container,
                )
                self.machine_panels.append(new_panel)
                self.grid_container.add_panel(new_panel)

        # 2. Remove panels for machines that went offline
        for ip, panel in list(existing_by_ip.items()):
            if ip not in discovered_ips:
                panel.close()
                self.machine_panels.remove(panel)
                self.grid_container.remove_panel(panel)

        if not self.machine_panels:
            placeholder = QLabel("No online Tailscale machines found. Check your Tailscale connection.")
            placeholder.setAlignment(Qt.AlignCenter)
            placeholder.setStyleSheet("color: #94a3b8; font-size: 15px; padding: 40px;")
            self.grid_container.add_panel(placeholder)
            self.status_bar.showMessage("No online Tailscale machines found.")
        else:
            if not background:
                self.status_bar.showMessage(
                    f"Discovery complete: {online_count} online machine(s) found ({ssh_ok_count} reachable, {ssh_unavail_count} unavailable)."
                )

    @Slot(str)
    def _on_discovery_error(self, err: str):
        self.status_bar.showMessage(f"Discovery error: {err}")

    @Slot()
    def _on_discovery_finished(self):
        self._is_discovering = False
        self.btn_scan.setEnabled(True)
        self.btn_scan.setText("↻ Refresh Tailnet")

    # ==========================================================================
    # IN-APP TRANSFER PROGRESS MONITOR
    # ==========================================================================

    def start_transfer_monitor(
        self,
        src_path: str,
        dst_path: str,
        src_host: str = "",
        dst_host: str = "",
    ):
        """Displays bottom progress bar for active file transfer with complete paths."""
        self.progress_dock.setVisible(True)
        self.progress_bar.setValue(0)

        src_disp = f"[{src_host}] {src_path}" if src_host else src_path
        dst_disp = f"[{dst_host}] {dst_path}" if dst_host else dst_path

        self.progress_path_label.setText(
            f"📄 <b>FROM:</b> {src_disp}<br>➔ <b>TO:</b> {dst_disp}"
        )
        self.progress_label.setText("🚀 Preparing file transfer...")

    @Slot(int, int, str, float)
    def update_transfer_progress(
        self, bytes_done: int, total_bytes: int, item_name: str, speed_bps: float
    ):
        """Updates progress bar value, byte counters, speed, and active file paths in real time."""
        percent = int((bytes_done / max(1, total_bytes)) * 100)
        self.progress_bar.setValue(min(100, percent))

        if " ➔ " in item_name or " -> " in item_name:
            parts = item_name.split(" ➔ ") if " ➔ " in item_name else item_name.split(" -> ")
            if len(parts) == 2:
                self.progress_path_label.setText(
                    f"📄 <b>FROM:</b> {parts[0].strip()}<br>➔ <b>TO:</b> {parts[1].strip()}"
                )

        def format_size(b: int) -> str:
            if b < 1024:
                return f"{b} B"
            elif b < 1024 * 1024:
                return f"{b / 1024:.1f} KB"
            elif b < 1024 * 1024 * 1024:
                return f"{b / (1024 * 1024):.1f} MB"
            else:
                return f"{b / (1024 * 1024 * 1024):.2f} GB"

        speed_str = f"{format_size(int(speed_bps))}/s" if speed_bps > 0 else "0 B/s"
        done_str = format_size(bytes_done)
        total_str = format_size(total_bytes)

        self.progress_label.setText(
            f"🚀 <b>Progress:</b> {percent}% ({done_str} / {total_str}) • <b>Speed:</b> {speed_str}"
        )

    def finish_transfer_monitor(self):
        """Hides progress bar after short delay upon transfer completion."""
        self.progress_bar.setValue(100)
        QTimer.singleShot(2500, lambda: self.progress_dock.setVisible(False))

    def closeEvent(self, event):
        """Ensures all SFTP sessions close cleanly upon exit."""
        self.discovery_timer.stop()
        for panel in self.machine_panels:
            panel.close()
        self.thread_pool.waitForDone(1000)
        super().closeEvent(event)

    def _select_theme(self, theme_id: str):
        ThemeManager.instance().switch_to_theme(theme_id)

    def _cycle_theme(self):
        ThemeManager.instance().cycle_next_theme()

    def _on_theme_changed(self, theme_id: str, theme_name: str):
        self._update_theme_button_label()
        if hasattr(self, "statusBar") and self.statusBar():
            self.statusBar().showMessage(f"🎨 Theme switched to: {theme_name}", 4000)

    def _update_theme_button_label(self):
        curr = ThemeManager.instance().get_current_theme()
        self.btn_theme.setText(f"🎨 {curr['name']} ▾")


# ==============================================================================
# 13. STYLESHEET LOADER
# ==============================================================================

FALLBACK_STYLESHEET = """
/* DirZero Cyber Dark Theme */
QMainWindow {
    background: #0f141c;
}

QWidget {
    font-family: "SF Pro Display", "Segoe UI", "Helvetica Neue", "Ubuntu", sans-serif;
    color: #e2e8f0;
}

/* Header Bar */
QWidget#headerWidget {
    background: #151b26;
    border: 1px solid #232d3f;
    border-radius: 10px;
    padding: 8px 16px;
}

/* Machine Card */
QFrame#machinePanel {
    background: #171e2c;
    border: 1px solid #28354b;
    border-radius: 12px;
    padding: 14px;
}

QPushButton#disconnect {
    background: #3b181a;
    color: #fca5a5;
    border: 1px solid #7f1d1d;
    border-radius: 6px;
    padding: 5px 12px;
    font-size: 11px;
    font-weight: 700;
}

QPushButton#disconnect:hover {
    background: #991b1b;
    color: #ffffff;
    border-color: #ef4444;
}

QPushButton#disconnect:pressed {
    background: #7f1d1d;
}

QPushButton#disconnect:disabled {
    background: #191213;
    color: #582424;
    border-color: #361717;
}

QFrame#machinePanel:hover {
    border-color: #3b4d6d;
}

QFrame#machinePanel[machineState="ONLINE_SSH_UNAVAILABLE"] {
    background: #131822;
    border: 1px dashed #78350f;
}

QFrame#machinePanel[machineState="CONNECTED"] {
    background: #16202f;
    border: 1px solid #10b981;
}

QFrame#machinePanel[machineState="AUTH_REQUIRED"] {
    background: #1a1715;
    border: 1px solid #d97706;
}

QFrame#machinePanel[machineState="ERROR"] {
    background: #1c1519;
    border: 1px solid #dc2626;
}

/* Machine Headings & Labels */
QLabel#machineHeading {
    color: #f8fafc;
    font-size: 17px;
    font-weight: 700;
    padding: 2px 0px;
}

QLabel#statusLabel {
    font-size: 12px;
    font-weight: 700;
    padding: 2px 0px;
}

QLabel#ipHeading {
    color: #64748b;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.5px;
    padding-top: 4px;
    padding-bottom: 1px;
}

QLabel#ipAddress {
    background: #0c1017;
    color: #93c5fd;
    border: 1px solid #232d3f;
    border-radius: 6px;
    font-family: "JetBrains Mono", "SF Mono", "Fira Code", monospace;
    font-size: 13px;
    font-weight: 600;
    padding: 6px 10px;
}

/* Filesystem TreeView with Draggable Column Divider Lines */
QTreeView {
    background: #0b0f16;
    color: #cbd5e1;
    border: 1px solid #232d3f;
    border-radius: 8px;
    padding: 4px;
    outline: none;
    font-size: 12px;
}

QTreeView:disabled {
    background: #090c12;
    color: #475569;
    border-color: #1a2230;
}

QTreeView::item {
    padding: 5px 6px;
    border-radius: 4px;
}

QTreeView::item:hover {
    background: #1b2434;
    color: #ffffff;
}

QTreeView::item:selected {
    background: #1e3a8a;
    color: #93c5fd;
}

QHeaderView::section {
    background: #151b26;
    color: #94a3b8;
    padding: 5px 8px;
    border: none;
    border-right: 1px solid #28354b;
    border-bottom: 1px solid #232d3f;
    font-size: 11px;
    font-weight: 700;
}

QHeaderView::section:hover {
    background: #1c2433;
    color: #f8fafc;
}

/* Scroll Area & Scrollbars */
QScrollArea {
    background: transparent;
    border: none;
}

QScrollBar:vertical {
    background: #0b0f16;
    width: 10px;
    margin: 0;
    border-radius: 5px;
}

QScrollBar::handle:vertical {
    background: #232d3f;
    min-height: 20px;
    border-radius: 5px;
}

QScrollBar::handle:vertical:hover {
    background: #3b4d6d;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

/* StatusBar */
QStatusBar {
    background: #0b0f16;
    color: #94a3b8;
    border-top: 1px solid #1a2230;
    font-size: 12px;
}
"""

THEMES_DIR = "themes"
THEME_CONFIG_FILE = ".active_theme"

DEFAULT_THEMES = [
    {
        "id": "cybersecurity_dark",
        "name": "🛡️ Cyber Dark",
        "file": os.path.join(THEMES_DIR, "cybersecurity_dark.qss"),
    },
    {
        "id": "light_grayscale",
        "name": "☀️ Light Grayscale",
        "file": os.path.join(THEMES_DIR, "light_grayscale.qss"),
    },
    {
        "id": "midnight_cyber",
        "name": "🌙 Midnight Cyber",
        "file": os.path.join(THEMES_DIR, "midnight_cyber.qss"),
    },
    {
        "id": "classic_dark",
        "name": "💻 Classic Dark",
        "file": os.path.join(THEMES_DIR, "classic_dark.qss"),
    },
]


class ThemeManager(QObject):
    """
    Manages dynamic theme loading, theme cycling across git branch themes,
    persistence, and signal notifications.
    """
    theme_changed = Signal(str, str)  # (theme_id, theme_name)

    _instance = None

    @classmethod
    def instance(cls) -> "ThemeManager":
        if cls._instance is None:
            cls._instance = ThemeManager()
        return cls._instance

    def __init__(self, parent: Optional[QObject] = None):
        super().__init__(parent)
        self.themes: List[Dict[str, str]] = []
        self.current_theme_index: int = 0
        self.load_available_themes()

    def load_available_themes(self):
        """Scans the themes directory and registers all .qss themes."""
        self.themes = list(DEFAULT_THEMES)
        registered_ids = {t["id"] for t in self.themes}

        if os.path.exists(THEMES_DIR):
            for fname in sorted(os.listdir(THEMES_DIR)):
                if fname.endswith(".qss"):
                    tid = fname.replace(".qss", "")
                    if tid not in registered_ids:
                        name = tid.replace("_", " ").title()
                        self.themes.append({
                            "id": tid,
                            "name": f"🎨 {name}",
                            "file": os.path.join(THEMES_DIR, fname)
                        })
                        registered_ids.add(tid)

        saved_id = self.get_saved_theme_id()
        if saved_id:
            for idx, theme in enumerate(self.themes):
                if theme["id"] == saved_id:
                    self.current_theme_index = idx
                    break

    def get_saved_theme_id(self) -> Optional[str]:
        if os.path.exists(THEME_CONFIG_FILE):
            try:
                with open(THEME_CONFIG_FILE, "r", encoding="utf-8") as f:
                    return f.read().strip()
            except Exception:
                pass
        return None

    def save_theme_id(self, theme_id: str):
        try:
            with open(THEME_CONFIG_FILE, "w", encoding="utf-8") as f:
                f.write(theme_id)
        except Exception:
            pass

    def get_current_theme(self) -> Dict[str, str]:
        if not self.themes:
            return {"id": "default", "name": "Default Theme", "file": QSS_STYLE_PATH}
        return self.themes[self.current_theme_index]

    def apply_current_theme(self, app: Optional[QApplication] = None) -> bool:
        if app is None:
            app = QApplication.instance()
        theme = self.get_current_theme()
        theme_file = theme["file"]

        stylesheet_content = ""
        if os.path.exists(theme_file):
            try:
                with open(theme_file, "r", encoding="utf-8") as f:
                    stylesheet_content = f.read()
            except Exception as e:
                print(f"[ThemeManager] Failed to read {theme_file}: {e}")

        if not stylesheet_content and os.path.exists(QSS_STYLE_PATH):
            try:
                with open(QSS_STYLE_PATH, "r", encoding="utf-8") as f:
                    stylesheet_content = f.read()
            except Exception:
                pass

        if not stylesheet_content:
            stylesheet_content = FALLBACK_STYLESHEET

        if app:
            app.setStyleSheet(stylesheet_content)
            self.save_theme_id(theme["id"])
            self.theme_changed.emit(theme["id"], theme["name"])
            return True
        return False

    def switch_to_theme(self, theme_id: str, app: Optional[QApplication] = None) -> bool:
        for idx, theme in enumerate(self.themes):
            if theme["id"] == theme_id:
                self.current_theme_index = idx
                return self.apply_current_theme(app)
        return False

    def cycle_next_theme(self, app: Optional[QApplication] = None) -> Tuple[str, str]:
        if not self.themes:
            return ("default", "Default Theme")
        self.current_theme_index = (self.current_theme_index + 1) % len(self.themes)
        self.apply_current_theme(app)
        curr = self.get_current_theme()
        return (curr["id"], curr["name"])


def load_stylesheet(app: QApplication):
    """Loads current theme via ThemeManager."""
    ThemeManager.instance().apply_current_theme(app)


# ==============================================================================
# 14. APPLICATION ENTRY POINT
# ==============================================================================

def parse_args():
    parser = argparse.ArgumentParser(
        description="DirZero - Cross-Platform Tailscale Remote File Manager"
    )
    parser.add_argument(
        "--user",
        "-u",
        type=str,
        default=os.environ.get("SSH_USER", getpass.getuser()),
        help="SSH username for remote authentication (defaults to current user)",
    )
    parser.add_argument(
        "--key",
        "-k",
        type=str,
        default=os.environ.get("SSH_KEY", get_first_available_ssh_key()),
        help="Path to private key file (defaults to ~/.ssh/id_ed25519 or first found key)",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    app = QApplication(sys.argv)
    app.setApplicationName("DirZero")
    load_stylesheet(app)

    window = Dir2ZeroWindow(username=args.user, key_path=args.key)
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()



