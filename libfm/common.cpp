/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#include "common.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QIcon>
#include <QDirIterator>
#include <QRegularExpression>
#include <QTextStream>
#include <QMap>
#include <QMapIterator>
#include <QDirIterator>
#include <QSettings>
#include <QPalette>
#include <QVector>
#include <QCryptographicHash>
#include <QUrl>
#include <QStandardPaths>
#include <QPainter>
#include <QBuffer>
#include <QImage>
#include <QSet>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#include <sys/stat.h>
#ifdef __NetBSD__
#include <sys/statvfs.h>
#endif

namespace {

bool themeHasIconFiles(const QString &themePath)
{
    QDirIterator it(themePath,
                     QStringList() << "*.png" << "*.svg" << "*.xpm",
                     QDir::Files,
                     QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileName() == "index.theme") {
            continue;
        }
        return true;
    }
    return false;
}

QStringList parseThemeIndexList(const QSettings &idx, const char *key)
{
    QString raw = idx.value(QString::fromLatin1(key)).toString();
    if (raw.isEmpty()) {
        return QStringList();
    }
    raw.replace(QLatin1Char(';'), QLatin1Char(','));
    QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString &part : parts) {
        part = part.trimmed();
    }
    return parts;
}

bool themeDirectoriesExist(const QString &themePath, const QStringList &directories)
{
    for (const QString &dirName : directories) {
        if (QDir(themePath + QLatin1Char('/') + dirName).exists()) {
            return true;
        }
    }
    return false;
}

QString findThemeOnDisk(const QString &themeName, const QStringList &iconRoots)
{
    if (themeName.isEmpty()) {
        return QString();
    }
    for (const QString &root : iconRoots) {
        QDir rootDir(root);
        if (!rootDir.exists()) {
            continue;
        }
        const QStringList entries = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            if (entry.compare(themeName, Qt::CaseInsensitive) == 0) {
                return entry;
            }
        }
    }
    return QString();
}

bool inheritedThemeProvidesIcons(const QString &inheritName,
                                 const QStringList &iconRoots,
                                 int depth)
{
    if (inheritName.isEmpty() || depth > 6) {
        return false;
    }
    const QString resolved = findThemeOnDisk(inheritName, iconRoots);
    if (resolved.isEmpty()) {
        return false;
    }
    for (const QString &root : iconRoots) {
        const QString path = root + QLatin1Char('/') + resolved;
        if (!QDir(path).exists()) {
            continue;
        }
        if (themeHasIconFiles(path)) {
            return true;
        }
        QSettings idx(path + QStringLiteral("/index.theme"), QSettings::IniFormat);
        idx.beginGroup(QStringLiteral("Icon Theme"));
        const QStringList inherits = parseThemeIndexList(idx, "Inherits");
        idx.endGroup();
        for (const QString &parent : inherits) {
            if (inheritedThemeProvidesIcons(parent, iconRoots, depth + 1)) {
                return true;
            }
        }
    }
    return false;
}

QString resolveUsableThemeName(const QString &themeName, const QStringList &iconRoots)
{
    const QString resolved = findThemeOnDisk(themeName, iconRoots);
    if (resolved.isEmpty()) {
        return QString();
    }
    for (const QString &root : iconRoots) {
        const QString path = root + QLatin1Char('/') + resolved;
        if (QDir(path).exists() && Common::isValidIconTheme(path)) {
            return resolved;
        }
    }
    return QString();
}

QString findMacIconsTheme(const QStringList &iconRoots)
{
    static const QStringList preferred = {
        QStringLiteral("MacIcons"),
        QStringLiteral("macicons"),
        QStringLiteral("MacOS"),
        QStringLiteral("McMunki-macOS"),
    };
    for (const QString &candidate : preferred) {
        for (const QString &root : iconRoots) {
            const QString resolved = findThemeOnDisk(candidate, QStringList() << root);
            if (resolved.isEmpty()) {
                continue;
            }
            const QString path = root + QLatin1Char('/') + resolved;
            if (Common::isValidIconTheme(path)) {
                return resolved;
            }
        }
    }
    for (const QString &root : iconRoots) {
        QDir rootDir(root);
        if (!rootDir.exists()) {
            continue;
        }
        const QStringList entries = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            if (entry.contains(QStringLiteral("macicon"), Qt::CaseInsensitive)
                || entry.contains(QStringLiteral("MacIcons"), Qt::CaseInsensitive)) {
                const QString path = root + QLatin1Char('/') + entry;
                if (Common::isValidIconTheme(path)) {
                    return entry;
                }
            }
        }
    }
    return QString();
}

} // namespace

