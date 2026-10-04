#pragma once

#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QToolButton>
#include "../security/CredentialStore.hpp"

class MachineAuthBar : public QFrame {
    Q_OBJECT

public:
    explicit MachineAuthBar(const QString& hostIp, const QString& hostName, QWidget* parent = nullptr);
    ~MachineAuthBar() override = default;

    void loadCredentials(const std::optional<Credential>& cred);
    void showAlert(const QString& message);
    void clearAlert();
    void clearPassword();

signals:
    void authRequested(const QString& username, const QString& password, const QString& keyPath, bool remember);
    void dismissed();

private slots:
    void onTogglePasswordVisibility();
    void onBrowseKeyFile();
    void onSubmit();

private:
    QString m_hostIp;
    QString m_hostName;
    bool m_isPasswordVisible;

    QLabel* m_titleLabel;
    QToolButton* m_btnClose;
    QLabel* m_alertLabel;
    QLineEdit* m_inputUser;
    QLineEdit* m_inputPass;
    QPushButton* m_btnTogglePass;
    QLineEdit* m_inputKey;
    QPushButton* m_btnBrowseKey;
    QCheckBox* m_chkRemember;
    QPushButton* m_btnConnect;

    void setupUi();
};
