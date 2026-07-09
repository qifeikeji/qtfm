#include "mimeutils.h"
#include "fileutils.h"

#include <QProcess>
#include <QDebug>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeType>
#include <QDir>
#include <QApplication>
#include <QUrl>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcessEnvironment>

#ifdef Q_OS_DARWIN
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#endif

#include "common.h"

namespace {

/** Escape a path for QProcess::splitCommand / shell -e (spaces stay one argument). */
QString quoteArgForCommandLine(const QString &arg)
{
    if (!arg.contains(QLatin1Char(' ')) && !arg.contains(QLatin1Char('\t'))
        && !arg.contains(QLatin1Char('"')) && !arg.contains(QLatin1Char('\''))
        && !arg.contains(QLatin1Char('\\'))) {
        return arg;
    }
    QString out;
    out.reserve(arg.size() + 4);
    out += QLatin1Char('"');
    for (const QChar ch : arg) {
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\\')) {
            out += QLatin1Char('\\');
        }
        out += ch;
    }
    out += QLatin1Char('"');
    return out;
}

#ifdef Q_OS_DARWIN
QString resolveMacAppBundlePath(const QString &candidate)
{
    if (candidate.isEmpty()) {
        return QString();
    }
    QFileInfo fi(candidate);
    if (candidate.endsWith(QLatin1String(".app"), Qt::CaseInsensitive) && fi.isBundle()) {
        return fi.canonicalFilePath();
    }
    const int dotApp = candidate.indexOf(QLatin1String(".app"), 0, Qt::CaseInsensitive);
    if (dotApp >= 0) {
        const QString bundlePath = candidate.left(dotApp + 4);
        const QFileInfo bundleInfo(bundlePath);
        if (bundleInfo.isBundle()) {
            return bundleInfo.canonicalFilePath();
        }
    }
    return QString();
}

bool openFileWithLaunchServices(const QFileInfo &file)
{
    if (!file.exists()) {
        return false;
    }
    CFURLRef ref = CFURLCreateWithFileSystemPath(
        nullptr, file.absoluteFilePath().toCFString(), kCFURLPOSIXPathStyle, file.isDir());
    if (!ref) {
        return false;
    }
    const OSStatus status = LSOpenCFURLRef(ref, nullptr);
    CFRelease(ref);
    return status == noErr;
}

/** Finder-launched apps often lack Homebrew on PATH; match terminal for CLI tools (mpv, etc.). */
QProcessEnvironment macGuiProcessEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value(QStringLiteral("PATH"));
    const QString prepend = QStringLiteral("/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin");
    if (!path.contains(QStringLiteral("/opt/homebrew/bin"))
        && !path.contains(QStringLiteral("/usr/local/bin"))) {
        env.insert(QStringLiteral("PATH"),
                   path.isEmpty() ? prepend : prepend + QLatin1Char(':') + path);
    }
    return env;
}

