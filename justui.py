from PySide6.QtWidgets import QApplication, QLabel, QGridLayout
from PySide6.QtCore import QFile
from PySide6.QtUiTools import QUiLoader


# Start application
app = QApplication()

# Load stylesheet
with open("style.qss", "r") as stylesheet:
    app.setStyleSheet(stylesheet.read())

loader = QUiLoader()


# Load main window
file = QFile("Dir2Zero.ui")
file.open(QFile.ReadOnly)

window = loader.load(file)

file.close()


# Get the central widget
central = window.centralWidget()

# Get the layout created in Qt Designer
vertical = central.layout()


# Create grid for machine panels
grid = QGridLayout()
vertical.addLayout(grid)


# Machine information
machines = [
    ("Machine A", "100.94.145.66"),
    ("Machine B", "100.94.227.29"),
    ("Machine C", "100.94.111.50"),
    ("Machine D", "100.94.222.50"),
]


# Create reusable machine panels
for i, (name, ip) in enumerate(machines):

    # Load MachinePanel.ui
    panel_file = QFile("MachinePanel.ui")
    panel_file.open(QFile.ReadOnly)

    panel = loader.load(panel_file)

    panel_file.close()


    # Set machine name
    heading = panel.findChild(QLabel, "machineHeading")

    if heading:
        heading.setText(name.upper())


    # Set IP address
    ip_label = panel.findChild(QLabel, "ipAddress")

    if ip_label:
        ip_label.setText(ip)


    # Position panel in 2 × 2 grid
    row = i // 2
    column = i % 2

    grid.addWidget(panel, row, column)


# Show application
window.show()

app.exec()