/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#include "common.h"
#include "bundledicons.h"

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
#include <QDateTime>
#include <QPainter>
#include <QUrl>
#include <QStandardPaths>
#include <QBuffer>
#include <QProcess>
#include <QTemporaryFile>
#include <QImageReader>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QCoreApplication>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#include <sys/stat.h>
#ifdef __NetBSD__
#include <sys/statvfs.h>
#endif

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
    Q_UNUSED(appPath);
    return BundledIcons::mimeIconDirectories();
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
    Q_UNUSED(appPath);
    Q_UNUSED(theme);
    Q_UNUSED(dir);
    return BundledIcons::iconFilePath(icon);
}

QString Common::findIcon(QString appPath,
                         QString theme,
                         QString fileIcon)
{
    Q_UNUSED(appPath);
    Q_UNUSED(theme);
    return BundledIcons::iconFilePath(fileIcon);
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
    const QString dirPath = qtfmThumbnailCacheDir();
    if (QDir(dirPath).exists()) {
        return QDir(dirPath).removeRecursively();
    }
    QFile legacy(QString("%1/thumbs.cache").arg(Common::configDir()));
    if (legacy.exists()) {
        return legacy.remove();
    }
    return false;
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
    palette.setColor(QPalette::Mid, QColor(64,66,68));
    palette.setColor(QPalette::Dark, QColor(46,47,48));
    palette.setColor(QPalette::Shadow, QColor(28,28,29));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Highlight, QColor(72, 120, 180));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, Qt::darkGray);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::darkGray);
    return palette;
}

