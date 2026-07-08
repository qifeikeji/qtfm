#include "apptranslator.h"

#include <QCoreApplication>

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QDir>
#include <QFile>

namespace {

QTranslator *s_qtTranslator = nullptr;
QTranslator *s_appTranslator = nullptr;

QString effectiveLocaleName(const QString &uiLanguage)
{
    const QString code = AppTranslator::normalizedLanguageCode(uiLanguage);
    if (code == QLatin1String("en")) {
        return QString();
    }
    if (code == QLatin1String("zh_CN")) {
        return QStringLiteral("zh_CN");
    }
    const QString sys = QLocale::system().name();
    if (sys.startsWith(QLatin1String("zh"))) {
        return QStringLiteral("zh_CN");
    }
    return QString();
}

void removeTranslators()
{
    if (s_appTranslator) {
        QApplication::removeTranslator(s_appTranslator);
        delete s_appTranslator;
        s_appTranslator = nullptr;
    }
    if (s_qtTranslator) {
        QApplication::removeTranslator(s_qtTranslator);
        delete s_qtTranslator;
        s_qtTranslator = nullptr;
    }
}

bool loadAppCatalog(QTranslator *tr, const QString &localeName)
{
    const QString fileName = QStringLiteral("qtfm_%1.qm").arg(localeName);
    const QString resourcePath = QStringLiteral(":/translations/qtfm_%1.qm").arg(localeName);
    if (tr->load(resourcePath)) {
        return true;
    }
    const QStringList candidates = {
        QCoreApplication::applicationDirPath()
            + QStringLiteral("/../share/qtfm/translations/") + fileName,
        QStringLiteral("/usr/share/qtfm/translations/") + fileName,
        QStringLiteral("/usr/local/share/qtfm/translations/") + fileName,
    };
    for (const QString &path : candidates) {
        if (path.startsWith(QLatin1Char(':'))) {
            if (tr->load(path)) {
                return true;
            }
            continue;
        }
        if (QFile::exists(path) && tr->load(path)) {
            return true;
        }
    }
    return false;
}

} // namespace

QString AppTranslator::normalizedLanguageCode(const QString &uiLanguage)
{
    const QString v = uiLanguage.trimmed();
    if (v.isEmpty() || v == QLatin1String("system")) {
        return QStringLiteral("system");
    }
    if (v == QLatin1String("en") || v.startsWith(QLatin1String("en_"))) {
        return QStringLiteral("en");
    }
    if (v == QLatin1String("zh_CN") || v == QLatin1String("zh")
        || v.startsWith(QLatin1String("zh_"))) {
        return QStringLiteral("zh_CN");
    }
    return v;
}

QStringList AppTranslator::availableLanguageCodes()
{
    return {QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("zh_CN")};
}

QString AppTranslator::languageDisplayName(const QString &code)
{
    const QString c = normalizedLanguageCode(code);
    if (c == QLatin1String("system")) {
        return QCoreApplication::translate("AppTranslator", "System default");
    }
    if (c == QLatin1String("en")) {
        return QStringLiteral("English");
    }
    if (c == QLatin1String("zh_CN")) {
        return QStringLiteral("简体中文");
    }
    return code;
}

void AppTranslator::installForApplication(QApplication *app, const QString &uiLanguage)
{
    Q_UNUSED(app)
    removeTranslators();

    const QString localeName = effectiveLocaleName(uiLanguage);
    if (localeName.isEmpty()) {
        return;
    }

    s_qtTranslator = new QTranslator;
    if (s_qtTranslator->load(QStringLiteral("qt_%1").arg(localeName),
                             QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(s_qtTranslator);
    } else {
        delete s_qtTranslator;
        s_qtTranslator = nullptr;
    }

    s_appTranslator = new QTranslator;
    if (loadAppCatalog(s_appTranslator, localeName)) {
        QApplication::installTranslator(s_appTranslator);
    } else {
        delete s_appTranslator;
        s_appTranslator = nullptr;
    }
}
