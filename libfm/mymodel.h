#ifndef MYMODEL_H
#define MYMODEL_H

#include <QtGui>
#include "mymodelitem.h"
#include "mimeutils.h"

#include "common.h"

#include <QAtomicInt>
#include <QMutex>

/**
 * @class myModel
 * @brief holding information about files in FS
 * @author Wittfella
 */
class myModel : public QAbstractItemModel {
  Q_OBJECT
public:

  myModel(bool realMime, MimeUtils* mimeUtils, QObject* parent = nullptr);
  ~myModel();
  void loadMimeTypes() const;
  void cacheInfo();
  void setMode(bool);
  void loadThumbs(QModelIndexList);
  void addCutItems(QStringList);
  void populateItem(myModelItem *item);
  void fetchMore(const QModelIndex & parent);
  void refresh();
  void refreshItems();
  void update();
  bool remove(const QModelIndex & index );
  bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent);
  bool isDir(const QModelIndex &index);
  bool canFetchMore (const QModelIndex & parent) const;
  bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
  bool setRootPath(const QString& path);
  QString getRootPath();
  void setRealMimeTypes(bool realMimeTypes);
  bool isRealMimeTypes() const;
  void setShowListDecorations(bool show);
  bool showListDecorations() const;
  QModelIndex index(int row, int column, const QModelIndex &parent) const;
  QModelIndex index(const QString& path) const;
  QModelIndex parent(const QModelIndex &index) const;
  QModelIndex insertFolder(QModelIndex parent);
  QModelIndex insertFile(QModelIndex parent);
  /** Creates an empty file with a unique name ending in @p suffix (e.g. "md", "txt"). */
  QModelIndex insertFileWithSuffix(QModelIndex parent, const QString &suffix);
  int rowCount(const QModelIndex &parent) const;
  qint64 size(const QModelIndex &index);
  QString fileName(const QModelIndex &index);
  QString filePath(const QModelIndex &index);
  QString getMimeType(const QModelIndex &index);
  QStringList mimeTypes() const;
  QFileInfo fileInfo(const QModelIndex &index);
  Qt::DropActions supportedDropActions () const;
  QMimeData* mimeData(const QModelIndexList & indexes) const;
  MimeUtils* getMimeUtils() const;
  QHash<QString,QIcon> *mimeIcons;
  QHash<QString,QIcon> *folderIcons;
  /** Per-file custom icon (absolute path -> bundled icon base name). */
  QHash<QString, QString> *pathIconNames;
  QCache<QString,QIcon> *icons;
  void setPathIcon(const QString &absolutePath, const QString &iconBaseName);
  void removePathIcon(const QString &absolutePath);
public slots:
  void notifyChange();
  void notifyProcess(int eventID, QString fileName = QString());
  void eventTimeout();
  void addWatcher(myModelItem* path);
  void clearCutItems();
  void clearIconCache();
  void forceRefresh();
  void refreshForegroundRoles();
  void pumpThumbnailQueue();
signals:
  void dragDropPaste(const QMimeData *data, QString newPath,
                     Common::DragMode mode = Common::DM_UNKNOWN);
  void thumbUpdate(const QString &path);
  void reloadDir(const QString &path);
protected:
  QVariant data(const QModelIndex & index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  bool setData(const QModelIndex & index, const QVariant & value,
               int role = Qt::EditRole);
  int columnCount(const QModelIndex &parent) const;
  Qt::ItemFlags flags(const QModelIndex &index) const;
  QVariant findIcon(myModelItem *item) const;
  QVariant findMimeIcon(myModelItem *item) const;
private:
  static bool fileWantsThumbnail(const QString &path, MimeUtils *mimeUtils);
  static QString generateThumbnailToCache(const QString &absolutePath,
                                          const QString &itemMime);

  bool realMimeTypes;
  bool showThumbs;
  bool m_showListDecorations = true;

  QPalette colors;
  QStringList cutItems;
  QHash<QString,QString> *mimeGlob;
  QHash<QString,QString> *mimeGeneric;
  QHash<QString, QString> *thumbPaths;

  mutable QMutex thumbMutex;
  QStringList thumbQueue;
  QAtomicInt thumbActiveJobs;

  myModelItem* rootItem;
  MimeUtils* mimeUtilsPtr;
  QString currentRootPath;
  QFileIconProvider* iconFactory;

  int inotifyFD;
  QSocketNotifier *notifier;
  QHash<int, QString> watchers;
  QTimer eventTimer;
  int lastEventID;
  QString lastEventFilename;
};

#endif // MYMODEL_H