QStringList Common::iconPaths(QString appPath)
{
    return BundledIcons::mimeIconDirectories();
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

bool Common::getDriveUsage(const QString &path, qint64 *usedBytes, qint64 *totalBytes)
{
    if (usedBytes) { *usedBytes = 0; }
    if (totalBytes) { *totalBytes = 0; }
    if (path.isEmpty()) { return false; }
#ifdef __NetBSD__
    struct statvfs info;
    if (statvfs(path.toLocal8Bit(), &info) != 0) { return false; }
#else
    struct statfs info;
    if (statfs(path.toLocal8Bit(), &info) != 0) { return false; }
#endif
    if (info.f_blocks == 0) { return false; }
    if (totalBytes) { *totalBytes = static_cast<qint64>(info.f_blocks) * info.f_bsize; }
    if (usedBytes) {
        *usedBytes = static_cast<qint64>(info.f_blocks - info.f_bavail) * info.f_bsize;
    }
    return true;
}

QString Common::getXdgCacheHome()
{
    QString result = qgetenv("XDG_CACHE_HOME");
    if (result.isEmpty()) {
        result = QString("%1/.cache").arg(QDir::homePath());
    }
    return result;
}

QString Common::qtfmThumbnailCacheDir()
{
    return getXdgCacheHome() + QStringLiteral("/qtfm/thumbnails/")
           + QString::number(thumbnailPixelSize);
}

QString Common::getThumbnailHash(const QString &filename)
{
    if (filename.isEmpty()) {
        return QString();
    }
    QFileInfo info(filename);
    const QByteArray payload = info.absoluteFilePath().toUtf8()
        + QByteArray::number(info.size())
        + QByteArray::number(info.lastModified().toSecsSinceEpoch());
    return QString(QCryptographicHash::hash(payload, QCryptographicHash::Md5).toHex());
}

QString Common::thumbnailCacheFile(const QString &absoluteFilePath)
{
    const QString hash = getThumbnailHash(absoluteFilePath);
    if (hash.length() < 2) {
        return QString();
    }
    return QStringLiteral("%1/%2/%3.png")
        .arg(qtfmThumbnailCacheDir())
        .arg(hash.left(2))
        .arg(hash);
}

bool Common::isThumbnailCacheValid(const QString &absoluteFilePath,
                                   const QString &cacheFile)
{
    if (absoluteFilePath.isEmpty()) {
        return false;
    }
    const QString path = cacheFile.isEmpty()
                             ? thumbnailCacheFile(absoluteFilePath)
                             : cacheFile;
    if (!QFileInfo::exists(path)) {
        return false;
    }
    const QFileInfo src(absoluteFilePath);
    if (!src.exists() || !src.isFile()) {
        return false;
    }
    return QFileInfo(path).lastModified() >= src.lastModified();
}

QImage Common::scaleToSquareThumbnail(const QImage &source)
{
    if (source.isNull()) {
        return QImage();
    }
    const int side = thumbnailPixelSize;
    const QImage scaled = source.scaled(
        side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage canvas(side, side, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.drawImage((side - scaled.width()) / 2,
                      (side - scaled.height()) / 2,
                      scaled);
    painter.end();
    return canvas;
}

QString Common::writeThumbnailForFile(const QString &absoluteFilePath,
                                      const QImage &source)
{
    const QImage canvas = scaleToSquareThumbnail(source);
    if (canvas.isNull()) {
        return QString();
    }
    const QString outPath = thumbnailCacheFile(absoluteFilePath);
    if (outPath.isEmpty()) {
        return QString();
    }
    QDir().mkpath(QFileInfo(outPath).absolutePath());
    if (!canvas.save(outPath, "PNG")) {
        return QString();
    }
    return outPath;
}

QImage Common::pdfFirstPageImage(const QString &pdfPath)
{
    if (pdfPath.isEmpty() || !QFileInfo::exists(pdfPath)) {
        return QImage();
    }
    const QString pdftoppm = QStandardPaths::findExecutable(QStringLiteral("pdftoppm"));
    if (pdftoppm.isEmpty()) {
        return QImage();
    }

    QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/qtfm-pdf-XXXXXX"));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        return QImage();
    }
    const QString prefix = tmp.fileName();
    tmp.close();

    QProcess proc;
    proc.setProgram(pdftoppm);
    proc.setArguments({
        QStringLiteral("-png"),
        QStringLiteral("-singlefile"),
        QStringLiteral("-f"), QStringLiteral("1"),
        QStringLiteral("-l"), QStringLiteral("1"),
        QStringLiteral("-scale-to"), QString::number(thumbnailPixelSize),
        pdfPath,
        prefix,
    });
    proc.start();
    if (!proc.waitForFinished(120000)) {
        proc.kill();
        QFile::remove(prefix + QStringLiteral(".png"));
        return QImage();
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QFile::remove(prefix + QStringLiteral(".png"));
        return QImage();
    }

    const QString pngPath = prefix + QStringLiteral(".png");
    QImage img;
    if (!img.load(pngPath)) {
        QFile::remove(pngPath);
        return QImage();
    }
    QFile::remove(pngPath);
    return img;
}

namespace {

/** Locate ffmpeg/ffprobe: PATH, Homebrew, or QtFM.app/Contents/Resources. */
QString findMediaToolExecutable(const QString &toolName)
{
    QString path = QStandardPaths::findExecutable(toolName);
    if (!path.isEmpty() && QFileInfo(path).isExecutable()) {
        return path;
    }
    const QStringList candidates = {
        QStringLiteral("/opt/homebrew/bin/") + toolName,
        QStringLiteral("/usr/local/bin/") + toolName,
        QStringLiteral("/usr/bin/") + toolName,
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    const QString bundled = QCoreApplication::applicationDirPath()
                            + QStringLiteral("/../Resources/") + toolName;
    const QString canonical = QFileInfo(bundled).canonicalFilePath();
    if (!canonical.isEmpty() && QFileInfo(canonical).isExecutable()) {
        return canonical;
    }
    return QString();
}

/** Runs ffmpeg with the given extra args, writing a single frame to outPng.
 *  Returns true on success (process exited 0 and the file was written). */
bool runFfmpegExtract(const QString &ffmpeg, const QStringList &extraArgs,
                      const QString &mediaPath, const QString &outPng)
{
    QProcess proc;
    proc.setProgram(ffmpeg);
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-loglevel") << QStringLiteral("error")
         << QStringLiteral("-nostdin");
    args += extraArgs;
    args << QStringLiteral("-i") << mediaPath;
    args << QStringLiteral("-frames:v") << QStringLiteral("1")
         << QStringLiteral("-vf")
         << QStringLiteral("scale='min(%1,iw)':'min(%1,ih)':force_original_aspect_ratio=decrease")
                .arg(Common::thumbnailPixelSize)
         << outPng;
    proc.setArguments(args);
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.start();
    if (!proc.waitForFinished(15000)) {
        proc.kill();
        proc.waitForFinished(2000);
        return false;
    }
    return proc.exitStatus() == QProcess::NormalExit
        && proc.exitCode() == 0
        && QFileInfo::exists(outPng)
        && QFileInfo(outPng).size() > 0;
}

/**
 * Uses ffprobe (ships alongside ffmpeg) to find the absolute stream index of
 * an embedded cover-art stream (disposition "attached_pic"), e.g. the poster
 * image muxed in by `yt-dlp --embed-thumbnail`. Returns -1 if ffprobe is
 * missing, the file has no such stream, or probing fails.
 */
int findAttachedPicStreamIndex(const QString &mediaPath)
{
    const QString ffprobe = findMediaToolExecutable(QStringLiteral("ffprobe"));
    if (ffprobe.isEmpty()) {
        return -1;
    }

    QProcess proc;
    proc.setProgram(ffprobe);
    proc.setArguments({
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_entries"),
        QStringLiteral("stream=index,codec_type:stream_disposition=attached_pic"),
        mediaPath,
    });
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.start();
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        proc.waitForFinished(2000);
        return -1;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return -1;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
    if (!doc.isObject()) {
        return -1;
    }
    const QJsonArray streams = doc.object().value(QStringLiteral("streams")).toArray();
    for (const QJsonValue &v : streams) {
        const QJsonObject stream = v.toObject();
        if (stream.value(QStringLiteral("codec_type")).toString() != QLatin1String("video")) {
            continue;
        }
        const QJsonObject disp = stream.value(QStringLiteral("disposition")).toObject();
        if (disp.value(QStringLiteral("attached_pic")).toInt() == 1) {
            return stream.value(QStringLiteral("index")).toInt(-1);
        }
    }
    return -1;
}

} // namespace

QImage Common::videoFirstFrameImage(const QString &mediaPath)
{
    if (mediaPath.isEmpty() || !QFileInfo::exists(mediaPath)) {
        return QImage();
    }
    const QString ffmpeg = findMediaToolExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        return QImage();
    }

    QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/qtfm-vid-XXXXXX"));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        return QImage();
    }
    const QString outPng = tmp.fileName() + QStringLiteral(".png");
    tmp.close();
    tmp.remove();

    bool ok = false;

    // 1) Prefer a precisely-identified embedded cover art stream (e.g. from
    //    `yt-dlp --embed-thumbnail`), located via ffprobe's disposition info.
    const int attachedPicIndex = findAttachedPicStreamIndex(mediaPath);
    if (attachedPicIndex >= 0) {
        ok = runFfmpegExtract(ffmpeg,
                              { QStringLiteral("-map"),
                                QStringLiteral("0:%1").arg(attachedPicIndex) },
                              mediaPath, outPng);
    }

    // 2) Heuristic fallback: many audio files only have a single video
    //    stream (the cover), so `-an` alone picks it up even without ffprobe.
    if (!ok) {
        ok = runFfmpegExtract(ffmpeg,
                              { QStringLiteral("-an") },
                              mediaPath, outPng);
    }

    // 3) Otherwise decode a real frame a little bit into the stream (avoids
    //    all-black leading frames); fall back to the very first frame.
    if (!ok) {
        ok = runFfmpegExtract(ffmpeg,
                              { QStringLiteral("-ss"), QStringLiteral("1") },
                              mediaPath, outPng);
    }
    if (!ok) {
        ok = runFfmpegExtract(ffmpeg, QStringList(), mediaPath, outPng);
    }

    if (!ok) {
        QFile::remove(outPng);
        return QImage();
    }

    QImage img;
    if (!img.load(outPng)) {
        QFile::remove(outPng);
        return QImage();
    }
    QFile::remove(outPng);
    return img;
}

QString Common::hasThumbnail(const QString &filename)
{
    if (!QFile::exists(filename)) {
        return QString();
    }
    const QString qtfmPath = thumbnailCacheFile(filename);
    if (isThumbnailCacheValid(filename, qtfmPath)) {
        return qtfmPath;
    }
    const QString filenameString = QUrl::fromUserInput(filename).toString();
    const QString legacyHash = QString(
        QCryptographicHash::hash(filenameString.toUtf8(), QCryptographicHash::Md5).toHex());
    const QString imagePath = QStringLiteral("%1/thumbnails/normal/%2.png")
                                  .arg(getXdgCacheHome())
                                  .arg(legacyHash);
    if (QFile::exists(imagePath)) {
        return imagePath;
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
