#pragma once

#include <QAbstractItemModel>
#include <QStringList>
#include <QList>
#include <memory>
#include "FSNode.hpp"
#include "RemoteEntry.hpp"

class RemoteFSModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column {
        ColName = 0,
        ColSize = 1,
        ColModified = 2,
        ColPermissions = 3,
        ColumnCount = 4
    };

    explicit RemoteFSModel(FSNode* rootNode, QObject* parent = nullptr);
    ~RemoteFSModel() override = default;

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    FSNode* nodeFromIndex(const QModelIndex& index) const;
    QModelIndex indexFromNode(FSNode* node, int column = 0) const;

    void populateNode(FSNode* parentNode, const QList<RemoteEntry>& entries, const QModelIndex& parentIndex = QModelIndex());
    void setNodeError(FSNode* parentNode, const QString& errorMsg, const QModelIndex& parentIndex = QModelIndex());

private:
    FSNode* m_rootNode;
    QStringList m_headers;
};
