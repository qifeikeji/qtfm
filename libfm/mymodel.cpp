/****************************************************************************
* This file is part of qtFM, a simple, fast file manager.
* Copyright (C) 2010,2011,2012 Wittfella
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*
* Contact e-mail: wittfella@qtfm.org
*
****************************************************************************/

#include "mymodel.h"
#include "bundledicons.h"
#include "dfmqstyleditemdelegate.h"
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <QApplication>
#include <QBuffer>
#include <QImageReader>
#include <QImageWriter>
#include <QMessageBox>
#include <QImage>
#include <QPainter>
#include <QSet>
#include <QtConcurrent/QtConcurrent>
#include "fileutils.h"

/**
 * @brief Creates file system model
 * @param realMime
 * @param mimeUtils
 */
myModel::myModel(bool realMime, MimeUtils *mimeUtils, QObject *parent)
    : QAbstractItemModel(parent) {

  // Stores mime utils
  mimeUtilsPtr = mimeUtils;

  // Initialization
  mimeGeneric = new QHash<QString,QString>;
  mimeGlob = new QHash<QString,QString>;
  mimeIcons = new QHash<QString,QIcon>;
  folderIcons = new QHash<QString,QIcon>;
  pathIconNames = new QHash<QString, QString>;
  thumbPaths = new QHash<QString, QString>;
  icons = new QCache<QString,QIcon>;
  icons->setMaxCost(500);
  thumbActiveJobs.storeRelaxed(0);

  // Loads cached mime icons
  QFile fileIcons(QString("%1/file.cache").arg(Common::configDir()));
  if (fileIcons.open(QIODevice::ReadOnly)) {
      QDataStream out(&fileIcons);
      out >> *mimeIcons;
      fileIcons.close();
      const QList<QString> staleKeys = mimeIcons->keys();
      for (const QString &key : staleKeys) {
          if (mimeIcons->value(key).isNull()) {
              mimeIcons->remove(key);
          }
      }
  }

  // Loads folder cache
  fileIcons.setFileName(QString("%1/folder.cache").arg(Common::configDir()));
  if (fileIcons.open(QIODevice::ReadOnly)) {
      QDataStream out(&fileIcons);
      out.setDevice(&fileIcons);
      out >> *folderIcons;
      fileIcons.close();
  }

  fileIcons.setFileName(QString("%1/path-icons.cache").arg(Common::configDir()));
  if (fileIcons.open(QIODevice::ReadOnly)) {
      QDataStream in(&fileIcons);
      in >> *pathIconNames;
      fileIcons.close();
  }

  // Create root item
  rootItem = new myModelItem(QFileInfo("/"), new myModelItem(QFileInfo(), nullptr));
  currentRootPath = "/";
  QDir root("/");
  QFileInfoList drives = root.entryInfoList( QDir::AllEntries | QDir::Files
                                            | QDir::Hidden | QDir::System
                                            | QDir::NoDotAndDotDot);

  // Create item per each drive
  foreach (QFileInfo drive, drives) {
    new myModelItem(drive, rootItem);
  }

  rootItem->walked = true;
  rootItem = rootItem->parent();

  iconFactory = new QFileIconProvider();

  inotifyFD = inotify_init();
  notifier = new QSocketNotifier(inotifyFD, QSocketNotifier::Read, this);
  connect(notifier, SIGNAL(activated(int)), this, SLOT(notifyChange()));
  connect(&eventTimer,SIGNAL(timeout()),this,SLOT(eventTimeout()));

  realMimeTypes = realMime;
}
//---------------------------------------------------------------------------

/**
 * @brief Deletes model of file system
 */
myModel::~myModel() {
  delete mimeGeneric;
  delete mimeGlob;
  delete mimeIcons;
  delete folderIcons;
  delete pathIconNames;
  delete thumbPaths;
  delete icons;
  delete rootItem;
  delete iconFactory;
}
//---------------------------------------------------------------------------

/**
 * @brief Deletes icon cache
 */
void myModel::clearIconCache() {
  folderIcons->clear();
  mimeIcons->clear();
  pathIconNames->clear();
  QFile(QString("%1/folder.cache").arg(Common::configDir())).remove();
  QFile(QString("%1/file.cache").arg(Common::configDir())).remove();
  QFile(QString("%1/path-icons.cache").arg(Common::configDir())).remove();
}

void myModel::setPathIcon(const QString &absolutePath, const QString &iconBaseName)
{
  if (absolutePath.isEmpty()) {
    return;
  }
  if (iconBaseName.isEmpty()) {
    pathIconNames->remove(absolutePath);
  } else {
    pathIconNames->insert(absolutePath, iconBaseName);
  }
  icons->remove(absolutePath);
  const QModelIndex idx = index(absolutePath);
  if (idx.isValid()) {
    emit dataChanged(idx, idx);
  }
}

void myModel::removePathIcon(const QString &absolutePath)
{
  setPathIcon(absolutePath, QString());
}

