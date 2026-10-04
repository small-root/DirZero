#include "Dir2ZeroWindow.hpp"
#include "ThemeManager.hpp"
#include "../workers/DiscoveryWorker.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QThreadPool>
#include <QMap>

Dir2ZeroWindow::Dir2ZeroWindow(const QString& defaultUser,
                               const QString& defaultKey,
                               QWidget* parent)
    : QMainWindow(parent)
    , m_defaultUser(defaultUser)
    , m_defaultKey(defaultKey)
    , m_isDiscovering(false)
{
    setupUi();

    // Auto-Discovery periodic timer: 20 seconds
    m_discoveryTimer = new QTimer(this);
    m_discoveryTimer->setInterval(20000);
    connect(m_discoveryTimer, &QTimer::timeout, this, [this]() {
        startDiscovery(true);
    });
    m_discoveryTimer->start();

    // Initial Discovery
    startDiscovery(false);
}

Dir2ZeroWindow::~Dir2ZeroWindow()
{
    if (m_discoveryTimer) {
        m_discoveryTimer->stop();
    }
}

void Dir2ZeroWindow::setupUi()
{
    setWindowTitle(QStringLiteral("DirZero — Tailscale Remote File Manager"));
    resize(1200, 820);
    setMinimumSize(880, 620);

    auto centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto baseLayout = new QVBoxLayout(centralWidget);
    baseLayout->setContentsMargins(16, 16, 16, 16);
    baseLayout->setSpacing(12);

    // =========================================================================
    // Header Bar
    // =========================================================================
    auto headerWidget = new QWidget(this);
    headerWidget->setObjectName(QStringLiteral("headerWidget"));
    auto headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(12);

    auto titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    auto titleLabel = new QLabel(QStringLiteral("DIRZERO"), this);
    titleLabel->setStyleSheet(QStringLiteral("color: #38bdf8; font-size: 22px; font-weight: 900; letter-spacing: 1.5px;"));
    auto subtitleLabel = new QLabel(QStringLiteral("Dynamic Tailscale Remote File Manager & Network Diagnostics"), this);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 500;"));
    titleBox->addWidget(titleLabel);
    titleBox->addWidget(subtitleLabel);
    headerLayout->addLayout(titleBox);

    headerLayout->addStretch();

    // Stats Badges
    m_badgeOnline = new QLabel(QStringLiteral("● 0 Online"), this);
    m_badgeOnline->setStyleSheet(QStringLiteral("background: rgba(6, 78, 59, 0.7); color: #34d399; padding: 6px 12px; border-radius: 6px; font-size: 12px; font-weight: 700; border: 1px solid rgba(16, 185, 129, 0.4);"));
    headerLayout->addWidget(m_badgeOnline);

    m_badgeSshOk = new QLabel(QStringLiteral("● 0 Reachable"), this);
    m_badgeSshOk->setStyleSheet(QStringLiteral("background: rgba(30, 58, 138, 0.7); color: #60a5fa; padding: 6px 12px; border-radius: 6px; font-size: 12px; font-weight: 700; border: 1px solid rgba(56, 189, 248, 0.4);"));
    headerLayout->addWidget(m_badgeSshOk);

    m_badgeUnavailable = new QLabel(QStringLiteral("⚠ 0 Unavailable"), this);
    m_badgeUnavailable->setStyleSheet(QStringLiteral("background: rgba(120, 53, 15, 0.7); color: #fbbf24; padding: 6px 12px; border-radius: 6px; font-size: 12px; font-weight: 700; border: 1px solid rgba(245, 158, 11, 0.4);"));
    headerLayout->addWidget(m_badgeUnavailable);

    // Theme Switcher Button & Menu
    m_btnTheme = new QPushButton(this);
    m_btnTheme->setFixedHeight(34);
    m_btnTheme->setToolTip(QStringLiteral("Choose or cycle UI themes"));
    m_btnTheme->setStyleSheet(QStringLiteral("QPushButton { background: rgba(30, 41, 59, 0.8); color: #38bdf8; border: 1px solid rgba(56, 189, 248, 0.3); border-radius: 6px; padding: 0 12px; font-weight: 700; } QPushButton:hover { background: rgba(51, 65, 85, 0.9); }"));

    auto themeMenu = new QMenu(this);
    for (const auto& th : ThemeManager::instance()->themes()) {
        auto act = themeMenu->addAction(th.name);
        QString tid = th.id;
        connect(act, &QAction::triggered, this, [tid]() {
            ThemeManager::instance()->switchToTheme(tid);
        });
    }
    themeMenu->addSeparator();
    auto cycleAct = themeMenu->addAction(QStringLiteral("🔄 Cycle Next Theme"), this, &Dir2ZeroWindow::onCycleTheme);
    Q_UNUSED(cycleAct);

    m_btnTheme->setMenu(themeMenu);
    headerLayout->addWidget(m_btnTheme);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &Dir2ZeroWindow::onThemeChanged);
    updateThemeButtonLabel();

    // Refresh Tailnet Button
    m_btnScan = new QPushButton(QStringLiteral("↻ Refresh Tailnet"), this);
    m_btnScan->setFixedHeight(34);
    m_btnScan->setStyleSheet(QStringLiteral("QPushButton { background: #2563eb; color: #ffffff; border: none; border-radius: 6px; padding: 0 16px; font-weight: 700; font-size: 12px; } QPushButton:hover { background: #1d4ed8; }"));
    connect(m_btnScan, &QPushButton::clicked, this, [this]() {
        startDiscovery(false);
    });
    headerLayout->addWidget(m_btnScan);

    baseLayout->addWidget(headerWidget);

    // =========================================================================
    // Responsive Machine Card Grid inside ScrollArea
    // =========================================================================
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_gridContainer = new ResponsiveGridContainer(m_scrollArea, 380);
    m_scrollArea->setWidget(m_gridContainer);
    baseLayout->addWidget(m_scrollArea, 1);

    // =========================================================================
    // In-App Floating Transfer Progress Dock
    // =========================================================================
    m_progressDock = new QFrame(this);
    m_progressDock->setObjectName(QStringLiteral("progressDock"));
    m_progressDock->setFrameShape(QFrame::StyledPanel);
    auto progressLayout = new QVBoxLayout(m_progressDock);
    progressLayout->setContentsMargins(10, 10, 10, 10);
    progressLayout->setSpacing(6);

    m_progressPathLabel = new QLabel(m_progressDock);
    m_progressPathLabel->setStyleSheet(QStringLiteral("color: #cbd5e1; font-size: 11px; font-weight: 500;"));
    m_progressPathLabel->setWordWrap(true);
    progressLayout->addWidget(m_progressPathLabel);

    m_progressLabel = new QLabel(QStringLiteral("🚀 Ready"), m_progressDock);
    m_progressLabel->setObjectName(QStringLiteral("progressLabel"));
    progressLayout->addWidget(m_progressLabel);

    m_progressBar = new QProgressBar(m_progressDock);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar);

    baseLayout->addWidget(m_progressDock);
    m_progressDock->setVisible(false); // Initially hidden

    // =========================================================================
    // Status Bar
    // =========================================================================
    statusBar()->showMessage(QStringLiteral("Ready. Initializing dynamic Tailscale discovery..."));
}

