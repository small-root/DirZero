#include "RemoteClipboard.hpp"
#include "../fs/FSNode.hpp"

RemoteClipboard* RemoteClipboard::s_instance = nullptr;

RemoteClipboard::RemoteClipboard(QObject* parent)
    : QObject(parent)
    , m_sourcePanel(nullptr)
    , m_isDir(false)
    , m_isCut(false)
{
}

RemoteClipboard* RemoteClipboard::instance()
{
    if (!s_instance) {
        s_instance = new RemoteClipboard();
    }
    return s_instance;
}

void RemoteClipboard::copy(MachinePanelWidget* panel, FSNode* node)
{
    if (!node) return;
    m_sourcePanel = panel;
    m_sourcePath = node->path;
    m_sourceName = node->name;
    m_isDir = node->isDir;
    m_isCut = false;
    emit clipboardChanged();
}

void RemoteClipboard::cut(MachinePanelWidget* panel, FSNode* node)
{
    if (!node) return;
    m_sourcePanel = panel;
    m_sourcePath = node->path;
    m_sourceName = node->name;
    m_isDir = node->isDir;
    m_isCut = true;
    emit clipboardChanged();
}

void RemoteClipboard::clear()
{
    m_sourcePanel = nullptr;
    m_sourcePath.clear();
    m_sourceName.clear();
    m_isDir = false;
    m_isCut = false;
    emit clipboardChanged();
}

bool RemoteClipboard::hasItem() const
{
    return (m_sourcePanel != nullptr && !m_sourcePath.isEmpty() && !m_sourceName.isEmpty());
}

QString RemoteClipboard::summary() const
{
    if (!hasItem()) {
        return QStringLiteral("Clipboard is empty");
    }
    QString action = m_isCut ? QStringLiteral("Cut") : QStringLiteral("Copy");
    return QStringLiteral("%1 '%2' ready to paste.").arg(action, m_sourceName);
}
