/*
 * This file is part of QtFM <https://qtfm.eu>
 *
 * Available under the GNU General Public License, version 2 or later.
 */

#ifndef DISKSMODEL_H
#define DISKSMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

// Custom data roles exposed by disksModel (only meaningful on this model).
#define DISK_DEVICE_PATH  Qt::UserRole+1  // UDisks object path, used as unique key
#define DISK_MOUNTPOINT   Qt::UserRole+2  // filesystem mountpoint, empty if unmounted
#define DISK_ICON_NAME    Qt::UserRole+3
#define DISK_IS_OPTICAL   Qt::UserRole+4
#define DISK_USED_BYTES   Qt::UserRole+5
#define DISK_TOTAL_BYTES  Qt::UserRole+6
#define DISK_IS_SEPARATOR Qt::UserRole+7

struct DiskListRow {
    bool separator = false;
    QString devicePath;
    QString name;
    QString mountpoint;
    QString iconName;
    bool isOptical = false;
};

/**
 * @class disksModel
 * @brief Flat list model for removable/external disks shown in the sidebar,
 *        separate from user bookmarks. Each row tracks identity (UDisks
 *        object path), display info and (once mounted) usage statistics.
 */
class disksModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit disksModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** Adds a new disk row, or updates an existing one matched by devicePath. */
    void upsertDisk(const QString &devicePath,
                    const QString &name,
                    const QString &mountpoint,
                    const QString &iconName,
                    bool isOptical);
    void removeDisk(const QString &devicePath);
    void setMountpoint(const QString &devicePath, const QString &mountpoint);
    bool contains(const QString &devicePath) const;
    QStringList allDevicePaths() const;
    /** Recomputes used/total bytes for every mounted disk (statfs). */
    void refreshUsage();
    /** Replaces all rows (disk volumes and optional group separators). */
    void setRows(const QVector<DiskListRow> &rows);

private:
    struct DiskEntry {
        bool isSeparator = false;
        QString devicePath;
        QString name;
        QString mountpoint;
        QString iconName;
        bool isOptical = false;
        qint64 usedBytes = 0;
        qint64 totalBytes = 0;
    };

    int indexOfDevice(const QString &devicePath) const;

    QVector<DiskEntry> disksData;
};

#endif // DISKSMODEL_H
