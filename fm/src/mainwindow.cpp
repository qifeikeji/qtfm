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

#include <QtGui>
#include <QDockWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QTabWidget>
#include <QTabBar>
#include <QInputDialog>
#include <QPushButton>
#include <QHBoxLayout>
#include <QDateTime>
#include <QApplication>
#include <QStatusBar>
#include <QMenu>
#include <QMenuBar>
#ifndef NO_DBUS
#include <QDBusConnection>
#include <QDBusError>
#endif
#include <fcntl.h>
#include <QResizeEvent>
#include <algorithm>

#include <QtConcurrent/QtConcurrent>

#include "mainwindow.h"
#ifndef NO_UDISKS
#include "udisks2.h"
#endif
#include "mymodel.h"
#include "fileutils.h"
#include "applicationdialog.h"
#include "sidebaritemdelegate.h"
#include "pathcombodelegate.h"

#include <QLayout>

namespace {
constexpr int kPathBarHeight = 28;
constexpr int kPathBarIconSize = kPathBarHeight - 10;
#ifdef Q_OS_MAC
bool macClipboardHasImage()
{
    QClipboard *cb = QApplication::clipboard();
    if (!cb) {
        return false;
    }
    if (!cb->image().isNull()) {
        return true;
    }
    const QMimeData *mime = cb->mimeData();
    return mime && mime->hasImage();
}
#endif
} // namespace

#include "common.h"
#include "openwithconfig.h"
#include "bundledicons.h"

#include "qtcopydialog.h"
#include "qtfilecopier.h"

#ifdef Q_OS_MAC
#include <QClipboard>
#include <QMimeData>
#include <QStyleFactory>
#include "macfileaccess.h"
#include <QFileSystemWatcher>
#include <QTimer>
#include "macdisks.h"
#endif

#ifndef NO_UDISKS
namespace {

QString effectiveMountpointForWholeDisk(Device *whole,
                                        const QMap<QString, Device *> &devices)
{
    if (!whole->mountpoint.isEmpty()) {
        return whole->mountpoint;
    }
    QMapIterator<QString, Device *> it(devices);
    while (it.hasNext()) {
        it.next();
        Device *d = it.value();
        if (d->drive != whole->drive || d->path == whole->path) { continue; }
        if (!d->mountpoint.isEmpty()) { return d->mountpoint; }
    }
    return QString();
}

QString mountableBlockPath(Device *whole, const QMap<QString, Device *> &devices)
{
    if (!whole->mountpoint.isEmpty() || !uDisks2::getFileSystem(whole->path).isEmpty()) {
        return whole->path;
    }
    QMapIterator<QString, Device *> it(devices);
    while (it.hasNext()) {
        it.next();
        Device *d = it.value();
        if (d->drive != whole->drive || d->path == whole->path) { continue; }
        if (!uDisks2::getFileSystem(d->path).isEmpty()) { return d->path; }
    }
    return whole->path;
}

QString diskOperationBlockPath(const QString &wholeDiskPath,
                               const QMap<QString, Device *> &devices)
{
    if (!devices.contains(wholeDiskPath)) { return wholeDiskPath; }
    Device *whole = devices.value(wholeDiskPath);
    if (!whole->mountpoint.isEmpty()) { return wholeDiskPath; }
    QMapIterator<QString, Device *> it(devices);
    while (it.hasNext()) {
        it.next();
        Device *d = it.value();
        if (d->drive != whole->drive || d->path == whole->path) { continue; }
        if (!d->mountpoint.isEmpty()) { return d->path; }
    }
    return wholeDiskPath;
}

bool shouldListWholeDisk(Device *whole, const QMap<QString, Device *> &devices)
{
    Q_UNUSED(devices);
    if (uDisks2::isIgnoredBlockDevice(whole->dev)) { return false; }
    if (uDisks2::isPartitionBlock(whole->path)) { return false; }

    if (whole->isOptical) {
        return whole->hasMedia;
    }
    if (whole->isRemovable) {
        return whole->hasMedia;
    }
    return true;
}

} // namespace
#endif

MainWindow::MainWindow(const QString &forcedStartPath)
{
#ifndef NO_UDISKS
    disks = new Disks(this);
    connect(disks, SIGNAL(updatedDevices()), this, SLOT(populateMedia()));
    connect(disks, SIGNAL(mountpointChanged(QString,QString)), this, SLOT(handleMediaMountpointChanged(QString,QString)));
    connect(disks, SIGNAL(foundNewDevice(QString)), this, SLOT(handleMediaAdded(QString)));
    connect(disks, SIGNAL(removedDevice(QString)), this, SLOT(handleMediaRemoved(QString)));
    connect(disks, SIGNAL(mediaChanged(QString,bool)), this, SLOT(handleMediaChanged(QString,bool)));
    connect(disks, SIGNAL(deviceErrorMessage(QString,QString)), this, SLOT(handleMediaError(QString,QString)));
#endif
#ifdef Q_OS_MAC
    {
        auto *macDiskTimer = new QTimer(this);
        connect(macDiskTimer, &QTimer::timeout, this, &MainWindow::populateMedia);
        macDiskTimer->start(5000);
        macVolumesWatcher = new QFileSystemWatcher(this);
        const QString volumesRoot = QStringLiteral("/Volumes");
        if (QDir(volumesRoot).exists()) {
            macVolumesWatcher->addPath(volumesRoot);
        }
        connect(macVolumesWatcher, &QFileSystemWatcher::directoryChanged,
                this, &MainWindow::populateMedia);
    }
#endif

    // dbus service
#ifndef NO_DBUS
    if (QDBusConnection::sessionBus().isConnected()) {
        if (QDBusConnection::sessionBus().registerService(FM_SERVICE)) {
            service = new qtfm(this);
            connect(service, SIGNAL(pathRequested(QString)), this, SLOT(handlePathRequested(QString)));
            if (!QDBusConnection::sessionBus().registerObject(FM_PATH, service, QDBusConnection::ExportAllSlots)) {
                qWarning() << QDBusConnection::sessionBus().lastError().message();
            }
        } else { qWarning() << QDBusConnection::sessionBus().lastError().message(); }
    }
#endif

    // get path from cmd
    startPath = QDir::currentPath();
    if (!forcedStartPath.isEmpty()) {
        startPath = forcedStartPath;
        if (startPath == QLatin1String(".")) {
            startPath = qEnvironmentVariable("PWD");
        } else if (QUrl(startPath).isLocalFile()) {
            startPath = QUrl(forcedStartPath).toLocalFile();
        }
        const QFileInfo argInfo(startPath);
        if (argInfo.exists()) {
            if (argInfo.isFile()) {
                pendingSingleFileTarget = argInfo.canonicalFilePath();
                startPath = argInfo.absolutePath();
            } else {
                startPath = argInfo.canonicalFilePath();
            }
        }
    } else {
    QStringList args = QApplication::arguments();

    if(args.count() > 1) {
        startPath = args.at(1);
        if (startPath == ".") {
            startPath = getenv("PWD");
        } else if (QUrl(startPath).isLocalFile()) {
            startPath = QUrl(args.at(1)).toLocalFile();
        }
        const QFileInfo argInfo(startPath);
        if (argInfo.exists()) {
            if (argInfo.isFile()) {
                pendingSingleFileTarget = argInfo.canonicalFilePath();
                startPath = argInfo.absolutePath();
            } else {
                startPath = argInfo.canonicalFilePath();
            }
        }
    }
    }

    settings = new QSettings(Common::configFile(), QSettings::IniFormat);
    if (settings->value("clearCache").toBool()) {
        qDebug() << "clear cache";
        Common::removeFileCache();
        Common::removeFolderCache();
        Common::removeThumbsCache();
        settings->setValue("clearCache", false);
    }

    // Application theme (independent of OS appearance on macOS).
#if QT_VERSION >= 0x050000
    {
        const bool darkUi = settings->value(QStringLiteral("darkTheme")).toBool();
        BundledIcons::setUiDarkMode(darkUi);
        qApp->setPalette(darkUi ? Common::darkTheme() : Common::lightTheme());
#ifdef Q_OS_MAC
        MacFileAccess::setApplicationAppearance(darkUi);
#endif
    }
#else
    if (settings->value("darkTheme").toBool()) {
        qApp->setPalette(Common::darkTheme());
    } else {
        qApp->setPalette(Common::lightTheme());
    }
#endif

    // set icon theme — file list uses bundled icons in share/icons/mimes/

    // Create mime utils
    mimeUtils = new MimeUtils(this);
    QString name = settings->value("defMimeAppsFile", MIME_APPS).toString();
    mimeUtils->setDefaultsFileName(name);

    // Create filesystem model
    bool realMime = settings->value("realMimeTypes", true).toBool();
    modelList = new myModel(realMime, mimeUtils, this);
    connect(modelList, SIGNAL(reloadDir(QString)), this, SLOT(handleReloadDir(QString)));

    m_reloadDirCoalesceTimer = new QTimer(this);
    m_reloadDirCoalesceTimer->setSingleShot(true);
    m_reloadDirCoalesceTimer->setInterval(900);
    connect(m_reloadDirCoalesceTimer, &QTimer::timeout, this, [this]() {
        dirLoaded(false);
    });

    m_thumbViewportCoalesceTimer = new QTimer(this);
    m_thumbViewportCoalesceTimer->setSingleShot(true);
    m_thumbViewportCoalesceTimer->setInterval(120);
    connect(m_thumbViewportCoalesceTimer, &QTimer::timeout, this, [this]() {
        if (currentView == 2) {
            detailTree->viewport()->update();
        } else {
            list->viewport()->update();
        }
    });

    dockTree = new QDockWidget(tr("Tree"),this,Qt::SubWindow);
    dockTree->setObjectName("treeDock");

    tree = new QTreeView(dockTree);
    dockTree->setWidget(tree);
    addDockWidget(Qt::LeftDockWidgetArea, dockTree);

    dockBookmarks = new QDockWidget(tr("Places"), this, Qt::SubWindow);
    dockBookmarks->setObjectName("bookmarksDock");
    dockBookmarks->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    sidebarTabs = new QTabWidget(dockBookmarks);
    sidebarTabs->setObjectName(QStringLiteral("sidebarPlacesTabs"));
    sidebarTabs->setTabPosition(QTabWidget::North);
    sidebarTabs->setDocumentMode(true);
    sidebarTabs->tabBar()->setDrawBase(false);
    sidebarTabs->tabBar()->setExpanding(true);
    sidebarTabs->tabBar()->setUsesScrollButtons(false);

    bookmarkPage = new QWidget(sidebarTabs);
    auto *bookmarkLayout = new QHBoxLayout(bookmarkPage);
    bookmarkLayout->setContentsMargins(0, 0, 0, 0);
    bookmarkLayout->setSpacing(0);

    bookmarkGroupBar = new BookmarkGroupBar(bookmarkPage);
    bookmarkLayout->addWidget(bookmarkGroupBar);

    bookmarksList = new QListView(bookmarkPage);
    bookmarksList->setMinimumHeight(24);
    bookmarksList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    bookmarksList->setFocusPolicy(Qt::ClickFocus);
    bookmarkLayout->addWidget(bookmarksList, 1);

    sidebarTabs->addTab(bookmarkPage, tr("Bookmarks"));

#if defined(QTFM_HAVE_SIDEBAR_DISKS)
    disksList = new QListView(sidebarTabs);
    disksList->setMinimumHeight(24);
    disksList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    disksList->setFocusPolicy(Qt::ClickFocus);
    sidebarTabs->addTab(disksList, tr("Disks"));
#endif

    sidebarTabs->setMinimumWidth(200);
    dockBookmarks->setWidget(sidebarTabs);
    addDockWidget(Qt::LeftDockWidgetArea, dockBookmarks);

    QWidget *main = new QWidget(this);
    mainLayout = new QVBoxLayout(main);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0,0,0,0);

    stackWidget = new QStackedWidget(this);
    QWidget *page = new QWidget(this);
    QHBoxLayout *hl1 = new QHBoxLayout(page);
    hl1->setSpacing(0);
    hl1->setContentsMargins(0,0,0,0);
    list = new IconFileListView(page);
    hl1->addWidget(list);
    stackWidget->addWidget(page);

    QWidget *page2 = new QWidget(this);
    hl1 = new QHBoxLayout(page2);
    hl1->setSpacing(0);
    hl1->setContentsMargins(0,0,0,0);
    detailTree = new DfmQTreeView(page2);
    hl1->addWidget(detailTree);
    stackWidget->addWidget(page2);

    tabs = new tabBar(modelList->folderIcons);

    mainLayout->addWidget(stackWidget);
    mainLayout->addWidget(tabs);

    setCentralWidget(main);

    modelTree = new viewsSortProxyModel();
    modelTree->setSourceModel(modelList);
    modelTree->setSortCaseSensitivity(Qt::CaseInsensitive);

    tree->setHeaderHidden(true);
    tree->setUniformRowHeights(true);
    tree->setModel(modelTree);
    tree->hideColumn(COLUMN_ICON);
    tree->hideColumn(COLUMN_SIZE);
    tree->hideColumn(COLUMN_DATE);
    tree->hideColumn(COLUMN_FORMAT);
    tree->hideColumn(COLUMN_FOLDER);

    modelView = new viewsSortProxyModel();
    modelView->setSourceModel(modelList);
    modelView->setSortCaseSensitivity(Qt::CaseInsensitive);

    list->setWrapping(true);
    list->setWordWrap(true);
    list->setModel(modelView);
    ivdelegate = new IconViewDelegate();
    ildelegate = new IconListDelegate();
    list->setTextElideMode(Qt::ElideNone);
    listSelectionModel = list->selectionModel();

    detailTree->setRootIsDecorated(false);
    detailTree->setItemsExpandable(false);
    detailTree->setUniformRowHeights(true);
    detailTree->setAlternatingRowColors(true);
    detailTree->setModel(modelView);
    detailTree->setSelectionModel(listSelectionModel);
    detailTree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setupFileListHeader();

    pathEdit = new QComboBox(this);
    pathEdit->setObjectName(QStringLiteral("pathAddressCombo"));
    pathEdit->setEditable(true);
    pathEdit->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pathEdit->setMinimumWidth(100);
    pathEdit->setFixedHeight(kPathBarHeight);
    auto *pathPopupView = new QListView(pathEdit);
    pathPopupView->setItemDelegate(new PathComboItemDelegate(pathPopupView));
    pathEdit->setView(pathPopupView);

    status = statusBar();
    status->setSizeGripEnabled(true);
    statusName = new QLabel();
    statusSize = new QLabel();
    statusDate = new QLabel();
    status->addPermanentWidget(statusName);
    status->addPermanentWidget(statusSize);
    status->addPermanentWidget(statusDate);

    treeSelectionModel = tree->selectionModel();
    connect(treeSelectionModel, SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(treeSelectionChanged(QModelIndex,QModelIndex)));
    if (!pendingSingleFileTarget.isEmpty()) {
        skipNextSingleFileClear = true;
    }
    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(startPath)));
    tree->scrollTo(tree->currentIndex());

    createActions();
    createToolBars();
    createMenus();

    setWindowIcon(QIcon::fromTheme("qtfm", QIcon(":/icons/app.svg")));
    setWindowTitle(APP_NAME);

    // Create custom action manager
    customActManager = new CustomActionsManager(settings, actionList, this);

    // Create bookmarks model
    modelBookmarks = new bookmarkmodel(/*modelList->folderIcons*/);
    connect(modelBookmarks, SIGNAL(bookmarksChanged()), this, SLOT(handleBookmarksChanged()));

    bookmarkListProxy = new BookmarkGroupProxy(this);
    bookmarkListProxy->setSourceModel(modelBookmarks);

    connect(bookmarkGroupBar, &BookmarkGroupBar::currentGroupChanged,
            this, &MainWindow::selectBookmarkGroup);
    connect(bookmarkGroupBar, &BookmarkGroupBar::addGroupRequested,
            this, &MainWindow::addBookmarkGroup);
    connect(bookmarkGroupBar, &BookmarkGroupBar::groupIconChangeRequested,
            this, &MainWindow::changeBookmarkGroupIcon);
    connect(bookmarkGroupBar, &BookmarkGroupBar::groupDeleteRequested,
            this, &MainWindow::removeBookmarkGroup);

    // Create disks model
