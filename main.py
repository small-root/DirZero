from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QFile
from PySide6.QtUiTools import QUiLoader
import paramiko
import os

host = "100.94.145.66"
username = "xiaogen"
port = 22

private_key_path = os.path.expanduser("~/.ssh/id_ed25519")
private_key = paramiko.Ed25519Key.from_private_key_file(private_key_path)

transport = paramiko.Transport((host, port))
transport.connect(username=username, pkey=private_key)
sftp = paramiko.SFTPClient.from_transport(transport)

app = QApplication()
loader = QUiLoader()

file = QFile("DirZero.ui")
file.open(QFile.ReadOnly)

window = loader.load(file)
file.close()

def show_output():
    files = sftp.listdir(".")
    window.MachineAOutput.setPlainText(str(files))

window.ListfilesinA.clicked.connect(show_output)
window.show()
app.exec()

sftp.close()
transport.close()