bool Common::isValidIconTheme(const QString &themeDirPath)
{
    const QString indexPath = themeDirPath + QStringLiteral("/index.theme");
    if (!QFile::exists(indexPath)) {
        return false;
    }

    QFile indexFile(indexPath);
    if (!indexFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QString head = QString::fromUtf8(indexFile.read(4096));
    indexFile.close();

    if (head.contains(QStringLiteral("[Desktop Entry]"))
        && !head.contains(QStringLiteral("[Icon Theme]"))) {
        return false;
    }
    if (!head.contains(QStringLiteral("[Icon Theme]"))) {
        return false;
    }

    QSettings idx(indexPath, QSettings::IniFormat);
    idx.beginGroup(QStringLiteral("Icon Theme"));
    if (idx.value(QStringLiteral("Hidden")).toBool()) {
        return false;
    }
    const QStringList directories = parseThemeIndexList(idx, "Directories");
    const QStringList inherits = parseThemeIndexList(idx, "Inherits");
    idx.endGroup();

    const bool hasDirs = themeDirectoriesExist(themeDirPath, directories);
    const bool hasIcons = themeHasIconFiles(themeDirPath);

    if (hasIcons) {
        return true;
    }

    if (directories.isEmpty() && inherits.isEmpty()) {
        return false;
    }
    if (!directories.isEmpty() && !hasDirs && inherits.isEmpty()) {
        return false;
    }
    if (!hasIcons && inherits.isEmpty()) {
        return false;
    }
    if (!hasIcons && !inherits.isEmpty()) {
        const QStringList roots = Common::iconLocations(QString());
        for (const QString &parent : inherits) {
            if (inheritedThemeProvidesIcons(parent, roots, 0)) {
                return true;
            }
        }
        return false;
    }
    return true;
}

QString Common::resolveIconThemeDirectoryName(const QString &themeName,
                                              const QString &appPath)
{
    installIconThemeSearchPaths(appPath);
    return resolveUsableThemeName(themeName, iconLocations(appPath));
}

void Common::applyIconThemeName(const QString &themeName, const QString &appPath)
{
    installIconThemeSearchPaths(appPath);
    const QStringList roots = iconLocations(appPath);
    QString resolved = resolveUsableThemeName(themeName, roots);

    if (resolved.isEmpty() && !themeName.isEmpty()) {
        const QString onDisk = findThemeOnDisk(themeName, roots);
        if (!onDisk.isEmpty()) {
            for (const QString &root : roots) {
                const QString path = root + QLatin1Char('/') + onDisk;
                if (QDir(path).exists()) {
                    qWarning() << "icon theme is not a usable icon set:" << onDisk << path;
                    break;
                }
            }
        } else {
            qWarning() << "icon theme directory not found:" << themeName;
        }
        resolved = findMacIconsTheme(roots);
    }

    if (resolved.isEmpty()) {
        resolved = QStringLiteral("hicolor");
    }

    qDebug() << "apply icon theme" << resolved << "paths" << QIcon::themeSearchPaths();
    QIcon::setThemeName(resolved);
}

QString Common::configDir()
{
    QString dir = QString("%1/.config/%2%3")
                  .arg(QDir::homePath())
                  .arg(APP)
                  .arg(FM_MAJOR);
    if (!QFile::exists(dir)) {
        QDir makedir(dir);
        if (!makedir.mkpath(dir)) { dir.clear(); }
    }
    return dir;
}

QString Common::configFile()
{
    return QString("%1/%2%3.conf")
           .arg(configDir())
           .arg(APP)
           .arg(FM_MAJOR);
}

QString Common::trashDir()
{
    QString dir = QString("%1/.local/share/Trash").arg(QDir::homePath());
    if (!QFile::exists(dir)) {
        QDir makedir(dir);
        if (!makedir.mkpath(dir)) { dir.clear(); }
    }
    return dir;
}

QStringList Common::iconLocations(QString appPath)
{
    QStringList result;
    result << QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                        "icons",
                                        QStandardPaths::LocateDirectory);

    const QByteArray xdgData = qgetenv("XDG_DATA_DIRS");
    if (!xdgData.isEmpty()) {
        const QList<QByteArray> parts = xdgData.split(':');
        for (const QByteArray &part : parts) {
            if (part.isEmpty()) {
                continue;
            }
            const QString iconsDir = QString::fromLocal8Bit(part) + QStringLiteral("/icons");
            if (QDir(iconsDir).exists()) {
                const QString canonical = QDir(iconsDir).canonicalPath();
                if (!canonical.isEmpty()) {
                    result << canonical;
                }
            }
        }
    }

#ifndef Q_OS_MAC
    if (QDir("/usr/share/icons").exists()) {
        result << QStringLiteral("/usr/share/icons");
    }
    if (QDir("/usr/local/share/icons").exists()) {
        result << QStringLiteral("/usr/local/share/icons");
    }
#endif
    const QString localIcons = QString("%1/.local/share/icons").arg(QDir::homePath());
    const QString dotIcons = QString("%1/.icons").arg(QDir::homePath());
    if (QDir(localIcons).exists()) {
        result << localIcons;
    }
    if (QDir(dotIcons).exists()) {
        result << dotIcons;
    }
    if (!appPath.isEmpty()) {
        const QString appDir = QFileInfo(appPath).absoluteDir().absolutePath();
        const QString bundledIcons = QDir(appDir).filePath(QStringLiteral("../share/icons"));
        if (QDir(bundledIcons).exists()) {
            const QString canonical = QDir(bundledIcons).canonicalPath();
            if (!canonical.isEmpty()) {
                result << canonical;
            }
        }
        result << QString("%1/../share/icons").arg(appPath);
    }
    result.removeDuplicates();
    return result;
}