void myModel::forceRefresh()
{
    qDebug() << "force refresh model view";
    beginResetModel();
    endResetModel();
}
//---------------------------------------------------------------------------

/**
 * @brief Sets whether use real mime types or not
 * @param realMimeTypes
 */
void myModel::setRealMimeTypes(bool realMimeTypes) {
  this->realMimeTypes = realMimeTypes;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns true if real mime types are used
 * @return true if real mime types are used
 */
bool myModel::isRealMimeTypes() const {
  return realMimeTypes;
}

void myModel::setShowListDecorations(bool show)
{
    if (m_showListDecorations == show) {
        return;
    }
    m_showListDecorations = show;
    const QModelIndex root;
    const int rows = rowCount(root);
    const int cols = columnCount(root);
    if (rows <= 0 || cols <= 0) {
        return;
    }
    emit dataChanged(index(0, 0, root), index(rows - 1, cols - 1, root));
}

bool myModel::showListDecorations() const
{
    return m_showListDecorations;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns mime utils
 * @return mime utils
 */
MimeUtils* myModel::getMimeUtils() const {
  return mimeUtilsPtr;
}
//---------------------------------------------------------------------------

QModelIndex myModel::index(int row, int column, const QModelIndex &parent) const
{
    if(parent.isValid() && parent.column() != 0) { return QModelIndex(); }

    myModelItem *parentItem = static_cast<myModelItem*>(parent.internalPointer());
    if(!parentItem) { parentItem = rootItem; }

    myModelItem *childItem = parentItem->childAt(row);
    if(childItem) { return createIndex(row, column, childItem); }

    return QModelIndex();
}

//---------------------------------------------------------------------------------------
QModelIndex myModel::index(const QString& path) const
{
    myModelItem *item = rootItem->matchPath(path.split(SEPARATOR),0);
    if(item) { return createIndex(item->childNumber(),0,item); }
    return QModelIndex();
}

//---------------------------------------------------------------------------------------
QModelIndex myModel::parent(const QModelIndex &index) const
{
    if(!index.isValid()) { return QModelIndex(); }

    myModelItem *childItem = static_cast<myModelItem*>(index.internalPointer());

    if(!childItem) { return QModelIndex(); }

    myModelItem *parentItem = childItem->parent();

    if (!parentItem || parentItem == rootItem) { return QModelIndex(); }

    return createIndex(parentItem->childNumber(), 0, parentItem);
}

//---------------------------------------------------------------------------------------
bool myModel::isDir(const QModelIndex &index)
{
    if (!index.isValid()) { return false; }
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());
    if(item && item != rootItem) { return item->fileInfo().isDir(); }
    return false;
}

//---------------------------------------------------------------------------------------
QFileInfo myModel::fileInfo(const QModelIndex &index)
{
    if (!index.isValid()) { return QFileInfo(); }
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());
    if(item) { return item->fileInfo(); }
    return QFileInfo();
}

//---------------------------------------------------------------------------------------
qint64 myModel::size(const QModelIndex &index)
{
    if (!index.isValid()) { return 0; }
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());
    if(item) { return item->fileInfo().size(); }
    return 0;
}

//---------------------------------------------------------------------------------------
QString myModel::fileName(const QModelIndex &index)
{
    if (!index.isValid()) { return QString(); }
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());
    if(item) { return item->fileName(); }
    return QString();
}

//---------------------------------------------------------------------------------------
QString myModel::filePath(const QModelIndex &index)
{
    if (!index.isValid()) { return QString(); }
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());
    if(item) { return item->absoluteFilePath(); }
    return QString();
}

//---------------------------------------------------------------------------------------
QString myModel::getMimeType(const QModelIndex &index)
{
    if (!index.isValid()) { return QString(); }
    qDebug() << "myModel getMimeType";
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());
    if(item->mMimeType.isNull()) {
        if(realMimeTypes) { item->mMimeType = mimeUtilsPtr->getMimeType(item->absoluteFilePath()); }
        else {
            if(item->fileInfo().isDir()) item->mMimeType = "folder";
            else item->mMimeType = item->fileInfo().suffix();
            if(item->mMimeType.isNull()) item->mMimeType = "file";
        }
    }
    qDebug() << "item mime" << item->absoluteFilePath() << item->mMimeType;
    return item->mMimeType;
}

//---------------------------------------------------------------------------------------
void myModel::notifyChange()
{
    notifier->setEnabled(0);

    int buffSize = 0;
    ioctl(inotifyFD, FIONREAD, (char *) &buffSize);

    QByteArray buffer;
    buffer.resize(buffSize);
    read(inotifyFD,buffer.data(),buffSize);
    const char *at = buffer.data();
    const char * const end = at + buffSize;

    while (at < end) {
        const inotify_event *event = reinterpret_cast<const inotify_event *>(at);
        int w = event->wd;
        lastEventFilename = event->name;
        if (eventTimer.isActive()) {
            if (w == lastEventID) {
                eventTimer.start(40);
            } else {
                eventTimer.stop();
                notifyProcess(lastEventID, lastEventFilename);
                lastEventID = w;
                eventTimer.start(40);
            }
        } else {
            lastEventID = w;
            eventTimer.start(40);
        }
        at += sizeof(inotify_event) + event->len;
    }

    notifier->setEnabled(1);
    //if (!lastEventFilename.isEmpty()) { emit reloadDir(); }
}

