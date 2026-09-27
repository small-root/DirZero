/********************************************************************************
** Form generated from reading UI file 'MachinePanel.ui'
**
** Created by: Qt User Interface Compiler version 6.11.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MACHINEPANEL_H
#define UI_MACHINEPANEL_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_MachinePanel
{
public:
    QVBoxLayout *cardLayout;
    QLabel *machineHeading;
    QLabel *statusLabel;
    QPushButton *disconnect;
    QLabel *ipHeading;
    QLabel *ipAddress;
    QTreeView *treeView;

    void setupUi(QFrame *machinePanel)
    {
        if (machinePanel->objectName().isEmpty())
            machinePanel->setObjectName("machinePanel");
        machinePanel->resize(360, 300);
        machinePanel->setFrameShape(QFrame::StyledPanel);
        machinePanel->setFrameShadow(QFrame::Raised);
        cardLayout = new QVBoxLayout(machinePanel);
        cardLayout->setSpacing(8);
        cardLayout->setObjectName("cardLayout");
        cardLayout->setContentsMargins(16, 16, 16, 16);
        machineHeading = new QLabel(machinePanel);
        machineHeading->setObjectName("machineHeading");

        cardLayout->addWidget(machineHeading);

        statusLabel = new QLabel(machinePanel);
        statusLabel->setObjectName("statusLabel");

        cardLayout->addWidget(statusLabel);

        disconnect = new QPushButton(machinePanel);
        disconnect->setObjectName("disconnect");

        cardLayout->addWidget(disconnect);

        ipHeading = new QLabel(machinePanel);
        ipHeading->setObjectName("ipHeading");

        cardLayout->addWidget(ipHeading);

        ipAddress = new QLabel(machinePanel);
        ipAddress->setObjectName("ipAddress");

        cardLayout->addWidget(ipAddress);

        treeView = new QTreeView(machinePanel);
        treeView->setObjectName("treeView");
        treeView->setMinimumSize(QSize(0, 100));
        treeView->setAlternatingRowColors(true);
        treeView->setHeaderHidden(false);
        treeView->setRootIsDecorated(true);

        cardLayout->addWidget(treeView);


        retranslateUi(machinePanel);

        QMetaObject::connectSlotsByName(machinePanel);
    } // setupUi

    void retranslateUi(QFrame *machinePanel)
    {
        machineHeading->setText(QCoreApplication::translate("MachinePanel", "MACHINE A", nullptr));
        statusLabel->setText(QCoreApplication::translate("MachinePanel", "\342\227\217 ONLINE", nullptr));
        disconnect->setText(QCoreApplication::translate("MachinePanel", "Disconnect", nullptr));
        ipHeading->setText(QCoreApplication::translate("MachinePanel", "IP ADDRESS", nullptr));
        ipAddress->setText(QCoreApplication::translate("MachinePanel", "100.94.227.29", nullptr));
        (void)machinePanel;
    } // retranslateUi

};

namespace Ui {
    class MachinePanel: public Ui_MachinePanel {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MACHINEPANEL_H