#if defined(QTFM_HAVE_SIDEBAR_DISKS)
    modelDisks = new disksModel(this);
    disksList->setModel(modelDisks);
    connect(disksList, SIGNAL(activated(QModelIndex)), this, SLOT(diskActivated(QModelIndex)));
    connect(disksList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(diskActivated(QModelIndex)));
#endif

    // Load settings before showing window
    loadSettings();

    // show window
    show();

    trashDir = Common::trashDir();
    ignoreReload = false;

    qApp->installEventFilter(this);

    QTimer::singleShot(0, this, SLOT(lateStart()));
}
//---------------------------------------------------------------------------

/**
 * @brief Initialization
 */
void MainWindow::lateStart() {

  // Update status panel
  status->showMessage(Common::getDriveInfo(curIndex.filePath()));

  // Configure bookmarks list
  bookmarksList->setDragDropMode(QAbstractItemView::DragDrop);
  bookmarksList->setDropIndicatorShown(true);
  bookmarksList->setDefaultDropAction(Qt::CopyAction);
  bookmarksList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  bookmarksList->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // Configure tree view
  tree->setDragDropMode(QAbstractItemView::DragDrop);
  tree->setDefaultDropAction(Qt::MoveAction);
  tree->setDropIndicatorShown(true);
  tree->setEditTriggers(QAbstractItemView::EditKeyPressed |
                        QAbstractItemView::SelectedClicked);

  // Configure detail view
  detailTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  detailTree->setSelectionBehavior(QAbstractItemView::SelectRows);
  detailTree->setDragDropMode(QAbstractItemView::DragDrop);
  detailTree->setDefaultDropAction(Qt::MoveAction);
  detailTree->setDropIndicatorShown(true);
  detailTree->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // Configure list view
  list->setResizeMode(QListView::Adjust);
  list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  list->setSelectionRectVisible(true);
  list->setFocus();
  list->setEditTriggers(QAbstractItemView::NoEditTriggers);
  list->setMouseTracking(true);
  bookmarksList->setMouseTracking(true);
#if defined(QTFM_HAVE_SIDEBAR_DISKS)
  if (disksList) {
    disksList->setMouseTracking(true);
    disksList->setDragDropMode(QAbstractItemView::NoDragDrop);
    disksList->setEditTriggers(QAbstractItemView::NoEditTriggers);
  }
#endif

  // Clipboard configuration
  clipboardChanged();

  // Completer configuration
  customComplete = new myCompleter();
  customComplete->setModel(modelTree);
  customComplete->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
  customComplete->setMaxVisibleItems(10);
  pathEdit->setCompleter(customComplete);
  applyViewChromeStyles();

  // Tabs configuration
  tabs->setDrawBase(0);
  tabs->setExpanding(0);

  // Connect mouse clicks in views
  if (settings->value("singleClick").toInt() == 1) {
    connect(list, SIGNAL(clicked(QModelIndex)),
            this, SLOT(listItemClicked(QModelIndex)));
    connect(detailTree, SIGNAL(clicked(QModelIndex)),
            this, SLOT(listItemClicked(QModelIndex)));
  }
  if (settings->value("singleClick").toInt() == 2) {
    connect(list, SIGNAL(clicked(QModelIndex))
            ,this, SLOT(listDoubleClicked(QModelIndex)));
    connect(detailTree, SIGNAL(clicked(QModelIndex)),
            this, SLOT(listDoubleClicked(QModelIndex)));
  }

  // Connect list view
  connect(list, SIGNAL(activated(QModelIndex)),
          this, SLOT(listDoubleClicked(QModelIndex)));

  // Connect custom action manager
  connect(customActManager, SIGNAL(actionMapped(QString)),
          SLOT(actionMapper(QString)));
  connect(customActManager, SIGNAL(actionsLoaded()), SLOT(readShortcuts()));
  connect(customActManager, SIGNAL(actionFinished()), SLOT(clearCutItems()));

  // Connect path edit
  connect(pathEdit, SIGNAL(activated(QString)),
          this, SLOT(pathEditChanged(QString)));
  connect(customComplete, SIGNAL(activated(QString)),
          this, SLOT(pathEditChanged(QString)));
  connect(pathEdit->lineEdit(), SIGNAL(cursorPositionChanged(int,int)),
          this, SLOT(addressChanged(int,int)));

  // Connect bookmarks
  connect(bookmarksList, SIGNAL(activated(QModelIndex)),
          this, SLOT(bookmarkClicked(QModelIndex)));
  connect(bookmarksList, SIGNAL(clicked(QModelIndex)),
          this, SLOT(bookmarkClicked(QModelIndex)));
  connect(bookmarksList, SIGNAL(pressed(QModelIndex)),
          this, SLOT(bookmarkPressed(QModelIndex)));

  // Connect selection
  connect(QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)),
          this, SLOT(clipboardChanged()));
  connect(detailTree,SIGNAL(activated(QModelIndex)),
          this, SLOT(listDoubleClicked(QModelIndex)));
  connect(listSelectionModel,
          SIGNAL(selectionChanged(const QItemSelection, const QItemSelection)),
          this, SLOT(listSelectionChanged(const QItemSelection,
                                          const QItemSelection)));

  // Connect bookmark model
  connect(modelBookmarks,
          SIGNAL(bookmarkPaste(const QMimeData *, QString, QStringList, bool)), this,
          SLOT(pasteLauncher(const QMimeData *, QString, QStringList, bool)));
  connect(modelBookmarks, SIGNAL(rowsInserted(QModelIndex, int, int)),
          this, SLOT(readShortcuts()));
  connect(modelBookmarks, SIGNAL(rowsRemoved(QModelIndex, int, int)),
          this, SLOT(readShortcuts()));

  // Connect list model
  connect(modelList,
          SIGNAL(dragDropPaste(const QMimeData *, QString, Common::DragMode)),
          this,
          SLOT(dragLauncher(const QMimeData *, QString, Common::DragMode)));

  // Connect tabs
  connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
  connect(tabs, SIGNAL(dragDropTab(const QMimeData *, QString, QStringList)),
          this, SLOT(pasteLauncher(const QMimeData *, QString, QStringList)));
  connect(tabs, SIGNAL(openInNewWindowRequested(int)), this, SLOT(openTabInNewWindow(int)));
  connect(list, SIGNAL(pressed(QModelIndex)),
          this, SLOT(listItemPressed(QModelIndex)));
  connect(detailTree, SIGNAL(pressed(QModelIndex)),
          this, SLOT(listItemPressed(QModelIndex)));

  connect(modelList, SIGNAL(thumbUpdate(QString)),
          this, SLOT(thumbUpdate(QString)));

  qApp->setKeyboardInputInterval(1000);

  const QByteArray savedGeo = settings->value(QStringLiteral("windowGeo")).toByteArray();
  if (!savedGeo.isEmpty()) {
      restoreGeometry(savedGeo);
  }

  // Read custom actions
  QTimer::singleShot(100, customActManager, SLOT(readActions()));

  // Read defaults
  QTimer::singleShot(100, mimeUtils, SLOT(generateDefaults()));

#ifdef Q_OS_MAC
  QTimer::singleShot(500, this, SLOT(maybeShowMacFileAccessHint()));
#endif
}
//---------------------------------------------------------------------------

/**
 * @brief Loads application settings
 */
