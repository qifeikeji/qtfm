#ifndef BUNDLEDICONS_H
#define BUNDLEDICONS_H

#include <QFileInfo>
#include <QIcon>
#include <QString>
#include <QStringList>

/**
 * Built-in file/folder icons from share/icons/mimes/ (SVG or PNG),
 * embedded via share/mimes.qrc and installed to share/qtfm/mimes/.
 */
class BundledIcons {
public:
    static QStringList mimeIconDirectories();
    static QStringList availableIconBaseNames();
    static QString iconFilePath(const QString &baseName);
    static QIcon iconByName(const QString &name);
    static QIcon iconForFileSuffix(const QString &suffix);
    static QIcon iconForMimeType(const QString &mime);
    static QIcon iconForFolder(const QFileInfo &info);
    /** Default sidebar bookmark icon from path (uses share/icons/mimes). */
    static QIcon iconForBookmarkPath(const QString &path);
    static QIcon iconForExecutable();
    /** List view column: folder / image / video / archive / other only. */
    static QIcon iconForListCategory(const QFileInfo &info, const QString &mime = QString());
    /** Guaranteed non-null when empty.svg is available; scales to requested size. */
    static QIcon emptyIcon();
    static QPixmap iconPixmap(const QIcon &icon, int size);
    static QString baseNameForSuffix(const QString &suffix);
    static QString baseNameForMime(const QString &mime);

    /** Toolbar/settings SVGs: use icons/.../white/ when dark UI mode is on. */
    static void setUiDarkMode(bool dark);
    static bool uiDarkMode();
    static QString bundledUiSvgResource(const QString &folder, const QString &baseName);
    static QIcon toolbarIcon(const QString &baseName);
    static QIcon settingsIcon(const QString &baseName);
};

#endif
