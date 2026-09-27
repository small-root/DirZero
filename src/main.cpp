ude <ctime>
#include <iostream>
#include <QProcess>

#include <QtCore/Qt>
#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QThreadPool>
#include <QtCore/QRunnable>
#include <QtCore/QFile>
#include <QtCore/QPoint>
#include <QtCore/QTimer>
#include <QtCore/QSize>
#include <QtCore/QMimeData>
#include <QtCore/QByteArray>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QFrame>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QToolButton>

#include <QtGui/QAction>
#include <QtGui/QKeySequence>
#include <QtGui/QFont>
#include <QtGui/QColor>
#include <QtGui/QDrag>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QKeyEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>

#include <QtUiTools/QUiLoader>
#include <ui_Dir2Zero.h>
#include <ui_MachinePanel.h>



int main(int argc, char *argv[]){
    QApplication app(argc, argv);
    QMainWindow window;
    Ui::DirZero root;
    root.setupUi(&window);

    QWidget machinePanel;
    Ui::MachinePanel machine;
    machine.setupUi(&machinePanel);
    window.show();
    return app.exec();

}