//---------------------------------------------------------------------------------------
void myModel::eventTimeout()
{
    notifyProcess(lastEventID, lastEventFilename);
    eventTimer.stop();
}

//---------------------------------------------------------------------------------------
void myModel::notifyProcess(int eventID, QString fileName)
{
    qDebug() << "notifyProcess" << eventID << fileName;
    QString folderChanged;
    if (watchers.contains(eventID)) {
        myModelItem *parent = rootItem->matchPath(watchers.value(eventID).split(SEPARATOR));
        if (parent) {
            parent->dirty = 1;
            QDir dir(parent->absoluteFilePath());
            folderChanged = dir.absolutePath();
            QFileInfoList all = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            foreach(myModelItem * child, parent->children()) {
                if (all.contains(child->fileInfo())) {
                    //just remove known items
                    all.removeOne(child->fileInfo());
                } else {
                    //must have been deleted, remove from model
                    if (child->fileInfo().isDir()) {
                        int wd = watchers.key(child->absoluteFilePath());
                        inotify_rm_watch(inotifyFD,wd);
                        watchers.remove(wd);
                    }
                    beginRemoveRows(index(parent->absoluteFilePath()),child->childNumber(),child->childNumber());
                    parent->removeChild(child);
                    endRemoveRows();
                }
            }
            foreach(QFileInfo one, all) { //only new items left in list
                beginInsertRows(index(parent->absoluteFilePath()),parent->childCount(),parent->childCount());
                new myModelItem(one,parent);
                endInsertRows();
            }
        }
    } else {
        inotify_rm_watch(inotifyFD,eventID);
        watchers.remove(eventID);
    }
    if (!fileName.isEmpty() && showThumbs) {
        lastEventFilename = fileName;
    }
    if (!folderChanged.isEmpty()) {
        qDebug() << "folder modified" << folderChanged;
        emit reloadDir(folderChanged);
    }
}

//---------------------------------------------------------------------------------
void myModel::addWatcher(myModelItem *item)
{
    qDebug() << "addWatcher" << item->absoluteFilePath();
    while(item != rootItem) {
        watchers.insert(inotify_add_watch(inotifyFD, item->absoluteFilePath().toLocal8Bit(), IN_CREATE | IN_MODIFY | IN_MOVE | IN_DELETE),item->absoluteFilePath()); //IN_ONESHOT | IN_ALL_EVENTS)
        item->watched = 1;
        item = item->parent();
    }
}

//---------------------------------------------------------------------------------
bool myModel::setRootPath(const QString& path)
{
    if (path != currentRootPath) {
        QMutexLocker lock(&thumbMutex);
        thumbQueue.clear();
    }
    currentRootPath = path;

    myModelItem *item = rootItem->matchPath(path.split(SEPARATOR));

    if (item == nullptr) {
        QMessageBox::warning(nullptr, tr("No such directory"), tr("Directory requested does not exists."));
        return false;
    }

    if (!item->watched) { addWatcher(item); }
    if (!item->walked || !item->watched) {
        populateItem(item);
        return false;
    } else {
        if(item->dirty) { //model is up to date, but view needs to be invalidated
            item->dirty = false;
            return true;
        }
    }
    return false;
}

QString myModel::getRootPath()
{
    return currentRootPath;
}

//---------------------------------------------------------------------------------------
bool myModel::canFetchMore (const QModelIndex & parent) const
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());

    if(item) {
        if(item->walked) { return false; }
    }

    return true;

}

bool myModel::hasChildren(const QModelIndex &parent) const
{
    if (!parent.isValid()) { return true; }
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());
    if (item && item->fileInfo().isDir()) {
        if (QDir(item->fileInfo()
                 .absoluteFilePath())
                .entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries|QDir::System)
                .count() > 0)
        {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------
void myModel::fetchMore (const QModelIndex & parent)
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());

    if (item) {
        populateItem(item);
        emit dataChanged(parent,parent);
    }
}

//---------------------------------------------------------------------------------------
void myModel::populateItem(myModelItem *item)
{
    if (item == nullptr) { return; }
    item->walked = 1;

    QDir dir(item->absoluteFilePath());
    QFileInfoList all = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

    foreach(QFileInfo one, all) { new myModelItem(one,item); }
}

//---------------------------------------------------------------------------------
int myModel::columnCount(const QModelIndex &parent) const
{
    return (parent.column() > 0) ? 0 : LIST_COLUMN_COUNT;
}

//---------------------------------------------------------------------------------------
int myModel::rowCount(const QModelIndex &parent) const
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());
    if(item) { return item->childCount(); }
    return rootItem->childCount();
}

