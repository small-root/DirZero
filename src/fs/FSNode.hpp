#pragma once

#include <QString>
#include <vector>
#include <memory>
#include "RemoteEntry.hpp"

class FSNode {
public:
    FSNode(const QString& name,
           const QString& path,
           bool isDir = false,
           qint64 size = 0,
           qint64 mtime = 0,
           quint32 mode = 0,
           FSNode* parent = nullptr,
           bool isDummy = false,
           const QString& error = QString());

    ~FSNode() = default;

    // Non-copyable, movable
    FSNode(const FSNode&) = delete;
    FSNode& operator=(const FSNode&) = delete;
    FSNode(FSNode&&) = default;
    FSNode& operator=(FSNode&&) = default;

    QString name;
    QString path;
    bool isDir;
    qint64 size;
    qint64 mtime;
    quint32 mode;
    FSNode* parent;
    std::vector<std::unique_ptr<FSNode>> children;
    bool isLoaded;
    bool isLoading;
    bool isDummy;
    QString error;

    int row() const;
    FSNode* child(int row) const;
    int childCount() const;

    void appendChild(std::unique_ptr<FSNode> child);
    void clearChildren();

    QString sizeFormatted() const;
    QString mtimeFormatted() const;
    QString permissionsFormatted() const;
};