void Common::prepareLinuxIconThemeEnvironment()
{
#ifndef Q_OS_LINUX
    Q_UNUSED(0);
#else
    if (!qEnvironmentVariableIsSet("APPIMAGE")) {
        return;
    }

    // linuxdeploy often sets XDG_DATA_DIRS to the AppDir only; merge host paths for icon discovery.
    QByteArray xdg = qgetenv("XDG_DATA_DIRS");
    if (!xdg.contains("/usr/share")) {
        if (xdg.isEmpty()) {
            qputenv("XDG_DATA_DIRS", "/usr/share:/usr/local/share");
        } else {
            xdg.append(":/usr/share:/usr/local/share");
            qputenv("XDG_DATA_DIRS", xdg.constData());
        }
    }

    // Bundled libqgtk3 makes QIcon::fromTheme use GTK settings and ignore QIcon::setThemeName.
    if (qgetenv("QT_QPA_PLATFORMTHEME").isEmpty()) {
        qputenv("QT_QPA_PLATFORMTHEME", QByteArray("qtfm"));
    }
#endif
}

void Common::installIconThemeSearchPaths(const QString &appPath)
{
    const QStringList paths = iconLocations(appPath);
    QIcon::setThemeSearchPaths(paths);
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    QStringList fallbackRoots;
    for (const QString &root : paths) {
        if (QDir(root + QStringLiteral("/hicolor")).exists()) {
            fallbackRoots << root;
        }
    }
    if (!fallbackRoots.isEmpty()) {
        QIcon::setFallbackSearchPaths(fallbackRoots);
    }
}

QStringList Common::pixmapLocations(QString appPath)
{
    QStringList result;
    result << QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                        "pixmaps",
                                        QStandardPaths::LocateDirectory);
    result << QString("%1/../share/pixmaps").arg(appPath);
    return result;
}

QStringList Common::applicationLocations(QString appPath)
{
    QStringList result;
    result << QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                        "applications",
                                        QStandardPaths::LocateDirectory);
    result << QString("%1/../share/applications").arg(appPath);
    return result;
}

QStringList Common::mimeGlobLocations(QString appPath)
{
    QStringList result;
    result << QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                        "mime/globs",
                                        QStandardPaths::LocateFile);
    result << QString("%1/../share/mime/globs").arg(appPath);
    return result;
}

