#include "bundledicons.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QtEndian>

namespace {

/** Load Apple .icns (PNG/JPEG chunks); works on Linux without Qt icns plugin. */
QIcon iconFromIcnsFile(const QString &path)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    const QIcon native(path);
    if (!native.isNull()) {
        return native;
    }
#endif
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QIcon();
    }
    const QByteArray data = file.readAll();
    if (data.size() < 8 || !data.startsWith("icns")) {
        return QIcon();
    }

    QImage best;
    int bestArea = 0;

    auto tryImage = [&](const QByteArray &payload) {
        if (payload.size() < 8) {
            return;
        }
        QImage img;
        if (!img.loadFromData(payload)) {
            return;
        }
        const int area = img.width() * img.height();
        if (area > bestArea) {
            best = img;
            bestArea = area;
        }
    };

    int offset = 8;
    while (offset + 8 <= data.size()) {
        const quint32 chunkSize = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar *>(data.constData() + offset + 4));
        if (chunkSize < 8 || offset + static_cast<int>(chunkSize) > data.size()) {
            break;
        }
        const QByteArray payload = data.mid(offset + 8, static_cast<int>(chunkSize) - 8);
        tryImage(payload);
        offset += static_cast<int>(chunkSize);
    }

    // Some icns nest PNG without clean chunk boundaries; scan as fallback.
    for (int i = 0; i + 8 < data.size(); ++i) {
        if (static_cast<uchar>(data.at(i)) == 0x89
            && data.at(i + 1) == 'P' && data.at(i + 2) == 'N'
            && data.at(i + 3) == 'G') {
            tryImage(data.mid(i));
        }
    }

    if (best.isNull()) {
        return QIcon();
    }
    QIcon icon;
    const int sizes[] = {16, 24, 32, 48, 64, 96, 128, 192, 256};
    for (int size : sizes) {
        QPixmap pm = QPixmap::fromImage(
            best.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        icon.addPixmap(pm);
    }
    return icon;
}

static const char *kBundledExtensions[] = {
    ".svg", ".png", ".icns", ".SVG", ".PNG", ".ICNS",
};


QStringList bundledMimeIconDirectories()
{
    QStringList dirs;
    const QString appDir = QCoreApplication::applicationDirPath();

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    dirs << appDir + QStringLiteral("/../Resources/mimes");
#endif
    dirs << appDir + QStringLiteral("/../share/qtfm/mimes");
    dirs << appDir + QStringLiteral("/../share/icons/mimes");
    const QString devTreeMimes = QDir(appDir).absoluteFilePath(
        QStringLiteral("../../share/icons/mimes"));
    if (QDir(devTreeMimes).exists()) {
        dirs << devTreeMimes;
    }

    const QStringList dataDirs = QStandardPaths::standardLocations(
        QStandardPaths::GenericDataLocation);
    for (const QString &dataDir : dataDirs) {
        const QString candidate = dataDir + QStringLiteral("/qtfm/mimes");
        if (QDir(candidate).exists()) {
            dirs << candidate;
        }
    }

    dirs.removeDuplicates();
    return dirs;
}

QString resolveBundledBaseName(const QString &name)
{
    const QString key = name.toLower();
    static const QHash<QString, QString> aliases = {
        {QStringLiteral("inode-directory"), QStringLiteral("folder")},
        {QStringLiteral("folder-open"), QStringLiteral("default-folder-open")},
        {QStringLiteral("folder-visiting"), QStringLiteral("inode-symlink")},
        {QStringLiteral("text-x-generic"), QStringLiteral("text")},
        {QStringLiteral("text-plain"), QStringLiteral("txt")},
        {QStringLiteral("text-markdown"), QStringLiteral("md")},
        {QStringLiteral("package-x-generic"), QStringLiteral("archive")},
        {QStringLiteral("application-x-executable"), QStringLiteral("exec")},
        {QStringLiteral("image-x-generic"), QStringLiteral("image")},
        {QStringLiteral("video-x-generic"), QStringLiteral("video")},
        {QStringLiteral("text-html"), QStringLiteral("html")},
        {QStringLiteral("text-x-script"), QStringLiteral("sh")},
        {QStringLiteral("user-home"), QStringLiteral("folder-home")},
        {QStringLiteral("user-desktop"), QStringLiteral("folder-desktop")},
        {QStringLiteral("computer"), QStringLiteral("folder-root")},
        {QStringLiteral("user-trash"), QStringLiteral("folder-grey")},
        {QStringLiteral("applications-other"), QStringLiteral("x-content-software")},
        {QStringLiteral("applications-internet"), QStringLiteral("folder-download")},
        {QStringLiteral("audio-x-generic"), QStringLiteral("folder-music")},
    };
    return aliases.value(key, key);
}