void MainWindow::loadSettings(bool wState, bool hState, bool tabState, bool thumbState) {

  // first run?
    bool isFirstRun = false;
    if (!settings->value("firstRun").isValid()) {
        isFirstRun = true;
        settings->setValue("firstRun", false);
    }

  // fix style — full chrome applied in applyViewChromeStyles()
  const int legacyV = settings->value("topModuleGapV", 5).toInt();
  const int legacyH = settings->value("topModuleGapH", 8).toInt();
  const int topGap = settings->value("topModuleGap", qMax(legacyV, legacyH)).toInt();
  topModuleGapV = topGap;
  topModuleGapH = topGap;
  applyNavToolBarInsets();

  // Restore window state
  if (wState) {
      qDebug() << "restore window state";
      if (!settings->value("windowState").isValid()) { // don't show dock tree/app as default
          dockTree->hide();
      }
      restoreState(settings->value("windowState").toByteArray(), 1);
      restoreGeometry(settings->value("windowGeo").toByteArray());
      if (settings->value("windowMax").toBool()) { showMaximized(); }
  }

  // Load info whether use real mime types
  modelList->setRealMimeTypes(settings->value("realMimeTypes", true).toBool());

  // Load information whether hidden files can be displayed
  if (hState) {
      hiddenAct->setChecked(settings->value("hiddenMode", 0).toBool());
      toggleHidden();
  }

  // Remove old bookmarks
  modelBookmarks->removeRows(0, modelBookmarks->rowCount());

  // Load bookmarks
  loadBookmarkGroups();
  currentBookmarkGroupId = bookmarkGroups.isEmpty()
                               ? QStringLiteral("default")
                               : bookmarkGroups.first().id;
  modelBookmarks->setActiveGroupId(currentBookmarkGroupId);
  bookmarkListProxy->setGroupId(currentBookmarkGroupId);
  loadBookmarks();

  // Set bookmarks
  firstRunBookmarks(isFirstRun);
  refreshBookmarkGroupBar();
#if defined(QTFM_HAVE_SIDEBAR_DISKS)
  populateMedia();
#endif
  bookmarksList->setModel(bookmarkListProxy);
  bookmarksList->setResizeMode(QListView::Adjust);
  bookmarksList->setFlow(QListView::TopToBottom);
  bookmarksList->setIconSize(QSize(24,24));
  bookmarksList->setItemDelegate(new BookmarkItemDelegate(bookmarksList));

#if defined(QTFM_HAVE_SIDEBAR_DISKS)
  disksList->setResizeMode(QListView::Adjust);
  disksList->setFlow(QListView::TopToBottom);
  disksList->setIconSize(QSize(24,24));
  disksList->setItemDelegate(new DiskItemDelegate(disksList));
  disksList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  sidebarTabs->setMinimumWidth(200);
#endif

  // Load information whether bookmarks are displayed
  wrapBookmarksAct->setChecked(settings->value("wrapBookmarks", 0).toBool());
  bookmarksList->setWrapping(wrapBookmarksAct->isChecked());

  // Lock information whether layout is locked or not
  lockLayoutAct->setChecked(settings->value("lockLayout", 1).toBool());
  toggleLockLayout();

  // Load zoom settings
  OpenWithConfig::load(settings);
  zoom = settings->value("zoom", 48).toInt();
  zoomTree = settings->value("zoomTree", 16).toInt();
  zoomBook = settings->value("zoomBook", 24).toInt();
  bookmarkGroupTabSize = settings->value("bookmarkGroupTabSize", 40).toInt();
  zoomList = settings->value("zoomList", 24).toInt();
  zoomDetail = settings->value("zoomDetail", 32).toInt();
  const int legacyGap = settings->value("iconViewGap", 4).toInt();
  iconViewGapH = settings->value("iconViewGapH", legacyGap).toInt();
  iconViewGapV = settings->value("iconViewGapV", legacyGap).toInt();
  ivdelegate->setCellGaps(iconViewGapH, iconViewGapV);
  updateGrid();
  detailTree->setIconSize(QSize(zoomDetail, zoomDetail));
  tree->setIconSize(QSize(zoomTree, zoomTree));
  bookmarksList->setIconSize(QSize(zoomBook, zoomBook));
  bookmarkGroupBar->setTabButtonSize(bookmarkGroupTabSize);

  // Load information whether thumbnails can be shown
  if (thumbState) {
    thumbsAct->setChecked(settings->value("showThumbs", 1).toBool());
  }

  // Load view mode — folder sort flags applied after view mode is known
  modelView->setFoldersAlwaysFirstSetting(settings->value("foldersAlwaysFirst", true).toBool());
  modelView->setFoldersAlwaysFirstIconSetting(
      settings->value("foldersAlwaysFirstIcon", true).toBool());
  modelView->resetDirectorySortOverride();

  int sortBy = settings->value("sortBy", 2).toInt();
  if (!settings->value("listColumnLayoutV2", false).toBool()) {
      if (sortBy == 3) {
          sortBy = 2;
      } else if (sortBy == 2) {
          sortBy = 3;
      }
      settings->setValue("listColumnLayoutV2", true);
  }
  if (!settings->value(QStringLiteral("listColumnLayoutV3"), false).toBool()) {
      sortBy = qBound(0, sortBy + 1, COLUMN_FOLDER);
      for (int col = 4; col >= 0; --col) {
          settings->setValue(QStringLiteral("listColumnWidth%1").arg(col + 1),
                             settings->value(QStringLiteral("listColumnWidth%1").arg(col)).toInt());
      }
      settings->setValue(QStringLiteral("listColumnWidth0"), 0);
      settings->setValue(QStringLiteral("header"), QByteArray());
      settings->setValue(QStringLiteral("listColumnLayoutV3"), true);
  }
  currentSortColumn = sortBy;
  currentSortOrder = static_cast<Qt::SortOrder>(
      settings->value("sortOrder", static_cast<int>(Qt::DescendingOrder)).toInt());

  switch (currentSortColumn) {
  case COLUMN_NAME: setSortColumn(sortNameAct); break;
  case COLUMN_SIZE: setSortColumn(sortSizeAct); break;
  case COLUMN_DATE: setSortColumn(sortDateAct); break;
  default: break;
  }
  setSortOrder(currentSortOrder);

  const QString fileViewMode = settings->value("fileViewMode").toString();
  if (fileViewMode == QLatin1String("icon")) {
      applyIconView();
  } else if (fileViewMode == QLatin1String("list")) {
      applyListView();
  } else if (settings->value("viewMode", 0).toInt() == 1
             || !settings->value("iconMode", false).toBool()) {
      applyListView();
  } else {
      applyIconView();
  }

  modelView->setIconViewSortContext(currentView == 1);
  const int sortColumnForView = (currentView == 1 && currentSortColumn == COLUMN_FOLDER)
                                    ? COLUMN_NAME
                                    : currentSortColumn;
  detailTree->setSortingEnabled(true);
  modelView->sort(sortColumnForView, currentSortOrder);
  if (currentView == 1) {
      list->viewport()->update();
  }

  if (settings->value("header").toByteArray().isEmpty()) {
      applyListColumnWidths();
  } else {
      applyListColumnWidths();
  }

  // Load terminal command
  term = settings->value("term", "xterm").toString();

  // custom actions
#ifndef Q_OS_MAC
  firstRunCustomActions(isFirstRun);
#endif

  // Load information whether tabs can be shown on top
  if (tabState) {
      tabsOnTopAct->setChecked(settings->value("tabsOnTop", 1).toBool());
      tabsOnTop();
  }

  // show/hide buttons
  homeAct->setVisible(settings->value("home_button", true).toBool());
  newTabAct->setVisible(settings->value("newtab_button", false).toBool());
  terminalAct->setVisible(settings->value("terminal_button", true).toBool());

  // path history
  pathHistory = settings->value("pathHistory", true).toBool();

  // path in window title
  showPathInWindowTitle = settings->value("windowTitlePath", true).toBool();
  if (!showPathInWindowTitle) {
    setWindowTitle(APP_NAME);
  } else if (!curIndex.filePath().isEmpty()) {
    const QString folderPart = curIndex.fileName().isEmpty()
                                   ? curIndex.absolutePath()
                                   : curIndex.fileName();
    setWindowTitle(QStringLiteral("%1 — %2").arg(QLatin1String(APP_NAME), folderPart));
  }

  // 'copy of' filename
  copyXof = settings->value("copyXof", COPY_X_OF).toString();
  copyXofTS = settings->value("copyXofTS", COPY_X_TS).toString();

#if QT_VERSION >= 0x050000
  applyThemeFromSettings();
#endif
}

void MainWindow::firstRunBookmarks(bool isFirstRun)
{
    if (!isFirstRun) { return; }
    //qDebug() << "first run, setup default bookmarks";
    modelBookmarks->addBookmark(tr("Computer"), "/", "", "folder-root", "", false, false);
#ifdef Q_OS_MAC
    modelBookmarks->addBookmark(tr("Applications"), "/Applications", "", "x-content-software", "", false, false);
#endif
    modelBookmarks->addBookmark(tr("Home"), QDir::homePath(), "", "folder-home", "", false, false);
    modelBookmarks->addBookmark(tr("Desktop"), QString("%1/Desktop").arg(QDir::homePath()), "", "folder-desktop", "", false, false);
    //modelBookmarks->addBookmark(tr("Documents"), QString("%1/Documents").arg(QDir::homePath()), "", "text-x-generic", "", false, false);
    //modelBookmarks->addBookmark(tr("Downloads"), QString("%1/Downloads").arg(QDir::homePath()), "", "applications-internet", "", false, false);
    //modelBookmarks->addBookmark(tr("Pictures"), QString("%1/Pictures").arg(QDir::homePath()), "", "image-x-generic", "", false, false);
    //modelBookmarks->addBookmark(tr("Videos"), QString("%1/Videos").arg(QDir::homePath()), "", "video-x-generic", "", false, false);
    //modelBookmarks->addBookmark(tr("Music"), QString("%1/Music").arg(QDir::homePath()), "", "audio-x-generic", "", false, false);
    modelBookmarks->addBookmark(tr("Trash"), QString("%1/.local/share/Trash").arg(QDir::homePath()), "", "folder-grey", "", false, false);
    modelBookmarks->addBookmark("", "", "", "", "", false, false);
    writeBookmarks();
}

void MainWindow::loadBookmarks()
{
    //qDebug() << "load bookmarks";
    settings->beginGroup("bookmarks");
    foreach (QString key,settings->childKeys()) {
      QStringList temp(settings->value(key).toStringList());
      const QString icon = temp.size() > 3 ? temp[3] : QString();
      const QString group = temp.size() > 4 ? temp[4] : QStringLiteral("default");
      modelBookmarks->addBookmark(temp.value(0), temp.value(1), temp.value(2), icon,
                                  QString(), false, false, group);
    }
    settings->endGroup();
}

void MainWindow::writeBookmarks()
{
    //qDebug() << "write bookmarks";
    settings->remove("bookmarks");
    settings->beginGroup("bookmarks");
    for (int i = 0; i < modelBookmarks->rowCount(); i++) {
      QStringList temp;
      temp << modelBookmarks->item(i)->text()
           << modelBookmarks->item(i)->data(BOOKMARK_PATH).toString()
           << modelBookmarks->item(i)->data(BOOKMARKS_AUTO).toString()
           << modelBookmarks->item(i)->data(BOOKMARK_ICON).toString()
           << modelBookmarks->item(i)->data(BOOKMARK_GROUP).toString();
      QString number = QString("%1").arg(i, 4, 10, QChar('0'));
      settings->setValue(number, temp);
    }
    settings->endGroup();
    writeBookmarkGroups();
}

void MainWindow::loadBookmarkGroups()
{
    bookmarkGroups.clear();
    settings->beginGroup("bookmarkGroups");
    const QStringList keys = settings->childKeys();
    if (keys.isEmpty()) {
        BookmarkGroupInfo def;
        def.id = QStringLiteral("default");
        def.iconName = QStringLiteral("folder");
        bookmarkGroups.append(def);
    } else {
        foreach (const QString &key, keys) {
            const QStringList temp = settings->value(key).toStringList();
            if (temp.size() < 2) { continue; }
            BookmarkGroupInfo g;
            g.id = temp.at(0);
            g.iconName = temp.at(1);
            bookmarkGroups.append(g);
        }
    }
    settings->endGroup();
    if (bookmarkGroups.isEmpty()) {
        BookmarkGroupInfo def;
        def.id = QStringLiteral("default");
        def.iconName = QStringLiteral("folder");
        bookmarkGroups.append(def);
    }
}

void MainWindow::writeBookmarkGroups()
{
    settings->remove("bookmarkGroups");
    settings->beginGroup("bookmarkGroups");
    for (int i = 0; i < bookmarkGroups.size(); ++i) {
        const BookmarkGroupInfo &g = bookmarkGroups.at(i);
        const QString key = QString("%1").arg(i, 4, 10, QChar('0'));
        settings->setValue(key, QStringList() << g.id << g.iconName);
    }
    settings->endGroup();
}

void MainWindow::refreshBookmarkGroupBar()
{
    if (!bookmarkGroupBar) { return; }
    bookmarkGroupBar->setGroups(bookmarkGroups, currentBookmarkGroupId);
}

void MainWindow::selectBookmarkGroup(const QString &groupId)
{
    if (groupId.isEmpty()) { return; }
    currentBookmarkGroupId = groupId;
    modelBookmarks->setActiveGroupId(groupId);
    if (bookmarkListProxy) {
        bookmarkListProxy->setGroupId(groupId);
    }
}

void MainWindow::addBookmarkGroup()
{
    BookmarkGroupInfo g;
    g.id = QStringLiteral("group_%1").arg(QDateTime::currentMSecsSinceEpoch());
    g.iconName = QStringLiteral("folder");
    bookmarkGroups.append(g);
    currentBookmarkGroupId = g.id;
    refreshBookmarkGroupBar();
    selectBookmarkGroup(g.id);
    writeBookmarkGroups();
}

void MainWindow::changeBookmarkGroupIcon(const QString &groupId)
{
    icondlg *themeIcons = new icondlg;
    if (themeIcons->exec() != QDialog::Accepted) {
        delete themeIcons;
        return;
    }
    const QString iconName = themeIcons->result;
    delete themeIcons;

    for (BookmarkGroupInfo &g : bookmarkGroups) {
        if (g.id == groupId) {
            g.iconName = iconName;
            break;
        }
    }
    refreshBookmarkGroupBar();
    writeBookmarkGroups();
}

