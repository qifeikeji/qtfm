#ifndef MACFILEACCESS_H
#define MACFILEACCESS_H

#ifdef Q_OS_MAC

namespace MacFileAccess {

/** Opens System Settings → Privacy → Full Disk Access. */
void openFullDiskAccessSettings();

/** Opens System Settings → Privacy → Files and Folders. */
void openFilesAndFoldersSettings();

/** Match NSApp appearance to QtFM light/dark UI (decoupled from system auto). */
void setApplicationAppearance(bool darkUi);

} // namespace MacFileAccess

#endif

#endif