QStringList Common::mimeGenericLocations(QString appPath)
{
    QStringList result;
    result << QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                        "mime/generic-icons",
                                        QStandardPaths::LocateFile);
    result << QString("%1/../share/mime/generic-icons").arg(appPath);
    return result;
}

QStringList Common::mimeTypeLocations(QString appPath)
{
    QStringList result;
    result << QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                        "mime/types",
                                        QStandardPaths::LocateFile);
    result << QString("%1/../share/mime/types").arg(appPath);
    return result;
}

QString Common::getDesktopIcon(QString desktop)
{
    QString result;
    if (desktop.isEmpty()) { return result; }
    QFile file(desktop);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return result; }
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) { continue; }
        if (line.trimmed().startsWith("Icon=")) {
            result = line.trimmed().replace("Icon=", "");
            break;
        }
    }
    file.close();
    return result;
}

QString Common::findIconInDir(QString appPath,
                              QString theme,
                              QString dir,
                              QString icon)
{
    QString result;
    if (dir.isEmpty() || icon.isEmpty()) { return result; }

    if (theme.isEmpty()) { theme = "hicolor"; }

    QStringList iconSizes;
    iconSizes << "128" << "64" << "48" << "32" << "22" << "16";

    // theme
    QDirIterator it(QString("%1/%2").arg(dir).arg(theme),
                    QStringList() << "*.png" << "*.jpg" << "*.xpm",
                    QDir::Files|QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString found = it.next();
        QString iconName = QFileInfo(found).completeBaseName();
        if (iconName == icon) {
            for (int i=0;i<iconSizes.size();++i) {
                QString hasFile = found.replace(QRegularExpression("/\\d+x\\d+/"),
                                                QString("/%1x%1/").arg(iconSizes.at(i)));
                if (QFile::exists(hasFile)) { return hasFile; }
            }
            return found;
        }
    }
    // hicolor
    if (theme != "hicolor") {
        QDirIterator hicolor(QString("%1/%2").arg(dir).arg("hicolor"),
                             QStringList() << "*.png" << "*.jpg" << "*.xpm",
                             QDir::Files|QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (hicolor.hasNext()) {
            QString found = hicolor.next();
            QString iconName = QFileInfo(found).completeBaseName();
            if (iconName == icon) {
                for (int i=0;i<iconSizes.size();++i) {
                    QString hasFile = found.replace(QRegularExpression("/\\d+x\\d+/"),
                                                    QString("/%1x%1/").arg(iconSizes.at(i)));
                    if (QFile::exists(hasFile)) { return hasFile; }
                }
                return found;
            }
        }
    }
    // pixmaps
    QStringList pixs = pixmapLocations(appPath);
    for (int i=0;i<pixs.size();++i) {
        QDirIterator pixmaps(pixs.at(i),
                             QStringList() << "*.png" << "*.jpg" << "*.xpm",
                             QDir::Files|QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (pixmaps.hasNext()) {
            QString found = pixmaps.next();
            QString iconName = QFileInfo(found).completeBaseName();
            if (iconName == icon) { return found; }
        }
    }
    return result;
}

QString Common::findIcon(QString appPath,
                         QString theme,
                         QString fileIcon)
{
    QString result;
    if (fileIcon.isEmpty()) { return result; }
    QStringList icons = iconLocations(appPath);
    for (int i=0;i<icons.size();++i) {
        QString icon = findIconInDir(appPath,
                                     theme,
                                     icons.at(i),
                                     fileIcon);
        if (!icon.isEmpty()) { return icon; }
    }
    return result;
}

QString Common::findApplication(QString appPath,
                                QString desktopFile)
{
    QString result;
    if (desktopFile.isEmpty()) { return result; }
    QStringList apps = applicationLocations(appPath);
    for (int i=0;i<apps.size();++i) {
        QDirIterator it(apps.at(i), QStringList("*.desktop"), QDir::Files|QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            QString found = it.next();
            if (found.split("/").takeLast()==desktopFile) {
                //qDebug() << "found app" << found;
                return found;
            }
        }
    }
    return result;
}

QStringList Common::findApplications(QString filename)
{
    QStringList result;
    if (filename.isEmpty()) { return result; }
    QString path = qgetenv("PATH");
    QStringList paths = path.split(":", Qt::SkipEmptyParts);
    for (int i=0;i<paths.size();++i) {
        QDirIterator it(paths.at(i),
                        QStringList("*"),
                        QDir::Files|QDir::Executable|QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            QString found = it.next();
            if (found.split("/").takeLast().startsWith(filename)) {
                result << found;
            }
        }
    }
    return result;
}

QString Common::findApplicationIcon(QString appPath,
                                    QString theme,
                                    QString app)
{
    QString result;
    QString desktop = findApplication(appPath, app);
    if (desktop.isEmpty()) { return result; }
    QString icon = getDesktopIcon(desktop);
    if (icon.isEmpty()) { return result; }
    result = findIcon(appPath, theme, icon);
    return result;
}

QMap<QString, QString> Common::readGlobMimesFromFile(QString filename)
{
    QMap<QString, QString> map;
    if (filename.isEmpty()) { return map; }
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly|QIODevice::Text)) { return map; }
    QTextStream s(&file);
    while (!s.atEnd()) {
        QStringList line = s.readLine().split(":");
        if (line.count() == 2) {
            QString suffix = line.at(1);
            if (!suffix.startsWith("*.")) { continue; }
            suffix.remove("*.");
            QString mime = line.at(0);
            mime.replace("/", "-");
            if (!suffix.isEmpty() && !mime.isEmpty()) { map[mime] = suffix; }
        }
    }
    file.close();
    return map;
}

