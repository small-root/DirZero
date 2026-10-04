#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QProgressBar>
#include <QTimer>
#include <QList>
#include <QSet>
#include <QGridLayout>
#include <memory>
#include "../core/MachineInfo.hpp"
#include "../helpers/ResponsiveGridContainer.hpp"
#include "MachinePanelWidget.hpp"

class QFileSystemModel;
class QStackedWidget;
class QTreeView;

class Dir2ZeroWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit Dir2ZeroWindow(const QString& defaultUser = QString(),
                            const QString& defaultKey = QString(),
                            QWidget* parent = nullptr);
    ~Dir2ZeroWindow() override;

    const QList<MachinePanelWidget*>& machinePanels() const { return m_panels; }

    void startDiscovery(bool background = false);
    void showStatusMessage(const QString& message, int timeout = 6000);

    // Transfer monitor API
    void startTransferMonitor(const QString& srcPath,
                              const QString& dstPath,
                              const QString& srcHost = QString(),
                              const QString& dstHost = QString());
    void updateTransferProgress(qint64 bytesDone, qint64 totalBytes, const QString& itemName, double speedBps);
    void finishTransferMonitor();

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onDiscoveryResult(const QList<MachineInfo>& machines);
    void onDiscoveryError(const QString& error);
    void onDiscoveryFinished();

    void onThemeChanged(const QString& themeId, const QString& themeName);
    void onCycleTheme();
    void toggleViewMode();
    void openLocalDrive(const QString& rootPath);

private:
    QString m_defaultUser;
    QString m_defaultKey;
    bool m_isDiscovering;
    bool m_isExplorerView;

    QTimer* m_discoveryTimer;
    QList<MachinePanelWidget*> m_panels;

    // Header Widgets
    QLabel* m_badgeOnline;
    QLabel* m_badgeSshOk;
    QLabel* m_badgeUnavailable;
    QPushButton* m_btnTheme;
    QPushButton* m_btnScan;
    QPushButton* m_btnViewMode;

    // Explorer and Tailnet view stack
    QStackedWidget* m_viewStack;
    QWidget* m_localDrivesPage;
    QWidget* m_localBrowsePage;
    QGridLayout* m_driveGrid;
    QFileSystemModel* m_localFileModel;
    QTreeView* m_localTreeView;
    QLabel* m_localPathLabel;

    // Tailnet grid
    QScrollArea* m_scrollArea;
    ResponsiveGridContainer* m_gridContainer;

    // Bottom In-App Progress Dock
    QFrame* m_progressDock;
    QLabel* m_progressPathLabel;
    QLabel* m_progressLabel;
    QProgressBar* m_progressBar;

    void setupUi();
    void updateThemeButtonLabel();
    void updateViewMode();
    void updateResponsiveHeader();
    void populateLocalDrives();
};