void MainWindow::removeBookmarkGroup(const QString &groupId)
{
    if (groupId.isEmpty()) {
        return;
    }
    if (bookmarkGroups.size() <= 1) {
        QMessageBox::information(this, tr("Delete bookmark group"),
                                 tr("At least one bookmark group must remain."));
        return;
    }

    QString label = groupId;
    for (const BookmarkGroupInfo &g : bookmarkGroups) {
        if (g.id == groupId) {
            label = g.id;
            break;
        }
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Delete bookmark group"),
        tr("Delete group \"%1\" and all bookmarks in it?").arg(label),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    for (int i = modelBookmarks->rowCount() - 1; i >= 0; --i) {
        if (modelBookmarks->item(i)->data(BOOKMARK_GROUP).toString() == groupId) {
            modelBookmarks->removeRow(i);
        }
    }

    for (int i = 0; i < bookmarkGroups.size(); ++i) {
        if (bookmarkGroups.at(i).id == groupId) {
            bookmarkGroups.removeAt(i);
            break;
        }
    }

    if (currentBookmarkGroupId == groupId) {
        currentBookmarkGroupId = bookmarkGroups.first().id;
    }
    refreshBookmarkGroupBar();
    selectBookmarkGroup(currentBookmarkGroupId);
    writeBookmarkGroups();
    writeBookmarks();
}

void MainWindow::handleBookmarksChanged()
{
    //qDebug() << "bookmarks changed, save";
    QTimer::singleShot(1000, this, SLOT(writeBookmarks()));
}

void MainWindow::firstRunCustomActions(bool isFirstRun)
{
    if (!isFirstRun) { return; }
    settings->beginGroup("customActions");
    int childs = settings->childKeys().size();
    if (childs>0) { return; }

    QVector<QStringList> defActions = Common::getDefaultActions();
    for (int i=0;i<defActions.size();++i) {
        settings->setValue(QString(i), defActions.at(i));
    }

    settings->endGroup();
    settings->sync();
}
//---------------------------------------------------------------------------

/**
 * @brief Close event
 * @param event
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    // Save settings
    writeSettings();

    modelList->cacheInfo();
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (!settings || !isVisible()) {
        return;
    }
    settings->setValue(QStringLiteral("windowGeo"), saveGeometry());
}
//---------------------------------------------------------------------------

/**
 * @brief Closes main window
 */
void MainWindow::exitAction() {
  qApp->quit();
}
//---------------------------------------------------------------------------

void MainWindow::treeSelectionChanged(QModelIndex current, QModelIndex previous)
{
    qDebug() << "treeSelectionChanged";
    Q_UNUSED(previous)

    QFileInfo name = modelList->fileInfo(modelTree->mapToSource(current));
    if (!name.exists() || name.isFile()) { return; }

    if (skipNextSingleFileClear) {
        skipNextSingleFileClear = false;
    } else {
        modelView->clearSingleFileFilter();
        pendingSingleFileTarget.clear();
    }

    curIndex = name;
    if (showPathInWindowTitle) {
        const QString folderPart = curIndex.fileName().isEmpty()
                                       ? curIndex.absolutePath()
                                       : curIndex.fileName();
        setWindowTitle(QStringLiteral("%1 — %2").arg(QLatin1String(APP_NAME), folderPart));
    } else {
        setWindowTitle(APP_NAME);
    }

    if (tree->hasFocus() && QApplication::mouseButtons() == Qt::MiddleButton) {
        listItemPressed(modelView->mapFromSource(modelList->index(name.filePath())));
        tabs->setCurrentIndex(tabs->count() - 1);
        if (currentView == 2) { detailTree->setFocus(Qt::TabFocusReason); }
        else { list->setFocus(Qt::TabFocusReason); }
    }

    if (curIndex.filePath() != pathEdit->itemText(0)) {
        if (tabs->count() && pathHistory) { tabs->addHistory(curIndex.filePath()); }
        if (!pathHistory && pathEdit->count()>0) { pathEdit->clear(); }
        pathEdit->insertItem(0,curIndex.filePath());
        syncPathComboDecorations();
        pathEdit->setCurrentIndex(0);
    }

    if (!bookmarksList->hasFocus()) { bookmarksList->clearSelection(); }

    if (modelList->setRootPath(name.filePath())) { modelView->invalidate(); }

    //////
    QModelIndex baseIndex = modelView->mapFromSource(modelList->index(name.filePath()));

    if (currentView == 2) { detailTree->setRootIndex(baseIndex); }
    else { list->setRootIndex(baseIndex); }

    if(tabs->count()) {
        QString tabText = curIndex.fileName();
        if (tabText.isEmpty()) { tabText = "/"; }
        tabs->setTabText(tabs->currentIndex(),tabText);
        tabs->setTabData(tabs->currentIndex(),curIndex.filePath());
        tabs->setIcon(tabs->currentIndex());
    }

    if(backIndex.isValid()) {
        listSelectionModel->setCurrentIndex(modelView->mapFromSource(backIndex),QItemSelectionModel::ClearAndSelect);
        if (currentView == 2) { detailTree->scrollTo(modelView->mapFromSource(backIndex)); }
        else { list->scrollTo(modelView->mapFromSource(backIndex)); }
    } else {
        listSelectionModel->blockSignals(1);
        listSelectionModel->clear();
    }

    listSelectionModel->blockSignals(0);
    updateGrid();

    if (!pendingSingleFileTarget.isEmpty()) {
        modelView->setSingleFileFilter(pendingSingleFileTarget);
    }

    qDebug() << "trigger dirloaded on tree selection changed";
    QTimer::singleShot(30,this,SLOT(dirLoaded()));
}

//---------------------------------------------------------------------------
void MainWindow::dirLoaded(bool thumbs)
{

    if (backIndex.isValid()) {
        backIndex = QModelIndex();
        return;
    }

    qDebug() << "dirLoaded triggered, thumbs?" << thumbs;
    qint64 bytes = 0;
    QModelIndexList items;
    bool includeHidden = hiddenAct->isChecked();

    for (int x = 0; x < modelList->rowCount(modelList->index(pathEdit->currentText())); ++x) {
        const QModelIndex idx = modelList->index(x, 0, modelList->index(pathEdit->currentText()));
        if (includeHidden || !modelList->fileInfo(idx).isHidden()) {
            items.append(idx);
            bytes += modelList->size(idx);
        }
    }

    QString total;

    if (!bytes) { total = ""; }
    else { total = Common::formatSize(bytes); }

    statusName->clear();
    statusSize->setText(QString("%1 items").arg(items.count()));
    statusDate->setText(QString("%1").arg(total));

    if (thumbsAct->isChecked() && thumbs) {
      myModel *modelListPtr = modelList;
      QMetaObject::invokeMethod(modelList, [modelListPtr, items]() {
        modelListPtr->loadThumbs(items);
      }, Qt::QueuedConnection);
    }

    if (!pendingSingleFileTarget.isEmpty()) {
        const QModelIndex src = modelList->index(pendingSingleFileTarget);
        if (src.isValid()) {
            const QModelIndex prox = modelView->mapFromSource(src);
            if (prox.isValid()) {
                listSelectionModel->setCurrentIndex(prox, QItemSelectionModel::ClearAndSelect);
                if (currentView == 2) {
                    detailTree->scrollTo(prox);
                } else {
                    list->scrollTo(prox);
                }
            }
        }
    }
    updateGrid();
    if (modelView && currentView == 1) {
        const int sortCol = currentSortColumn == COLUMN_FOLDER ? COLUMN_NAME : currentSortColumn;
        modelView->sort(sortCol, currentSortOrder);
        list->viewport()->update();
    }
}

void MainWindow::updateDir()
{
    dirLoaded(false /* don't refresh thumb*/);
}

void MainWindow::handleReloadDir(const QString &path)
{
    qDebug() << "handle reload dir" << path << modelList->getRootPath();
    if (path != modelList->getRootPath()) {
        return;
    }
    if (m_reloadDirCoalesceTimer) {
        m_reloadDirCoalesceTimer->start();
    }
}

void MainWindow::thumbUpdate(const QString &path)
{
    if (path != modelList->getRootPath()) {
        return;
    }
    if (m_thumbViewportCoalesceTimer) {
        m_thumbViewportCoalesceTimer->start();
    }
}

//---------------------------------------------------------------------------
void MainWindow::listSelectionChanged(const QItemSelection selected, const QItemSelection deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)

    QModelIndexList items;

    if (listSelectionModel->selectedRows(0).count()) { items = listSelectionModel->selectedRows(0); }
    else { items = listSelectionModel->selectedIndexes(); }

    statusSize->clear();
    statusDate->clear();
    statusName->clear();

    if(items.count() == 0) {
        curIndex = pathEdit->itemText(0);
        return;
    }

    if (QApplication::focusWidget() != bookmarksList) { bookmarksList->clearSelection(); }

    curIndex = modelList->fileInfo(modelView->mapToSource(listSelectionModel->currentIndex()));

    qint64 bytes = 0;
    int folders = 0;
    int files = 0;

    foreach(QModelIndex theItem,items) {
        if (modelList->isDir(modelView->mapToSource(theItem))) { folders++; }
        else { files++; }
        bytes = bytes + modelList->size(modelView->mapToSource(theItem));
    }

    QString total,name;

    if (!bytes) { total = ""; }
    else { total = Common::formatSize(bytes); }

    if (items.count() == 1) {
        QFileInfo file(modelList->filePath(modelView->mapToSource(items.at(0))));

        name = file.fileName();
        if (file.isSymLink()) { name = "Link --> " + file.symLinkTarget(); }

        statusName->setText(name + "   ");
        statusSize->setText(QString("%1   ").arg(total));
        statusDate->setText(QString("%1").arg(QLocale::system().toString(file.lastModified(),
                                                                         QLocale::FormatType::ShortFormat)));
    }
    else {
        statusName->setText(total + "   ");
        if (files) { statusSize->setText(QString("%1 files  ").arg(files)); }
        if (folders) { statusDate->setText(QString("%1 folders").arg(folders)); }
    }
}

//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
void MainWindow::listItemClicked(QModelIndex current)
{
    if (modelList->filePath(modelView->mapToSource(current)) == pathEdit->currentText()) { return; }

    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if (mods == Qt::ControlModifier || mods == Qt::ShiftModifier) { return; }
    if (modelList->isDir(modelView->mapToSource(current))) {
        tree->setCurrentIndex(modelTree->mapFromSource(modelView->mapToSource(current)));
    }
}

//---------------------------------------------------------------------------
void MainWindow::listItemPressed(QModelIndex current)
{
    //middle-click -> open new tab
    //ctrl+middle-click -> open new instance

    if (QApplication::mouseButtons() == Qt::MiddleButton) {
        if(modelList->isDir(modelView->mapToSource(current))) {
            if (QApplication::keyboardModifiers() == Qt::ControlModifier) { openFile(); }
            else { addTab(modelList->filePath(modelView->mapToSource(current))); }
        } else { openFile(); }
    }
}

//---------------------------------------------------------------------------
void MainWindow::openTab()
{
    if(curIndex.isDir()) {
        addTab(curIndex.filePath());
    } else {
        addTab(QDir::homePath());
    }
}

void MainWindow::openNewWindowFromSelection()
{
    QFileInfo info(curIndex.filePath());
    if (!info.isDir()) { return; }
    newWindow(curIndex.filePath());
}

void MainWindow::openNewTab()
{
    QFileInfo info(curIndex.filePath());
    if (!info.isDir()) { return; }
    addTab(curIndex.filePath());
}

//---------------------------------------------------------------------------
int MainWindow::addTab(QString path)
{
    if (tabs->count() == 0) { tabs->addNewTab(pathEdit->currentText(),currentView); }
    return tabs->addNewTab(path,currentView);
}

//---------------------------------------------------------------------------
void MainWindow::tabsOnTop()
{
    if(tabsOnTopAct->isChecked()) {
        mainLayout->setDirection(QBoxLayout::BottomToTop);
        tabs->setShape(QTabBar::RoundedNorth);
    } else {
        mainLayout->setDirection(QBoxLayout::TopToBottom);
        tabs->setShape(QTabBar::RoundedSouth);
    }
    applyViewChromeStyles();
}

void MainWindow::applyThemeFromSettings()
{
#if QT_VERSION >= 0x050000
    if (!settings) {
        return;
    }
    const bool darkUi = settings->value(QStringLiteral("darkTheme")).toBool();
    BundledIcons::setUiDarkMode(darkUi);
    qApp->setPalette(darkUi ? Common::darkTheme() : Common::lightTheme());
#ifdef Q_OS_MAC
    MacFileAccess::setApplicationAppearance(darkUi);
#endif
    refreshBundledUiIcons();
    applyWidgetPalettes();
    applyViewChromeStyles();
    if (bookmarkGroupBar) {
        bookmarkGroupBar->applyButtonSizes();
    }
    if (modelList) {
        modelList->refreshForegroundRoles();
    }
    if (list && list->viewport()) {
        list->viewport()->update();
    }
    if (detailTree && detailTree->viewport()) {
        detailTree->viewport()->update();
    }
#endif
}

void MainWindow::applyWidgetPalettes()
{
    const QPalette pal = qApp->palette();
    const auto syncPalette = [&pal](QWidget *w) {
        if (!w) {
            return;
        }
        w->setPalette(pal);
        w->setAutoFillBackground(true);
    };

    syncPalette(centralWidget());
    syncPalette(stackWidget);
    syncPalette(list);
    syncPalette(detailTree);
    syncPalette(tree);
    syncPalette(bookmarksList);
#if defined(QTFM_HAVE_SIDEBAR_DISKS)
    syncPalette(disksList);
#endif
    syncPalette(bookmarkPage);
    syncPalette(sidebarTabs);
    syncPalette(dockBookmarks);
    syncPalette(dockTree);
    syncPalette(menuToolBar);
    syncPalette(navToolBar);
    syncPalette(addressToolBar);
    syncPalette(status);
    if (bookmarkGroupBar) {
        bookmarkGroupBar->setPalette(pal);
    }
}