//---------------------------------------------------------------------------------
void myModel::refresh()
{
    myModelItem *item = rootItem->matchPath(QStringList("/"));

    //free all inotify watches
    foreach(int w, watchers.keys()) { inotify_rm_watch(inotifyFD,w); }
    watchers.clear();

    beginResetModel();
    if (item) { item->clearAll(); }
    endResetModel();
}

//---------------------------------------------------------------------------------
void myModel::update()
{
    myModelItem *item = rootItem->matchPath(currentRootPath.split(SEPARATOR));
    if (item == nullptr) { return; }
    foreach(myModelItem *child, item->children()) { child->refreshFileInfo(); }
}

//---------------------------------------------------------------------------------
void myModel::refreshItems()
{
    myModelItem *item = rootItem->matchPath(currentRootPath.split(SEPARATOR));
    if (item == nullptr) { return; }
    qDebug() << "refresh items";
    item->clearAll();
    populateItem(item);
}

//---------------------------------------------------------------------------------
QModelIndex myModel::insertFolder(QModelIndex parent)
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());

    int num = 0;
    QString name;

    do {
        num++;
        name = QString("new_folder%1").arg(num);
    }
    while(item->hasChild(name));


    QDir temp(currentRootPath);
    if(!temp.mkdir(name)) { return QModelIndex(); }

    beginInsertRows(parent,item->childCount(),item->childCount());
    new myModelItem(QFileInfo(currentRootPath + "/" + name),item);
    endInsertRows();

    return index(item->childCount() - 1, COLUMN_NAME, parent);
}

//---------------------------------------------------------------------------------
QModelIndex myModel::insertFile(QModelIndex parent)
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());

    int num = 0;
    QString name;

    do {
        num++;
        name = QString("new_file%1").arg(num);
    }
    while (item->hasChild(name));


    QFile temp(currentRootPath + "/" + name);
    if(!temp.open(QIODevice::WriteOnly)) { return QModelIndex(); }
    temp.close();

    beginInsertRows(parent,item->childCount(),item->childCount());
    new myModelItem(QFileInfo(temp),item);
    endInsertRows();

    return index(item->childCount() - 1, COLUMN_NAME, parent);
}

//---------------------------------------------------------------------------------
QModelIndex myModel::insertFileWithSuffix(QModelIndex parent, const QString &suffix)
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());
    if (item == nullptr || suffix.isEmpty()) {
        return QModelIndex();
    }

    int num = 0;
    QString name;
    do {
        num++;
        if (num == 1) {
            name = QStringLiteral("untitled.%1").arg(suffix);
        } else {
            name = QStringLiteral("untitled_%1.%2").arg(num).arg(suffix);
        }
    } while (item->hasChild(name));

    const QString path = currentRootPath + SEPARATOR + name;
    QFile temp(path);
    if (!temp.open(QIODevice::WriteOnly)) {
        return QModelIndex();
    }
    temp.close();

    beginInsertRows(parent, item->childCount(), item->childCount());
    new myModelItem(QFileInfo(path), item);
    endInsertRows();

    return index(item->childCount() - 1, COLUMN_NAME, parent);
}

//---------------------------------------------------------------------------------
Qt::DropActions myModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

//---------------------------------------------------------------------------------
QStringList myModel::mimeTypes() const
{
    return QStringList("text/uri-list");
}

//---------------------------------------------------------------------------------
QMimeData * myModel::mimeData(const QModelIndexList & indexes) const
{
    QMimeData *data = new QMimeData();

    QList<QUrl> files;

    foreach(QModelIndex index, indexes) {
        myModelItem *item = static_cast<myModelItem*>(index.internalPointer());
        QUrl url = QUrl::fromLocalFile(item->absoluteFilePath());
        if (!files.contains(url)) { files.append(url); }
    }

    data->setUrls(files);
    return data;
}


//---------------------------------------------------------------------------------
void myModel::cacheInfo()
{
    QFile fileIcons(QString("%1/file.cache").arg(Common::configDir()));
    if (fileIcons.open(QIODevice::WriteOnly)) {
        QDataStream out(&fileIcons);
        out << *mimeIcons;
        fileIcons.close();
    }

    fileIcons.setFileName(QString("%1/path-icons.cache").arg(Common::configDir()));
    if (fileIcons.open(QIODevice::WriteOnly)) {
        QDataStream out(&fileIcons);
        out << *pathIconNames;
        fileIcons.close();
    }

    fileIcons.setFileName(QString("%1/folder.cache").arg(Common::configDir()));
    if (fileIcons.open(QIODevice::WriteOnly)) {
        QDataStream out(&fileIcons);
        out.setDevice(&fileIcons);
        out << *folderIcons;
        fileIcons.close();
    }
}

//---------------------------------------------------------------------------

/**
 * @brief Sets indicator whether show thumbnails of pictures
 * @param icons
 */
void myModel::setMode(bool icons) {
  showThumbs = icons;
}
//---------------------------------------------------------------------------

/**
 * @brief Loads mime types
 */
