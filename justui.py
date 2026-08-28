from PySide6.QtWidgets import QApplication, QLabel, QTreeView, QWidget, QVBoxLayout, QGridLayout
from PySide6.QtCore import QFile
from PySide6.QtUiTools import QUiLoader


app = QApplication()
loader = QUiLoader()

file = QFile("Dir2Zero.ui")
file.open(QFile.ReadOnly)

window = loader.load(file)
file.close()


# Get the central widget
central = window.centralWidget()

# Get the vertical layout already created in Designer
vertical = central.layout()


# Create our machine grid dynamically
grid = QGridLayout()

vertical.addLayout(grid)


machines = [
    ("Machine A", "100.94.145.66"),
    ("Machine B", "100.94.227.29"),
    ("Machine C", "100.94.111.50"),
    ("Machine D", "100.94.222.50"),
]


for i, (name, ip) in enumerate(machines):

    panel = QWidget()
    layout = QVBoxLayout(panel)

    label = QLabel(f"{name}: {ip}")

    label.setStyleSheet("""
        QLabel {
            font-size: 18px;
            font-weight: bold;
            padding: 8px;
        }
    """)

    tree = QTreeView()

    layout.addWidget(label)
    layout.addWidget(tree)

    row = i // 2
    column = i % 2

    grid.addWidget(panel, row, column)


window.show()
app.exec()
