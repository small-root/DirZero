#pragma once

#include <QString>
#include <QDateTime>

struct RemoteEntry {
    QString name;
    QString path;
    bool isDirectory = false;
    qint64 size = 0;
    qint64 mtime = 0;
    quint32 mode = 0;
    bool hidden = false;

    QString sizeFormatted() const;
    QString mtimeFormatted() const;
    QString permissionsFormatted() const;
};

inline QString RemoteEntry::sizeFormatted() const {
    if (isDirectory) {
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

inline QString RemoteEntry::mtimeFormatted() const {
    if (mtime <= 0) {
        return QStringLiteral("-");
    }
    QDateTime dt = QDateTime::fromSecsSinceEpoch(mtime);
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

inline QString RemoteEntry::permissionsFormatted() const {
    if (mode == 0) {
        return QStringLiteral("-");
    }
    QString str = QStringLiteral("----------");
    if (isDirectory) str[0] = 'd';

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
