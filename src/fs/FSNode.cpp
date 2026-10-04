#include "FSNode.hpp"
#include <QDateTime>

FSNode::FSNode(const QString& name,
               const QString& path,
               bool isDir,
               qint64 size,
               qint64 mtime,
               quint32 mode,
               FSNode* parent,
               bool isDummy,
               const QString& error)
    : name(name)
    , path(path)
    , isDir(isDir)
    , size(size)
    , mtime(mtime)
    , mode(mode)
    , parent(parent)
    , isLoaded(false)
    , isLoading(false)
    , isDummy(isDummy)
    , error(error)
{
    // If it's a directory and not a dummy, add a "Loading..." placeholder node
    // so QTreeView shows the expansion arrow for lazy loading.
    if (this->isDir && !this->isDummy) {
        auto placeholder = std::make_unique<FSNode>(
            QStringLiteral("Loading..."),
            QString(),
            false,
            0,
            0,
            0,
            this,
            true
        );
        children.push_back(std::move(placeholder));
    }
}

int FSNode::row() const
{
    if (!parent) {
        return 0;
    }
    for (size_t i = 0; i < parent->children.size(); ++i) {
        if (parent->children[i].get() == this) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

FSNode* FSNode::child(int row) const
{
    if (row >= 0 && static_cast<size_t>(row) < children.size()) {
        return children[static_cast<size_t>(row)].get();
    }
    return nullptr;
}

int FSNode::childCount() const
{
    return static_cast<int>(children.size());
}

void FSNode::appendChild(std::unique_ptr<FSNode> child)
{
    if (child) {
        child->parent = this;
        children.push_back(std::move(child));
    }
}

void FSNode::clearChildren()
{
    children.clear();
}

QString FSNode::sizeFormatted() const
{
    if (isDir || isDummy) {
        return QStringLiteral("-");
    }
    if (size < 1024) {
        return QStringLiteral("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(QString::number(size / 1024.0, 'f', 1));
    } else if (size < 1024LL * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(QString::number(size / (1024.0 * 1024.0), 'f', 1));
    } else {
        return QStringLiteral("%1 GB").arg(QString::number(size / (1024.0 * 1024.0 * 1024.0), 'f', 2));
    }
}

QString FSNode::mtimeFormatted() const
{
    if (isDummy || mtime <= 0) {
        return QStringLiteral("-");
    }
    QDateTime dt = QDateTime::fromSecsSinceEpoch(mtime);
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString FSNode::permissionsFormatted() const
{
    if (isDummy || mode == 0) {
        return QStringLiteral("-");
    }
    QString str = QStringLiteral("----------");
    if (isDir) str[0] = 'd';

    if (mode & 0400) str[1] = 'r';
    if (mode & 0200) str[2] = 'w';
    if (mode & 0100) str[3] = 'x';

    if (mode & 0040) str[4] = 'r';
    if (mode & 0020) str[5] = 'w';
    if (mode & 0010) str[6] = 'x';

    if (mode & 0004) str[7] = 'r';
    if (mode & 0002) str[8] = 'w';
    if (mode & 0001) str[9] = 'x';

    return str;
}
