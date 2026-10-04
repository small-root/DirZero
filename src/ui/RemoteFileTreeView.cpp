#include "RemoteFileTreeView.hpp"
#include "MachinePanelWidget.hpp"
#include "../fs/FSNode.hpp"
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>

RemoteFileTreeView::RemoteFileTreeView(MachinePanelWidget* panel, QWidget* parent)
    : QTreeView(parent)
    , m_panel(panel)
{
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::CopyAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
}

void RemoteFileTreeView::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    if (!m_panel) return;

    FSNode* node = m_panel->selectedNode();
    if (!node || node->isDummy || !m_panel->isConnected()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("source_ip"), m_panel->machineInfo().ip());
    payload.insert(QStringLiteral("source_host"), m_panel->machineInfo().name());
    payload.insert(QStringLiteral("source_path"), node->path);
    payload.insert(QStringLiteral("source_name"), node->name);
    payload.insert(QStringLiteral("is_dir"), node->isDir);

    QJsonDocument doc(payload);
    auto mimeData = new QMimeData();
    mimeData->setData(QStringLiteral("application/x-dirzero-item"), doc.toJson(QJsonDocument::Compact));
    mimeData->setText(QStringLiteral("%1:%2").arg(m_panel->machineInfo().name(), node->path));

    auto drag = new QDrag(this);
    drag->setMimeData(mimeData);

    // Create glassmorphic visual drag preview badge
    QPixmap pixmap(200, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Glass pill background
    painter.setBrush(QColor(23, 37, 84, 220)); // Deep sapphire
    painter.setPen(QColor(56, 189, 248, 200)); // Glowing cyan border
    painter.drawRoundedRect(1, 1, 198, 30, 6, 6);

    painter.setPen(QColor(248, 250, 252));
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);

    QString icon = node->isDir ? QStringLiteral("📁") : QStringLiteral("📄");
    QString displayText = QStringLiteral("%1 %2").arg(icon, node->name);
    if (displayText.length() > 22) {
        displayText = displayText.left(20) + QStringLiteral("…");
    }
    painter.drawText(10, 21, displayText);
    painter.end();

    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(15, 16));

    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void RemoteFileTreeView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-dirzero-item")) &&
        m_panel && m_panel->isConnected())
    {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void RemoteFileTreeView::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-dirzero-item")) &&
        m_panel && m_panel->isConnected())
    {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void RemoteFileTreeView::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasFormat(QStringLiteral("application/x-dirzero-item")) ||
        !m_panel || !m_panel->isConnected())
    {
        event->ignore();
        return;
    }

    QByteArray rawData = event->mimeData()->data(QStringLiteral("application/x-dirzero-item"));
    QJsonDocument doc = QJsonDocument::fromJson(rawData);
    if (!doc.isObject()) {
        event->ignore();
        return;
    }

    QJsonObject obj = doc.object();
    QString srcIp = obj.value(QStringLiteral("source_ip")).toString();
    QString srcPath = obj.value(QStringLiteral("source_path")).toString();
    QString srcName = obj.value(QStringLiteral("source_name")).toString();
    bool isDir = obj.value(QStringLiteral("is_dir")).toBool(false);

    if (srcPath.isEmpty() || srcName.isEmpty()) {
        event->ignore();
        return;
    }

    QPoint pos = event->position().toPoint();
    QModelIndex dropIdx = indexAt(pos);
    QString destDir = QStringLiteral(".");

    if (dropIdx.isValid()) {
        auto targetNode = static_cast<FSNode*>(dropIdx.internalPointer());
        if (targetNode && targetNode->isDir && !targetNode->isDummy) {
            destDir = targetNode->path;
        } else if (targetNode && targetNode->parent && targetNode->parent != m_panel->rootNode()) {
            destDir = targetNode->parent->path;
        }
    }

    bool isCut = (event->dropAction() == Qt::MoveAction) || (event->modifiers() & Qt::ShiftModifier);

    m_panel->handleDropTransfer(srcIp, srcPath, srcName, isDir, destDir, isCut);
    event->acceptProposedAction();
}

void RemoteFileTreeView::keyPressEvent(QKeyEvent* event)
{
    if (!m_panel) {
        QTreeView::keyPressEvent(event);
        return;
    }

    // Copy: Ctrl+C
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        m_panel->copySelected();
        event->accept();
        return;
    }

    // Cut: Ctrl+X
    if (event->key() == Qt::Key_X && (event->modifiers() & Qt::ControlModifier)) {
        m_panel->cutSelected();
        event->accept();
        return;
    }

    // Paste: Ctrl+V
    if (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier)) {
        m_panel->pasteClipboard();
        event->accept();
        return;
    }

    // Delete: Delete or Backspace
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        m_panel->deleteSelected();
        event->accept();
        return;
    }

    // Rename: F2
    if (event->key() == Qt::Key_F2) {
        m_panel->renameSelected();
        event->accept();
        return;
    }

    // Refresh: F5
    if (event->key() == Qt::Key_F5) {
        m_panel->reloadFilesystem();
        event->accept();
        return;
    }

    QTreeView::keyPressEvent(event);
}