void myModel::loadMimeTypes() const {
    QMapIterator<QString, QString> globs(Common::getMimesGlobs(qApp->applicationFilePath()));
    while(globs.hasNext()) {
        globs.next();
        mimeGlob->insert(globs.value(), globs.key());
    }
    QMapIterator<QString, QString> generic(Common::getMimesGeneric(qApp->applicationFilePath()));
    while(generic.hasNext()) {
        generic.next();
        mimeGeneric->insert(generic.key(), generic.value());
    }
}
//---------------------------------------------------------------------------

/**
 * @brief Loads thumbnails (disk cache + queued background generation)
 * @param indexes
 */
void myModel::loadThumbs(QModelIndexList indexes) {

  QStringList files;
  QString listDir;

  for (const QModelIndex &item : indexes) {
    const QString filename = filePath(item);
    if (filename.isEmpty()) {
      continue;
    }
    if (listDir.isEmpty()) {
      listDir = QFileInfo(filename).absolutePath();
    }
    if (fileWantsThumbnail(filename, mimeUtilsPtr)) {
      files.append(filename);
    }
  }

  if (files.isEmpty()) {
    return;
  }

  bool scheduled = false;
  {
    QMutexLocker lock(&thumbMutex);
    for (const QString &path : files) {
      const QString cache = Common::thumbnailCacheFile(path);
      if (Common::isThumbnailCacheValid(path, cache)) {
        thumbPaths->insert(path, cache);
        continue;
      }
      if (!thumbQueue.contains(path)) {
        if (thumbQueue.size() >= 512) {
          continue;
        }
        thumbQueue.append(path);
        scheduled = true;
      }
    }
  }

  emit thumbUpdate(listDir);

  if (scheduled) {
    QMetaObject::invokeMethod(this, "pumpThumbnailQueue", Qt::QueuedConnection);
  }
}

namespace {

QMutex g_thumbnailGeneratorMutex;

} // namespace

void myModel::pumpThumbnailQueue()
{
  constexpr int kMaxConcurrent = 1;
  while (thumbActiveJobs.loadRelaxed() < kMaxConcurrent) {
    QString path;
    QString itemMime;
    {
      QMutexLocker lock(&thumbMutex);
      if (thumbQueue.isEmpty()) {
        return;
      }
      path = thumbQueue.takeFirst();
      itemMime = mimeUtilsPtr->getMimeType(path);
    }

    thumbActiveJobs.ref();
    QtConcurrent::run([this, path, itemMime]() {
      QString cache;
      {
        QMutexLocker genLock(&g_thumbnailGeneratorMutex);
        cache = generateThumbnailToCache(path, itemMime);
      }
      if (!cache.isEmpty()) {
        QMutexLocker lock(&thumbMutex);
        thumbPaths->insert(path, cache);
      }
      thumbActiveJobs.deref();
      const QString dir = QFileInfo(path).absolutePath();
      QMetaObject::invokeMethod(this, [this, dir]() {
        emit thumbUpdate(dir);
        pumpThumbnailQueue();
      }, Qt::QueuedConnection);
    });
  }
}

//---------------------------------------------------------------------------

bool myModel::fileWantsThumbnail(const QString &path, MimeUtils *mimeUtils)
{
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    return false;
  }
  if (path.endsWith(QLatin1String(".desktop"))) {
    return true;
  }
  const QString mime = mimeUtils->getMimeType(path);
  if (mime.startsWith(QLatin1String("image/"))
      || mime.startsWith(QLatin1String("video/"))) {
    return true;
  }
  if (mime == QLatin1String("application/pdf")) {
    return true;
  }
  if (mime.startsWith(QLatin1String("audio/"))) {
    return true;
  }
  static const QSet<QString> kExt = {
      QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
      QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"),
      QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("heic"),
      QStringLiteral("heif"), QStringLiteral("avif"), QStringLiteral("svg"),
      QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("mov"),
      QStringLiteral("avi"), QStringLiteral("webm"), QStringLiteral("m4v"),
      QStringLiteral("mpeg"), QStringLiteral("mpg"), QStringLiteral("wmv"),
      QStringLiteral("pdf"),
  };
  return kExt.contains(FileUtils::getRealSuffix(path).toLower());
}

