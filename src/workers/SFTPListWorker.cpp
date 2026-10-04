#include "SFTPListWorker.hpp"
#include "../fs/SFTPManager.hpp"
#include "../fs/FSNode.hpp"

SFTPListWorker::SFTPListWorker(SFTPManager* manager,
                               const QString& remotePath,
                               FSNode* parentNode,
                               const QModelIndex& parentIndex)
    : m_manager(manager)
    , m_remotePath(remotePath)
    , m_parentNode(parentNode)
    , m_parentIndex(parentIndex)
{
    setAutoDelete(true);
}

void SFTPListWorker::run()
{
    emit workerSignals.started();
    QString error;
    QList<RemoteEntry> entries = m_manager->listDirectory(m_remotePath, error);
    if (error.isEmpty()) {
        emit workerSignals.listResult(m_parentNode, entries, m_parentIndex);
    } else {
        emit workerSignals.error(QStringLiteral("%1|%2").arg(m_remotePath, error));
    }
    emit workerSignals.finished();
}
