/********************************************************************************
** Form generated from reading UI file 'Dir2Zero.ui'
**
** Created by: Qt User Interface Compiler version 6.11.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DIR2ZERO_H
#define UI_DIR2ZERO_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DirZero
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *DirZero)
    {
        if (DirZero->objectName().isEmpty())
            DirZero->setObjectName("DirZero");
        DirZero->resize(900, 700);
        centralwidget = new QWidget(DirZero);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setSpacing(12);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(20, 20, 20, 20);
        DirZero->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(DirZero);
        statusbar->setObjectName("statusbar");
        DirZero->setStatusBar(statusbar);

        retranslateUi(DirZero);

        QMetaObject::connectSlotsByName(DirZero);
    } // setupUi

    void retranslateUi(QMainWindow *DirZero)
    {
        DirZero->setWindowTitle(QCoreApplication::translate("DirZero", "DirZero", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DirZero: public Ui_DirZero {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DIR2ZERO_H
