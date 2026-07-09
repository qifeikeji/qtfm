#ifndef MACDISKS_H
#define MACDISKS_H

#if defined(Q_OS_MAC) || (defined(__APPLE__) && !defined(__IOS__))

#include <QString>
#include <QVector>

struct MacDiskVolume {
    /** diskutil DeviceIdentifier, e.g. disk3s1 — unique row key. */
    QString deviceIdentifier;
    QString displayTitle;
    QString mountPoint;
    bool isOptical = false;
    /** Whole disk to eject (disk3); may equal deviceIdentifier. */
    QString wholeDiskIdentifier;
    /** Top-level AllDisksAndPartitions entry (disk1, disk2) for sidebar grouping. */
    QString physicalDiskGroup;
};

namespace MacDisks {

QVector<MacDiskVolume> listVolumes();

bool mountVolume(const QString &deviceIdentifier);
bool unmountVolume(const QString &deviceIdentifier);
bool ejectWholeDisk(const QString &wholeDiskIdentifier);
QString lastErrorMessage();

} // namespace MacDisks

#endif

#endif
