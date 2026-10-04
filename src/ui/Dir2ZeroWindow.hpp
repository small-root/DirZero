#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QProgressBar>
#include <QTimer>
#include <QList>
#include <QSet>
#include <memory>
#include "../core/MachineInfo.hpp"
#include "../helpers/ResponsiveGridContainer.hpp"
#include "MachinePanelWidget.hpp"

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

private slots:
    void onDiscoveryResult(const QList<MachineInfo>& machines);
    void onDiscoveryError(const QString& error);
    void onDiscoveryFinished();

    void onThemeChanged(const QString& themeId, const QString& themeName);
    void onCycleTheme();

private:
    QString m_defaultUser;
    QString m_defaultKey;
    bool m_isDiscovering;

    QTimer* m_discoveryTimer;
    QList<MachinePanelWidget*> m_panels;

    // Header Widgets
    QLabel* m_badgeOnline;
    QLabel* m_badgeSshOk;
    QLabel* m_badgeUnavailable;
    QPushButton* m_btnTheme;
    QPushButton* m_btnScan;

    // Grid Container & Scroll Area
    QScrollArea* m_scrollArea;
    ResponsiveGridContainer* m_gridContainer;

    // Bottom In-App Progress Dock
    QFrame* m_progressDock;
    QLabel* m_progressPathLabel;
    QLabel* m_progressLabel;
    QProgressBar* m_progressBar;

    void setupUi();
    void updateThemeButtonLabel();
};
