#include "MachinePanelWidget.hpp"
#include "Dir2ZeroWindow.hpp"
#include "../security/CredentialStore.hpp"
#include "../core/PermissionPolicy.hpp"
#include "../helpers/RemoteClipboard.hpp"
#include "../workers/SFTPConnectWorker.hpp"
#include "../workers/SFTPListWorker.hpp"
#include "../workers/SFTPFileOpWorker.hpp"
#include "../workers/CrossMachineTransferWorker.hpp"
#include "../discovery/ProbeWorker.hpp"

#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QStyle>
#include <QDir>

MachinePanelWidget::MachinePanelWidget(const MachineInfo& info,
                                       const QString& defaultUser,
                                       const QString& defaultKey,
                                       QWidget* parent)
    : QFrame(parent)
    , m_info(info)
    , m_state(MachineState::Discovering)
    , m_username(defaultUser)
    , m_keyPath(defaultKey)
    , m_fsModel(nullptr)
    , m_authBar(nullptr)
    , m_treeView(nullptr)
{
    if (m_username.isEmpty()) {
        m_username = QDir::home().dirName();
    }
    if (m_keyPath.isEmpty()) {
        m_keyPath = CredentialStore::getFirstAvailableSshKey();
    }

    // Load saved credentials if present
    auto saved = CredentialStore::get(m_info.ip(), m_info.name());
    if (saved.has_value()) {
        if (!saved->username.isEmpty()) m_username = saved->username;
        if (!saved->password.isEmpty()) m_password = saved->password;
        if (!saved->keyPath.isEmpty()) m_keyPath = saved->keyPath;
    }

    m_sftpManager = std::make_unique<SFTPManager>(
        m_info.ip(),
        SFTPManager::DEFAULT_PORT,
        m_username,
        m_password,
        m_keyPath
    );

    m_rootNode = std::make_unique<FSNode>(QStringLiteral("root"), QStringLiteral("."), true);
    m_fsModel = new RemoteFSModel(m_rootNode.get(), this);

    setupUi();
    applyMachineData();

    connect(RemoteClipboard::instance(), &RemoteClipboard::clipboardChanged,
            this, &MachinePanelWidget::onClipboardChanged);
}

MachinePanelWidget::~MachinePanelWidget()
{
    disconnectMachine();
}

