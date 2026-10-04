#pragma once

#include <QObject>
#include <QString>

class MachinePanelWidget;
class FSNode;

class RemoteClipboard : public QObject {
    Q_OBJECT

public:
    static RemoteClipboard* instance();

    void copy(MachinePanelWidget* panel, FSNode* node);
    void cut(MachinePanelWidget* panel, FSNode* node);
    void clear();

    bool hasItem() const;
    QString summary() const;

    MachinePanelWidget* sourcePanel() const { return m_sourcePanel; }
    QString sourcePath() const { return m_sourcePath; }
    QString sourceName() const { return m_sourceName; }
    bool isDirectory() const { return m_isDir; }
    bool isCut() const { return m_isCut; }

signals:
    void clipboardChanged();

private:
    explicit RemoteClipboard(QObject* parent = nullptr);
    static RemoteClipboard* s_instance;

    MachinePanelWidget* m_sourcePanel;
    QString m_sourcePath;
    QString m_sourceName;
    bool m_isDir;
    bool m_isCut;
};