QIcon iconFromSvgFile(const QString &path)
{
    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        return QIcon();
    }
    QIcon icon;
    const int sizes[] = {16, 24, 32, 48, 64, 96, 128, 192, 256};
    for (int size : sizes) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        renderer.render(&painter);
        icon.addPixmap(pm);
    }
    return icon;
}

QIcon loadIconFromBaseName(const QString &baseName)
{
    static QHash<QString, QIcon> cache;
    QString key = resolveBundledBaseName(baseName);
    if (cache.contains(key)) {
        return cache.value(key);
    }

    for (const QString &dirPath : bundledMimeIconDirectories()) {
        for (const char *ext : kBundledExtensions) {
            const QString path = dirPath + QLatin1Char('/') + key
                                 + QString::fromLatin1(ext);
            if (!QFileInfo::exists(path)) {
                continue;
            }
            QIcon icon;
            if (path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
                icon = iconFromSvgFile(path);
            } else if (path.endsWith(QLatin1String(".icns"), Qt::CaseInsensitive)) {
                icon = iconFromIcnsFile(path);
            } else {
                icon = QIcon(path);
            }
            if (!icon.isNull()) {
                cache.insert(key, icon);
                return icon;
            }
        }
    }

    for (const char *ext : kBundledExtensions) {
        const QString resource = QStringLiteral(":/icons/mimes/") + key
                                 + QString::fromLatin1(ext);
        if (!QFile::exists(resource)) {
            continue;
        }
        QIcon icon;
        if (resource.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
            icon = iconFromSvgFile(resource);
        } else if (resource.endsWith(QLatin1String(".icns"), Qt::CaseInsensitive)) {
            icon = iconFromIcnsFile(resource);
        } else {
            icon = QIcon(resource);
        }
        if (!icon.isNull()) {
            cache.insert(key, icon);
            return icon;
        }
    }

    cache.insert(key, QIcon());
    return QIcon();
}

QString findIconFilePath(const QString &baseName)
{
    const QString key = resolveBundledBaseName(baseName);
    for (const QString &dirPath : bundledMimeIconDirectories()) {
        for (const char *ext : kBundledExtensions) {
            const QString path = dirPath + QLatin1Char('/') + key + QString::fromLatin1(ext);
            if (QFileInfo::exists(path)) {
                return path;
            }
        }
    }
    for (const char *ext : kBundledExtensions) {
        const QString resource = QStringLiteral(":/icons/mimes/") + key + QString::fromLatin1(ext);
        if (!QFile::exists(resource)) {
            continue;
        }
        if (QFileInfo::exists(resource)) {
            return resource;
        }
    }
    return QString();
}

QIcon iconWithFallbacks(const QStringList &baseNames)
{
    for (const QString &name : baseNames) {
        const QIcon icon = loadIconFromBaseName(name);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return loadIconFromBaseName(QStringLiteral("empty"));
}

} // namespace

QIcon BundledIcons::emptyIcon()
{
    static QIcon empty = loadIconFromBaseName(QStringLiteral("empty"));
    return empty;
}

QPixmap BundledIcons::iconPixmap(const QIcon &icon, int size)
{
    const QSize pxSize(qMax(size, 1), qMax(size, 1));
    QIcon use = icon;
    if (use.isNull()) {
        use = emptyIcon();
    }
    QPixmap pm = use.pixmap(pxSize);
    if (!pm.isNull()) {
        return pm;
    }
    return emptyIcon().pixmap(pxSize);
}

QString BundledIcons::baseNameForSuffix(const QString &suffix)
{
    const QString s = suffix.toLower();
    if (s.isEmpty()) {
        return QString();
    }
    if (s == QLatin1String("jpeg")) {
        return QStringLiteral("jpg");
    }
    if (s == QLatin1String("yml")) {
        return QStringLiteral("yaml");
    }
    if (s == QLatin1String("htm")) {
        return QStringLiteral("html");
    }
    if (s == QLatin1String("markdown")) {
        return QStringLiteral("md");
    }
    if (s == QLatin1String("blend1")) {
        return QStringLiteral("blend");
    }
    return s;
}

