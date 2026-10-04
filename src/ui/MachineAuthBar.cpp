#include "MachineAuthBar.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDir>

MachineAuthBar::MachineAuthBar(const QString& hostIp, const QString& hostName, QWidget* parent)
    : QFrame(parent)
    , m_hostIp(hostIp)
    , m_hostName(hostName)
    , m_isPasswordVisible(false)
{
    setupUi();
}

void MachineAuthBar::setupUi()
{
    setObjectName(QStringLiteral("authFrame"));
    setFrameShape(QFrame::StyledPanel);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Header with Title & Dismiss Button
    auto headerBox = new QHBoxLayout();
    m_titleLabel = new QLabel(QStringLiteral("🔑 SSH Authentication"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("color: #38bdf8; font-weight: 700; font-size: 11px;"));
    headerBox->addWidget(m_titleLabel);
    headerBox->addStretch();

    m_btnClose = new QToolButton(this);
    m_btnClose->setText(QStringLiteral("✕"));
    m_btnClose->setStyleSheet(QStringLiteral("QToolButton { background: transparent; color: #94a3b8; border: none; font-weight: 700; } QToolButton:hover { color: #ef4444; }"));
    connect(m_btnClose, &QToolButton::clicked, this, &MachineAuthBar::dismissed);
    headerBox->addWidget(m_btnClose);
    mainLayout->addLayout(headerBox);

    // Alert Banner
    m_alertLabel = new QLabel(this);
    m_alertLabel->setStyleSheet(QStringLiteral("color: #f87171; font-size: 10px; font-weight: 600;"));
    m_alertLabel->setWordWrap(true);
    m_alertLabel->setVisible(false);
    mainLayout->addWidget(m_alertLabel);

    // Row 1: Username & Password
    auto row1 = new QHBoxLayout();
    row1->setSpacing(6);

    m_inputUser = new QLineEdit(this);
    m_inputUser->setFixedHeight(30);
    m_inputUser->setPlaceholderText(QStringLiteral("Username"));
    m_inputUser->setText(QDir::home().dirName());
    row1->addWidget(m_inputUser, 1);

    auto passBox = new QHBoxLayout();
    passBox->setSpacing(0);

    m_inputPass = new QLineEdit(this);
    m_inputPass->setFixedHeight(30);
    m_inputPass->setPlaceholderText(QStringLiteral("Password"));
    m_inputPass->setEchoMode(QLineEdit::Password);
    connect(m_inputPass, &QLineEdit::returnPressed, this, &MachineAuthBar::onSubmit);
    passBox->addWidget(m_inputPass);

    m_btnTogglePass = new QPushButton(QStringLiteral("👁"), this);
    m_btnTogglePass->setFixedSize(30, 30);
    m_btnTogglePass->setStyleSheet(QStringLiteral("QPushButton { border-left: none; border-top-right-radius: 6px; border-bottom-right-radius: 6px; font-size: 13px; }"));
    connect(m_btnTogglePass, &QPushButton::clicked, this, &MachineAuthBar::onTogglePasswordVisibility);
    passBox->addWidget(m_btnTogglePass);

    row1->addLayout(passBox, 1);
    mainLayout->addLayout(row1);

    // Row 2: Private Key Path & Browse Button
    auto row2 = new QHBoxLayout();
    row2->setSpacing(6);

    m_inputKey = new QLineEdit(this);
    m_inputKey->setFixedHeight(30);
    m_inputKey->setPlaceholderText(QStringLiteral("Private Key (e.g. ~/.ssh/id_ed25519)"));
    row2->addWidget(m_inputKey, 1);

    m_btnBrowseKey = new QPushButton(QStringLiteral("📂 Browse..."), this);
    m_btnBrowseKey->setFixedHeight(30);
    m_btnBrowseKey->setMinimumWidth(90);
    connect(m_btnBrowseKey, &QPushButton::clicked, this, &MachineAuthBar::onBrowseKeyFile);
    row2->addWidget(m_btnBrowseKey);

    mainLayout->addLayout(row2);

    // Row 3: Remember checkbox & Connect Button
    auto row3 = new QHBoxLayout();
    row3->setSpacing(8);

    m_chkRemember = new QCheckBox(QStringLiteral("Save credentials"), this);
    m_chkRemember->setChecked(true);
    row3->addWidget(m_chkRemember);

    row3->addStretch();

    m_btnConnect = new QPushButton(QStringLiteral("🔑 Connect"), this);
    m_btnConnect->setFixedHeight(30);
    m_btnConnect->setMinimumWidth(100);
    m_btnConnect->setStyleSheet(QStringLiteral("QPushButton { background: #2563eb; color: #ffffff; border: none; font-weight: 700; font-size: 12px; padding: 4px 16px; border-radius: 6px; } QPushButton:hover { background: #1d4ed8; }"));
    connect(m_btnConnect, &QPushButton::clicked, this, &MachineAuthBar::onSubmit);
    row3->addWidget(m_btnConnect);

    mainLayout->addLayout(row3);
}

void MachineAuthBar::loadCredentials(const std::optional<Credential>& cred)
{
    if (cred.has_value()) {
        if (!cred->username.isEmpty()) m_inputUser->setText(cred->username);
        if (!cred->password.isEmpty()) m_inputPass->setText(cred->password);
        if (!cred->keyPath.isEmpty()) m_inputKey->setText(cred->keyPath);
    }
}

void MachineAuthBar::showAlert(const QString& message)
{
    m_alertLabel->setText(QStringLiteral("⚠️ %1").arg(message));
    m_alertLabel->setVisible(true);
    setVisible(true);
}

void MachineAuthBar::clearAlert()
{
    m_alertLabel->setVisible(false);
}

void MachineAuthBar::clearPassword()
{
    m_inputPass->clear();
}

void MachineAuthBar::onTogglePasswordVisibility()
{
    m_isPasswordVisible = !m_isPasswordVisible;
    if (m_isPasswordVisible) {
        m_inputPass->setEchoMode(QLineEdit::Normal);
        m_btnTogglePass->setText(QStringLiteral("🔒"));
    } else {
        m_inputPass->setEchoMode(QLineEdit::Password);
        m_btnTogglePass->setText(QStringLiteral("👁"));
    }
}

void MachineAuthBar::onBrowseKeyFile()
{
    QString homeSsh = QDir::homePath() + QStringLiteral("/.ssh");
    QString selected = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select SSH Private Key File"),
        homeSsh,
        QStringLiteral("Key Files (*id_* *.pem *.key);;All Files (*)")
    );
    if (!selected.isEmpty()) {
        m_inputKey->setText(selected);
    }
}

void MachineAuthBar::onSubmit()
{
    QString user = m_inputUser->text().trimmed();
    if (user.isEmpty()) {
        user = QDir::home().dirName();
    }
    QString pass = m_inputPass->text();
    QString key = m_inputKey->text().trimmed();
    bool remember = m_chkRemember->isChecked();

    emit authRequested(user, pass, key, remember);
}