void MainWindow::applyViewChromeStyles()
{
    if (!tabs || !list) {
        return;
    }
    const QPalette pal = list->palette();
    const bool darkUi = settings && settings->value(QStringLiteral("darkTheme")).toBool();
    const QString comboArrowUrl = darkUi
        ? QStringLiteral(":/icons/toolbar/white/chevron-down.svg")
        : QStringLiteral(":/icons/toolbar/chevron-down.svg");

    QColor highlight = pal.highlight().color();
    QColor hover = highlight;
    hover.setAlpha(qMin(255, hover.alpha() + static_cast<int>(255 * 0.28)));
    if (hover.alpha() < 72) {
        hover.setAlpha(72);
    }
    QColor selected = highlight;
    selected.setAlpha(qMin(255, selected.alpha() + static_cast<int>(255 * 0.40)));
    if (selected.alpha() < 100) {
        selected.setAlpha(100);
    }

    const QColor chromeLine = pal.color(QPalette::Mid);
    const QColor contentBg = pal.color(QPalette::Base);
    const QColor controlBg = pal.color(QPalette::Button);
    const QColor windowBg = pal.color(QPalette::Window);
    const QColor flatBorder = pal.color(QPalette::Mid);
    QColor flatHover = highlight;
    flatHover.setAlpha(72);

    const QColor sidebarTabSelected = contentBg;
    QColor sidebarTabHover = highlight;
    sidebarTabHover.setAlpha(72);

    const QString chromeBtnSize = QString::number(kPathBarHeight);
    const QString flatToolBtnQss = QStringLiteral(
        "QToolBar#Navigate QToolButton {"
        " border: 1px solid %1; border-radius: 4px; background: %2;"
        " padding: 0px; min-width: %4px; max-width: %4px;"
        " min-height: %4px; max-height: %4px; }"
        "QToolBar#Navigate QToolButton:hover {"
        " background: %3; }"
        "QToolBar#Navigate QToolButton:pressed {"
        " background: %3; }"
        "QToolBar#Navigate QToolButton:checked {"
        " background: %2; border: 1px solid %1; }")
        .arg(flatBorder.name(), controlBg.name(), flatHover.name(QColor::HexArgb), chromeBtnSize);
    const QString topModuleChromeQss = QStringLiteral(
        "QToolBar#Navigate {"
        " margin: 0; border: none; background: transparent; }");

    if (navToolBar) {
        navToolBar->setIconSize(QSize(kPathBarIconSize, kPathBarIconSize));
    }
    if (menuToolBar) {
        menuToolBar->setIconSize(QSize(kPathBarIconSize, kPathBarIconSize));
    }

    QString shellQss = QStringLiteral(
        "QMainWindow { background-color: %1; }"
        "QToolBar { padding: 0; border: none; background: %1; spacing: 4px; }"
        "QToolBar#Navigate { border: none; background: transparent; }"
        "QMenuBar { background-color: %1; color: palette(windowText); }"
        "QMenuBar::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
        "QStatusBar { background: %1; color: palette(windowText); }"
        "QDockWidget { background: %1; color: palette(windowText); }"
        "QDockWidget::title { background: %1; padding: 4px; }"
        "QHeaderView::section {"
        " background-color: palette(button); color: palette(buttonText);"
        " border: none; border-right: 1px solid %2; border-bottom: 1px solid %2;"
        " padding: 4px 6px; }"
        "QListView, QTreeView { background-color: palette(base);"
        " alternate-background-color: palette(alternateBase); color: palette(text); }"
        "QTabWidget::pane { border: none; background: palette(base); }"
    ).arg(windowBg.name(), chromeLine.name());
#ifdef Q_OS_MAC
    shellQss += QStringLiteral(
        "QMainWindow::separator { height: 0px; width: 0px; margin: 0; padding: 0; border: none; }");
#endif
    setStyleSheet(shellQss);

    if (navToolBar) {
        navToolBar->setStyleSheet(flatToolBtnQss + topModuleChromeQss);
    }

    const QString tabQss = QStringLiteral(
        "QTabBar::tab { min-height: 32px; max-height: 32px; padding: 6px 14px;"
        " border: none; border-radius: 0px; margin: 0px; background: transparent; }"
        "QTabBar::tab:selected { background: %1; }"
        "QTabBar::tab:hover:!selected { background: %2; }"
        "QTabBar::tab:!selected { min-height: 32px; max-height: 32px; }"
        "QTabBar::tab:selected:!hover { min-height: 32px; max-height: 32px; }"
    ).arg(selected.name(QColor::HexArgb), hover.name(QColor::HexArgb));

    tabs->setStyleSheet(tabQss);
    tabs->setUsesScrollButtons(false);
    tabs->setExpanding(true);
    tabs->setDocumentMode(true);
    if (sidebarTabs) {
        sidebarTabs->tabBar()->setDrawBase(false);
        const QString sidebarTabQss = QStringLiteral(
            "QTabWidget#sidebarPlacesTabs::pane {"
            " border: none; top: 0; margin: 0; padding: 0; background: %1; }"
            "QTabWidget#sidebarPlacesTabs QTabBar {"
            " background: %4; border: none; }"
            "QTabWidget#sidebarPlacesTabs QTabBar::tab { min-height: 30px; max-height: 30px;"
            " padding: 4px 12px; border: none; border-radius: 0px; margin: 0px;"
            " border-top: none; }"
            "QTabWidget#sidebarPlacesTabs QTabBar::tab:selected {"
            " background: %2; border: none; border-top: none; margin-top: 0; }"
            "QTabWidget#sidebarPlacesTabs QTabBar::tab:!selected {"
            " min-height: 30px; max-height: 30px; background: %4;"
            " border: none; border-top: none; margin-top: 0; padding-top: 4px; }"
            "QTabWidget#sidebarPlacesTabs QTabBar::tab:hover:!selected { background: %3; }"
            "QTabWidget#sidebarPlacesTabs QTabBar::tab:selected:!hover {"
            " min-height: 30px; max-height: 30px; }"
        ).arg(contentBg.name(), sidebarTabSelected.name(), sidebarTabHover.name(QColor::HexArgb),
              windowBg.name());
        sidebarTabs->setStyleSheet(sidebarTabQss);
    }

    if (pathEdit) {
        const QString pathBarH = QString::number(kPathBarHeight);
        const QString dropW = QString::number(kPathBarHeight);
        const QString pathComboQss = QStringLiteral(
            "QComboBox#pathAddressCombo {"
            " background: %1; border: 1px solid %2; border-radius: 4px;"
            " padding: 2px %5px 2px 8px; min-height: %4px; max-height: %4px; }"
            "QComboBox#pathAddressCombo:hover { border: 1px solid %2; }"
            "QComboBox#pathAddressCombo QLineEdit {"
            " border: none; background: transparent; padding: 0; margin: 0; }"
            "QComboBox#pathAddressCombo::drop-down {"
            " subcontrol-origin: padding; subcontrol-position: top right;"
            " width: %5px; border: none; border-left: 1px solid %2;"
            " border-top-right-radius: 4px; border-bottom-right-radius: 4px;"
            " background: transparent; }"
            "QComboBox#pathAddressCombo::down-arrow {"
            " width: 14px; height: 14px; image: url(%6); }"
            "QComboBox#pathAddressCombo QAbstractItemView {"
            " background: %1; border: 1px solid %2; border-radius: 4px;"
            " padding: 4px; outline: none; selection-background-color: %3;"
            " selection-color: palette(text); }"
            "QComboBox#pathAddressCombo QAbstractItemView::item {"
            " min-height: 32px; border-radius: 4px; }"
        ).arg(controlBg.name(), flatBorder.name(), flatHover.name(QColor::HexArgb),
              pathBarH, dropW, comboArrowUrl);
        pathEdit->setStyleSheet(pathComboQss);
        pathEdit->setFixedHeight(kPathBarHeight);
    }

    if (customComplete) {
        QAbstractItemView *pop = customComplete->popup();
        if (pop) {
        const QString completerQss = QStringLiteral(
            "QAbstractItemView { background: %1; border: 1px solid %2;"
            " border-radius: 4px; padding: 4px; outline: none; }"
            "QAbstractItemView::item { min-height: 32px; border-radius: 4px; }"
            "QAbstractItemView::item:hover { background: %3; }"
            "QAbstractItemView::item:selected { background: %4; }"
        ).arg(contentBg.name(), flatBorder.name(), flatHover.name(QColor::HexArgb),
              sidebarTabSelected.name());
        pop->setStyleSheet(completerQss);
        if (auto *tree = qobject_cast<QTreeView *>(pop)) {
            tree->setIndentation(24);
            tree->setRootIsDecorated(false);
            tree->setUniformRowHeights(true);
        }
        }
    }

    const int rowH = qMax(18, zoomDetail);
    const QString treeQss = QStringLiteral(
        "QTreeView::item { height: %1px; }"
        "QTreeView::item:hover { background-color: %2; }"
        "QTreeView::item:selected { background-color: %3; }"
        "QTreeView QLineEdit {"
        " background: palette(base); color: palette(text);"
        " border: 1px solid %4; border-radius: 2px; padding: 1px 4px;"
        " selection-background-color: %3; selection-color: palette(highlighted-text);"
        "}"
    ).arg(rowH)
     .arg(hover.name(QColor::HexArgb), selected.name(QColor::HexArgb),
          pal.color(QPalette::Mid).name());
    if (detailTree) {
        detailTree->setStyleSheet(treeQss);
    }

    applyNavToolBarInsets();
}

void MainWindow::applyNavToolBarInsets()
{
    if (!navToolBar) {
        return;
    }
#ifdef Q_OS_MAC
    navToolBar->setFloatable(false);
#endif
    navToolBar->setContentsMargins(0, 0, 0, 0);
    navToolBar->setIconSize(QSize(kPathBarIconSize, kPathBarIconSize));
    const int barHeight = kPathBarHeight + 2 * topModuleGapV;
#ifdef Q_OS_MAC
    navToolBar->setMinimumHeight(barHeight);
    navToolBar->setMaximumHeight(barHeight);
    navToolBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
#endif

    if (QLayout *tbLayout = navToolBar->layout()) {
        tbLayout->setContentsMargins(topModuleGapH, topModuleGapV, topModuleGapH, topModuleGapV);
        tbLayout->setSpacing(4);
        tbLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

#ifdef Q_OS_MAC
    if (QLayout *mwLayout = layout()) {
        mwLayout->setSpacing(0);
        mwLayout->setContentsMargins(0, 0, 0, 0);
    }
#endif
}

void MainWindow::updateTabBarPalette()
{
    applyViewChromeStyles();
}

void MainWindow::syncPathComboDecorations()
{
    if (!pathEdit) {
        return;
    }
    const QIcon folderIcon = BundledIcons::iconByName(QStringLiteral("folder"));
    for (int i = 0; i < pathEdit->count(); ++i) {
        pathEdit->setItemIcon(i, folderIcon);
    }
}

//---------------------------------------------------------------------------
void MainWindow::tabChanged(int index)
{
    if (tabs->count() == 0) { return; }

    pathEdit->clear();
    pathEdit->addItems(*tabs->getHistory(index));
    syncPathComboDecorations();

    int type = tabs->getType(index);
    if (currentView != type) {
        if (type == 1) {
            applyIconView();
        } else {
            applyListView();
        }
    }

    if(!tabs->tabData(index).toString().isEmpty()) {
        tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(tabs->tabData(index).toString())));
    }
}

void MainWindow::newWindow(const QString &path)
{
    if (settings->value("clearCache").toBool()) {
        settings->setValue("clearCache", false);
    }
#ifdef Q_OS_MAC
    writeSettings();
    auto *w = new MainWindow(path);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
    w->raise();
    w->activateWindow();
#else
    writeSettings();
    QStringList args;
    if (!path.isEmpty()) { args << path; }
    QProcess::startDetached(qApp->applicationFilePath(), args);
#endif
}

void MainWindow::openTabInNewWindow(int index)
{
    if (index < 0 || index >= tabs->count()) { return; }
    const QString path = tabs->tabData(index).toString();
    newWindow(path);
    // Move semantics: close the tab in this window now that it lives in the new one.
    if (tabs->count() > 1) {
        tabs->setCurrentIndex(index);
        tabs->closeTab();
    }
}


//---------------------------------------------------------------------------

/**
 * @brief Doubleclick on icon/launcher
 * @param current
 */