QString resolveMacCommandExecutable(const QString &program)
{
    if (program.isEmpty()) {
        return QString();
    }
    const QFileInfo direct(program);
    if (direct.isAbsolute() && direct.isExecutable()) {
        return direct.canonicalFilePath();
    }
    QString fromPath = QStandardPaths::findExecutable(program);
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
    const QStringList prefixes = {
        QStringLiteral("/opt/homebrew/bin/"),
        QStringLiteral("/usr/local/bin/"),
        QStringLiteral("/usr/bin/"),
    };
    for (const QString &prefix : prefixes) {
        const QString candidate = prefix + program;
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    return program;
}

bool startDetachedMac(const QString &program, const QStringList &arguments)
{
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(arguments);
    proc.setProcessEnvironment(macGuiProcessEnvironment());
    return proc.startDetached();
}

/** Parse Open-with template + file like Terminal: file path is never split on spaces. */
bool launchMacFromOpenWithTemplate(const QString &exeTemplate, const QFileInfo &file)
{
    if (!file.exists() || file.isDir()) {
        return false;
    }
    QString t = exeTemplate.trimmed();
    if (t.isEmpty()) {
        return false;
    }
    const QString fpath = file.absoluteFilePath();

    auto stripFilePlaceholders = [](QString &s) {
        s.replace(QStringLiteral("%F"), QString(), Qt::CaseInsensitive);
        s.replace(QStringLiteral("%f"), QString(), Qt::CaseInsensitive);
        s.replace(QStringLiteral("%U"), QString(), Qt::CaseInsensitive);
        s.replace(QStringLiteral("%u"), QString(), Qt::CaseInsensitive);
        while (s.contains(QStringLiteral("  "))) {
            s.replace(QStringLiteral("  "), QStringLiteral(" "));
        }
        s = s.trimmed();
    };
    stripFilePlaceholders(t);

    QString bundle = resolveMacAppBundlePath(t);
    if (!bundle.isEmpty()) {
        return startDetachedMac(QStringLiteral("/usr/bin/open"),
                                {QStringLiteral("-a"), bundle, fpath});
    }

    const int dotApp = t.indexOf(QLatin1String(".app"), 0, Qt::CaseInsensitive);
    if (dotApp >= 0) {
        const QString prefix = t.left(dotApp + 4);
        const QString suffix = t.mid(dotApp + 4).trimmed();
        bundle = resolveMacAppBundlePath(prefix);
        if (!bundle.isEmpty()) {
            QStringList openArgs;
            openArgs << QStringLiteral("-a") << bundle;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            if (!suffix.isEmpty()) {
                openArgs += QProcess::splitCommand(suffix);
            }
#else
            if (!suffix.isEmpty()) {
                openArgs += suffix.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            }
#endif
            openArgs << fpath;
            return startDetachedMac(QStringLiteral("/usr/bin/open"), openArgs);
        }
    }

    if (t.startsWith(QLatin1String("open"))) {
        QString rest = t.length() > 4 ? t.mid(4).trimmed() : QString();
        QStringList openArgs;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (!rest.isEmpty()) {
            openArgs = QProcess::splitCommand(rest);
        }
#else
        if (!rest.isEmpty()) {
            openArgs = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
#endif
        if (openArgs.size() == 1) {
            const QString only = resolveMacAppBundlePath(openArgs.at(0));
            if (!only.isEmpty()) {
                openArgs = QStringList{QStringLiteral("-a"), only};
            }
        }
        openArgs << fpath;
        return startDetachedMac(QStringLiteral("/usr/bin/open"), openArgs);
    }

    QStringList parts;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    parts = QProcess::splitCommand(t);
#else
    parts = t.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
    if (parts.isEmpty()) {
        return false;
    }
    const QString program = resolveMacCommandExecutable(parts.takeFirst());
    parts << fpath;
    return startDetachedMac(program, parts);
}
#endif

QString substituteSingleFileTokens(QString line, const QFileInfo &file)
{
    const QString path = quoteArgForCommandLine(file.absoluteFilePath());
    const QString url = QUrl::fromLocalFile(file.absoluteFilePath())
                            .toString(QUrl::FullyEncoded);
    if (line.contains(QStringLiteral("%F"), Qt::CaseInsensitive)) {
        line.replace(QStringLiteral("%F"), path, Qt::CaseInsensitive);
    } else if (line.contains(QStringLiteral("%U"), Qt::CaseInsensitive)) {
        line.replace(QStringLiteral("%U"), url, Qt::CaseInsensitive);
    } else if (line.contains(QStringLiteral("%f"), Qt::CaseInsensitive)) {
        line.replace(QStringLiteral("%f"), path, Qt::CaseInsensitive);
    } else if (line.contains(QStringLiteral("%u"), Qt::CaseInsensitive)) {
        line.replace(QStringLiteral("%u"), url, Qt::CaseInsensitive);
    } else {
        line = line.trimmed();
        if (!line.isEmpty()) {
            line += QLatin1Char(' ');
        }
        line += path;
    }
    return line;
}

bool startDetachedCommand(const QString &commandLine, const QString &termCmd,
                          const QFileInfo &contextFile = QFileInfo())
{
    const QString line = commandLine.trimmed();
    if (line.isEmpty()) {
        return false;
    }
#ifndef Q_OS_DARWIN
    Q_UNUSED(contextFile);
#endif

#ifdef Q_OS_DARWIN
    if (termCmd.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        QStringList parts = QProcess::splitCommand(line);
#else
        QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
        if (!parts.isEmpty()) {
            if (parts.at(0) == QLatin1String("open")) {
                return startDetachedMac(QStringLiteral("/usr/bin/open"), parts.mid(1));
            }
            const QString first = parts.at(0);
            QString bundlePath = resolveMacAppBundlePath(first);
            if (bundlePath.isEmpty()) {
                const QFileInfo firstInfo(first);
                if (first.endsWith(QLatin1String(".app"), Qt::CaseInsensitive)
                    || firstInfo.isBundle()) {
                    bundlePath = firstInfo.isBundle() ? firstInfo.canonicalFilePath() : first;
                }
            }
            if (!bundlePath.isEmpty()) {
                QStringList openArgs;
                openArgs << QStringLiteral("-a") << bundlePath;
                const QStringList rest = parts.mid(1);
                if (!rest.isEmpty()) {
                    openArgs << rest;
                } else if (contextFile.exists()) {
                    openArgs << contextFile.absoluteFilePath();
                }
                return startDetachedMac(QStringLiteral("/usr/bin/open"), openArgs);
            }
        }
    }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList parts = QProcess::splitCommand(line);
#else
    QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
    if (parts.isEmpty()) {
        return false;
    }
    const QString program = parts.takeFirst();
    if (program.isEmpty()) {
        return false;
    }
    if (!termCmd.isEmpty()) {
        return QProcess::startDetached(termCmd, QStringList() << QStringLiteral("-e") << line);
    }
#ifdef Q_OS_DARWIN
    {
        QString bundlePath = resolveMacAppBundlePath(program);
        if (bundlePath.isEmpty()) {
            const QFileInfo programInfo(program);
            if (program.endsWith(QLatin1String(".app"), Qt::CaseInsensitive)
                && programInfo.isBundle()) {
                bundlePath = programInfo.canonicalFilePath();
            }
        }
        if (!bundlePath.isEmpty()) {
            QStringList openArgs;
            openArgs << QStringLiteral("-a") << bundlePath;
            if (!parts.isEmpty()) {
                openArgs << parts;
            } else if (contextFile.exists()) {
                openArgs << contextFile.absoluteFilePath();
            }
            return startDetachedMac(QStringLiteral("/usr/bin/open"), openArgs);
        }
    }
    const QString resolved = resolveMacCommandExecutable(program);
    return startDetachedMac(resolved, parts);
#else
    return QProcess::startDetached(program, parts);
#endif
}

} // namespace


/**
 * @brief Creates mime utils
 * @param parent
 */
MimeUtils::MimeUtils(QObject *parent) : QObject(parent) {
  defaultsFileName = MIME_APPS;
  defaults = new Properties();
  loadDefaults();
}
//---------------------------------------------------------------------------

/**
 * @brief Loads list of default applications for mimes
 * @return properties with default applications
 */
void MimeUtils::loadDefaults() {
  defaults->load(QDir::homePath() + defaultsFileName, "Default Applications");
  defaultsChanged = false;
}
//---------------------------------------------------------------------------

/**
 * @brief Destructor
 */
MimeUtils::~MimeUtils() {
  delete defaults;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns mime type of given file
 * @note This operation is slow, prevent its mass application
 * @param path path to file
 * @return mime type
 */
QString MimeUtils::getMimeType(const QString &path) {
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForFile(path);
    //qDebug() << "mime type" << type.name() << path;
    return type.name();
}
//---------------------------------------------------------------------------

/**
 * @brief Returns list of mime types
 * @return list of available mimetypes
 */
QStringList MimeUtils::getMimeTypes() const {
    QStringList result = Common::getMimeTypes(qApp->applicationFilePath());
    //qDebug() << "getMimeTypes"  << result;
    return result;
}
//---------------------------------------------------------------------------

/**
 * @brief Opens file in a default application
 * @param file
 * @param processOwner
 */
void MimeUtils::openInApp(const QFileInfo &file, QString termCmd) {
    qDebug() << "openInApp without app";
  QString mime = getMimeType(file.absoluteFilePath());
  QString app = defaults->value(mime).toString().split(";").first();
  if (app.isEmpty() && mime.startsWith("text/") && mime != "text/plain") {
      // fallback for text
      app = defaults->value("text/plain").toString().split(";").first();
  }
  QString desktop = Common::findApplication(qApp->applicationFilePath(), app);
  qDebug() << "openInApp" << file.absoluteFilePath() << termCmd << mime << app << desktop;
  if (!desktop.isEmpty()) {
    DesktopFile df = DesktopFile(desktop);
    if (!df.isTerminal()) { termCmd.clear(); }
    else {
        if (termCmd.isEmpty()) { termCmd = "xterm"; }
    }
    openInApp(df.getExec(), file, termCmd);
  } else {
#ifdef Q_OS_DARWIN
      CFURLRef ref = CFURLCreateWithFileSystemPath(nullptr,
                                                   file.absoluteFilePath().toCFString(),
                                                   kCFURLPOSIXPathStyle,
                                                   file.isDir());
      LSOpenCFURLRef(ref, nullptr);
#else

     QString title = tr("No default application");
     QString msg = tr("No default application for mime: %1!").arg(mime);
     QMessageBox::warning(nullptr, title, msg);
#endif
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Opens file in a given application
 * @param exe name of application to be executed
 * @param file to be opened in executed application
 * @param processOwner process owner (default NULL)
 */
void MimeUtils::openInApp(QString exe, const QFileInfo &file,
                          QString termCmd) {

  qDebug() << "openInApp" << exe << file.absoluteFilePath() << termCmd;

  if (exe.contains(QStringLiteral("qpdfview"))) {
    exe = QStringLiteral("qpdfview");
  }

#ifdef Q_OS_DARWIN
  if (termCmd.isEmpty() && launchMacFromOpenWithTemplate(exe, file)) {
    return;
  }
#endif

  const QString commandLine = substituteSingleFileTokens(exe, file);
  qDebug() << "launch" << commandLine;
  if (startDetachedCommand(commandLine, termCmd, file)) {
    return;
  }
#ifdef Q_OS_DARWIN
  if (!exe.trimmed().isEmpty() && file.exists() && !file.isDir()) {
    qWarning() << "openInApp: custom command failed, using system default for"
               << file.absoluteFilePath();
    openFileWithLaunchServices(file);
  }
#endif
}

void MimeUtils::openFilesInApp(QString exe, const QStringList &files, QString termCmd)
{
    QString line = exe;
    if (line.contains(QStringLiteral("%F"), Qt::CaseInsensitive)) {
        QStringList quoted;
        for (const QString &p : files) {
            quoted << quoteArgForCommandLine(p);
        }
        line.replace(QStringLiteral("%F"), quoted.join(QLatin1Char(' ')), Qt::CaseInsensitive);
    } else if (line.contains(QStringLiteral("%U"), Qt::CaseInsensitive)) {
        QStringList urls;
        for (const QString &path : files) {
            urls << QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
        }
        line.replace(QStringLiteral("%U"), urls.join(QLatin1Char(' ')), Qt::CaseInsensitive);
    } else if (line.contains(QStringLiteral("%f"), Qt::CaseInsensitive)) {
        QStringList quoted;
        for (const QString &p : files) {
            quoted << quoteArgForCommandLine(p);
        }
        line.replace(QStringLiteral("%f"), quoted.join(QLatin1Char(' ')), Qt::CaseInsensitive);
    } else if (line.contains(QStringLiteral("%u"), Qt::CaseInsensitive)) {
        QStringList urls;
        for (const QString &path : files) {
            urls << QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
        }
        line.replace(QStringLiteral("%u"), urls.join(QLatin1Char(' ')), Qt::CaseInsensitive);
    } else {
        line = line.trimmed();
        for (const QString &p : files) {
            if (!line.isEmpty()) {
                line += QLatin1Char(' ');
            }
            line += quoteArgForCommandLine(p);
        }
    }

    startDetachedCommand(line, termCmd, QFileInfo(files.isEmpty() ? QString() : files.first()));
}
//---------------------------------------------------------------------------

/**
 * @brief Sets defaults file name (name of file where defaults are stored)
 * @param fileName
 */
void MimeUtils::setDefaultsFileName(const QString &fileName) {
  this->defaultsFileName = fileName;
  loadDefaults();
}
//---------------------------------------------------------------------------

/**
 * @brief Returns defaults file name
 * @return name of file where defaults are stored
 */
QString MimeUtils::getDefaultsFileName() const {
    return defaultsFileName;
}

QString MimeUtils::getAppForMimeType(const QString &mime) const
{
    return defaults->value(mime).toString().split(";").first();
}
//---------------------------------------------------------------------------

/**
 * @brief Generates default application-mime associations
 */
void MimeUtils::generateDefaults() {

  // Load list of applications
  QList<DesktopFile> apps = FileUtils::getApplications();
  QStringList names;

  // Find defaults; for each application...
  // ------------------------------------------------------------------------
  foreach (DesktopFile a, apps) {

  // ignore NoDisplay=true
  if (a.noDisplay()) { continue; }

    // For each mime of current application...
    QStringList mimes = a.getMimeType();
    foreach (QString mime, mimes) {

      // Current app name
      QString name = a.getPureFileName() + ".desktop";
      names.append(name);

      // If current mime is not mentioned in the list of defaults, add it
      // together with current application and continue
      if (!defaults->contains(mime)) {
        defaults->set(mime, name);
        defaultsChanged = true;
        continue;
      }

      // Retrieve list of default applications for current mime, if it does
      // not contain current application, add this application to list
      QStringList appNames = defaults->value(mime).toString().split(";");
      if (!appNames.contains(name)) {
        appNames.append(name);
        defaults->set(mime, appNames.join(";"));
        defaultsChanged = true;
      }
    }
  }

  // Delete dead defaults (non existing apps)
  // ------------------------------------------------------------------------
  foreach (QString key, defaults->getKeys()) {
    QStringList tmpNames1 = defaults->value(key).toString().split(";");
    QStringList tmpNames2 = QStringList();
    foreach (QString name, tmpNames1) {
      if (names.contains(name)) {
        tmpNames2.append(name);
      }
    }
    if (tmpNames1.size() != tmpNames2.size()) {
      defaults->set(key, tmpNames2.join(";"));
      defaultsChanged = true;
    }
  }

  // Save defaults if changed
  saveDefaults();
}
//---------------------------------------------------------------------------

/**
 * @brief Sets default mime association
 * @param mime mime name
 * @param apps list of applications (desktop file names)
 */
void MimeUtils::setDefault(const QString &mime, const QStringList &apps) {
  QString value = apps.join(";");
  if (value.compare(defaults->value(mime, "").toString()) != 0) {
    defaults->set(mime, value);
    defaultsChanged = true;
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Returns default applications for given mime
 * @param mime
 * @return list of default applications name
 */
QStringList MimeUtils::getDefault(const QString &mime) const {
  return defaults->value(mime).toString().split(";");
}
//---------------------------------------------------------------------------

/**
 * @brief Saves defaults
 */
void MimeUtils::saveDefaults() {
  if (defaultsChanged) {
    defaults->save(QDir::homePath() + defaultsFileName, "Default Applications");
    defaultsChanged = false;
  }
}
//---------------------------------------------------------------------------