void Dir2ZeroWindow::updateThemeButtonLabel()
{
    ThemeDefinition curr = ThemeManager::instance()->currentTheme();
    m_btnTheme->setText(QStringLiteral("🎨 %1 ▾").arg(curr.name));
}

void Dir2ZeroWindow::onThemeChanged(const QString& themeId, const QString& themeName)
{
    Q_UNUSED(themeId);
    updateThemeButtonLabel();
    showStatusMessage(QStringLiteral("🎨 Theme switched to: %1").arg(themeName), 4000);
}

void Dir2ZeroWindow::onCycleTheme()
{
    ThemeManager::instance()->cycleNextTheme();
}

void Dir2ZeroWindow::showStatusMessage(const QString& message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

void Dir2ZeroWindow::startDiscovery(bool background)
{
    if (m_isDiscovering) return;
    m_isDiscovering = true;

    if (!background) {
        m_btnScan->setEnabled(false);
        m_btnScan->setText(QStringLiteral("Scanning..."));
        showStatusMessage(QStringLiteral("Scanning Tailscale network and probing port 22..."));
    }

    auto worker = new DiscoveryWorker(true);
    connect(&worker->workerSignals, &WorkerSignals::discoveryResult, this, &Dir2ZeroWindow::onDiscoveryResult);
    connect(&worker->workerSignals, &WorkerSignals::error, this, &Dir2ZeroWindow::onDiscoveryError);
    connect(&worker->workerSignals, &WorkerSignals::finished, this, &Dir2ZeroWindow::onDiscoveryFinished);

    QThreadPool::globalInstance()->start(worker);
}

void Dir2ZeroWindow::onDiscoveryResult(const QList<MachineInfo>& machines)
{
    int onlineCount = machines.size();
    int sshOkCount = 0;
    for (const MachineInfo& m : machines) {
        if (m.sshAvailable()) {
            sshOkCount++;
        }
    }
    int sshUnavailCount = onlineCount - sshOkCount;

    m_badgeOnline->setText(QStringLiteral("● %1 Online").arg(onlineCount));
    m_badgeSshOk->setText(QStringLiteral("● %1 Reachable").arg(sshOkCount));
    m_badgeUnavailable->setText(QStringLiteral("⚠ %1 Unavailable").arg(sshUnavailCount));

    // Smart reconciliation: update existing panels without dropping active SFTP sessions
    QMap<QString, MachinePanelWidget*> existingByIp;
    for (MachinePanelWidget* p : m_panels) {
        existingByIp.insert(p->machineInfo().ip(), p);
    }

    QSet<QString> discoveredIps;
    for (const MachineInfo& m : machines) {
        discoveredIps.insert(m.ip());

        if (existingByIp.contains(m.ip())) {
            MachinePanelWidget* panel = existingByIp.value(m.ip());
            panel->updateMachineInfo(m);
            if (panel->machineInfo().sshAvailable() != m.sshAvailable()) {
                if (m.sshAvailable() && panel->currentState() == MachineState::OnlineSshUnavailable) {
                    panel->setState(MachineState::OnlineSshOk);
                    panel->startConnection();
                } else if (!m.sshAvailable() && !panel->isConnected()) {
                    panel->setState(MachineState::OnlineSshUnavailable);
                }
            }
        } else {
            // New machine card
            auto panel = new MachinePanelWidget(m, m_defaultUser, m_defaultKey, m_gridContainer);
            m_panels.append(panel);
            m_gridContainer->addPanel(panel);
        }
    }

    // Remove panels for machines that have dropped offline
    for (auto it = existingByIp.begin(); it != existingByIp.end(); ++it) {
        if (!discoveredIps.contains(it.key())) {
            MachinePanelWidget* panel = it.value();
            panel->disconnectMachine();
            m_panels.removeOne(panel);
            m_gridContainer->removePanel(panel);
            delete panel;
        }
    }

    showStatusMessage(QStringLiteral("Discovery complete: %1 online machine(s) found (%2 reachable, %3 unavailable).")
        .arg(onlineCount).arg(sshOkCount).arg(sshUnavailCount));
}

void Dir2ZeroWindow::onDiscoveryError(const QString& error)
{
    showStatusMessage(QStringLiteral("Discovery error: %1").arg(error));
}

void Dir2ZeroWindow::onDiscoveryFinished()
{
    m_isDiscovering = false;
    m_btnScan->setEnabled(true);
    m_btnScan->setText(QStringLiteral("↻ Refresh Tailnet"));
}

void Dir2ZeroWindow::startTransferMonitor(const QString& srcPath,
                                         const QString& dstPath,
                                         const QString& srcHost,
                                         const QString& dstHost)
{
    m_progressDock->setVisible(true);
    m_progressBar->setValue(0);

    QString srcDisp = srcHost.isEmpty() ? srcPath : QStringLiteral("[%1] %2").arg(srcHost, srcPath);
    QString dstDisp = dstHost.isEmpty() ? dstPath : QStringLiteral("[%1] %2").arg(dstHost, dstPath);

    m_progressPathLabel->setText(QStringLiteral("📄 <b>FROM:</b> %1<br>➔ <b>TO:</b> %2").arg(srcDisp, dstDisp));
    m_progressLabel->setText(QStringLiteral("🚀 Preparing file transfer..."));
}

void Dir2ZeroWindow::updateTransferProgress(qint64 bytesDone, qint64 totalBytes, const QString& itemName, double speedBps)
{
    int percent = static_cast<int>((bytesDone * 100.0) / std::max<qint64>(1, totalBytes));
    m_progressBar->setValue(std::min(100, percent));

    if (itemName.contains(QStringLiteral(" ➔ "))) {
        QStringList parts = itemName.split(QStringLiteral(" ➔ "));
        if (parts.size() == 2) {
            m_progressPathLabel->setText(QStringLiteral("📄 <b>FROM:</b> %1<br>➔ <b>TO:</b> %2").arg(parts[0].trimmed(), parts[1].trimmed()));
        }
    }

    auto formatSize = [](qint64 b) -> QString {
        if (b < 1024) return QStringLiteral("%1 B").arg(b);
        if (b < 1024 * 1024) return QStringLiteral("%1 KB").arg(QString::number(b / 1024.0, 'f', 1));
        if (b < 1024LL * 1024 * 1024) return QStringLiteral("%1 MB").arg(QString::number(b / (1024.0 * 1024.0), 'f', 1));
        return QStringLiteral("%1 GB").arg(QString::number(b / (1024.0 * 1024.0 * 1024.0), 'f', 2));
    };

    QString speedStr = (speedBps > 0) ? QStringLiteral("%1/s").arg(formatSize(static_cast<qint64>(speedBps))) : QStringLiteral("0 B/s");
    QString doneStr = formatSize(bytesDone);
    QString totalStr = formatSize(totalBytes);

    m_progressLabel->setText(QStringLiteral("🚀 <b>Progress:</b> %1% (%2 / %3) • <b>Speed:</b> %4")
        .arg(QString::number(percent), doneStr, totalStr, speedStr));
}

void Dir2ZeroWindow::finishTransferMonitor()
{
    m_progressBar->setValue(100);
    m_progressLabel->setText(QStringLiteral("✅ <b>Transfer completed successfully!</b>"));
    QTimer::singleShot(2500, this, [this]() {
        m_progressDock->setVisible(false);
    });
}

void Dir2ZeroWindow::closeEvent(QCloseEvent* event)
{
    if (m_discoveryTimer) {
        m_discoveryTimer->stop();
    }
    for (MachinePanelWidget* p : m_panels) {
        p->disconnectMachine();
    }
    QThreadPool::globalInstance()->waitForDone(1000);
    QMainWindow::closeEvent(event);
}