QMap<QString, QString> Common::getMimesGlobs(QString appPath)
{
    QMap<QString, QString> map;
    QStringList mimes = mimeGlobLocations(appPath);
    for (int i=0;i<mimes.size();++i) {
        QMapIterator<QString, QString> globs(readGlobMimesFromFile(mimes.at(i)));
        while (globs.hasNext()) {
            globs.next();
            map[globs.key()] = globs.value();
        }
    }
    return map;
}

QMap<QString, QString> Common::readGenericMimesFromFile(QString filename)
{
    QMap<QString, QString> map;
    if (filename.isEmpty()) { return map; }
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly|QIODevice::Text)) { return map; }
    QTextStream s(&file);
    while (!s.atEnd()) {
        QStringList line = s.readLine().split(":");
        if (line.count() == 2) {
            QString mimeName = line.at(0);
            mimeName.replace("/","-");
            QString mimeIcon = line.at(1);
            if (!mimeName.isEmpty() && !mimeIcon.isEmpty()) { map[mimeName] = mimeIcon; }
        }
    }
    file.close();
    return map;
}

QMap<QString, QString> Common::getMimesGeneric(QString appPath)
{
    QMap<QString, QString> map;
    QStringList mimes = mimeGenericLocations(appPath);
    for (int i=0;i<mimes.size();++i) {
        QMapIterator<QString, QString> generic(readGenericMimesFromFile(mimes.at(i)));
        while (generic.hasNext()) {
            generic.next();
            map[generic.key()] = generic.value();
        }
    }
    return map;
}

QStringList Common::getPixmaps(QString appPath)
{
    QStringList result;
    QStringList pixs = pixmapLocations(appPath);
    for (int i=0;i<pixs.size();++i) {
        QDir pixmaps(pixs.at(i), "", QDir::NoSort,
                     static_cast<QDir::Filters>(QDir::Files | QDir::NoDotAndDotDot));
        for (int i=0;i<pixmaps.entryList().size();++i) {
            result << QString("%1/%2").arg(pixmaps.absolutePath()).arg(pixmaps.entryList().at(i));
        }
    }
    return result;
}

QStringList Common::getMimeTypes(QString appPath)
{
    QStringList result;
    QStringList mimes = mimeTypeLocations(appPath);
    for (int i=0;i<mimes.size();++i) {
        QFile file(mimes.at(i));
        if (!file.open(QIODevice::ReadOnly|QIODevice::Text)) { continue; }
        QTextStream s(&file);
        while (!s.atEnd()) {
            QString line = s.readLine();
            if (!line.isEmpty()) { result.append(line); }
        }
        file.close();
    }
    return result;
}

