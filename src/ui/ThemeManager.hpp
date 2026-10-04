#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

struct ThemeDefinition {
    QString id;
    QString name;
    QString qssContent;
};

class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager* instance();

    const QList<ThemeDefinition>& themes() const { return m_themes; }
    ThemeDefinition currentTheme() const;
    QString currentThemeId() const;

    bool switchToTheme(const QString& themeId);
    ThemeDefinition cycleNextTheme();
    void applyCurrentTheme();

signals:
    void themeChanged(const QString& themeId, const QString& themeName);

private:
    explicit ThemeManager(QObject* parent = nullptr);
    static ThemeManager* s_instance;

    QList<ThemeDefinition> m_themes;
    int m_currentIndex;

    void initializeThemes();
    QString savedThemeId() const;
    void saveThemeId(const QString& id);
};