QString BundledIcons::baseNameForMime(const QString &mime)
{
    const QString m = mime.toLower();
    if (m.startsWith(QLatin1String("video/"))) {
        if (m.contains(QLatin1String("mp4")) || m.contains(QLatin1String("quicktime"))) {
            return QStringLiteral("mp4");
        }
        if (m.contains(QLatin1String("x-msvideo")) || m.contains(QLatin1String("avi"))) {
            return QStringLiteral("avi");
        }
        if (m.contains(QLatin1String("matroska")) || m.contains(QLatin1String("mkv"))) {
            return QStringLiteral("mkv");
        }
        return QStringLiteral("video");
    }
    if (m.startsWith(QLatin1String("image/"))) {
        if (m.contains(QLatin1String("jpeg"))) {
            return QStringLiteral("jpg");
        }
        if (m.contains(QLatin1String("svg"))) {
            return QStringLiteral("svg");
        }
        if (m.contains(QLatin1String("png"))) {
            return QStringLiteral("png");
        }
        return QStringLiteral("image");
    }
    if (m.contains(QLatin1String("python"))) {
        return QStringLiteral("py");
    }
    if (m.startsWith(QLatin1String("text/html"))) {
        return QStringLiteral("html");
    }
    if (m.contains(QLatin1String("javascript"))) {
        return QStringLiteral("js");
    }
    if (m.contains(QLatin1String("css"))) {
        return QStringLiteral("css");
    }
    if (m.contains(QLatin1String("yaml"))) {
        return QStringLiteral("yaml");
    }
    if (m.contains(QLatin1String("markdown"))) {
        return QStringLiteral("md");
    }
    if (m.endsWith(QLatin1String("zip")) || m.contains(QLatin1String("compressed"))) {
        if (m.contains(QLatin1String("rar"))) {
            return QStringLiteral("rar");
        }
        return QStringLiteral("zip");
    }
    if (m.startsWith(QLatin1String("text/"))) {
        return QStringLiteral("txt");
    }
    if (m.contains(QLatin1String("executable"))) {
        return QStringLiteral("exec");
    }
    return QString();
}

QIcon BundledIcons::iconForFileSuffix(const QString &suffix)
{
    const QString base = baseNameForSuffix(suffix);
    if (base.isEmpty()) {
        return loadIconFromBaseName(QStringLiteral("empty"));
    }

    QStringList tryNames;
    tryNames << base;

    static const QSet<QString> videoExt = {
        QStringLiteral("mp4"), QStringLiteral("avi"), QStringLiteral("mov"),
        QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("m4v"),
        QStringLiteral("mpeg"), QStringLiteral("mpg"), QStringLiteral("wmv"),
    };
    static const QSet<QString> imageExt = {
        QStringLiteral("jpg"), QStringLiteral("png"), QStringLiteral("svg"),
        QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"),
        QStringLiteral("tiff"), QStringLiteral("tif"), QStringLiteral("heic"),
        QStringLiteral("heif"), QStringLiteral("ico"), QStringLiteral("icns"),
    };
    static const QSet<QString> archiveExt = {
        QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"),
        QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("xz"),
        QStringLiteral("bz2"),
    };
    static const QSet<QString> modelExt = {
        QStringLiteral("fbx"), QStringLiteral("obj"), QStringLiteral("blend"),
        QStringLiteral("stl"), QStringLiteral("glb"), QStringLiteral("gltf"),
    };
    static const QSet<QString> codeExt = {
        QStringLiteral("py"), QStringLiteral("js"), QStringLiteral("css"),
        QStringLiteral("html"), QStringLiteral("yaml"), QStringLiteral("json"),
        QStringLiteral("xml"), QStringLiteral("sh"),
    };

    if (videoExt.contains(base)) {
        tryNames << QStringLiteral("video");
    } else if (imageExt.contains(base)) {
        tryNames << QStringLiteral("image");
    } else if (archiveExt.contains(base)) {
        tryNames << QStringLiteral("archive");
    } else if (modelExt.contains(base)) {
        tryNames << QStringLiteral("model3d");
    } else if (codeExt.contains(base) || base == QLatin1String("md")
               || base == QLatin1String("txt")) {
        tryNames << QStringLiteral("text");
    }

    tryNames << QStringLiteral("empty");
    return iconWithFallbacks(tryNames);
}

