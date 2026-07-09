/*
 * This file is part of QtFM <https://qtfm.eu>
 *
 * Available under the GNU General Public License, version 2 or later.
 */

#include "disksmodel.h"
#include "bundledicons.h"
#include "common.h"

#include <QIcon>
#include <algorithm>

disksModel::disksModel(QObject *parent) : QAbstractListModel(parent)
{
}

int disksModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) { return 0; }
    return disksData.count();
}

QVariant disksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= disksData.count()) {
        return QVariant();
    }
    const DiskEntry &d = disksData.at(index.row());
    if (d.isSeparator) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::DecorationRole:
        case DISK_DEVICE_PATH:
        case DISK_MOUNTPOINT:
            return QVariant();
        case DISK_IS_SEPARATOR:
            return true;
        default:
            return QVariant();
        }
    }
    switch (role) {
    case Qt::DisplayRole:
        return d.name;
    case Qt::DecorationRole:
        return BundledIcons::iconByName(QStringLiteral("drive-harddisk"));
    case Qt::ToolTipRole:
        return d.mountpoint.isEmpty() ? d.name : d.mountpoint;
    case DISK_DEVICE_PATH:
        return d.devicePath;
    case DISK_MOUNTPOINT:
        return d.mountpoint;
    case DISK_ICON_NAME:
        return d.iconName;
    case DISK_IS_OPTICAL:
        return d.isOptical;
    case DISK_USED_BYTES:
        return d.usedBytes;
    case DISK_TOTAL_BYTES:
        return d.totalBytes;
    case DISK_IS_SEPARATOR:
        return false;
    default:
        return QVariant();
    }
}

int disksModel::indexOfDevice(const QString &devicePath) const
{
    for (int i = 0; i < disksData.count(); ++i) {
        if (disksData.at(i).devicePath == devicePath) { return i; }
    }
    return -1;
}

bool disksModel::contains(const QString &devicePath) const
{
    return indexOfDevice(devicePath) >= 0;
}

QStringList disksModel::allDevicePaths() const
{
    QStringList paths;
    for (const DiskEntry &d : disksData) {
        if (!d.isSeparator && !d.devicePath.isEmpty()) {
            paths << d.devicePath;
        }
    }
    return paths;
}

void disksModel::upsertDisk(const QString &devicePath, const QString &name,
                            const QString &mountpoint, const QString &iconName,
                            bool isOptical)
{
    const int row = indexOfDevice(devicePath);
    if (row >= 0) {
        DiskEntry &d = disksData[row];
        d.name = name;
        d.mountpoint = mountpoint;
        d.iconName = iconName;
        d.isOptical = isOptical;
        emit dataChanged(index(row), index(row));
        return;
    }

    DiskEntry d;
    d.devicePath = devicePath;
    d.name = name;
    d.mountpoint = mountpoint;
    d.iconName = iconName;
    d.isOptical = isOptical;

    beginInsertRows(QModelIndex(), disksData.count(), disksData.count());
    disksData.append(d);
    endInsertRows();
}

void disksModel::removeDisk(const QString &devicePath)
{
    const int row = indexOfDevice(devicePath);
    if (row < 0) { return; }
    beginRemoveRows(QModelIndex(), row, row);
    disksData.remove(row);
    endRemoveRows();
}

void disksModel::setMountpoint(const QString &devicePath, const QString &mountpoint)
{
    const int row = indexOfDevice(devicePath);
    if (row < 0) { return; }
    disksData[row].mountpoint = mountpoint;
    emit dataChanged(index(row), index(row));
}

void disksModel::refreshUsage()
{
    if (disksData.isEmpty()) { return; }
    for (int i = 0; i < disksData.count(); ++i) {
        DiskEntry &d = disksData[i];
        if (d.isSeparator) {
            continue;
        }
        if (d.mountpoint.isEmpty()) {
            d.usedBytes = 0;
            d.totalBytes = 0;
            continue;
        }
        qint64 used = 0;
        qint64 total = 0;
        if (Common::getDriveUsage(d.mountpoint, &used, &total)) {
            d.usedBytes = used;
            d.totalBytes = total;
        }
    }
    emit dataChanged(index(0), index(disksData.count() - 1));
}

void disksModel::setRows(const QVector<DiskListRow> &rows)
{
    beginResetModel();
    disksData.clear();
    for (const DiskListRow &r : rows) {
        DiskEntry d;
        d.isSeparator = r.separator;
        d.devicePath = r.devicePath;
        d.name = r.name;
        d.mountpoint = r.mountpoint;
        d.iconName = r.iconName;
        d.isOptical = r.isOptical;
        disksData.append(d);
    }
    endResetModel();
}
