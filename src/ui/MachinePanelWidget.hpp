#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QThreadPool>
#include <memory>
#include <optional>
#include "../core/MachineInfo.hpp"
#include "../core/MachineState.hpp"
#include "../fs/FSNode.hpp"
#include "../fs/RemoteFSModel.hpp"
#include "../fs/SFTPManager.hpp"
#include "MachineAuthBar.hpp"
#include "RemoteFileTreeView.hpp"

class Dir2ZeroWindow;

class MachinePanelWidget : public QFrame {
    Q_OBJECT

public:
    explicit MachinePanelWidget(const MachineInfo& info,
                                const QString& defaultUser = QString(),
                                const QString& defaultKey = QString(),
                                QWidget* parent = nullptr);
    ~MachinePanelWidget() override;

    MachineInfo machineInfo() const { return m_info; }
    void updateMachineInfo(const MachineInfo& info);

    SFTPManager* sftpManager() { return m_sftpManager.get(); }
    FSNode* rootNode() { return m_rootNode.get(); }
    RemoteFSModel* fsModel() { return m_fsModel; }
    bool isConnected() const;

    MachineState currentState() const { return m_state; }
    void setState(MachineState state, const QString& detail = QString());

    FSNode* selectedNode() const;
    QString targetDirectory(FSNode* node = nullptr) const;

    void startConnection();
    void disconnectMachine();
    void reloadFilesystem();

    void copySelected();
    void cutSelected();
    void pasteClipboard();
    void deleteSelected();
    void renameSelected();

    void pasteIntoDir(const QString& destDir);
    void handleDropTransfer(const QString& srcIp,
                            const QString& srcPath,
                            const QString& srcName,
                            bool isDir,
                            const QString& destDir,
                            bool isCut);

    void notifyStatus(const QString& message);

private slots:
    void onAuthRequested(const QString& username, const QString& password, const QString& keyPath);
    void onActionRetryClicked();
    void onTreeExpanded(const QModelIndex& index);
    void onCustomContextMenuRequested(const QPoint& pos);
    void onClipboardChanged();

    // Async worker handlers
    void onPortCheckResult(bool isOpen);
    void onSftpConnected(bool success);
    void onSftpError(const QString& err);
    void onDirListed(FSNode* parentNode, const QList<RemoteEntry>& entries, const QModelIndex& parentIndex);
    void onFileOpSuccess(const QString& opType, const QString& msg);
    void onFileOpError(const QString& err);

private:
    MachineInfo m_info;
    MachineState m_state;
    QString m_username;
    QString m_password;
    QString m_keyPath;

    std::unique_ptr<FSNode> m_rootNode;
    RemoteFSModel* m_fsModel;
    std::unique_ptr<SFTPManager> m_sftpManager;

    // UI Widgets
    QVBoxLayout* m_mainLayout;
    QLabel* m_headingLabel;
    QLabel* m_statusLabel;
    QLabel* m_ipHeading;
    QLabel* m_ipLabel;
    QLabel* m_pathLabel;

    QHBoxLayout* m_toolbarLayout;
    QHBoxLayout* m_actionsLayout;
    QPushButton* m_btnNewFile;
    QPushButton* m_btnNewFolder;
    QPushButton* m_btnPaste;
    QPushButton* m_btnAuthToggle;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnRetry;
    QPushButton* m_btnDisconnect;

    MachineAuthBar* m_authBar;
    RemoteFileTreeView* m_treeView;

    void setupUi();
    void applyMachineData();
    void probePort22();

    void createNewFile(const QString& targetDir = QString());
    void createNewFolder(const QString& targetDir = QString());
    void renameNode(FSNode* node);
    void deleteNode(FSNode* node);
};