QStringList Common::getIconThemes(QString appPath)
{
    QStringList result;
    const QStringList icons = iconLocations(appPath);
    QSet<QString> seen;
    for (int i = 0; i < icons.size(); ++i) {
        QDirIterator it(icons.at(i), QDir::Dirs | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            it.next();
            const QString dirName = it.fileName();
            if (dirName == QLatin1String("hicolor")
                || dirName == QLatin1String("default")) {
                continue;
            }
            const QString path = it.filePath();
            if (!isValidIconTheme(path)) {
                continue;
            }
            if (seen.contains(dirName)) {
                continue;
            }
            seen.insert(dirName);
            result.append(dirName);
        }
    }

    qDebug() << "icon theme roots" << icons << "found" << result.size();
    QStringList preferredFirst;
    const QString macIcons = findMacIconsTheme(icons);
    if (!macIcons.isEmpty()) {
        preferredFirst << macIcons;
    }
    for (const QString &name : result) {
        if (!preferredFirst.contains(name, Qt::CaseInsensitive)) {
            preferredFirst.append(name);
        }
    }
    return preferredFirst;
}

bool Common::removeFileCache()
{
    QFile cache(QString("%1/file.cache").arg(Common::configDir()));
    if (cache.exists()) {
        return cache.remove();
    }
    return false;
}

bool Common::removeFolderCache()
{
    QFile cache(QString("%1/folder.cache").arg(Common::configDir()));
    if (cache.exists()) {
        return cache.remove();
    }
    return false;
}

bool Common::removeThumbsCache()
{
    QFile cache(QString("%1/thumbs.cache").arg(Common::configDir()));
    if (cache.exists()) {
        return cache.remove();
    }
    return false;
}

void Common::setupIconTheme(QString appFilePath)
{
    QSettings settings(Common::configFile(), QSettings::IniFormat);
    installIconThemeSearchPaths(appFilePath);
    const QStringList roots = iconLocations(appFilePath);

    QString temp = resolveUsableThemeName(settings.value("fallbackTheme").toString(), roots);

    if (temp.isEmpty() || temp == QLatin1String("hicolor")) {
        if (QFile::exists(QDir::homePath() + "/" + ".gtkrc-2.0")) {
            QSettings gtkFile(QDir::homePath() + "/.gtkrc-2.0", QSettings::IniFormat);
            temp = resolveUsableThemeName(
                gtkFile.value("gtk-icon-theme-name").toString().remove("\""), roots);
        } else {
            QSettings gtkFile(QDir::homePath() + "/.config/gtk-3.0/settings.ini",
                              QSettings::IniFormat);
            temp = resolveUsableThemeName(
                gtkFile.value("Settings/gtk-icon-theme-name").toString().remove("\""), roots);
            if (temp.isEmpty()) {
                temp = resolveUsableThemeName(
                    gtkFile.value("gtk-icon-theme-name").toString().remove("\""), roots);
            }
        }
    }

    if (temp.isEmpty() || temp == QLatin1String("hicolor")) {
#ifndef Q_OS_MACX
        temp = findMacIconsTheme(roots);
#endif
    }
    if (temp.isEmpty() || temp == QLatin1String("hicolor")) {
#ifndef Q_OS_MACX
        static const char *fallbacks[] = {"Papirus", "Adwaita", "breeze", "Tango", "Humanity"};
        for (const char *name : fallbacks) {
            const QString resolved = resolveUsableThemeName(QString::fromUtf8(name), roots);
            if (!resolved.isEmpty()) {
                temp = resolved;
                break;
            }
        }
#endif
#ifdef Q_OS_MACX
        if (temp.isEmpty()) {
            temp = resolveUsableThemeName(QStringLiteral("Adwaita"), roots);
        }
#endif
    }

    applyIconThemeName(temp, appFilePath);

    const QString applied = QIcon::themeName();
    if (!applied.isEmpty() && applied != QLatin1String("hicolor")) {
        settings.setValue("fallbackTheme", applied);
    }
}

Common::DragMode Common::int2dad(int value)
{
    switch (value) {
    case 0:
        return DM_UNKNOWN;
    case 1:
        return DM_COPY;
    case 2:
        return DM_MOVE;
    case 3:
        return DM_LINK;
    default:
        return DM_MOVE;
    }
}

QVariant Common::readSetting(QString key, QString fallback) {
    QSettings settings(Common::configFile(), QSettings::IniFormat);
    return settings.value(key, fallback);
}

