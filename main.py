from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QFile
from PySide6.QtUiTools import QUiLoader
import paramiko
import os

hostA = "100.94.145.66"
usernamea = "xiaogen"
porta = 22

hostB = "100.94.227.29"
usernameb = "xiaogen"
portb = 22

private_key_path = os.path.expanduser("~/.ssh/id_ed25519")
private_key = paramiko.Ed25519Key.from_private_key_file(private_key_path)

transportA = paramiko.Transport((hostA, porta))
transportA.connect(username=usernamea, pkey=private_key)
sftpA = paramiko.SFTPClient.from_transport(transportA)

transportB = paramiko.Transport((hostB, portb))
transportB.connect(username=usernameb, pkey=private_key)
sftpB = paramiko.SFTPClient.from_transport(transportB)


app = QApplication()
loader = QUiLoader()

file = QFile("DirZero.ui")
file.open(QFile.ReadOnly)

window = loader.load(file)
file.close()

def show_output_mach_a():
    files = sftpA.listdir(".")
    window.MachineAOutput.setPlainText(str(files))

def show_output_mach_b():
    files = sftpB.listdir(".")
    window.MachineBOutput.setPlainText(str(files))

window.ListfilesinA.clicked.connect(show_output_mach_a)
window.ListfilesinB.clicked.connect(show_output_mach_b)
window.show()
app.exec()

sftpA.close()
transportA.close()
sftpB.close()
transportB.close()
