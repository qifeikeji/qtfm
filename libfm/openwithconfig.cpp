#include "openwithconfig.h"

#include "fileutils.h"

#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace {

QVector<SuffixOpenModule> g_suffixModules;
QHash<QString, QVector<OpenWithEntry>> g_categoryModules;

const QHash<QString, QStringList> kCategorySuffixes = {
    {QStringLiteral("image"),
     {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
      QStringLiteral("svg"), QStringLiteral("bmp"), QStringLiteral("gif"),
      QStringLiteral("webp"), QStringLiteral("tif"), QStringLiteral("tiff"),
      QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("ico"),
      QStringLiteral("icns")}},
    {QStringLiteral("video"),
     {QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
      QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("m4v"),
      QStringLiteral("mpeg"), QStringLiteral("mpg"), QStringLiteral("wmv")}},
    {QStringLiteral("text"),
     {QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("py"),
      QStringLiteral("js"), QStringLiteral("sh"), QStringLiteral("xml"),
      QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("json"),
      QStringLiteral("css"), QStringLiteral("html"), QStringLiteral("htm")}},
    {QStringLiteral("archive"),
     {QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"),
      QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("xz"),
      QStringLiteral("bz2"), QStringLiteral("tgz"), QStringLiteral("tbz2")}},
};

const QStringList kCategoryOrder = {
    QStringLiteral("image"),
    QStringLiteral("video"),
    QStringLiteral("text"),
    QStringLiteral("archive"),
};

OpenWithEntry readEntry(QSettings *s, const QString &prefix)
{
    OpenWithEntry e;
    e.name = s->value(prefix + QStringLiteral("/name")).toString();
    e.commandLine1 = s->value(prefix + QStringLiteral("/cmd1")).toString();
    e.commandLine2 = s->value(prefix + QStringLiteral("/cmd2")).toString();
    e.iconPath = s->value(prefix + QStringLiteral("/icon")).toString();
    return e;
}

void writeEntry(QSettings *s, const QString &prefix, const OpenWithEntry &e)
{
    s->setValue(prefix + QStringLiteral("/name"), e.name);
    s->setValue(prefix + QStringLiteral("/cmd1"), e.commandLine1);
    s->setValue(prefix + QStringLiteral("/cmd2"), e.commandLine2);
    s->setValue(prefix + QStringLiteral("/icon"), e.iconPath);
}

QStringList parseSuffixList(const QString &text)
{
    QString normalized = text;
    normalized.replace(QLatin1Char(';'), QLatin1Char(','));
    normalized.replace(QLatin1Char(' '), QLatin1Char(','));
    const QStringList parts = normalized.split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList out;
    for (QString p : parts) {
        p = p.trimmed().toLower();
        if (p.startsWith(QLatin1Char('.'))) {
            p.remove(0, 1);
        }
        if (!p.isEmpty()) {
            out << p;
        }
    }
    return out;
}

bool suffixMatchesModule(const QString &suffix, const SuffixOpenModule &mod)
{
    const QStringList list = parseSuffixList(mod.suffixesText);
    return list.contains(suffix.toLower());
}

} // namespace

QString OpenWithEntry::fullCommand() const
{
    QString cmd = commandLine1.trimmed();
    const QString extra = commandLine2.trimmed();
    if (!extra.isEmpty()) {
        if (!cmd.isEmpty()) {
            cmd += QLatin1Char(' ');
        }
        cmd += extra;
    }
    return cmd;
}

QIcon OpenWithEntry::menuIcon() const
{
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        return QIcon(iconPath);
    }
    return QIcon();
}

const QStringList &OpenWithConfig::categoryIds()
{
    return kCategoryOrder;
}

QString OpenWithConfig::categoryTitle(const QString &categoryId)
{
    if (categoryId == QLatin1String("image")) {
        return QStringLiteral("Image");
    }
    if (categoryId == QLatin1String("video")) {
        return QStringLiteral("Video");
    }
    if (categoryId == QLatin1String("text")) {
        return QStringLiteral("Text & code");
    }
    if (categoryId == QLatin1String("archive")) {
        return QStringLiteral("Archive");
    }
    return categoryId;
}

QStringList OpenWithConfig::suffixesForCategory(const QString &categoryId)
{
    return kCategorySuffixes.value(categoryId);
}

QString OpenWithConfig::categoryForSuffix(const QString &suffix)
{
    const QString s = suffix.toLower();
    for (auto it = kCategorySuffixes.constBegin(); it != kCategorySuffixes.constEnd(); ++it) {
        if (it.value().contains(s)) {
            return it.key();
        }
    }
    return QString();
}

QVector<SuffixOpenModule> &OpenWithConfig::suffixModules()
{
    return g_suffixModules;
}

QVector<OpenWithEntry> &OpenWithConfig::categoryModules(const QString &categoryId)
{
    return g_categoryModules[categoryId];
}