void MainWindow::listDoubleClicked(QModelIndex current) {
  Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
  if (mods == Qt::ControlModifier || mods == Qt::ShiftModifier) {
    return;
  }
#ifdef Q_OS_MAC
  if (modelList->isDir(modelView->mapToSource(current)) && !modelList->fileName(modelView->mapToSource(current)).endsWith(".app")) {
#else
  if (modelList->isDir(modelView->mapToSource(current))) {
#endif
    QModelIndex i = modelView->mapToSource(current);
    tree->setCurrentIndex(modelTree->mapFromSource(i));
  } else {
    executeFile(current, 0);
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Reaction for change of path edit (location edit)
 * @param path
 */
void MainWindow::pathEditChanged(QString path) {
  QString info = path;
  if (!QFileInfo(path).exists()) { return; }
  info.replace("~",QDir::homePath());
  tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(info)));
}
//---------------------------------------------------------------------------

/**
 * @brief Handle clipboard changes
 */
void MainWindow::clipboardChanged()
{
    qDebug() << "clipboard changed";
    if (QApplication::clipboard()->mimeData()) {
        if (QApplication::clipboard()->mimeData()->hasUrls()) {
            qDebug() << "clipboard has data, enable paste";
            pasteAct->setEnabled(true);
            return;
        }
    }
    // clear tmp and disable paste if no mime
    modelList->clearCutItems();
    pasteAct->setEnabled(false);
}
//---------------------------------------------------------------------------

/**
 * @brief Pastes from clipboard
 */
void MainWindow::pasteClipboard() {
  QString newPath;
  QStringList cutList;

  if (curIndex.isDir()) { newPath = curIndex.filePath(); }
  else { newPath = pathEdit->itemText(0); }

  // Check list of files that are to be cut
  QFile tempFile(Common::getTempClipboardFile());
  if (tempFile.exists()) {
    tempFile.open(QIODevice::ReadOnly);
    QDataStream out(&tempFile);
    out >> cutList;
    tempFile.close();
  }
  pasteLauncher(QApplication::clipboard()->mimeData(), newPath, cutList);
}
//---------------------------------------------------------------------------

/**
 * @brief Drags data to the new location
 * @param data data to be pasted
 * @param newPath path of new location
 * @param dragMode mode of dragging
 */
void MainWindow::dragLauncher(const QMimeData *data, const QString &newPath,
                              Common::DragMode dragMode) {

  // Retrieve urls (paths) of data
  QList<QUrl> files = data->urls();

  // get original path
  QStringList getOldPath = files.at(0).toLocalFile().split("/", Qt::SkipEmptyParts);
  QString oldPath;
  for (int i=0;i<getOldPath.size()-1;++i) { oldPath.append(QString("/%1").arg(getOldPath.at(i))); }
  QString oldDevice = Common::getDeviceForDir(oldPath);
  QString newDevice = Common::getDeviceForDir(newPath);

  qDebug() << "oldpath:" << oldDevice << oldPath;
  qDebug() << "newpath:" << newDevice << newPath;

  QString extraText;
  Common::DragMode currentDragMode = dragMode;
  if (oldDevice != newDevice) {
      extraText = QString(tr("Source and destination is on a different storage."));
      currentDragMode = Common::DM_UNKNOWN;
  }

  // If drag mode is unknown then ask what to do
  if (currentDragMode == Common::DM_UNKNOWN) {
    QMessageBox box;
    box.setWindowTitle(tr("Select file action"));
    box.setWindowIcon(QIcon::fromTheme("qtfm", QIcon(":/icons/app.svg")));
    box.setIconPixmap(QIcon::fromTheme("dialog-information").pixmap(QSize(32, 32)));
    box.setText(tr("<h3>What do you want to do?</h3>"));
    if (!extraText.isEmpty()) {
        box.setText(QString("%1<p>%2</p>").arg(box.text()).arg(extraText));
    }
    QAbstractButton *move = box.addButton(tr("Move here"), QMessageBox::ActionRole);
    QAbstractButton *copy = box.addButton(tr("Copy here"), QMessageBox::ActionRole);
    QAbstractButton *link = box.addButton(tr("Link here"), QMessageBox::ActionRole);
    QAbstractButton *canc = box.addButton(QMessageBox::Cancel);
    move->setIcon(QIcon::fromTheme("edit-cut"));
    copy->setIcon(QIcon::fromTheme("edit-copy"));
    link->setIcon(QIcon::fromTheme("insert-link"));
    canc->setIcon(QIcon::fromTheme("edit-delete"));

    box.exec();
    if (box.clickedButton() == move) {
      dragMode = Common::DM_MOVE;
    } else if (box.clickedButton() == copy) {
      dragMode = Common::DM_COPY;
    } else if (box.clickedButton() == link) {
      dragMode = Common::DM_LINK;
    } else if (box.clickedButton() == canc) {
      return;
    }
    currentDragMode = dragMode;
  }

  // If moving is enabled, cut files from the original location
  QStringList cutList;
  if (currentDragMode == Common::DM_MOVE) {
    foreach (QUrl item, files) {
      cutList.append(item.path());
    }
  }

  // Paste launcher (this method has to be called instead of that with 'data'
  // parameter, because that 'data' can timeout)
  pasteLauncher(files, newPath, cutList, dragMode == Common::DM_LINK);
}
//---------------------------------------------------------------------------

/**
 * @brief Pastes data to the new location
 * @param data data to be pasted
 * @param newPath path of new location
 * @param cutList list of items to remove
 */
void MainWindow::pasteLauncher(const QMimeData *data,
                               const QString &newPath,
                               const QStringList &cutList,
                               bool link)
{
  QList<QUrl> files = data->urls();
  if (files.isEmpty()) { return; }
  pasteLauncher(files, newPath, cutList, link);
}
//---------------------------------------------------------------------------


void MainWindow::pasteLauncher(const QList<QUrl> &files,
                                const QString &newPath,
                                const QStringList &cutList,
                                bool link)
{
    //qDebug() << "==> PASTE LAUNCHER v2" << "COPY" << files << "OR MOVE" << cutList << "TO" << newPath << "SYMLINK?" << link;

    if (!QFile::exists(newPath)) {
        qDebug() << "destination path does not exists" << newPath;
        return;
    }
    if (files.size()==0 && cutList.size()==0) {
        qDebug() << "nothing to copy or move ...";
        return;
    }
    if (link && (files.size()==0 || cutList.size()>0)) {
        qDebug() << "is symlink but nothing to copy ...";
        return;
    }

    QStringList _files, _dirs;
    if (cutList.size()>0) { // move
        for (int i=0; i<cutList.size(); i++) {
            QFileInfo info(cutList.at(i));
            if (!info.isDir() && !info.isFile()) { continue; }
            if (cutList.at(i) == (newPath + "/" + info.fileName())) { continue; }
            if (info.isDir()) { _dirs << info.absoluteFilePath(); }
            else if (info.isFile()) { _files << info.absoluteFilePath(); }
        }
        if (_files.size()>0 || _dirs.size()>0) {
            QtFileCopier *copyHandler = new QtFileCopier(this);
            QtCopyDialog *copyDialog = new QtCopyDialog(copyHandler, this);
            copyDialog->setMinimumDuration(100);
            copyDialog->setAutoClose(true);
            if (_files.size()>0) {
                copyHandler->moveFiles(_files, newPath);
                //qDebug() << "MOVE FILES" << _files << "TO" << newPath;
            }
            if (_dirs.size()>0) {
                for (int i=0;i<_dirs.size();++i) {
                    copyHandler->moveDirectory(_dirs.at(i), newPath);
                    //qDebug() << "MOVE DIR" << _dirs.at(i) << "TO" << newPath;
                }
            }
        }
    } else if (files.size()>0) { // copy
        for (int i=0; i<files.size(); i++) {
            QFileInfo info(files.at(i).toLocalFile());
            if (!info.isDir() && !info.isFile()) { continue; }
            if (info.isDir()) { _dirs << info.absoluteFilePath(); }
            else if (info.isFile()) { _files << info.absoluteFilePath(); }
        }
        if (_files.size()>0 || _dirs.size()>0) {
            QtFileCopier *copyHandler = new QtFileCopier(this);
            QtCopyDialog *copyDialog = new QtCopyDialog(copyHandler, this);
            copyDialog->setMinimumDuration(100);
            copyDialog->setAutoClose(true);
            if (_files.size()>0) {
                if (link) { copyHandler->copyFiles(_files, newPath, QtFileCopier::MakeLinks); }
                else { copyHandler->copyFiles(_files, newPath); }
                //qDebug() << "COPY FILES" << _files << "TO" << newPath << "SYMLINK?" << link;
            }
            if (_dirs.size()>0) {
                for (int i=0;i<_dirs.size();++i) {
                    if (link) { copyHandler->copyDirectory(_dirs.at(i), newPath, QtFileCopier::MakeLinks); }
                    else { copyHandler->copyDirectory(_dirs.at(i), newPath); }
                    //qDebug() << "COPY DIR" <<_dirs.at(i) << "TO" << newPath << "SYMLINK?" << link;
                }
            }
        }
    }
}

//---------------------------------------------------------------------------


//---------------------------------------------------------------------------



//---------------------------------------------------------------------------

void MainWindow::folderPropertiesLauncher()
{
    QModelIndexList selList;
    if (focusWidget() == bookmarksList) {
        selList.append(modelView->mapFromSource(modelList->index(bookmarksList->currentIndex().data(BOOKMARK_PATH).toString())));
    } else if (focusWidget() == list || focusWidget() == detailTree) {
        if (listSelectionModel->selectedRows(0).count()) { selList = listSelectionModel->selectedRows(0); }
        else { selList = listSelectionModel->selectedIndexes(); }
    }

    if (selList.count() == 0) { selList << modelView->mapFromSource(modelList->index(pathEdit->currentText())); }

    QStringList paths;

    foreach (QModelIndex item, selList) {
        paths.append(modelList->filePath(modelView->mapToSource(item)));
    }

    properties = new PropertiesDialog(paths, modelList);
    connect(properties,SIGNAL(propertiesUpdated()),this,SLOT(clearCutItems()));
}

//---------------------------------------------------------------------------

/**
 * @brief Writes settings into config file
 */
void MainWindow::writeSettings() {

  // Write general settings
  settings->setValue("viewMode", stackWidget->currentIndex());
  settings->setValue("iconMode", iconAct->isChecked());
  settings->setValue("fileViewMode",
                     currentView == 1 ? QStringLiteral("icon") : QStringLiteral("list"));
  settings->setValue("foldersAlwaysFirst", modelView->foldersAlwaysFirstSetting());
  settings->setValue("foldersAlwaysFirstIcon", modelView->foldersAlwaysFirstIconSetting());
  settings->setValue("zoom", zoom);
  settings->setValue("zoomTree", zoomTree);
  settings->setValue("zoomBook", zoomBook);
  settings->setValue("bookmarkGroupTabSize",
                     bookmarkGroupBar ? bookmarkGroupBar->tabButtonSize() : bookmarkGroupTabSize);
  settings->setValue("zoomList", zoomList);
  settings->setValue("zoomDetail", zoomDetail);
  settings->setValue("iconViewGapH", iconViewGapH);
  settings->setValue("iconViewGapV", iconViewGapV);
  settings->setValue("iconViewGap", iconViewGapH);
  settings->setValue("topModuleGap", topModuleGapV);
  settings->setValue("topModuleGapV", topModuleGapV);
  settings->setValue("topModuleGapH", topModuleGapH);
  settings->setValue("sortBy", currentSortColumn);
  settings->setValue("sortOrder", currentSortOrder);
  settings->setValue("showThumbs", thumbsAct->isChecked());
  settings->setValue("hiddenMode", hiddenAct->isChecked());
  settings->setValue("lockLayout", lockLayoutAct->isChecked());
  settings->setValue("tabsOnTop", tabsOnTopAct->isChecked());
  settings->setValue("windowState", saveState(1));
  settings->setValue("windowGeo", saveGeometry());
  settings->setValue("windowMax", isMaximized());
  settings->setValue("header", detailTree->header()->saveState());
  QHeaderView *listHeader = detailTree->header();
  for (int col = 0; col < LIST_COLUMN_COUNT; ++col) {
      settings->setValue(QStringLiteral("listColumnWidth%1").arg(col),
                         listHeader->sectionSize(col));
  }
  settings->setValue("realMimeTypes",  modelList->isRealMimeTypes());

  // Write bookmarks
  writeBookmarks();
}
//---------------------------------------------------------------------------

/**
 * @brief Display popup menu
 * @param event
 */
void MainWindow::contextMenuEvent(QContextMenuEvent * event) {

  QWidget *hit = QApplication::widgetAt(event->globalPos());
  const auto isUnder = [hit](QWidget *root) {
      if (!hit || !root) {
          return false;
      }
      for (QWidget *w = hit; w; w = w->parentWidget()) {
          if (w == root) {
              return true;
          }
      }
      return false;
  };

  if (bookmarkGroupBar && isUnder(bookmarkGroupBar)) {
      return;
  }

  // Retrieve widget under mouse
  QMenu *popup;
  QWidget *widget = childAt(event->pos());
  //qDebug() << "WIDGET" << widget;

  // Create popup for tab or for status bar
  if (widget == tabs) {
    popup = new QMenu(this);
    popup->addAction(closeTabAct);
    popup->exec(event->globalPos());
    return;
  } else if (widget == status) {
    popup = createPopupMenu();
    popup->addSeparator();
    popup->addAction(lockLayoutAct);
    popup->exec(event->globalPos());
    return;
  } else if (widget == navToolBar) {
      qDebug() << "TOOLBAR";
      return;
  }

  QToolButton *isToolButton = dynamic_cast<QToolButton*>(childAt(event->pos()));
  if (isToolButton) {
      qDebug() << "TOOLBUTTON";
      return;
  }

#ifndef Q_OS_MAC
  QMenuBar *isMenuBar = dynamic_cast<QMenuBar*>(childAt(event->pos()));
  if (isMenuBar) {
      qDebug() << "MENUBAR";
      return;
  }
#endif

  // Continue with popups for folders and files
  QList<QAction*> actions;
  popup = new QMenu(this);

  bool isMedia = false;
  bool isTreeFile = false;

  if (isUnder(bookmarksList)) {
    listSelectionModel->clearSelection();
    const QModelIndex hit = bookmarksList->indexAt(
        bookmarksList->mapFromGlobal(event->globalPos()));
    if (hit.isValid()) {
      bookmarksList->setCurrentIndex(hit);
      const QModelIndex src = bookmarkListProxy->mapToSource(hit);
      const QString bookmarkPath = src.data(BOOKMARK_PATH).toString();
      const bool isSeparator = src.data(Qt::DisplayRole).toString().isEmpty()
                               && bookmarkPath.isEmpty();
      if (isSeparator) {
        popup->addAction(removeSeparatorAct);
      } else {
        if (!bookmarkPath.isEmpty()) {
          popup->addAction(renameBookmarkAct);
          popup->addAction(editBookmarkAct);
        }
        popup->addSeparator();
        popup->addAction(delBookmarkAct);
      }
    } else {
      bookmarksList->clearSelection();
      popup->addAction(addSeparatorAct);
      popup->addAction(wrapBookmarksAct);
    }
    popup->exec(event->globalPos());
    delete popup;
    return;
  }

  if (disksList && isUnder(disksList)) {
    listSelectionModel->clearSelection();
    if (disksList->indexAt(disksList->mapFromGlobal(event->globalPos())).isValid()) {
#if defined(QTFM_HAVE_SIDEBAR_DISKS)
      if (disksList->currentIndex().data(DISK_IS_SEPARATOR).toBool()) {
          delete popup;
          return;
      }
      isMedia = true;
      const QString mediaPath = disksList->currentIndex().data(DISK_DEVICE_PATH).toString();
      const QString mountpoint = disksList->currentIndex().data(DISK_MOUNTPOINT).toString();
      const bool optical = disksList->currentIndex().data(DISK_IS_OPTICAL).toBool();
#ifndef NO_UDISKS
      const QString opPath = diskOperationBlockPath(mediaPath, disks->devices);
      if (!mediaPath.isEmpty() && disks->devices.contains(opPath)) {
          const QString mp = effectiveMountpointForWholeDisk(disks->devices[mediaPath], disks->devices);
          if (!mp.isEmpty()) {
              popup->addAction(mediaUnmountAct);
          } else if (disks->devices[mediaPath]->isOptical) {
              popup->addAction(mediaEjectAct);
          }
      }
#elif defined(Q_OS_MAC)
      if (!mediaPath.isEmpty()) {
          if (!mountpoint.isEmpty()) {
              popup->addAction(mediaUnmountAct);
          } else if (optical) {
              popup->addAction(mediaEjectAct);
          }
      }
#endif
#endif
    } else {
      disksList->clearSelection();
    }
    if (!popup->actions().isEmpty()) {
      popup->exec(event->globalPos());
    }
    delete popup;
    return;
  }

  if (focusWidget() == list || focusWidget() == detailTree || isUnder(stackWidget)) {

    if (!isUnder(stackWidget)) {
      delete popup;
      return;
    }

    if (currentView == 1 && isUnder(list)) {
      const QPoint vp = list->viewport()->mapFromGlobal(event->globalPos());
      const QModelIndex idx = list->indexAt(vp);
      if (!idx.isValid()) {
        listSelectionModel->clearSelection();
      } else {
        listSelectionModel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
      }
    }

    if (currentView == 2 && isUnder(detailTree)) {
      const QPoint vp = detailTree->viewport()->mapFromGlobal(event->globalPos());
      if (!detailTree->isNameColumnRowHit(vp)) {
        listSelectionModel->clearSelection();
      } else {
        const QModelIndex idx = detailTree->indexAt(vp);
        if (idx.isValid()) {
          listSelectionModel->setCurrentIndex(
              idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
      }
    }

    // Clear selection in bookmarks
    bookmarksList->clearSelection();

    // Could be file or folder
    if (listSelectionModel->hasSelection()) {

      // Get index of source model
      curIndex = modelList->filePath(modelView->mapToSource(listSelectionModel->currentIndex()));

      // File
      if (!curIndex.isDir()) {
        QString type = modelList->getMimeType(modelList->index(curIndex.filePath()));

        // Add custom actions to the list of actions
        QHashIterator<QString, QAction*> i(*customActManager->getActions());
        while (i.hasNext()) {
          i.next();
          qDebug() << "custom action" << i.key() << i.key() << i.value();
          if (curIndex.completeSuffix().endsWith(i.key())) { actions.append(i.value()); }
        }

        // Add run action or open with default application action
        if (curIndex.isExecutable() || curIndex.isBundle() || type.endsWith("appimage") || curIndex.absoluteFilePath().endsWith(".desktop")) {
          popup->addAction(runAct);
        } else {
          popup->addAction(openAct);
        }

        // Add open action
        /*foreach (QAction* action, actions) {
          if (action->text() == "Open") {
            popup->addAction(action);
            break;
          }
        }*/

        // Add open with menu
        popup->addSeparator();
        popup->addMenu(createOpenWithMenu());
        //if (popup->actions().count() == 0) popup->addAction(openAct);

        // Add custom actions that are associated only with this file type
        if (!actions.isEmpty()) {
          popup->addSeparator();
          popup->addActions(actions);
          popup->addSeparator();
        }

        // Add menus
        // TODO: ???
        QHashIterator<QString, QMenu*> m(*customActManager->getMenus());
        while (m.hasNext()) {
          m.next();
          if (curIndex.completeSuffix().endsWith(m.key())) { popup->addMenu(m.value()); }
        }

        // Add cut/copy/paste/rename actions
        popup->addSeparator();
        popup->addAction(cutAct);
        popup->addAction(copyAct);
        popup->addAction(pasteAct);
#ifdef Q_OS_MAC
        popup->addAction(macCopyFilePathAct);
        if (type.startsWith(QLatin1String("image/"))) {
          popup->addAction(macCopyImageToClipboardAct);
        }
#endif
        popup->addSeparator();
        popup->addAction(renameAct);
        popup->addSeparator();

        // Add custom actions that are associated with all file types
        foreach (QMenu* parent, customActManager->getMenus()->values("*")) {
          popup->addMenu(parent);
        }
        actions = (customActManager->getActions()->values("*"));
        popup->addActions(actions);
        if (customActManager->getActionList()->size()>0) {
            popup->addSeparator();
        }
        if (modelList->getRootPath() != trashDir) {
            popup->addAction(trashAct);
        }
        popup->addAction(deleteAct);
        popup->addSeparator();
        actions = customActManager->getActions()->values(curIndex.path());    //children of $parent
        if (actions.count()) {
          popup->addActions(actions);
          popup->addSeparator();
        }
      }
      // Folder/directory
      else {
        //popup->addAction(openAct);
        popup->addAction(openInTabAct);
        popup->addAction(openInNewWindowAct);
        popup->addSeparator();
        popup->addAction(addBookmarkAct);
        popup->addSeparator();
        popup->addAction(cutAct);
        popup->addAction(copyAct);
        popup->addAction(pasteAct);
#ifdef Q_OS_MAC
        popup->addAction(macCopyFilePathAct);
#endif
        popup->addSeparator();
        popup->addAction(renameAct);
        popup->addSeparator();

        foreach (QMenu* parent, customActManager->getMenus()->values("*")) {
          popup->addMenu(parent);
        }

        actions = customActManager->getActions()->values("*");
        popup->addActions(actions);
        if (modelList->getRootPath() != trashDir) {
            popup->addAction(trashAct);
        }
        popup->addAction(deleteAct);
        popup->addSeparator();

        foreach (QMenu* parent, customActManager->getMenus()->values("folder")) {
          popup->addMenu(parent);
        }
        actions = customActManager->getActions()->values(curIndex.fileName());   // specific folder
        actions.append(customActManager->getActions()->values(curIndex.path())); // children of $parent
        actions.append(customActManager->getActions()->values("folder"));        // all folders
        if (actions.count()) {
          popup->addActions(actions);
          popup->addSeparator();
        }
      }
      popup->addAction(folderPropertiesAct);
    }
    // Whitespace
    else {
      popup->addAction(backAct);
      popup->addAction(upAct);
      popup->addAction(homeAct);
      popup->addAction(refreshAct);
      popup->addSeparator();
      popup->addAction(newDirAct);
      popup->addAction(newFileAct);
      popup->addAction(newMdFileAct);
      popup->addAction(newTxtFileAct);
#ifdef Q_OS_MAC
      popup->addSeparator();
      macPasteImageAct->setEnabled(macClipboardHasImage());
      macPasteTextAct->setEnabled(!QApplication::clipboard()->text().isEmpty());
      popup->addAction(macPasteImageAct);
      popup->addAction(macPasteTextAct);
      popup->addAction(macOpenTerminalHereAct);
#endif
      popup->addSeparator();
      if (pasteAct->isEnabled()) {
        popup->addAction(pasteAct);
        popup->addSeparator();
      }
      popup->addAction(addBookmarkAct);
      popup->addSeparator();

      foreach (QMenu* parent, customActManager->getMenus()->values("folder")) {
        popup->addMenu(parent);
      }
      actions = customActManager->getActions()->values(curIndex.fileName());
      actions.append(customActManager->getActions()->values("folder"));
      if (actions.count()) {
        foreach (QAction*action, actions) {
          popup->addAction(action);
        }
        popup->addSeparator();
      }
      popup->addAction(folderPropertiesAct);
    }
  }
  // Tree (sidebar folder tree)
  else {
      curIndex = modelList->filePath(modelTree->mapToSource(tree->currentIndex()));
      if (curIndex.isFile()) { isTreeFile = true;}

      bookmarksList->clearSelection();
      if (!isTreeFile) { // only for folders (for now)
          popup->addAction(newDirAct);
          popup->addAction(newFileAct);
          popup->addAction(newWinAct);
          popup->addAction(openTabAct);
          popup->addAction(openInNewWindowAct);
          popup->addSeparator();
          popup->addAction(cutAct);
          popup->addAction(copyAct);
          popup->addAction(pasteAct);
          popup->addSeparator();
          popup->addAction(renameAct);
          popup->addSeparator();
          if (modelList->getRootPath() != trashDir) {
            popup->addAction(trashAct);
          }
          popup->addAction(deleteAct);
      }
    popup->addSeparator();

    if (!isTreeFile) { // not a selected file in tree (dock)
        foreach (QMenu* parent, customActManager->getMenus()->values("folder")) {
          popup->addMenu(parent);
        }
        actions = customActManager->getActions()->values(curIndex.fileName());
        actions.append(customActManager->getActions()->values(curIndex.path()));
        actions.append(customActManager->getActions()->values("folder"));
        if (actions.count()) {
          foreach (QAction*action, actions) { popup->addAction(action); }
          popup->addSeparator();
        }
        if (!isMedia && !curIndex.path().isEmpty()) { popup->addAction(folderPropertiesAct); }
    }
  }

  if (popup->actions().isEmpty()) {
    delete popup;
    return;
  }
  popup->exec(event->globalPos());
  delete popup;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates menu for opening file in selected application
 * @return menu
 */
QMenu* MainWindow::createOpenWithMenu() {

  qDebug() << "open with";
  if (settings) {
    OpenWithConfig::load(settings);
  }
  QMenu *openMenu = new QMenu(tr("Open with"));

  QAction *selectAppAct = new QAction(tr("Select..."), openMenu);
  selectAppAct->setStatusTip(tr("Select application for opening the file"));
  connect(selectAppAct, SIGNAL(triggered()), this, SLOT(selectAppForFiles()));

  const QFileInfo fileInfo(curIndex.filePath());
  const QList<OpenWithEntry> handlers = OpenWithConfig::handlersFor(fileInfo);

  QList<QAction*> customApps;
  foreach (const OpenWithEntry &entry, handlers) {
    const QString cmd = entry.fullCommand();
    if (cmd.isEmpty()) {
      continue;
    }
    QAction *action = new QAction(entry.name.isEmpty() ? cmd : entry.name, openMenu);
    action->setData(cmd);
    const QIcon icon = entry.menuIcon();
    if (!icon.isNull()) {
      action->setIcon(icon);
    }
    connect(action, SIGNAL(triggered()), SLOT(openWithConfiguredApp()));
    customApps.append(action);
    openMenu->addAction(action);
  }

  if (customApps.isEmpty()) {
    QString mime = mimeUtils->getMimeType(curIndex.filePath());
    QStringList appNames = mimeUtils->getDefault(mime);
    if (appNames.size()==1 && appNames.at(0).isEmpty() && mime.startsWith("text/")) {
      appNames = mimeUtils->getDefault("text/plain");
    }
    foreach (QString appName, appNames) {
      if (appName.isEmpty()) { continue; }
      QString appDesktopFile = Common::findApplication(qApp->applicationFilePath(), appName);
      if (appDesktopFile.isEmpty()) { continue; }
      DesktopFile df = DesktopFile(appDesktopFile);
      QAction* action = new QAction(df.getName(), openMenu);
      action->setData(appDesktopFile);
      action->setIcon(FileUtils::searchAppIcon(df));
      connect(action, SIGNAL(triggered()), SLOT(openInApp()));
      customApps.append(action);
      openMenu->addAction(action);
    }
#ifdef Q_OS_MAC
    if (customApps.isEmpty() && curIndex.exists() && !curIndex.isDir()) {
      QAction *systemDefault = new QAction(tr("Default Application"), openMenu);
      connect(systemDefault, &QAction::triggered, this, [this]() {
        mimeUtils->openInApp(curIndex, QString());
      });
      openMenu->addAction(systemDefault);
      customApps.append(systemDefault);
    }
#endif
  }

  if (!customApps.isEmpty()) {
    openMenu->addSeparator();
  }
  openMenu->addAction(selectAppAct);
  return openMenu;
}

void MainWindow::openWithConfiguredApp()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (!action) {
    return;
  }
  const QString cmd = action->data().toString();
  if (cmd.isEmpty()) {
    return;
  }

  QModelIndexList items;
  if (listSelectionModel->selectedRows(0).count()) {
    items = listSelectionModel->selectedRows(0);
  } else {
    items = listSelectionModel->selectedIndexes();
  }
  if (items.isEmpty()) {
    mimeUtils->openInApp(cmd, curIndex, QString());
    return;
  }
  foreach (QModelIndex index, items) {
    const QFileInfo fi(modelList->filePath(modelView->mapToSource(index)));
    if (fi.isDir()) {
      continue;
    }
    mimeUtils->openInApp(cmd, fi, QString());
  }
}

bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(e);
        switch (me->button()) {
        case Qt::BackButton:
            goBackDir();
            break;
        default:;
        }
    }

      if (dynamic_cast<QListView*>(o) != nullptr ){
        if (e->type()==QEvent::KeyPress) {
            QKeyEvent* key = static_cast<QKeyEvent*>(e);
            if ( (key->key()==Qt::Key_Tab) ) {
                qDebug()<< "Tab pressed: path completion "<< o ;
                QListView *completionList = dynamic_cast<QListView*>(o);
                // Remove incomplete phrase and replace it with a current index
                QModelIndex index = completionList->currentIndex();
                QString itemText = index.data(Qt::DisplayRole).toString();
                QString currentPath = pathEdit->lineEdit()->text();
                QStringList tempList = currentPath.split("/");
                tempList.takeLast();
                tempList << itemText;
                QString newPath = tempList.join("/");
                pathEdit->lineEdit()->setText(newPath);
                // Force update the Main View
                pathEditChanged(newPath);
                // Enter Edit Mode right after the event system is ready
                QTimer::singleShot(0, pathEdit->lineEdit(), SLOT(setFocus()));
                // Add the trailing / for subsequent completions
                pathEdit->lineEdit()->setText(newPath + QString("/"));
            }
        }
    }
    return QMainWindow::eventFilter(o, e);
}

void MainWindow::refresh(bool modelRefresh, bool loadDir)
{
    qDebug() << "refresh" << modelRefresh << loadDir;
    if (modelRefresh) {
        modelList->refreshItems();
        modelList->forceRefresh();
    }

    QModelIndex baseIndex = modelView->mapFromSource(modelList->index(pathEdit->currentText()));
    if (currentView == 2) { detailTree->setRootIndex(baseIndex); }
    else { list->setRootIndex(baseIndex); }

    pathEditChanged(pathEdit->currentText());

    if (loadDir) {
        qDebug() << "trigger dirloaded from refresh";
        dirLoaded();
    }
}

void MainWindow::enableReload()
{
    qDebug() << "enable reload";
    ignoreReload = false;
}
//---------------------------------------------------------------------------

/**
 * @brief Selects application for opening file
 */
void MainWindow::selectApp() {
  ApplicationDialog *dialog = new ApplicationDialog(true, this);
  if (dialog->exec()) {
    if (dialog->getCurrentLauncher().compare("") != 0) {
      QString appName = dialog->getCurrentLauncher() + ".desktop";
      QString desktop = Common::findApplication(qApp->applicationFilePath(), appName);
      if (desktop.isEmpty()) { return; }
      DesktopFile df = DesktopFile(desktop);
      QModelIndex idx = listSelectionModel->currentIndex();
      if (!idx.isValid()) { return; }
      QFileInfo fileInfo(modelList->filePath(modelView->mapToSource(idx)));
      mimeUtils->openInApp(df.getExec(), fileInfo, df.isTerminal()?term:"");
    }
  }
}

void MainWindow::selectAppForFiles()
{
    // Selection
    QModelIndexList items;
    if (listSelectionModel->selectedRows(0).count()) {
      items = listSelectionModel->selectedRows(0);
    } else {
      items = listSelectionModel->selectedIndexes();
    }

    // Files
    QStringList files;
    foreach (QModelIndex index, items) {
      //executeFile(index, 0);
      QModelIndex srcIndex = modelView->mapToSource(index);
      files << modelList->filePath(srcIndex);
    }

    // Select application in the dialog
    ApplicationDialog *dialog = new ApplicationDialog(true, this);
    if (dialog->exec()) {
      if (dialog->getCurrentLauncher().compare("") != 0) {
        QString appName = dialog->getCurrentLauncher() + ".desktop";
        QString desktop = Common::findApplication(qApp->applicationFilePath(), appName);
        if (desktop.isEmpty()) { return; }
        DesktopFile df = DesktopFile(desktop);
        if (df.getExec().contains("%F") || df.getExec().contains("%U")) { // app supports multiple files
            mimeUtils->openFilesInApp(df.getExec(), files, df.isTerminal()?term:"");
        } else { // launch new instance for each file
            for (int i=0;i<files.size();++i) {
                QFileInfo fileInfo(files.at(i));
                mimeUtils->openInApp(df.getExec(), fileInfo, df.isTerminal()?term:"");
            }
        }
      }
    }
}
//---------------------------------------------------------------------------

/**
 * @brief Opens files in application
 */
void MainWindow::openInApp()
{
    QAction* action = dynamic_cast<QAction*>(sender());
    if (!action) { return; }
    DesktopFile df = DesktopFile(action->data().toString());
    if (df.getExec().isEmpty()) { return; }

    // get selection
    QModelIndexList items;
    if (listSelectionModel->selectedRows(0).count()) {
        items = listSelectionModel->selectedRows(0);
    } else {
        items = listSelectionModel->selectedIndexes();
    }

    // get files and mimes
    QStringList fileList;
    foreach (QModelIndex index, items) {
        QModelIndex srcIndex = modelView->mapToSource(index);
        QString filePath = modelList->filePath(srcIndex);
        fileList << filePath;
    }

    if (df.getExec().contains("%F") || df.getExec().contains("%U")) { // app supports multiple files
        mimeUtils->openFilesInApp(df.getExec(), fileList, df.isTerminal()?term:"");
    } else { // launch new instance for each file
        for (int i=0;i<fileList.size();++i) {
            QFileInfo fileInfo(fileList.at(i));
            mimeUtils->openInApp(df.getExec(), fileInfo, df.isTerminal()?term:"");
        }
    }
}

void MainWindow::updateGrid()
{
    if (list->viewMode() != QListView::IconMode) { return; }
    ivdelegate->setCellGaps(iconViewGapH, iconViewGapV);
    const QSize grid = IconViewDelegate::iconGridSize(
        zoom, iconViewGapH, iconViewGapV, fontMetrics());
    list->setIconSize(QSize(zoom, zoom));
    if (list->gridSize() != grid) {
        list->setGridSize(grid);
    }
}

//---------------------------------------------------------------------------
/**
 * @brief media support
 */
#if defined(QTFM_HAVE_SIDEBAR_DISKS)
namespace {

QString diskDisplayNameFromTitle(const QString &rowTitle)
{
    const int sep = rowTitle.indexOf(QStringLiteral(" — "));
    if (sep >= 0) {
        return rowTitle.mid(sep + 3).trimmed();
    }
    return rowTitle;
}

QString diskWholeGroupKey(const QString &deviceIdentifier)
{
    const int s = deviceIdentifier.indexOf(QLatin1Char('s'));
    if (s > 0) {
        return deviceIdentifier.left(s);
    }
    return deviceIdentifier;
}

struct DiskPopulateItem {
    QString devicePath;
    QString name;
    QString mountpoint;
    bool isOptical = false;
    QString groupKey;
    qint64 groupTotalBytes = 0;
};

void applyGroupedDisks(disksModel *model, QVector<DiskPopulateItem> items)
{
    std::sort(items.begin(), items.end(),
              [](const DiskPopulateItem &a, const DiskPopulateItem &b) {
                  if (a.groupKey != b.groupKey) {
                      if (a.groupTotalBytes != b.groupTotalBytes) {
                          return a.groupTotalBytes > b.groupTotalBytes;
                      }
                      return a.groupKey < b.groupKey;
                  }
                  return a.devicePath < b.devicePath;
              });

    QVector<DiskListRow> rows;
    QString lastGroup;
    for (const DiskPopulateItem &item : items) {
        if (!lastGroup.isEmpty() && item.groupKey != lastGroup) {
            DiskListRow sep;
            sep.separator = true;
            rows.append(sep);
        }
        DiskListRow row;
        row.devicePath = item.devicePath;
        row.name = item.name;
        row.mountpoint = item.mountpoint;
        row.isOptical = item.isOptical;
        rows.append(row);
        lastGroup = item.groupKey;
    }
    model->setRows(rows);
    model->refreshUsage();
}

} // namespace

void MainWindow::populateMedia()
{
    QVector<DiskPopulateItem> items;
#ifndef NO_UDISKS
    QMapIterator<QString, Device *> it(disks->devices);
    while (it.hasNext()) {
        it.next();
        Device *d = it.value();
        if (!shouldListWholeDisk(d, disks->devices)) { continue; }

        const QString mp = effectiveMountpointForWholeDisk(d, disks->devices);
        QString subtitle = d->name;
        if (subtitle.isEmpty()) { subtitle = tr("Storage"); }
        const QString rowTitle = QStringLiteral("%1 — %2").arg(d->dev, subtitle);
        DiskPopulateItem item;
        item.devicePath = d->path;
        item.name = diskDisplayNameFromTitle(rowTitle);
        item.mountpoint = mp;
        item.isOptical = d->isOptical;
        item.groupKey = d->drive.isEmpty() ? diskWholeGroupKey(d->dev) : d->drive;
        items.append(item);
    }
#elif defined(Q_OS_MAC)
    const QVector<MacDiskVolume> volumes = MacDisks::listVolumes();
    for (const MacDiskVolume &v : volumes) {
        DiskPopulateItem item;
        item.devicePath = v.deviceIdentifier;
        item.name = diskDisplayNameFromTitle(v.displayTitle);
        item.mountpoint = v.mountPoint;
        item.isOptical = v.isOptical;
        item.groupKey = v.physicalDiskGroup.isEmpty()
                            ? (v.wholeDiskIdentifier.isEmpty()
                                   ? diskWholeGroupKey(v.deviceIdentifier)
                                   : v.wholeDiskIdentifier)
                            : v.physicalDiskGroup;
        item.groupTotalBytes = v.physicalDiskSizeBytes;
        items.append(item);
    }
#endif
    applyGroupedDisks(modelDisks, items);
}

#ifndef NO_UDISKS
void MainWindow::handleMediaMountpointChanged(QString path, QString mountpoint)
{
    Q_UNUSED(path)
    Q_UNUSED(mountpoint)
    populateMedia();
}

void MainWindow::handleMediaAdded(QString path)
{
    Q_UNUSED(path)
    populateMedia();
}

void MainWindow::handleMediaRemoved(QString path)
{
    Q_UNUSED(path)
    populateMedia();
}

void MainWindow::handleMediaChanged(QString path, bool present)
{
    if (path.isEmpty()) { return; }
    if (disks->devices[path]->isOptical && !present && modelDisks->contains(path)) {
        handleMediaRemoved(path);
    } else if (disks->devices[path]->isOptical && present && !modelDisks->contains(path)) {
        handleMediaAdded(path);
    }
}

void MainWindow::handleMediaError(QString path, QString error)
{
    QMessageBox::warning(this, path, error);
}
#endif

void MainWindow::handleMediaUnmount()
{
    QModelIndex index = disksList->currentIndex();
    if (!index.isValid() || index.data(DISK_IS_SEPARATOR).toBool()) { return; }
    const QString path = index.data(DISK_DEVICE_PATH).toString();
    if (path.isEmpty()) { return; }
#ifndef NO_UDISKS
    const QString opPath = diskOperationBlockPath(path, disks->devices);
    if (disks->devices.contains(opPath)) {
        disks->devices[opPath]->unmount();
    }
#elif defined(Q_OS_MAC)
    if (!MacDisks::unmountVolume(path)) {
        QMessageBox::warning(this, tr("Disks"), MacDisks::lastErrorMessage());
    } else {
        populateMedia();
    }
#endif
}

void MainWindow::handleMediaEject()
{
    QModelIndex index = disksList->currentIndex();
    if (!index.isValid() || index.data(DISK_IS_SEPARATOR).toBool()) { return; }
    const QString path = index.data(DISK_DEVICE_PATH).toString();
    if (path.isEmpty()) { return; }
#ifndef NO_UDISKS
    if (disks->devices.contains(path)) {
        disks->devices[path]->eject();
    }
#elif defined(Q_OS_MAC)
    const int s = path.indexOf(QLatin1Char('s'));
    const QString whole = s > 0 ? path.left(s) : path;
    if (!MacDisks::ejectWholeDisk(whole)) {
        QMessageBox::warning(this, tr("Disks"), MacDisks::lastErrorMessage());
    } else {
        populateMedia();
    }
#endif
}

#endif

void MainWindow::diskActivated(QModelIndex item)
{
    if (!item.isValid()) { return; }
    if (item.data(DISK_IS_SEPARATOR).toBool()) { return; }
    QString mountpoint = item.data(DISK_MOUNTPOINT).toString();
#ifndef NO_UDISKS
    const QString devicePath = item.data(DISK_DEVICE_PATH).toString();
    if (mountpoint.isEmpty() && !devicePath.isEmpty() && disks->devices.contains(devicePath)) {
        const QString mountPath = mountableBlockPath(disks->devices[devicePath], disks->devices);
        if (disks->devices.contains(mountPath)) {
            disks->devices[mountPath]->mount();
        }
        mountpoint = modelDisks->index(item.row()).data(DISK_MOUNTPOINT).toString();
    }
#elif defined(Q_OS_MAC)
    const QString devicePath = item.data(DISK_DEVICE_PATH).toString();
    if (mountpoint.isEmpty() && !devicePath.isEmpty()) {
        if (MacDisks::mountVolume(devicePath)) {
            populateMedia();
            mountpoint = modelDisks->index(item.row()).data(DISK_MOUNTPOINT).toString();
        } else {
            QMessageBox::warning(this, tr("Disks"), MacDisks::lastErrorMessage());
            return;
        }
    }
#endif
    if (mountpoint.isEmpty()) { return; }
    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(mountpoint)));
    status->showMessage(Common::getDriveInfo(mountpoint));
}