QString myModel::generateThumbnailToCache(const QString &item, const QString &itemMime)
{
  if (item.isEmpty()) {
    return QString();
  }

  if (item.endsWith(QLatin1String(".desktop"))) {
    const QString iconFile = Common::findIcon(
        QString(), QIcon::themeName(), Common::getDesktopIcon(item));
    if (!iconFile.isEmpty()) {
      QImage img(iconFile);
      if (!img.isNull()) {
        return Common::writeThumbnailForFile(item, img);
      }
    }
    return QString();
  }

  const QString mime = itemMime;

  static const QSet<QString> kVideoExt = {
      QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("mov"),
      QStringLiteral("avi"), QStringLiteral("webm"), QStringLiteral("m4v"),
      QStringLiteral("mpeg"), QStringLiteral("mpg"), QStringLiteral("wmv"),
      QStringLiteral("flv"), QStringLiteral("ts"), QStringLiteral("m2ts"),
  };
  const QString ext = FileUtils::getRealSuffix(item).toLower();
  const bool isVideo = mime.startsWith(QLatin1String("video/"))
                       || kVideoExt.contains(ext);
  const bool isAudio = mime.startsWith(QLatin1String("audio/"));
  if (isVideo || isAudio) {
    // Decoding is done by the external ffmpeg binary (see
    // Common::videoFirstFrameImage) so a malformed/hostile media file can
    // only crash a short-lived helper process, never qtfm itself.
    const QImage img = Common::videoFirstFrameImage(item);
    if (!img.isNull()) {
      return Common::writeThumbnailForFile(item, img);
    }
    return QString();
  }

  if (itemMime == QLatin1String("application/pdf")
      || item.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
    const QImage pdfImg = Common::pdfFirstPageImage(item);
    if (!pdfImg.isNull()) {
      return Common::writeThumbnailForFile(item, pdfImg);
    }
    return QString();
  }

  if (itemMime.startsWith(QLatin1String("image/"))
      || item.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
    // fall through to QImageReader below
  } else if (!itemMime.startsWith(QLatin1String("image/"))) {
    const QString ext = FileUtils::getRealSuffix(item).toLower();
    static const QSet<QString> kImageExt = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"),
        QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("heic"),
        QStringLiteral("heif"), QStringLiteral("avif"), QStringLiteral("svg"),
    };
    if (!kImageExt.contains(ext)) {
      return QString();
    }
  }

  QImageReader pic(item);
  pic.setAutoTransform(true);
  if (!pic.canRead()) {
    return QString();
  }
  const QSize srcSize = pic.size();
  if (srcSize.isValid() && (srcSize.width() > Common::thumbnailPixelSize
                            || srcSize.height() > Common::thumbnailPixelSize)) {
    pic.setScaledSize(srcSize.scaled(Common::thumbnailPixelSize,
                                     Common::thumbnailPixelSize,
                                     Qt::KeepAspectRatio));
  }
  const QImage img = pic.read();
  if (img.isNull()) {
    return QString();
  }
  return Common::writeThumbnailForFile(item, img);
}

//---------------------------------------------------------------------------

/**
 * @brief Returns model data (information about directories and files)
 * @param index
 * @param role
 * @return model data
 */
QVariant myModel::data(const QModelIndex & index, int role) const {
  // Retrieve model item
  myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

  // Color of filename (depends on file type)
  if (role == Qt::ForegroundRole) {
    if (!Common::readSetting("fileColor").toBool()) { return colors.windowText(); }
    QFileInfo type(item->fileInfo());
    if (cutItems.contains(type.filePath())) {
      return colors.windowText();
    } else if (type.isHidden()) {
      return colors.dark();
    } else if (type.isSymLink()) {
      return colors.link();
    } else if (type.isDir()) {
      return colors.windowText();
    } else if (type.isExecutable()) {
      return QBrush(QColor(Qt::darkGreen));
    }
  }
  // Alignment of filename
  else if (role == Qt::TextAlignmentRole) {
    if (index.column() == COLUMN_SIZE) {
      return Qt::AlignRight + Qt::AlignVCenter;
    }
  }
  // Display information about file
  else if (role == Qt::DisplayRole) {
    QVariant data;
    switch (index.column()) {
      case COLUMN_ICON :
        data = m_showListDecorations ? item->fileName() : QString();
        break;
      case COLUMN_NAME :
        data = item->fileName();
        break;
      case COLUMN_SIZE :
        data = item->fileInfo().isDir() ? "" : Common::formatSize(
               item->fileInfo().size());
        break;
      case COLUMN_DATE :
        data = QLocale::system().toString(item->fileInfo().lastModified(),
                                          QLocale::FormatType::ShortFormat);
        break;
      case COLUMN_FORMAT :
        if (item->mMimeType.isNull()) {
          if (realMimeTypes) {
            item->mMimeType = mimeUtilsPtr->getMimeType(item->absoluteFilePath());
          } else {
            item->mMimeType = item->fileInfo().isDir() ? "folder" :
                              item->fileInfo().suffix();
            if (item->mMimeType.isNull()) { item->mMimeType = "file"; }
          }
        }
        data = item->mMimeType;
        break;
      case COLUMN_FOLDER :
        data = item->fileInfo().isDir() ? QObject::tr("Folder") : QObject::tr("File");
        break;
      default :
        data = "";
        break;
    }
    return data;
  }
  // Display file icon
  else if (role == Qt::DecorationRole) {
    if (!m_showListDecorations && index.column() == COLUMN_ICON) {
      QString mime;
      if (!item->fileInfo().isDir()) {
        if (realMimeTypes) {
          if (item->mMimeType.isNull()) {
            item->mMimeType = mimeUtilsPtr->getMimeType(item->absoluteFilePath());
          }
          mime = item->mMimeType;
        } else {
          mime = item->fileInfo().suffix();
        }
      }
      return BundledIcons::iconForListCategory(item->fileInfo(), mime);
    }
    if (m_showListDecorations
        && (index.column() == COLUMN_ICON || index.column() == COLUMN_NAME)) {
      return findIcon(item);
    }
    return QVariant();
  }
  // Display file name
  else if(role == Qt::EditRole) {
    return item->fileName();
  }

  if (role == Qt::StatusTipRole) {
    return item->fileName();
  }
  return QVariant();
}
//---------------------------------------------------------------------------

