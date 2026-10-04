#include "RemoteFSModel.hpp"

RemoteFSModel::RemoteFSModel(FSNode* rootNode, QObject* parent)
    : QAbstractItemModel(parent)
    , m_rootNode(rootNode)
{
    m_headers << QStringLiteral("Name")
              << QStringLiteral("Size")
              << QStringLiteral("Modified")
              << QStringLiteral("Permissions");
}

int RemoteFSModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

int RemoteFSModel::rowCount(const QModelIndex& parent) const
{
    FSNode* parentNode = nullptr;
    if (!parent.isValid()) {
        parentNode = m_rootNode;
    } else {
        parentNode = static_cast<FSNode*>(parent.internalPointer());
    }

    return parentNode ? parentNode->childCount() : 0;
}

QModelIndex RemoteFSModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    FSNode* parentNode = nullptr;
    if (!parent.isValid()) {
        parentNode = m_rootNode;
    } else {
        parentNode = static_cast<FSNode*>(parent.internalPointer());
    }

    if (!parentNode) {
        return QModelIndex();
    }

    FSNode* childNode = parentNode->child(row);
    if (childNode) {
        return createIndex(row, column, childNode);
    }
    return QModelIndex();
}

QModelIndex RemoteFSModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    FSNode* childNode = static_cast<FSNode*>(index.internalPointer());
    if (!childNode) {
        return QModelIndex();
    }

    FSNode* parentNode = childNode->parent;
    if (!parentNode || parentNode == m_rootNode) {
        return QModelIndex();
    }

    return createIndex(parentNode->row(), 0, parentNode);
}

QVariant RemoteFSModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    FSNode* node = static_cast<FSNode*>(index.internalPointer());
    if (!node) {
        return QVariant();
    }

    const int col = index.column();

    if (role == Qt::DisplayRole) {
        switch (col) {
            case ColName: {
                if (!node->error.isEmpty()) {
                    return QStringLiteral("⚠️ %1").arg(node->name);
                } else if (node->isDummy) {
                    return QStringLiteral("⏳ %1").arg(node->name);
                } else if (node->isDir) {
                    return QStringLiteral("📁 %1").arg(node->name);
                } else {
                    return QStringLiteral("📄 %1").arg(node->name);
                }
            }
            case ColSize:
                return node->sizeFormatted();
            case ColModified:
                return node->mtimeFormatted();
            case ColPermissions:
                return node->permissionsFormatted();
            default:
                break;
        }
    } else if (role == Qt::ToolTipRole) {
        if (!node->error.isEmpty()) {
            return QStringLiteral("Error: %1").arg(node->error);
        }
        return QStringLiteral("Path: %1\nSize: %2\nModified: %3\nPermissions: %4")
            .arg(node->path)
            .arg(node->sizeFormatted())
            .arg(node->mtimeFormatted())
            .arg(node->permissionsFormatted());
    }

    return QVariant();
}

QVariant RemoteFSModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section >= 0 && section < m_headers.size()) {
            return m_headers.at(section);
        }
    }
    return QVariant();
}

FSNode* RemoteFSModel::nodeFromIndex(const QModelIndex& index) const
{
    if (index.isValid()) {
        return static_cast<FSNode*>(index.internalPointer());
    }
    return m_rootNode;
}

QModelIndex RemoteFSModel::indexFromNode(FSNode* node, int column) const
{
    if (!node || node == m_rootNode) {
        return QModelIndex();
    }
    return createIndex(node->row(), column, node);
}

void RemoteFSModel::populateNode(FSNode* parentNode, const QList<RemoteEntry>& entries, const QModelIndex& parentIndex)
{
    if (!parentNode) {
        return;
    }

    // 1. Remove existing children
    if (parentNode->childCount() > 0) {
        beginRemoveRows(parentIndex, 0, parentNode->childCount() - 1);
        parentNode->clearChildren();
        endRemoveRows();
    }

    // 2. Insert new children
    if (!entries.isEmpty()) {
        beginInsertRows(parentIndex, 0, entries.size() - 1);
        for (const RemoteEntry& entry : entries) {
            auto child = std::make_unique<FSNode>(
                entry.name,
                entry.path,
                entry.isDirectory,
                entry.size,
                entry.mtime,
                entry.mode,
                parentNode
            );
            parentNode->appendChild(std::move(child));
        }
        endInsertRows();
    }

    parentNode->isLoaded = true;
    parentNode->isLoading = false;
    parentNode->error.clear();
}

void RemoteFSModel::setNodeError(FSNode* parentNode, const QString& errorMsg, const QModelIndex& parentIndex)
{
    if (!parentNode) {
        return;
    }

    if (parentNode->childCount() > 0) {
        beginRemoveRows(parentIndex, 0, parentNode->childCount() - 1);
        parentNode->clearChildren();
        endRemoveRows();
    }

    beginInsertRows(parentIndex, 0, 0);
    auto errChild = std::make_unique<FSNode>(
        QStringLiteral("[%1]").arg(errorMsg),
        parentNode->path,
        false,
        0,
        0,
        0,
        parentNode,
        false,
        errorMsg
    );
    parentNode->appendChild(std::move(errChild));
    endInsertRows();

    parentNode->isLoaded = true;
    parentNode->isLoading = false;
    parentNode->error = errorMsg;
}