void MainWindow::clearCache()
{
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Clear cache"),
        tr("Clear icon and thumbnail caches? You must restart QtFM for this to take effect."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }
    settings->setValue("clearCache", true);
    QMessageBox::information(this, tr("Close window"),
                             tr("Please close window to apply action."));
}

void MainWindow::handlePathRequested(QString path)
{
    qDebug() << "handle service path requested" << path;
    if (path == pathEdit->currentText() || path.isEmpty()) { return; }
    if (path.contains("/.")) { modelList->setRootPath(path); }
    pathEdit->setItemText(0, path);
    QTimer::singleShot(100, this, SLOT(slowPathEdit()));
}

void MainWindow::slowPathEdit()
{
    pathEditChanged(pathEdit->currentText());
    status->showMessage(Common::getDriveInfo(curIndex.filePath()));
}
//---------------------------------------------------------------------------

void MainWindow::actionMapper(QString cmd)
{
    QModelIndexList selList;
    QStringList temp;

    if (focusWidget() == list || focusWidget() == detailTree) {
        QFileInfo file = modelList->fileInfo(modelView->mapToSource(listSelectionModel->currentIndex()));

        if (file.isDir()) {
            cmd.replace("%n",file.fileName().replace(" ","\\"));
        } else {
            cmd.replace("%n",file.baseName().replace(" ","\\"));
        }

        if (listSelectionModel->selectedRows(0).count()) { selList = listSelectionModel->selectedRows(0); }
        else { selList = listSelectionModel->selectedIndexes(); }
    }
    else {
        selList << modelView->mapFromSource(modelList->index(curIndex.filePath()));
    }

    cmd.replace("~",QDir::homePath());


    //process any input tokens
    int pos = 0;
    while(pos >= 0) {
        pos = cmd.indexOf("%i",pos);
        if(pos != -1) {
            pos += 2;
            QString var = cmd.mid(pos,cmd.indexOf(" ",pos) - pos);
            QString input = QInputDialog::getText(this,tr("Input"), var, QLineEdit::Normal);
            if(input.isNull()) { return; } // cancelled
            else { cmd.replace("%i" + var,input); }
        }
    }


    foreach(QModelIndex index,selList) {
        temp.append(modelList->fileName(modelView->mapToSource(index)).replace(" ","\\"));
    }

    cmd.replace("%f",temp.join(" "));

    temp.clear();

    foreach(QModelIndex index,selList) {
        temp.append(QStringLiteral("\"") + modelList->filePath(modelView->mapToSource(index))
                    + QStringLiteral("\""));
    }

    cmd.replace("%F",temp.join(" "));

    customActManager->execAction(cmd, pathEdit->itemText(0));
}

//---------------------------------------------------------------------------------
void MainWindow::clearCutItems()
{
    qDebug() << "clearCutItems";
    //this refreshes existing items, sizes etc but doesn't re-sort
    modelList->clearCutItems();
    modelList->update();

    QModelIndex baseIndex = modelView->mapFromSource(modelList->index(pathEdit->currentText()));

    if (currentView == 2) { detailTree->setRootIndex(baseIndex); }
    else { list->setRootIndex(baseIndex); }

    qDebug() << "trigger updateDir from clearCutItems";
    QTimer::singleShot(50,this,SLOT(updateDir()));
}
