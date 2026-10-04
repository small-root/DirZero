#include "SFTPFileOpWorker.hpp"
#include "../fs/SFTPManager.hpp"

SFTPFileOpWorker::SFTPFileOpWorker(const QString& opType,
                                   SFTPManager* manager,
                                   const QString& path1,
                                   const QString& path2)
    : m_opType(opType)
    , m_manager(manager)
    , m_path1(path1)
    , m_path2(path2)
{
    setAutoDelete(true);
}

void SFTPFileOpWorker::run()
{
    emit workerSignals.started();
    QString error;
    bool success = false;

    if (m_opType == QStringLiteral("create_file")) {
        success = m_manager->createFile(m_path1, error);
    } else if (m_opType == QStringLiteral("create_dir")) {
        success = m_manager->createDirectory(m_path1, error);
    } else if (m_opType == QStringLiteral("delete_file")) {
        success = m_manager->deleteFile(m_path1, error);
    } else if (m_opType == QStringLiteral("delete_dir")) {
        success = m_manager->deleteDirectoryRecursive(m_path1, error);
    } else if (m_opType == QStringLiteral("rename")) {
        success = m_manager->renamePath(m_path1, m_path2, error);
    }

    if (success) {
        emit workerSignals.fileOpResult(m_opType, QStringLiteral("Operation successful."));
    } else {
        emit workerSignals.error(error.isEmpty() ? QStringLiteral("Operation failed.") : error);
    }
    emit workerSignals.finished();
}
