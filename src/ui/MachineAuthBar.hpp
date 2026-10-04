#pragma once

#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>

class MachineAuthBar : public QFrame {
    Q_OBJECT

public:
    explicit MachineAuthBar(QWidget* parent = nullptr);
    ~MachineAuthBar() override = default;

    void setDefaultKeyPath(const QString& keyPath);
    void showAlert(const QString& message);
    void clearAlert();
    void clearPassword();

signals:
    void authRequested(const QString& username, const QString& password, const QString& keyPath);
    void dismissed();

private slots:
    void onTogglePasswordVisibility();
    void onBrowseKeyFile();
    void onSubmit();

private:
    bool m_isPasswordVisible;

    QLabel* m_titleLabel;
    QToolButton* m_btnClose;
    QLabel* m_alertLabel;
    QLineEdit* m_inputUser;
    QLineEdit* m_inputPass;
    QPushButton* m_btnTogglePass;
    QLineEdit* m_inputKey;
    QPushButton* m_btnBrowseKey;
    QPushButton* m_btnConnect;

    void setupUi();
};