void MachinePanelWidget::setupUi()
{
    setObjectName(QStringLiteral("machinePanel"));
    setFrameShape(QFrame::StyledPanel);
    setMinimumSize(360, 440);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);
    m_mainLayout->setSpacing(8);

    // Heading Label (OS Icon + Machine Name)
    m_headingLabel = new QLabel(this);
    m_headingLabel->setObjectName(QStringLiteral("machineHeading"));
    m_mainLayout->addWidget(m_headingLabel);

    // Status Label
    m_statusLabel = new QLabel(QStringLiteral("● INITIALIZING..."), this);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    m_mainLayout->addWidget(m_statusLabel);

    // IP Heading & Chip
    m_ipHeading = new QLabel(QStringLiteral("IP ADDRESS"), this);
    m_ipHeading->setObjectName(QStringLiteral("ipHeading"));
    m_mainLayout->addWidget(m_ipHeading);

    m_ipLabel = new QLabel(m_info.ip(), this);
    m_ipLabel->setObjectName(QStringLiteral("ipAddress"));
    m_mainLayout->addWidget(m_ipLabel);

    // 1. Session & Path Row
    m_toolbarLayout = new QHBoxLayout();
    m_toolbarLayout->setContentsMargins(0, 4, 0, 2);
    m_toolbarLayout->setSpacing(6);

    m_pathLabel = new QLabel(QStringLiteral("📁 (Connecting...)"), this);
    m_pathLabel->setStyleSheet(QStringLiteral("color: #38bdf8; font-size: 11px; font-weight: 600;"));
    m_pathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbarLayout->addWidget(m_pathLabel, 1);

    m_btnAuthToggle = new QPushButton(QStringLiteral("🔑 Auth"), this);
    m_btnAuthToggle->setFixedHeight(28);
    m_btnAuthToggle->setMinimumWidth(68);
    m_btnAuthToggle->setToolTip(QStringLiteral("Open authentication form to configure credentials"));
    connect(m_btnAuthToggle, &QPushButton::clicked, this, [this]() {
        m_authBar->setVisible(!m_authBar->isVisible());
    });
    m_toolbarLayout->addWidget(m_btnAuthToggle);

    m_btnRefresh = new QPushButton(QStringLiteral("↻ Refresh"), this);
    m_btnRefresh->setFixedHeight(28);
    m_btnRefresh->setMinimumWidth(78);
    m_btnRefresh->setToolTip(QStringLiteral("Refresh directory listing (F5)"));
    connect(m_btnRefresh, &QPushButton::clicked, this, &MachinePanelWidget::reloadFilesystem);
    m_toolbarLayout->addWidget(m_btnRefresh);

    m_btnRetry = new QPushButton(QStringLiteral("⟳ Connect"), this);
    m_btnRetry->setFixedHeight(28);
    m_btnRetry->setMinimumWidth(85);
    m_btnRetry->setVisible(false);
    connect(m_btnRetry, &QPushButton::clicked, this, &MachinePanelWidget::onActionRetryClicked);
    m_toolbarLayout->addWidget(m_btnRetry);

    m_btnDisconnect = new QPushButton(QStringLiteral("Disconnect"), this);
    m_btnDisconnect->setObjectName(QStringLiteral("disconnect"));
    m_btnDisconnect->setFixedHeight(28);
    m_btnDisconnect->setMinimumWidth(88);
    m_btnDisconnect->setToolTip(QStringLiteral("Disconnect session and forget saved credentials"));
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MachinePanelWidget::disconnectMachine);
    m_toolbarLayout->addWidget(m_btnDisconnect);

    m_mainLayout->addLayout(m_toolbarLayout);

    // 2. Action Toolbar Row (New File, New Folder, Paste)
    m_actionsLayout = new QHBoxLayout();
    m_actionsLayout->setContentsMargins(0, 2, 0, 4);
    m_actionsLayout->setSpacing(8);

    m_btnNewFile = new QPushButton(QStringLiteral("📄 + New File"), this);
    m_btnNewFile->setFixedHeight(32);
    m_btnNewFile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_btnNewFile->setToolTip(QStringLiteral("Create a new file in the current directory"));
    connect(m_btnNewFile, &QPushButton::clicked, this, [this]() { createNewFile(); });
    m_actionsLayout->addWidget(m_btnNewFile);

    m_btnNewFolder = new QPushButton(QStringLiteral("📁 + New Folder"), this);
    m_btnNewFolder->setFixedHeight(32);
    m_btnNewFolder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_btnNewFolder->setToolTip(QStringLiteral("Create a new directory in the current directory"));
    connect(m_btnNewFolder, &QPushButton::clicked, this, [this]() { createNewFolder(); });
    m_actionsLayout->addWidget(m_btnNewFolder);

    m_btnPaste = new QPushButton(QStringLiteral("📥 Paste"), this);
    m_btnPaste->setFixedHeight(32);
    m_btnPaste->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_btnPaste->setEnabled(false);
    m_btnPaste->setToolTip(QStringLiteral("Paste copied or cut items into current directory (Ctrl+V)"));
    connect(m_btnPaste, &QPushButton::clicked, this, &MachinePanelWidget::pasteClipboard);
    m_actionsLayout->addWidget(m_btnPaste);

    m_mainLayout->addLayout(m_actionsLayout);

    // In-App Authentication Bar
    m_authBar = new MachineAuthBar(m_info.ip(), m_info.name(), this);
    connect(m_authBar, &MachineAuthBar::authRequested, this, &MachinePanelWidget::onAuthRequested);
    connect(m_authBar, &MachineAuthBar::dismissed, this, [this]() { m_authBar->setVisible(false); });
    m_authBar->setVisible(false);

    auto saved = CredentialStore::get(m_info.ip(), m_info.name());
    m_authBar->loadCredentials(saved);
    m_mainLayout->addWidget(m_authBar);

    // Tree View
    m_treeView = new RemoteFileTreeView(this, this);
    m_treeView->setObjectName(QStringLiteral("treeView"));
    m_treeView->setModel(m_fsModel);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QHeaderView* header = m_treeView->header();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    header->setSectionsMovable(true);
    header->setSectionsClickable(true);
    header->resizeSection(0, 180); // Name
    header->resizeSection(1, 75);  // Size
    header->resizeSection(2, 125); // Modified
    header->resizeSection(3, 90);  // Permissions

    connect(m_treeView, &QTreeView::expanded, this, &MachinePanelWidget::onTreeExpanded);

    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &MachinePanelWidget::onCustomContextMenuRequested);

    m_mainLayout->addWidget(m_treeView, 1);
}

