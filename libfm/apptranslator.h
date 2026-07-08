#ifndef APPTRANSLATOR_H
#define APPTRANSLATOR_H

#include <QString>
#include <QStringList>

class QApplication;

namespace AppTranslator {

/** Settings value: "system", "en", or "zh_CN". */
QString normalizedLanguageCode(const QString &uiLanguage);

QStringList availableLanguageCodes();
QString languageDisplayName(const QString &code);

/** Removes previous app translators and installs Qt + app catalog for @p uiLanguage. */
void installForApplication(QApplication *app, const QString &uiLanguage);

} // namespace AppTranslator

#endif
