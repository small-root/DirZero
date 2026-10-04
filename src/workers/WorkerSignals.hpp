#pragma once

#include <QObject>
#include <QString>
#include <QModelIndex>
#include <QList>
#include "../core/MachineInfo.hpp"
#include "../fs/RemoteEntry.hpp"

class FSNode;

class WorkerSignals : public QObject {
    Q_OBJECT

public:
    explicit WorkerSignals(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void started();
    void finished();
    void error(const QString& errorMessage);
    void progress(qint64 bytesDone, qint64 totalBytes, const QString& itemName, double speedBps);

    // Specialized results
    void discoveryResult(const QList<MachineInfo>& machines);
    void portCheckResult(bool isOpen);
    void connectResult(bool success);
    void listResult(FSNode* parentNode, const QList<RemoteEntry>& entries, const QModelIndex& parentIndex);
    void fileOpResult(const QString& opType, const QString& message);
    void transferResult(bool success);
};
