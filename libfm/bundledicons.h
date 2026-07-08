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
    static QIcon iconForExecutable();
    static QString baseNameForSuffix(const QString &suffix);
    static QString baseNameForMime(const QString &mime);
};

#endif