void Common::writeSetting(QString key, QVariant value) {
    QSettings settings(Common::configFile(), QSettings::IniFormat);
    settings.setValue(key, value);
}

Common::DragMode Common::getDADaltMod()
{
    QSettings settings(Common::configFile(), QSettings::IniFormat);
    return int2dad(settings.value("dad_alt", DM_UNKNOWN).toInt());
}

Common::DragMode Common::getDADctrlMod()
{
    QSettings settings(Common::configFile(), QSettings::IniFormat);
    return int2dad(settings.value("dad_ctrl", DM_COPY).toInt());
}

Common::DragMode Common::getDADshiftMod()
{
    QSettings settings(Common::configFile(), QSettings::IniFormat);
    return int2dad(settings.value("dad_shift", DM_MOVE).toInt());
}

Common::DragMode Common::getDefaultDragAndDrop()
{
    QSettings settings(Common::configFile(), QSettings::IniFormat);
    return int2dad(settings.value("dad", DM_MOVE).toInt());
}

QString Common::getDeviceForDir(QString dir)
{
    QFile mtab("/etc/mtab");
    if (!mtab.open(QIODevice::ReadOnly)) { return QString(); }
    QTextStream ts(&mtab);
    QString root;
    QVector<QStringList> result;
    QStringList entries = ts.readAll().split("\n", Qt::SkipEmptyParts);
    for (int i=0;i<entries.length();++i) {
        QString line = entries.at(i);
        QStringList info = line.split(" ", Qt::SkipEmptyParts);
        if (info.size()>=2) {
            QString dev = info.at(0);
            QString mnt = info.at(1);
            if (mnt == "/") {
                root = dev;
                continue;
            }
            if (dir.startsWith(mnt)) { result.append(QStringList() << dev << mnt); }
        }
    }
    mtab.close();

    if (result.size()==0) { return root; }
    if (result.size()==1) { return result.at(0).at(0); }
    if (result.size()>1) {
        int lastDevCount = 0;
        QString lastDevice;
        for (int i=0;i<result.size();++i) {
            QStringList device = result.at(i);
            QStringList devCount = device.at(1).split("/");
            if (devCount.size()>lastDevCount) {
                lastDevCount = devCount.size();
                lastDevice = device.at(0);
            }
        }
        return lastDevice;
    }
    return QString();
}

QPalette Common::darkTheme()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(64,66,68));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(46,47,48));
    palette.setColor(QPalette::AlternateBase, QColor(64,66,68));
    //palette.setColor(QPalette::ToolTipBase, Qt::white);
    //palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Link, Qt::white);
    palette.setColor(QPalette::LinkVisited, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::black);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(64,66,68));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Highlight, QColor(28,28,29));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, Qt::darkGray);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::darkGray);
    return palette;
}

QStringList Common::iconPaths(QString appPath)
{
    QStringList iconsPath = QIcon::themeSearchPaths();
    iconsPath << iconLocations(appPath);
    /*QString iconsHomeLocal = QString("%1/.local/share/icons").arg(QDir::homePath());
    QString iconsHome = QString("%1/.icons").arg(QDir::homePath());
    if (QFile::exists(iconsHomeLocal) && !iconsPath.contains(iconsHomeLocal)) { iconsPath.prepend(iconsHomeLocal); }
    if (QFile::exists(iconsHome) && !iconsPath.contains(iconsHome)) { iconsPath.prepend(iconsHome); }
    iconsPath << QString("%1/../share/icons").arg(appPath);*/
    iconsPath.removeDuplicates();
    return  iconsPath;
}