void OpenWithConfig::ensureSeedDefaults()
{
    if (!g_suffixModules.isEmpty() || !g_categoryModules.isEmpty()) {
        return;
    }
    SuffixOpenModule pdf;
    pdf.suffixesText = QStringLiteral("pdf");
    g_suffixModules.append(pdf);

    SuffixOpenModule blend;
    blend.suffixesText = QStringLiteral("blend");
    g_suffixModules.append(blend);

    SuffixOpenModule obj;
    obj.suffixesText = QStringLiteral("obj");
    g_suffixModules.append(obj);

    SuffixOpenModule glb;
    glb.suffixesText = QStringLiteral("glb,gltf");
    g_suffixModules.append(glb);

    for (const QString &cat : kCategoryOrder) {
        g_categoryModules[cat] = QVector<OpenWithEntry>();
        g_categoryModules[cat].append(OpenWithEntry());
    }
}

void OpenWithConfig::load(QSettings *settings)
{
    g_suffixModules.clear();
    g_categoryModules.clear();

    settings->beginGroup(QStringLiteral("openWith"));
    const int suffixCount = settings->beginReadArray(QStringLiteral("suffixModules"));
    for (int i = 0; i < suffixCount; ++i) {
        settings->setArrayIndex(i);
        SuffixOpenModule mod;
        mod.suffixesText = settings->value(QStringLiteral("suffixes")).toString();
        mod.entry = readEntry(settings, QStringLiteral("entry"));
        g_suffixModules.append(mod);
    }
    settings->endArray();

    for (const QString &cat : kCategoryOrder) {
        QVector<OpenWithEntry> list;
        const int n = settings->beginReadArray(QStringLiteral("category/") + cat);
        for (int i = 0; i < n; ++i) {
            settings->setArrayIndex(i);
            list.append(readEntry(settings, QStringLiteral("entry")));
        }
        settings->endArray();
        g_categoryModules[cat] = list;
    }
    settings->endGroup();

    bool hasCategoryModule = false;
    for (const QString &cat : kCategoryOrder) {
        if (!g_categoryModules.value(cat).isEmpty()) {
            hasCategoryModule = true;
            break;
        }
    }
    if (g_suffixModules.isEmpty() && !hasCategoryModule) {
        ensureSeedDefaults();
    }
}

void OpenWithConfig::save(QSettings *settings)
{
    settings->beginGroup(QStringLiteral("openWith"));
    settings->remove(QStringLiteral("suffixModules"));
    settings->beginWriteArray(QStringLiteral("suffixModules"));
    for (int i = 0; i < g_suffixModules.size(); ++i) {
        settings->setArrayIndex(i);
        settings->setValue(QStringLiteral("suffixes"), g_suffixModules.at(i).suffixesText);
        writeEntry(settings, QStringLiteral("entry"), g_suffixModules.at(i).entry);
    }
    settings->endArray();

    for (const QString &cat : kCategoryOrder) {
        settings->remove(QStringLiteral("category/") + cat);
        const QVector<OpenWithEntry> list = g_categoryModules.value(cat);
        settings->beginWriteArray(QStringLiteral("category/") + cat);
        for (int i = 0; i < list.size(); ++i) {
            settings->setArrayIndex(i);
            writeEntry(settings, QStringLiteral("entry"), list.at(i));
        }
        settings->endArray();
    }
    settings->endGroup();
}

QString OpenWithConfig::defaultCommandFor(const QFileInfo &fileInfo)
{
    return defaultEntryFor(fileInfo).fullCommand();
}

OpenWithEntry OpenWithConfig::defaultEntryFor(const QFileInfo &fileInfo)
{
    const QString suffix = FileUtils::getRealSuffix(fileInfo.fileName()).toLower();
    for (const SuffixOpenModule &mod : g_suffixModules) {
        if (!suffixMatchesModule(suffix, mod)) {
            continue;
        }
        if (!mod.entry.fullCommand().isEmpty()) {
            return mod.entry;
        }
    }
    const QString cat = categoryForSuffix(suffix);
    if (!cat.isEmpty()) {
        const QVector<OpenWithEntry> list = g_categoryModules.value(cat);
        for (const OpenWithEntry &e : list) {
            if (!e.fullCommand().isEmpty()) {
                return e;
            }
        }
    }
    return OpenWithEntry();
}

QList<OpenWithEntry> OpenWithConfig::handlersFor(const QFileInfo &fileInfo)
{
    QList<OpenWithEntry> result;
    QSet<QString> seenCommands;
    const QString suffix = FileUtils::getRealSuffix(fileInfo.fileName()).toLower();

    for (const SuffixOpenModule &mod : g_suffixModules) {
        if (!suffixMatchesModule(suffix, mod)) {
            continue;
        }
        const QString cmd = mod.entry.fullCommand();
        if (cmd.isEmpty() || seenCommands.contains(cmd)) {
            continue;
        }
        seenCommands.insert(cmd);
        result.append(mod.entry);
    }

    const QString cat = categoryForSuffix(suffix);
    if (!cat.isEmpty()) {
        for (const OpenWithEntry &e : g_categoryModules.value(cat)) {
            const QString cmd = e.fullCommand();
            if (cmd.isEmpty() || seenCommands.contains(cmd)) {
                continue;
            }
            seenCommands.insert(cmd);
            result.append(e);
        }
    }
    return result;
}