QIcon BundledIcons::iconForMimeType(const QString &mime)
{
    const QString base = baseNameForMime(mime);
    if (!base.isEmpty()) {
        return iconForFileSuffix(base);
    }
    return loadIconFromBaseName(QStringLiteral("empty"));
}

QIcon BundledIcons::iconForFolder(const QFileInfo &info)
{
    const QString name = info.fileName();
    if (name == QLatin1String("home")) {
        return iconWithFallbacks({QStringLiteral("folder-home"), QStringLiteral("folder")});
    }
    if (name == QLatin1String("Desktop") || name == QLatin1String("desktop")) {
        return iconWithFallbacks({QStringLiteral("folder-desktop"), QStringLiteral("folder")});
    }
    if (name == QLatin1String("Documents")) {
        return iconWithFallbacks({QStringLiteral("folder-documents"), QStringLiteral("folder")});
    }
    if (name == QLatin1String("Downloads")) {
        return iconWithFallbacks({QStringLiteral("folder-download"), QStringLiteral("folder")});
    }
    if (name == QLatin1String("Pictures")) {
        return iconWithFallbacks({QStringLiteral("folder-pictures"), QStringLiteral("folder")});
    }
    if (name == QLatin1String("Videos")) {
        return iconWithFallbacks({QStringLiteral("folder-videos"), QStringLiteral("folder")});
    }
    if (name == QLatin1String("Music")) {
        return iconWithFallbacks({QStringLiteral("folder-music"), QStringLiteral("folder")});
    }
    return iconWithFallbacks({QStringLiteral("folder")});
}

QIcon BundledIcons::iconForBookmarkPath(const QString &path)
{
    if (path.isEmpty()) {
        return QIcon();
    }
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    const QString home = QDir::homePath();

    if (path == QLatin1String("/") || canonical == QLatin1String("/")) {
        return iconWithFallbacks({QStringLiteral("folder-root"), QStringLiteral("folder")});
    }
    if (canonical == home || QFileInfo(path).absoluteFilePath() == QFileInfo(home).absoluteFilePath()) {
        return iconWithFallbacks({QStringLiteral("folder-home"), QStringLiteral("folder")});
    }
    if (canonical.endsWith(QLatin1String("/.local/share/Trash"))
        || path.endsWith(QLatin1String("/.local/share/Trash"))) {
        return iconWithFallbacks({QStringLiteral("folder-grey"), QStringLiteral("folder")});
    }
#ifdef Q_OS_MAC
    if (canonical == QLatin1String("/Applications") || path == QLatin1String("/Applications")) {
        return iconWithFallbacks({QStringLiteral("x-content-software"), QStringLiteral("folder")});
    }
#endif
    return iconForFolder(info);
}

QIcon BundledIcons::iconForExecutable()
{
    return iconWithFallbacks({QStringLiteral("exec"), QStringLiteral("empty")});
}

QStringList BundledIcons::mimeIconDirectories()
{
    return bundledMimeIconDirectories();
}

QString BundledIcons::iconFilePath(const QString &baseName)
{
    return findIconFilePath(baseName);
}

QIcon BundledIcons::iconByName(const QString &name)
{
    if (name.isEmpty()) {
        return loadIconFromBaseName(QStringLiteral("empty"));
    }
    QIcon icon = loadIconFromBaseName(name);
    if (!icon.isNull()) {
        return icon;
    }
    return loadIconFromBaseName(QStringLiteral("empty"));
}

QStringList BundledIcons::availableIconBaseNames()
{
    QStringList names;
    for (const QString &dirPath : bundledMimeIconDirectories()) {
        QDir dir(dirPath);
        for (const char *ext : kBundledExtensions) {
            const QString pattern = QStringLiteral("*") + QString::fromLatin1(ext);
            for (const QFileInfo &fi : dir.entryInfoList({pattern}, QDir::Files)) {
                names << fi.completeBaseName();
            }
        }
    }
    QDir resourceDir(QStringLiteral(":/icons/mimes"));
    if (resourceDir.exists()) {
        for (const char *ext : kBundledExtensions) {
            if (qstrcmp(ext, ".icns") == 0 || qstrcmp(ext, ".ICNS") == 0) {
                continue; // large icns are filesystem-only, not in qrc
            }
            const QString pattern = QStringLiteral("*") + QString::fromLatin1(ext);
            for (const QString &entry : resourceDir.entryList({pattern}, QDir::Files)) {
                names << QFileInfo(entry).completeBaseName();
            }
        }
    }
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}