/**
 * @brief Finds icon of a file
 * @param item
 * @return icon
 */
QVariant myModel::findIcon(myModelItem *item) const {

  if (item == nullptr) { return  QIcon(); }

  //qDebug() << "findicon" << item->absoluteFilePath();
  // If type of file is directory, return icon of directory
  QFileInfo type(item->fileInfo());
  const QString absPath = type.absoluteFilePath();
  if (pathIconNames->contains(absPath)) {
    const QIcon custom = BundledIcons::iconByName(pathIconNames->value(absPath));
    if (!custom.isNull()) {
      return custom;
    }
    pathIconNames->remove(absPath);
  }

  if (type.isDir()) {
    if (folderIcons->contains(type.fileName())) {
      return folderIcons->value(type.fileName());
    }
#ifdef Q_OS_MAC
    if (type.fileName().endsWith(".app")) {
        return iconFactory->icon(type);
    }
#endif
    return FileUtils::searchFolderIcon(type, iconFactory->icon(type));
  }

  // AppImage bundles the app's icon (often qtfm); use generic unknown-file icon instead.
  if (type.fileName().endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive)) {
    return BundledIcons::emptyIcon();
  }

    // If thumbnails are allowed and current file has it, show it
    if (showThumbs) {
        if (icons->contains(item->absoluteFilePath())) {
            qDebug() << "USING ICON CACHE FOR" << item->absoluteFilePath();
            return *icons->object(item->absoluteFilePath());
        } else if (thumbPaths->contains(item->absoluteFilePath())) {
            qDebug() << "USING THUMB CACHE FOR" << item->absoluteFilePath();
            QPixmap pic;
            pic.load(thumbPaths->value(item->absoluteFilePath()));
            if (!pic.isNull()) {
                icons->insert(item->absoluteFilePath(), new QIcon(pic), 1);
                return *icons->object(item->absoluteFilePath());
            }
        } else if (!Common::hasThumbnail(item->absoluteFilePath()).isEmpty()) {
            qDebug() << "USING XDG CACHE FOR" << item->absoluteFilePath();
            QPixmap pic;
            pic.load(Common::hasThumbnail(item->absoluteFilePath()));
            icons->insert(item->absoluteFilePath(), new QIcon(pic), 1);
            return *icons->object(item->absoluteFilePath());
        }
    }

  // NOTE: Suffix is resolved using method getRealSuffix instead of suffix()
  // method. It is because files can contain version suffix e.g. .so.1.0.0

  // If there is icon for current suffix then return it
  QString suffix = FileUtils::getRealSuffix(type.fileName()); /*type.suffix();*/
  if (mimeIcons->contains(suffix)) {
      const QIcon cached = mimeIcons->value(suffix);
      if (!cached.isNull()) {
          qDebug() << "USING SUFFIX ICON FOR" << suffix << item->absoluteFilePath();
          return cached;
      }
      mimeIcons->remove(suffix);
  }

  // The icon
  QIcon theIcon;

  // If file has not suffix
  if (suffix.isEmpty()) {

    // If file is not executable, read mime type info from the system and create
    // an icon for it
    // NOTE: the icon cannot be cached because this file has not any suffix,
    // however operation 'getMimeType' could cause slowdown
    if (!type.isExecutable()) {
      QString mime = mimeUtilsPtr->getMimeType(type.absoluteFilePath());
      qDebug() << "USING MIME ICON FOR" << mime << item->absoluteFilePath();
      return FileUtils::searchMimeIcon(mime);
    }

    // If file is executable, set suffix to exec and find/create icon for it
    suffix = "exec";
    if (mimeIcons->contains(suffix)) {
      theIcon = mimeIcons->value(suffix);
      if (theIcon.isNull()) {
          mimeIcons->remove(suffix);
          theIcon = BundledIcons::iconForExecutable();
      }
    } else {
      theIcon = BundledIcons::iconForExecutable();
    }
  }
  // If file has unknown suffix (icon hasn't been assigned)
  else {

    // Load mime/suffix associations if they aren't loaded yet
    if (mimeGlob->count() == 0) loadMimeTypes();

    // Retrieve mime type for current suffix, if suffix is not present in list
    // from '/usr/share/mime/globs', its mime has to be detected manually
    QString mimeType = mimeGlob->value(suffix.toLower(), "");
    if (mimeType.isEmpty()) {
      mimeType = mimeUtilsPtr->getMimeType(type.absoluteFilePath());
      mimeGlob->insert(suffix.toLower(), mimeType);
    }

    // Load icon by extension (built-in set under share/icons/mimes/)
    theIcon = BundledIcons::iconForFileSuffix(suffix);
  }

  if (theIcon.isNull()) {
    theIcon = BundledIcons::emptyIcon();
  }
  mimeIcons->insert(suffix, theIcon);
  return theIcon;
}
//---------------------------------------------------------------------------

