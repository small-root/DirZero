#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include "ui/Dir2ZeroWindow.hpp"
#include "ui/ThemeManager.hpp"
#include "security/CredentialStore.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DirZero"));
    app.setApplicationDisplayName(QStringLiteral("DirZero — Tailscale Remote File Manager"));
    app.setOrganizationName(QStringLiteral("DirZero"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("DirZero - Cross-Platform Tailscale Remote File Manager (C++ Edition)"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption userOption(
        QStringList() << QStringLiteral("u") << QStringLiteral("user"),
        QStringLiteral("SSH username for remote authentication (defaults to current user)."),
        QStringLiteral("username"),
        QDir::home().dirName()
    );
    parser.addOption(userOption);

    QCommandLineOption keyOption(
        QStringList() << QStringLiteral("k") << QStringLiteral("key"),
        QStringLiteral("Path to private key file (defaults to ~/.ssh/id_ed25519 or first found key)."),
        QStringLiteral("keypath"),
        CredentialStore::getFirstAvailableSshKey()
    );
    parser.addOption(keyOption);

    parser.process(app);

    QString user = parser.value(userOption);
    QString key = parser.value(keyOption);

    // Apply the active glass theme
    ThemeManager::instance()->applyCurrentTheme();

    Dir2ZeroWindow window(user, key);
    window.show();

    return app.exec();
}