QVector<QStringList> Common::getDefaultActions()
{
    QVector<QStringList> result;
    result.append(QStringList()<< "tar.gz,tar.bz2,tar.xz,tar,tgz,tbz,tbz2,txz" << "Extract tar here ..." << "package-x-generic" << "tar xvf %F");
    result.append(QStringList()<< "7z" << "Extract 7z here ..." << "package-x-generic" << "7za x %F");
    result.append(QStringList()<< "rar" << "Extract rar here ..." << "package-x-generic" << "unrar x %F");
    result.append(QStringList()<< "zip" << "Extract zip here ..." << "package-x-generic" << "unzip %F");
    result.append(QStringList()<< "gz" << "Extract gz here ..." << "package-x-generic" << "gunzip --keep %F");
    result.append(QStringList()<< "bz2" << "Extract bz2 here ..." << "package-x-generic" << "bunzip2 --keep %F");
    result.append(QStringList()<< "xz" << "Extract xz here ..." << "package-x-generic" << "xz -d --keep %F");
    result.append(QStringList()<< "*" << "Compress to tar.gz" << "package-x-generic" << "tar cvvzf %n.tar.gz %F");
    result.append(QStringList()<< "*" << "Compress to tar.bz2" << "package-x-generic" << "tar cvvjf %n.tar.bz2 %F");
    result.append(QStringList()<< "*" << "Compress to tar.xz" << "package-x-generic" << "tar cvvJf %n.tar.xz %F");
    result.append(QStringList()<< "*" << "Compress to zip" << "package-x-generic" << "zip -r %n.zip %F");
    return result;
}

QString Common::formatSize(qint64 num)
{
    QString total;
    const qint64 kb = 1024;
    const qint64 mb = 1024 * kb;
    const qint64 gb = 1024 * mb;
    const qint64 tb = 1024 * gb;

    if (num >= tb) { total = QString("%1TB").arg(QString::number(qreal(num) / tb, 'f', 2)); }
    else if (num >= gb) { total = QString("%1GB").arg(QString::number(qreal(num) / gb, 'f', 2)); }
    else if (num >= mb) { total = QString("%1MB").arg(QString::number(qreal(num) / mb, 'f', 1)); }
    else if (num >= kb) { total = QString("%1KB").arg(QString::number(qreal(num) / kb,'f', 1)); }
    else { total = QString("%1 bytes").arg(num); }

    return total;
}

QString Common::getDriveInfo(QString path)
{
#ifdef __NetBSD__
    struct statvfs info;
    statvfs(path.toLocal8Bit(), &info);
#else
    struct statfs info;
    statfs(path.toLocal8Bit(), &info);
#endif
    if(info.f_blocks == 0) return "";

    return QString("%1  /  %2  (%3%)").arg(formatSize((qint64) (info.f_blocks - info.f_bavail)*info.f_bsize))
            .arg(formatSize((qint64) info.f_blocks*info.f_bsize))
            .arg((info.f_blocks - info.f_bavail)*100/info.f_blocks);
}

QString Common::getXdgCacheHome()
{
    QString result = qgetenv("XDG_CACHE_HOME");
    if (result.isEmpty()) {
        result = QString("%1/.cache").arg(QDir::homePath());
    }
    return result;
}

QString Common::getThumbnailHash(const QString &filename)
{
    if (!filename.isEmpty()) {
        QString filenameString = QUrl::fromUserInput(filename).toString();
        QString hash = QString(QCryptographicHash::hash(filenameString.toUtf8(),
                                                        QCryptographicHash::Md5).toHex());
        return hash;
    }
    return QString();
}

QString Common::hasThumbnail(const QString &filename)
{
    if (QFile::exists(filename)) {
        QString imagePath = QString("%1/thumbnails/normal/%2.png")
                            .arg(getXdgCacheHome())
                            .arg(getThumbnailHash(filename));
        if (QFile::exists(imagePath)) { return imagePath; }
    }
    return QString();
}

QString Common::getTempPath()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    qDebug() << "Temp folder" << path;
    QDir dir(path);
    if (!dir.exists()) {
        if (!dir.mkpath(path)) { return QString(); }
    }
    return path;
}

QString Common::getTempClipboardFile()
{
    const QString path = getTempPath();
    if (path.isEmpty()) { return QString(); }
    return QString("%1/clipboard.tmp").arg(path);
}

QByteArray Common::thumbnailBmp(const QImage &source, int pixSize)
{
    if (source.isNull() || pixSize < 1) {
        return QByteArray();
    }
    const QImage scaled = source.scaled(
        pixSize, pixSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage canvas(pixSize, pixSize, QImage::Format_RGB32);
    canvas.fill(Qt::black);
    QPainter painter(&canvas);
    painter.drawImage((pixSize - scaled.width()) / 2,
                      (pixSize - scaled.height()) / 2,
                      scaled);
    painter.end();
    QByteArray result;
    QBuffer buffer(&result);
    buffer.open(QIODevice::WriteOnly);
    canvas.save(&buffer, "BMP");
    return result;
}
