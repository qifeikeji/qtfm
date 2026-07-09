#ifndef MACFILEACCESS_H
#define MACFILEACCESS_H

#if defined(Q_OS_MAC) || (defined(__APPLE__) && !defined(__IOS__))

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