/**
 * @brief Finds icon of a file, based on file mime type
 * @param item
 * @return icon
 */
QVariant myModel::findMimeIcon(myModelItem *item) const {

  if (item == nullptr) { return QIcon(); }

  // Retrieve mime and search cache for it
  QString mime = mimeUtilsPtr->getMimeType(item->absoluteFilePath());
  if (mimeIcons->contains(mime)) {
    const QIcon cached = mimeIcons->value(mime);
    if (!cached.isNull()) {
      return cached;
    }
    mimeIcons->remove(mime);
  }

  // Search file system for icon
  QIcon theIcon = FileUtils::searchMimeIcon(mime);
  if (theIcon.isNull()) {
    theIcon = BundledIcons::emptyIcon();
  }
  mimeIcons->insert(mime, theIcon);
  return theIcon;
}

//---------------------------------------------------------------------------

bool myModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    Q_UNUSED(role)

    //can only set the filename
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    //physically change the name on disk
    bool ok = QFile::rename(item->absoluteFilePath(),item->parent()->absoluteFilePath() + SEPARATOR + value.toString());

    //change the details in the modelItem
    if(ok) {
        item->mMimeType.clear();                //clear the suffix/mimetype in case the user changes type
        item->changeName(value.toString());
        emit dataChanged(index,index);
    }

    return ok;
}

//---------------------------------------------------------------------------------
bool myModel::remove(const QModelIndex & theIndex)
{
    myModelItem *item = static_cast<myModelItem*>(theIndex.internalPointer());

    QString path = item->absoluteFilePath();

    //physically remove files from disk
    QDirIterator it(path,QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    QStringList children;

    while (it.hasNext()) { children.prepend(it.next()); }
    children.append(path);

    children.removeDuplicates();

    bool error = false;
    for (int i = 0; i < children.count(); i++) {
        QFileInfo info(children.at(i));
        if(info.isDir()) {
            int wd = watchers.key(info.filePath());
            inotify_rm_watch(inotifyFD,wd);
            watchers.remove(wd);
            error |= QDir().rmdir(info.filePath());
        }
        else { error |= QFile::remove(info.filePath()); }
    }

    //remove from model
    beginRemoveRows(index(item->parent()->absoluteFilePath()),item->childNumber(),item->childNumber());
    item->parent()->removeChild(item);
    endRemoveRows();
    return error;
}

//---------------------------------------------------------------------------------

/**
 * @brief Drag drop to current datamodel
 * @param data
 * @param action
 * @param row
 * @param column
 * @param parent
 * @return true if successful
 */
bool myModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                           int row, int column, const QModelIndex &parent) {

  // Unused
  Q_UNUSED(action);
  Q_UNUSED(row);
  Q_UNUSED(column);

  // If parent is not a directory, exit
  if (!isDir(parent)) {
    return false;
  }

  // Get urls of files
  QList<QUrl> files = data->urls();
  //QStringList cutList;

  // Don't do anything if drag and drop in same folder
  if (QFileInfo(files.at(0).path()).canonicalPath() == filePath(parent)) {
    return false;
  }

  // Holding ctrl is copy, holding shift is move, holding alt is ask
  Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
  Common::DragMode mode = Common::getDefaultDragAndDrop();
  if (mods == Qt::ControlModifier) {
    mode = Common::getDADctrlMod();
  } else if (mods == Qt::ShiftModifier) {
    mode = Common::getDADshiftMod();
  } else if (mods == Qt::AltModifier) {
    mode = Common::getDADaltMod();
  }


    /*foreach (QUrl item, files) {
      cutList.append(item.path());
    }*/


  // Emit drag drop paste
  emit dragDropPaste(data, filePath(parent), mode);
  return true;
}
//------------------------------------------------------------------------------

QVariant myModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation)

    if(role == Qt::DisplayRole) {
        switch(section) {
        case COLUMN_ICON: return QString();
        case COLUMN_NAME: return tr("Name");
        case COLUMN_SIZE: return tr("Size");
        case COLUMN_DATE: return tr("Date Modified");
        case COLUMN_FORMAT: return tr("Format");
        case COLUMN_FOLDER: return tr("Folder");
        default: return QVariant();
        }
    }

    return QVariant();
}

//---------------------------------------------------------------------------------------
Qt::ItemFlags myModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) { return Qt::NoItemFlags; }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

//---------------------------------------------------------------------------------
void myModel::addCutItems(QStringList files)
{
    cutItems = files;
}

//---------------------------------------------------------------------------------
void myModel::clearCutItems()
{
    cutItems.clear();
    QFile temp(Common::getTempClipboardFile());
    if (temp.exists()) { temp.remove(); }
}

