#include "fileutils.h"
#include <QDirIterator>
#include <QUrl>
#include <QApplication>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif

#include "common.h"
#include "bundledicons.h"

/**
 * @brief Recursive removes file or directory
 * @param path path to file
 * @param name name of file
 * @return true if file/directory was successfully removed
 */
bool FileUtils::removeRecurse(const QString &path, const QString &name) {

  // File location
  QString url = path + QDir::separator() + name;

  // Check whether file or directory exists
  QFileInfo file(url);
  if (!file.exists()) {
    return false;
  }

  // List of files that will be deleted
  QStringList files;

  // If given file is a directory, collect all children of given directory
  if (file.isDir()) {
    QDirIterator it(url, QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot
                    | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
      files.prepend(it.next());
    }
  }

  // Append given file to the list of files and delete all
  files.append(url);
  foreach (QString file, files) {
    QFile(file).remove();
  }
  return true;
}
//---------------------------------------------------------------------------

/**
 * @brief Collects all file names in given path (recursive)
 * @param path path
 * @param parent parent path
 * @param list resulting list of files
 */
void FileUtils::recurseFolder(const QString &path, const QString &parent,
                              QStringList *list) {

  // Get all files in this path
  QDir dir(path);
  QStringList files = dir.entryList(QDir::AllEntries | QDir::Files
                                    | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

  // Go through all files in current directory
  for (int i = 0; i < files.count(); i++) {

    // If current file is folder perform this method again. Otherwise add file
    // to list of results
    QString current = parent + QDir::separator() + files.at(i);
    if (QFileInfo(files.at(i)).isDir()) {
      recurseFolder(files.at(i), current, list);
    }
    else list->append(current);
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Returns size of all given files/dirs (including nested files/dirs)
 * @param files
 * @return total size
 */
qint64 FileUtils::totalSize(const QList<QUrl> &files) {
  qint64 total = 1;
  foreach (QUrl url, files) {
    QFileInfo file(url.path());
    if (file.isFile()) total += file.size();
    else {
      QDirIterator it(url.path(), QDir::AllEntries | QDir::System
                      | QDir::NoDotAndDotDot | QDir::NoSymLinks
                      | QDir::Hidden, QDirIterator::Subdirectories);
      while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
      }
    }
  }
  return total;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns names of available applications
 * @return application name list
 */
QStringList FileUtils::getApplicationNames() {
  QStringList appNames;
  QStringList locations = Common::applicationLocations(qApp->applicationFilePath());
  for (int i=0;i<locations.size();++i) {
      QDirIterator it1(locations.at(i), QStringList("*.desktop"),
                      QDir::Files | QDir::NoDotAndDotDot,
                      QDirIterator::Subdirectories);
      while (it1.hasNext()) {
        it1.next();
        appNames.append(it1.fileName());
      }
  }
  //qDebug() << "applications names" << appNames;
  return appNames;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns list of available applications
 * @return application list
 */
QList<DesktopFile> FileUtils::getApplications() {
  QList<DesktopFile> apps;
  QStringList locations = Common::applicationLocations(qApp->applicationFilePath());
  for (int i=0;i<locations.size();++i) {
      QDirIterator it(locations.at(i), QStringList("*.desktop"),
                      QDir::Files | QDir::NoDotAndDotDot,
                      QDirIterator::Subdirectories);
      while (it.hasNext()) {
        it.next();
        apps.append(DesktopFile(it.filePath()));
      }
  }
  return apps;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns real suffix for given file
 * @param name
 * @return suffix
 */
QString FileUtils::getRealSuffix(const QString &name) {

  // Strip version suffix
  QStringList tmp = name.split(".");
  bool ok;
  while (tmp.size() > 1) {
    tmp.last().toInt(&ok);
    if (!ok) {
      return tmp.last();
    }
    tmp.removeLast();
  }
  return "";
}
//---------------------------------------------------------------------------

/**
 * @brief Returns mime icon
 * @param mime
 * @return icon
 */
QIcon FileUtils::searchMimeIcon(QString mime, const QIcon &defaultIcon) {
  const QIcon icon = BundledIcons::iconForMimeType(mime);
  return icon.isNull() ? defaultIcon : icon;
}
//---------------------------------------------------------------------------

QIcon FileUtils::searchFolderIcon(const QFileInfo &info, const QIcon &defaultIcon)
{
    const QIcon icon = BundledIcons::iconForFolder(info);
    return icon.isNull() ? defaultIcon : icon;
}
//---------------------------------------------------------------------------

/**
 * @brief Searches for generic icon
 * @param category
 * @return icon
 */
QIcon FileUtils::searchGenericIcon(const QString &category,
                                   const QIcon &defaultIcon) {
  QIcon icon = BundledIcons::iconByName(category + "-generic");
  if (!icon.isNull()) {
    return icon;
  }
  icon = BundledIcons::iconByName(category + "-x-generic");
  return icon.isNull() ? defaultIcon : icon;
}
//---------------------------------------------------------------------------

/**
 * @brief Searches for application icon in the filesystem
 * @param app
 * @param defaultIcon
 * @return icon
 */
QIcon FileUtils::searchAppIcon(const DesktopFile &app, const QIcon &defaultIcon)
{
  if (QFile::exists(app.getIcon())) { return QIcon(app.getIcon()); }
  QIcon icon = BundledIcons::iconByName(app.getIcon());
  if (!icon.isNull()) { return icon; }
  const QString path = Common::findIcon(qApp->applicationFilePath(), QString(), app.getIcon());
  if (!path.isEmpty()) { return QIcon(path); }
  return defaultIcon;
}
//---------------------------------------------------------------------------