void MachinePanelWidget::applyMachineData()
{
    m_headingLabel->setText(QStringLiteral("%1 %2").arg(m_info.osIcon(), m_info.displayName()));
    m_ipLabel->setText(m_info.ip());

    if (m_info.sshAvailable()) {
        setState(MachineState::OnlineSshOk);
        startConnection();
    } else {
        setState(MachineState::OnlineSshUnavailable);
    }
}

void MachinePanelWidget::updateMachineInfo(const MachineInfo& info)
{
    m_info = info;
    m_headingLabel->setText(QStringLiteral("%1 %2").arg(m_info.osIcon(), m_info.displayName()));
    m_ipLabel->setText(m_info.ip());
}

bool MachinePanelWidget::isConnected() const
{
    return m_state == MachineState::Connected && m_sftpManager && m_sftpManager->isConnected();
}

void MachinePanelWidget::setState(MachineState state, const QString& detail)
{
    m_state = state;
    setProperty("machineState", machineStateToString(state));
    style()->unpolish(this);
    style()->polish(this);

    bool connected = (state == MachineState::Connected);
    m_btnNewFile->setEnabled(connected);
    m_btnNewFolder->setEnabled(connected);
    m_btnPaste->setEnabled(connected && RemoteClipboard::instance()->hasItem());
    m_btnDisconnect->setEnabled(connected || state == MachineState::Connecting);

    if (state == MachineState::OnlineSshOk) {
        m_statusLabel->setText(machineStateDisplayName(state));
        m_statusLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: 700;").arg(machineStateColor(state)));
        m_btnRefresh->setVisible(true);
        m_btnRetry->setVisible(false);
        m_treeView->setEnabled(true);
    } else if (state == MachineState::Connecting) {
        m_statusLabel->setText(QStringLiteral("● CONNECTING..."));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #38bdf8; font-weight: 700;"));
        m_btnRefresh->setEnabled(false);
    } else if (state == MachineState::Connected) {
        m_statusLabel->setText(QStringLiteral("● CONNECTED"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #10b981; font-weight: 700;"));
        m_btnRefresh->setVisible(true);
        m_btnRefresh->setEnabled(true);
        m_btnRetry->setVisible(false);
        m_authBar->setVisible(false);
        m_authBar->clearAlert();
        m_treeView->setEnabled(true);
    } else if (state == MachineState::AuthRequired) {
        m_statusLabel->setText(QStringLiteral("🔑 AUTHENTICATION REQUIRED"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f59e0b; font-weight: 700;"));
        m_btnRefresh->setVisible(false);
        m_btnRetry->setText(QStringLiteral("🔑 Authenticate"));
        m_btnRetry->setVisible(true);
        m_btnRetry->setEnabled(true);
        m_treeView->setEnabled(false);
        m_fsModel->setNodeError(m_rootNode.get(), QStringLiteral("Authentication required. Enter password or select private key."));
        m_authBar->showAlert(detail.isEmpty() ? QStringLiteral("Authentication required. Please enter credentials.") : detail);
    } else if (state == MachineState::OnlineSshUnavailable) {
        m_statusLabel->setText(QStringLiteral("⚠ ONLINE — SSH UNAVAILABLE"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f59e0b; font-weight: 700;"));
        m_btnRefresh->setVisible(false);
        m_btnRetry->setText(QStringLiteral("Probe Port 22"));
        m_btnRetry->setVisible(true);
        m_btnRetry->setEnabled(true);
        m_treeView->setEnabled(false);
        m_fsModel->setNodeError(m_rootNode.get(), QStringLiteral("Port 22 unreachable. SSH/SFTP service not running."));
    } else if (state == MachineState::Error) {
        m_statusLabel->setText(detail.isEmpty() ? QStringLiteral("✕ CONNECTION FAILED") : QStringLiteral("✕ FAILED: %1").arg(detail));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #ef4444; font-weight: 700;"));
        m_btnRefresh->setVisible(false);
        m_btnRetry->setText(QStringLiteral("⟳ Retry"));
        m_btnRetry->setVisible(true);
        m_btnRetry->setEnabled(true);
        m_treeView->setEnabled(true);
        if (!detail.isEmpty()) {
            m_fsModel->setNodeError(m_rootNode.get(), detail);
        }
    }
}

FSNode* MachinePanelWidget::selectedNode() const
{
    if (!m_treeView) return nullptr;
    QModelIndexList selected = m_treeView->selectionModel()->selectedRows();
    if (!selected.isEmpty()) {
        return static_cast<FSNode*>(selected.first().internalPointer());
    }
    QModelIndex cur = m_treeView->currentIndex();
    if (cur.isValid()) {
        return static_cast<FSNode*>(cur.internalPointer());
    }
    return nullptr;
}

QString MachinePanelWidget::targetDirectory(FSNode* node) const
{
    if (node && node->isDir && !node->isDummy) {
        return node->path;
    } else if (node && node->parent && node->parent != m_rootNode.get()) {
        return node->parent->path;
    }
    return QStringLiteral(".");
}

void MachinePanelWidget::startConnection()
{
    setState(MachineState::Connecting);

    auto worker = new SFTPConnectWorker(m_sftpManager.get());
    connect(&worker->workerSignals, &WorkerSignals::connectResult, this, &MachinePanelWidget::onSftpConnected);
    connect(&worker->workerSignals, &WorkerSignals::error, this, &MachinePanelWidget::onSftpError);

    QThreadPool::globalInstance()->start(worker);
}

void MachinePanelWidget::disconnectMachine()
{
    if (m_sftpManager) {
        m_sftpManager->disconnectSession();
    }

    // Forget saved credentials
    CredentialStore::remove(m_info.ip());
    if (!m_info.name().isEmpty()) {
        CredentialStore::remove(m_info.name());
    }

    m_password.clear();
    if (m_sftpManager) {
        m_sftpManager->setPassword(QString());
    }
    if (m_authBar) {
        m_authBar->clearPassword();
    }

    m_rootNode->clearChildren();
    m_rootNode->isLoaded = false;
    m_rootNode->isLoading = false;
    m_fsModel->setNodeError(m_rootNode.get(), QStringLiteral("Disconnected. Enter password to authenticate."));

    m_pathLabel->setText(QStringLiteral("📁 Path: (Disconnected)"));
    setState(MachineState::AuthRequired, QStringLiteral("Disconnected. Please enter password to re-authenticate."));
    m_authBar->setVisible(true);

    notifyStatus(QStringLiteral("[%1] Disconnected & credentials forgotten.").arg(m_info.name()));
}

void MachinePanelWidget::reloadFilesystem()
{
    if (!isConnected()) {
        startConnection();
        return;
    }

    m_btnRefresh->setEnabled(false);
    m_rootNode->isLoaded = false;
    m_rootNode->isLoading = true;

    auto worker = new SFTPListWorker(m_sftpManager.get(), QStringLiteral("."), m_rootNode.get(), QModelIndex());
    connect(&worker->workerSignals, &WorkerSignals::listResult, this, &MachinePanelWidget::onDirListed);
    connect(&worker->workerSignals, &WorkerSignals::error, this, &MachinePanelWidget::onFileOpError);

    QThreadPool::globalInstance()->start(worker);
}

void MachinePanelWidget::onTreeExpanded(const QModelIndex& index)
{
    if (!index.isValid()) return;
    auto node = static_cast<FSNode*>(index.internalPointer());
    if (node && node->isDir && !node->isLoaded && !node->isLoading) {
        node->isLoading = true;
        auto worker = new SFTPListWorker(m_sftpManager.get(), node->path, node, index);
        connect(&worker->workerSignals, &WorkerSignals::listResult, this, &MachinePanelWidget::onDirListed);
        connect(&worker->workerSignals, &WorkerSignals::error, this, &MachinePanelWidget::onFileOpError);
        QThreadPool::globalInstance()->start(worker);
    }
}

void MachinePanelWidget::onDirListed(FSNode* parentNode, const QList<RemoteEntry>& entries, const QModelIndex& parentIndex)
{
    m_fsModel->populateNode(parentNode, entries, parentIndex);
    m_btnRefresh->setEnabled(true);
    if (parentNode == m_rootNode.get()) {
        QString absPath = m_sftpManager->getAbsolutePath(QStringLiteral("."));
        m_pathLabel->setText(QStringLiteral("📁 %1").arg(absPath));
    }
}

void MachinePanelWidget::onSftpConnected(bool success)
{
    Q_UNUSED(success);
    setState(MachineState::Connected);
    reloadFilesystem();
}

void MachinePanelWidget::onSftpError(const QString& err)
{
    QString lower = err.toLower();
    bool isAuth = lower.contains(QStringLiteral("auth")) ||
                  lower.contains(QStringLiteral("permission denied")) ||
                  lower.contains(QStringLiteral("publickey")) ||
                  lower.contains(QStringLiteral("password"));

    if (isAuth) {
        setState(MachineState::AuthRequired, err);
    } else {
        setState(MachineState::Error, err);
    }
}

void MachinePanelWidget::onAuthRequested(const QString& username, const QString& password, const QString& keyPath, bool remember)
{
    m_username = username;
    m_password = password;
    m_keyPath = keyPath;

    if (remember) {
        CredentialStore::save(m_info.ip(), m_username, m_password, m_keyPath);
    }

    m_sftpManager->setUsername(m_username);
    m_sftpManager->setPassword(m_password);
    m_sftpManager->setKeyPath(m_keyPath);

    startConnection();
}

void MachinePanelWidget::onActionRetryClicked()
{
    if (m_state == MachineState::OnlineSshUnavailable) {
        probePort22();
    } else if (m_state == MachineState::AuthRequired) {
        m_authBar->setVisible(true);
    } else {
        startConnection();
    }
}

void MachinePanelWidget::probePort22()
{
    m_statusLabel->setText(QStringLiteral("● PROBING PORT 22..."));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #38bdf8; font-weight: 700;"));
    m_btnRetry->setEnabled(false);

    auto worker = new ProbeWorker(m_info.ip());
    connect(&worker->workerSignals, &WorkerSignals::portCheckResult, this, &MachinePanelWidget::onPortCheckResult);
    QThreadPool::globalInstance()->start(worker);
}

void MachinePanelWidget::onPortCheckResult(bool isOpen)
{
    m_btnRetry->setEnabled(true);
    if (isOpen) {
        m_info.setSshAvailable(true);
        setState(MachineState::OnlineSshOk);
        startConnection();
    } else {
        m_statusLabel->setText(QStringLiteral("⚠ PORT 22 STILL UNREACHABLE"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f59e0b; font-weight: 700;"));
    }
}

void MachinePanelWidget::onClipboardChanged()
{
    m_btnPaste->setEnabled(isConnected() && RemoteClipboard::instance()->hasItem());
}

void MachinePanelWidget::onCustomContextMenuRequested(const QPoint& pos)
{
    if (!isConnected()) return;

    QModelIndex index = m_treeView->indexAt(pos);
    FSNode* selected = index.isValid() ? static_cast<FSNode*>(index.internalPointer()) : nullptr;

    QMenu menu(this);

    if (selected && !selected->isDummy) {
        auto copyAct = menu.addAction(QStringLiteral("📋 Copy"), this, &MachinePanelWidget::copySelected);
        copyAct->setShortcut(QKeySequence::Copy);
        auto cutAct = menu.addAction(QStringLiteral("✂️ Cut"), this, &MachinePanelWidget::cutSelected);
        cutAct->setShortcut(QKeySequence::Cut);

        if (RemoteClipboard::instance()->hasItem()) {
            QString target = targetDirectory(selected);
            auto pasteAct = menu.addAction(QStringLiteral("📥 Paste '%1'").arg(RemoteClipboard::instance()->sourceName()), this, [this, target]() {
                pasteIntoDir(target);
            });
            pasteAct->setShortcut(QKeySequence::Paste);
        }

        menu.addSeparator();
        auto renameAct = menu.addAction(QStringLiteral("✏️ Rename..."), this, &MachinePanelWidget::renameSelected);
        renameAct->setShortcut(QKeySequence(Qt::Key_F2));
        auto delAct = menu.addAction(QStringLiteral("🗑️ Delete"), this, &MachinePanelWidget::deleteSelected);
        delAct->setShortcut(QKeySequence::Delete);
        menu.addSeparator();
    } else {
        if (RemoteClipboard::instance()->hasItem()) {
            auto pasteAct = menu.addAction(QStringLiteral("📥 Paste '%1'").arg(RemoteClipboard::instance()->sourceName()), this, [this]() {
                pasteIntoDir(QStringLiteral("."));
            });
            pasteAct->setShortcut(QKeySequence::Paste);
            menu.addSeparator();
        }
    }

    QString target = targetDirectory(selected);
    menu.addAction(QStringLiteral("+ 📄 New File..."), this, [this, target]() { createNewFile(target); });
    menu.addAction(QStringLiteral("+ 📁 New Folder..."), this, [this, target]() { createNewFolder(target); });
    menu.addSeparator();
    auto refAct = menu.addAction(QStringLiteral("↻ Refresh"), this, &MachinePanelWidget::reloadFilesystem);
    refAct->setShortcut(QKeySequence(Qt::Key_F5));

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

void MachinePanelWidget::copySelected()
{
    FSNode* node = selectedNode();
    if (node && !node->isDummy) {
        RemoteClipboard::instance()->copy(this, node);
        notifyStatus(QStringLiteral("Copied '%1' from [%2]. Ready to paste.").arg(node->name, m_info.name()));
    } else {
        notifyStatus(QStringLiteral("[%1] Please select a file or folder to copy.").arg(m_info.name()));
    }
}

void MachinePanelWidget::cutSelected()
{
    FSNode* node = selectedNode();
    if (node && !node->isDummy) {
        RemoteClipboard::instance()->cut(this, node);
        notifyStatus(QStringLiteral("Cut '%1' from [%2]. Ready to paste.").arg(node->name, m_info.name()));
    } else {
        notifyStatus(QStringLiteral("[%1] Please select a file or folder to cut.").arg(m_info.name()));
    }
}

void MachinePanelWidget::pasteClipboard()
{
    FSNode* node = selectedNode();
    QString target = targetDirectory(node);
    pasteIntoDir(target);
}

void MachinePanelWidget::pasteIntoDir(const QString& destDir)
{
    RemoteClipboard* clip = RemoteClipboard::instance();
    if (!clip->hasItem()) {
        notifyStatus(QStringLiteral("Clipboard is empty."));
        return;
    }

    MachinePanelWidget* srcPanel = clip->sourcePanel();
    QString srcPath = clip->sourcePath();
    QString srcName = clip->sourceName();
    bool isDir = clip->isDirectory();
    bool isCut = clip->isCut();

    handleDropTransfer(srcPanel->machineInfo().ip(), srcPath, srcName, isDir, destDir, isCut);
}

void MachinePanelWidget::handleDropTransfer(const QString& srcIp,
                                            const QString& srcPath,
                                            const QString& srcName,
                                            bool isDir,
                                            const QString& destDir,
                                            bool isCut)
{
    // Permission validation
    QString destPath = (destDir == QStringLiteral(".")) ? srcName : (destDir + QStringLiteral("/") + srcName);
    auto check = PermissionPolicy::defaultPolicy().validateTransfer(srcPath, destPath, isCut);
    if (!check.allowed) {
        QMessageBox::warning(this, QStringLiteral("Permission Restricted"), check.reason);
        notifyStatus(QStringLiteral("Transfer blocked: %1").arg(check.reason));
        return;
    }

    // Locate source panel from parent Dir2ZeroWindow
    auto mainWin = qobject_cast<Dir2ZeroWindow*>(window());
    MachinePanelWidget* srcPanel = nullptr;
    if (mainWin) {
        for (MachinePanelWidget* p : mainWin->machinePanels()) {
            if (p->machineInfo().ip() == srcIp) {
                srcPanel = p;
                break;
            }
        }
    }

    if (!srcPanel) {
        notifyStatus(QStringLiteral("Drop transfer failed: Source machine panel not found."));
        return;
    }

    QString actionStr = isCut ? QStringLiteral("Moving") : QStringLiteral("Copying");
    notifyStatus(QStringLiteral("%1 '%2' from [%3] to [%4]...").arg(actionStr, srcName, srcPanel->machineInfo().name(), m_info.name()));

    if (mainWin) {
        QString srcAbs = srcPanel->sftpManager()->getAbsolutePath(srcPath);
        QString dstAbs = m_sftpManager->getAbsolutePath(destPath);
        mainWin->startTransferMonitor(srcAbs, dstAbs, srcPanel->machineInfo().name(), m_info.name());
    }

    auto worker = new CrossMachineTransferWorker(
        srcPanel->sftpManager(),
        m_sftpManager.get(),
        srcPath,
        destPath,
        isDir,
        isCut,
        srcPanel->machineInfo().name(),
        m_info.name()
    );

    if (mainWin) {
        connect(&worker->workerSignals, &WorkerSignals::progress, mainWin, &Dir2ZeroWindow::updateTransferProgress);
        connect(&worker->workerSignals, &WorkerSignals::finished, mainWin, &Dir2ZeroWindow::finishTransferMonitor);
    }

    connect(&worker->workerSignals, &WorkerSignals::transferResult, this, [this, srcPanel, isCut, srcName](bool success) {
        Q_UNUSED(success);
        reloadFilesystem();
        if (isCut && srcPanel) {
            srcPanel->reloadFilesystem();
            RemoteClipboard::instance()->clear();
        }
        notifyStatus(QStringLiteral("Successfully transferred '%1' to [%2]!").arg(srcName, m_info.name()));
    });
    connect(&worker->workerSignals, &WorkerSignals::error, this, &MachinePanelWidget::onFileOpError);

    QThreadPool::globalInstance()->start(worker);
}

void MachinePanelWidget::createNewFile(const QString& targetDir)
{
    if (!isConnected()) {
        notifyStatus(QStringLiteral("[%1] Cannot create file: not connected.").arg(m_info.name()));
        return;
    }

    QString dir = targetDir.isEmpty() ? targetDirectory(selectedNode()) : targetDir;
    QString absTarget = m_sftpManager->getAbsolutePath(dir);

    bool ok = false;
    QString name = QInputDialog::getText(
        this,
        QStringLiteral("Create New File"),
        QStringLiteral("Target directory:\n%1\n\nEnter new filename:").arg(absTarget),
        QLineEdit::Normal,
        QString(),
        &ok
    );

    if (ok && !name.trimmed().isEmpty()) {
        name = name.trimmed();
        auto check = PermissionPolicy::defaultPolicy().validateCreateFile(dir, name);
        if (!check.allowed) {
            QMessageBox::warning(this, QStringLiteral("Permission Restricted"), check.reason);
            return;
        }

        QString fullPath = (dir == QStringLiteral(".")) ? name : (dir + QStringLiteral("/") + name);
        notifyStatus(QStringLiteral("Creating file '%1' on [%2]...").arg(name, m_info.name()));

        auto worker = new SFTPFileOpWorker(QStringLiteral("create_file"), m_sftpManager.get(), fullPath);
        connect(&worker->workerSignals, &WorkerSignals::fileOpResult, this, &MachinePanelWidget::onFileOpSuccess);
        connect(&worker->workerSignals, &WorkerSignals::error, this, &MachinePanelWidget::onFileOpError);
        QThreadPool::globalInstance()->start(worker);
    }
}

void MachinePanelWidget::createNewFolder(const QString& targetDir)
{
    if (!isConnected()) {
        notifyStatus(QStringLiteral("[%1] Cannot create folder: not connected.").arg(m_info.name()));
        return;
    }

    QString dir = targetDir.isEmpty() ? targetDirectory(selectedNode()) : targetDir;
    QString absTarget = m_sftpManager->getAbsolutePath(dir);

    bool ok = false;
    QString name = QInputDialog::getText(
        this,
        QStringLiteral("Create New Folder"),
        QStringLiteral("Target directory:\n%1\n\nEnter new folder name:").arg(absTarget),
        QLineEdit::Normal,
        QString(),
        &ok
    );

    if (ok && !name.trimmed().isEmpty()) {
        name = name.trimmed();
        auto check = PermissionPolicy::defaultPolicy().validateCreateDirectory(dir, name);
        if (!check.allowed) {
            QMessageBox::warning(this, QStringLiteral("Permission Restricted"), check.reason);
            return;
        }

        QString fullPath = (dir == QStringLiteral(".")) ? name : (dir + QStringLiteral("/") + name);
        notifyStatus(QStringLiteral("Creating folder '%1' on [%2]...").arg(name, m_info.name()));

        auto worker = new SFTPFileOpWorker(QStringLiteral("create_dir"), m_sftpManager.get(), fullPath);
        connect(&worker->workerSignals, &WorkerSignals::fileOpResult, this, &MachinePanelWidget::onFileOpSuccess);
        connect(&worker->workerSignals, &WorkerSignals::error, this, &MachinePanelWidget::onFileOpError);
        QThreadPool::globalInstance()->start(worker);
    }
}

void MachinePanelWidget::renameSelected()
{
    FSNode* node = selectedNode();
    if (node && !node->isDummy) {
        renameNode(node);
    }
}

void MachinePanelWidget::renameNode(FSNode* node)
{
    if (!node || node->isDummy) return;

    bool ok = false;
    QString newName = QInputDialog::getText(
        this,
        QStringLiteral("Rename Item"),
        QStringLiteral("Enter new name for '%1':").arg(node->name),
        QLineEdit::Normal,
        node->name,
        &ok
    );

    if (ok && !newName.trimmed().isEmpty() && newName.trimmed() != node->name) {
        newName = newName.trimmed();
        QString parentDir = node->path.contains('/') ? node->path.left(node->path.lastIndexOf('/')) : QString();
        QString newPath = parentDir.isEmpty() ? newName : (parentDir + QStringLiteral("/") + newName);

        auto check = PermissionPolicy::defaultPolicy().validateRename(node->path, newPath);
        if (!check.allowed) {
            QMessageBox::warning(this, QStringLiteral("Permission Restricted"), check.reason);
            return;
        }

        notifyStatus(QStringLiteral("Renaming '%1' to '%2'...").arg(node->name, newName));

        auto worker = new SFTPFileOpWorker(QStringLiteral("rename"), m_sftpManager.get(), node->path, newPath);
        connect(&worker->workerSignals, &WorkerSignals::fileOpResult, this, &MachinePanelWidget::onFileOpSuccess);
        connect(&worker->workerSignals, &WorkerSignals::error, this, &MachinePanelWidget::onFileOpError);
        QThreadPool::globalInstance()->start(worker);
    }
}

void MachinePanelWidget::deleteSelected()
{
    FSNode* node = selectedNode();
    if (node && !node->isDummy) {
        deleteNode(node);
    }
}

void MachinePanelWidget::deleteNode(FSNode* node)
{
    if (!node || node->isDummy) return;

    auto check = PermissionPolicy::defaultPolicy().validateDelete(node->path, node->isDir);
    if (!check.allowed) {
        QMessageBox::warning(this, QStringLiteral("Permission Restricted"), check.reason);
        return;
    }

    QString kind = node->isDir ? QStringLiteral("directory and all its contents") : QStringLiteral("file");
    auto reply = QMessageBox::question(
        this,
        QStringLiteral("Confirm Delete"),
        QStringLiteral("Are you sure you want to delete the %1:\n\n'%2'\n\nfrom %3?").arg(kind, node->name, m_info.name()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        QString op = node->isDir ? QStringLiteral("delete_dir") : QStringLiteral("delete_file");
        notifyStatus(QStringLiteral("Deleting '%1' from [%2]...").arg(node->name, m_info.name()));

        auto worker = new SFTPFileOpWorker(op, m_sftpManager.get(), node->path);
        connect(&worker->workerSignals, &WorkerSignals::fileOpResult, this, &MachinePanelWidget::onFileOpSuccess);
        connect(&worker->workerSignals, &WorkerSignals::error, this, &MachinePanelWidget::onFileOpError);
        QThreadPool::globalInstance()->start(worker);
    }
}

void MachinePanelWidget::onFileOpSuccess(const QString& opType, const QString& msg)
{
    Q_UNUSED(opType);
    reloadFilesystem();
    notifyStatus(QStringLiteral("[%1] %2").arg(m_info.name(), msg));
}

void MachinePanelWidget::onFileOpError(const QString& err)
{
    notifyStatus(QStringLiteral("[%1] Operation error: %2").arg(m_info.name(), err));
    QMessageBox::warning(this, QStringLiteral("Operation Error"), QStringLiteral("Error on %1:\n\n%2").arg(m_info.name(), err));
}

void MachinePanelWidget::notifyStatus(const QString& message)
{
    auto mainWin = qobject_cast<Dir2ZeroWindow*>(window());
    if (mainWin) {
        mainWin->showStatusMessage(message);
    }
}
