#ifndef OPENWITHCONFIG_H
#define OPENWITHCONFIG_H

#include <QFileInfo>
#include <QIcon>
#include <QList>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVector>

struct OpenWithEntry {
    QString name;
    QString commandLine1;
    QString commandLine2;
    QString iconPath;

    QString fullCommand() const;
    QIcon menuIcon() const;
};

struct SuffixOpenModule {
    QString suffixesText;
    OpenWithEntry entry;
};

/**
 * User-defined “Open with” handlers (settings tab). Higher priority than MIME defaults.
 */
class OpenWithConfig {
public:
    static const QStringList &categoryIds();
    static QString categoryTitle(const QString &categoryId);
    static QStringList suffixesForCategory(const QString &categoryId);
    static QString categoryForSuffix(const QString &suffix);

    static void load(QSettings *settings);
    static void save(QSettings *settings);
    static void ensureSeedDefaults();

    static QVector<SuffixOpenModule> &suffixModules();
    static QVector<OpenWithEntry> &categoryModules(const QString &categoryId);

    /** First matching handler: suffix module, else first in category. Empty if none. */
    static QString defaultCommandFor(const QFileInfo &fileInfo);
    static OpenWithEntry defaultEntryFor(const QFileInfo &fileInfo);

    /** Menu entries: suffix modules for this file, then category modules (deduped). */
    static QList<OpenWithEntry> handlersFor(const QFileInfo &fileInfo);
};

#endif
