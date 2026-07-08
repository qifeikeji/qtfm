/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#ifndef UDISKS2_H
#define UDISKS2_H

#include <QStringList>

#define DBUS_SERVICE "org.freedesktop.UDisks2"
#define DBUS_PATH "/org/freedesktop/UDisks2"
#define DBUS_OBJMANAGER "org.freedesktop.DBus.ObjectManager"
#define DBUS_PROPERTIES "org.freedesktop.DBus.Properties"
#define DBUS_INTROSPECTABLE "org.freedesktop.DBus.Introspectable"
#define DBUS_DEVICE_ADDED "InterfacesAdded"
#define DBUS_DEVICE_REMOVED "InterfacesRemoved"

class uDisks2
{
public:
    static const QString getDrivePath(QString path);
    static bool hasPartition(QString path);
    static const QString getFileSystem(QString path);
    static bool isRemovable(QString path);
    static bool isOptical(QString path);
    static bool hasMedia(QString path);
    static bool hasOpticalMedia(QString path);
    static bool canEject(QString path);
    static bool opticalMediaIsBlank(QString path);
    static int opticalDataTracks(QString path);
    static int opticalAudioTracks(QString path);
    static const QString getMountPointOptical(QString path);
    static const QString getMountPoint(QString path);
    static const QString getDeviceName(QString path);
    static const QString getDeviceLabel(QString path);
    static const QString mountDevice(QString path);
    static const QString mountOptical(QString path);
    static const QString unmountDevice(QString path);
    static const QString unmountOptical(QString path);
    static const QString ejectDevice(QString path);
    static const QStringList getDevices();

    /** Block node name only (e.g. sda, nvme0n1), not sda1 / nvme0n1p1. */
    static QString blockDeviceName(const QString &blockObjectPath);
    /** loop*, snap*, zram*, etc. — hide from the external disks panel. */
    static bool isIgnoredBlockDevice(const QString &blockDeviceName);
    /** True when this UDisks block object is a partition (sda1, nvme0n1p1, …). */
    static bool isPartitionBlock(const QString &blockObjectPath);
    /** User/data mounts (e.g. /media, /run/media), not system root. */
    static bool isExternalUserMountPoint(const QString &mountpoint);
};

#endif // UDISKS2_H
