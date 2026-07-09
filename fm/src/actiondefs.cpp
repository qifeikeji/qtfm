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

#include "mainwindow.h"
#include "bundledicons.h"
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QApplication>
#include <QLineEdit>
#include <QWidget>
#include <QSizePolicy>
#include <QStatusBar>

void MainWindow::createActionIcons() {

  if (!actionIcons) {
    actionIcons = new QList<QIcon>;
  }
  actionIcons->clear();

  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("folder-new")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("file-new")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("cut")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("copy")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("paste")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("up")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("back")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("home")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("details")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("icons")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("hidden")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("bookmark")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("bookmark")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("clear")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("delete")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("preferences")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("properties")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("terminal")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("document-open")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("refresh")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("exit")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("lock")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("key-bindings")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("zoom-in")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("zoom-out")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("window-close")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("folder-new")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("user-trash")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("file-new")));
  actionIcons->append(BundledIcons::toolbarIcon(QStringLiteral("window-new")));
}

void MainWindow::applyBundledToolbarIcons()
{
  if (!newDirAct) {
    return;
  }
  createActionIcons();
  newDirAct->setIcon(actionIcons->at(0));
  newFileAct->setIcon(actionIcons->at(1));
  newMdFileAct->setIcon(actionIcons->at(1));
  newTxtFileAct->setIcon(actionIcons->at(1));
  newWinAct->setIcon(actionIcons->at(29));
  openTabAct->setIcon(actionIcons->at(26));
  openInTabAct->setIcon(actionIcons->at(26));
  closeTabAct->setIcon(actionIcons->at(25));
  cutAct->setIcon(actionIcons->at(2));
  copyAct->setIcon(actionIcons->at(3));
  pasteAct->setIcon(actionIcons->at(4));
  upAct->setIcon(actionIcons->at(5));
  backAct->setIcon(actionIcons->at(6));
  homeAct->setIcon(actionIcons->at(7));
  newTabAct->setIcon(actionIcons->at(26));
  listViewAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("view-list-mode")));
  iconAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("view-icon-mode")));
  hiddenAct->setIcon(actionIcons->at(10));
  addBookmarkAct->setIcon(actionIcons->at(12));
  delBookmarkAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("window-close")));
  renameBookmarkAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("rename")));
  editBookmarkAct->setIcon(actionIcons->at(15));
  trashAct->setIcon(actionIcons->at(27));
  deleteAct->setIcon(actionIcons->at(14));
  settingsAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("settings")));
  renameAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("rename")));
  terminalAct->setIcon(actionIcons->at(17));
  openAct->setIcon(actionIcons->at(18));
  runAct->setIcon(actionIcons->at(19));
  exitAct->setIcon(actionIcons->at(20));
  closeAct->setIcon(actionIcons->at(25));
  folderPropertiesAct->setIcon(actionIcons->at(16));
  lockLayoutAct->setIcon(actionIcons->at(21));
  refreshAct->setIcon(actionIcons->at(19));
  zoomInAct->setIcon(actionIcons->at(23));
  zoomOutAct->setIcon(actionIcons->at(24));
  if (tabsOnTopAct) {
    tabsOnTopAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("tabs-on-top")));
  }
  if (thumbsAct) {
    thumbsAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("show-thumbs")));
  }
  if (aboutAct) {
    aboutAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("about-qtfm")));
  }
  if (clearCacheAct) {
    clearCacheAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("clear-cache")));
  }
  if (openInNewWindowAct) {
    openInNewWindowAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("window-new")));
  }
  if (bookmarkGroupBar) {
    bookmarkGroupBar->refreshToolbarIcons();
  }
}

void MainWindow::refreshBundledUiIcons()
{
  applyBundledToolbarIcons();
}
//---------------------------------------------------------------------------

void MainWindow::createActions() {
  createActionIcons();
  actionList = new QList<QAction*>;

  newDirAct = new QAction(tr("New folder"), this);
  newDirAct->setStatusTip(tr("Create a new folder"));
  connect(newDirAct, SIGNAL(triggered()), this, SLOT(newDir()));
  newDirAct->setIcon(actionIcons->at(0));
  actionList->append(newDirAct);

  newFileAct = new QAction(tr("New file"), this);
  newFileAct->setStatusTip(tr("Create a new file"));
  connect(newFileAct, SIGNAL(triggered()), this, SLOT(newFile()));
  newFileAct->setIcon(actionIcons->at(1));
  actionList->append(newFileAct);

  newMdFileAct = new QAction(tr("New Markdown file"), this);
  newMdFileAct->setStatusTip(tr("Create a new Markdown (.md) file"));
  connect(newMdFileAct, SIGNAL(triggered()), this, SLOT(newMdFile()));
  newMdFileAct->setIcon(actionIcons->at(1));
  actionList->append(newMdFileAct);

  newTxtFileAct = new QAction(tr("New text file"), this);
  newTxtFileAct->setStatusTip(tr("Create a new text (.txt) file"));
  connect(newTxtFileAct, SIGNAL(triggered()), this, SLOT(newTxtFile()));
  newTxtFileAct->setIcon(actionIcons->at(1));
  actionList->append(newTxtFileAct);

  newWinAct = new QAction(tr("New window"), this);
  connect(newWinAct, SIGNAL(triggered()), this, SLOT(newWindow()));
  newWinAct->setIcon(actionIcons->at(29));
  actionList->append(newWinAct);

  openTabAct = new QAction(tr("New tab"), this);
  openTabAct->setStatusTip(tr("Middle-click things to open tab"));
  connect(openTabAct, SIGNAL(triggered()), this, SLOT(openTab()));
  openTabAct->setIcon(actionIcons->at(26));
  actionList->append(openTabAct);

  openInTabAct = new QAction(tr("Open in new tab"), this);
  connect(openInTabAct, SIGNAL(triggered()), this, SLOT(openNewTab()));
  openInTabAct->setIcon(actionIcons->at(26));

  openInNewWindowAct = new QAction(tr("Open in new window"), this);
  openInNewWindowAct->setIcon(actionIcons->at(29));
  connect(openInNewWindowAct, SIGNAL(triggered()), this, SLOT(openNewWindowFromSelection()));

  closeTabAct = new QAction(tr("Close tab"), this);
  closeTabAct->setStatusTip(tr("Middle-click tabs to close"));
  connect(closeTabAct, SIGNAL(triggered()), tabs, SLOT(closeTab()));
  closeTabAct->setIcon(actionIcons->at(25));
  actionList->append(closeTabAct);

  tabsOnTopAct = new QAction(tr("Tabs on top"), this);
  tabsOnTopAct->setStatusTip(tr("Tabs on top"));
  tabsOnTopAct->setCheckable(true);
  tabsOnTopAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("tabs-on-top")));
  connect(tabsOnTopAct, SIGNAL(triggered()), this, SLOT(tabsOnTop()));
  actionList->append(tabsOnTopAct);

  cutAct = new QAction(tr("Cut"), this);
  cutAct->setStatusTip(tr("Move the current file"));
  connect(cutAct, SIGNAL(triggered()), this, SLOT(cutFile()));
  cutAct->setIcon(actionIcons->at(2));
  actionList->append(cutAct);

  copyAct = new QAction(tr("Copy"), this);
  copyAct->setStatusTip(tr("Copy the current file"));
  connect(copyAct, SIGNAL(triggered()), this, SLOT(copyFile()));
  copyAct->setIcon(actionIcons->at(3));
  actionList->append(copyAct);

  pasteAct = new QAction(tr("Paste"), this);
  pasteAct->setStatusTip(tr("Paste the file here"));
  pasteAct->setEnabled(false);
  connect(pasteAct, SIGNAL(triggered()), this, SLOT(pasteClipboard()));
  pasteAct->setIcon(actionIcons->at(4));
  actionList->append(pasteAct);

  upAct = new QAction(tr("Up"),this);
  upAct->setStatusTip(tr("Go up one directory"));
  connect(upAct, SIGNAL(triggered()),this,SLOT(goUpDir()));
  upAct->setIcon(actionIcons->at(5));
  actionList->append(upAct);

  backAct = new QAction(tr("Back"),this);
  backAct->setStatusTip(tr("Go back one directory"));
  connect(backAct, SIGNAL(triggered()),this,SLOT(goBackDir()));
  actionIcons->append(newDirAct->icon());
  backAct->setIcon(actionIcons->at(6));
  actionList->append(backAct);

  homeAct = new QAction(tr("Home"),this);
  homeAct->setStatusTip(tr("Go to home directory"));
  connect(homeAct, SIGNAL(triggered()),this,SLOT(goHomeDir()));
  actionIcons->append(newDirAct->icon());
  homeAct->setIcon(actionIcons->at(7));
  actionList->append(homeAct);

  newTabAct = new QAction(tr("New tab"), this);
  newTabAct->setStatusTip(tr("Open new tab"));
  connect(newTabAct, SIGNAL(triggered()), this, SLOT(openTab()));
  newTabAct->setIcon(actionIcons->at(26));

  listViewAct = new QAction(tr("List view"), this);
  listViewAct->setStatusTip(tr("List view with columns (no icons)"));
  listViewAct->setCheckable(true);
  listViewAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("view-list-mode")));
  actionList->append(listViewAct);

  iconAct = new QAction(tr("Icon view"), this);
  iconAct->setStatusTip(tr("Icon view"));
  iconAct->setCheckable(true);
  connect(iconAct, SIGNAL(triggered()), this, SLOT(applyIconView()));
  iconAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("view-icon-mode")));
  actionList->append(iconAct);

  viewModeActGrp = new QActionGroup(this);
  viewModeActGrp->addAction(iconAct);
  viewModeActGrp->addAction(listViewAct);
  connect(listViewAct, SIGNAL(triggered()), this, SLOT(applyListView()));

  sortNameAct = new QAction(tr("Name"), this);
  sortNameAct->setStatusTip(tr("Sort icons by name"));
  sortNameAct->setCheckable(true);

  sortDateAct = new QAction(tr("Date"), this);
  sortDateAct->setStatusTip(tr("Sort icons by date"));
  sortDateAct->setCheckable(true);

  sortSizeAct = new QAction(tr("Size"), this);
  sortSizeAct->setStatusTip(tr("Sort icons by size"));
  sortSizeAct->setCheckable(true);

  sortByActGrp = new QActionGroup(this);
  sortByActGrp->addAction(sortNameAct);
  sortByActGrp->addAction(sortDateAct);
  sortByActGrp->addAction(sortSizeAct);
  connect(sortByActGrp, SIGNAL(triggered(QAction*)), SLOT(toggleSortBy(QAction*)));

  sortAscAct = new QAction(tr("Ascending"), this);
  sortAscAct->setStatusTip(tr("Sort icons in ascending order"));
  sortAscAct->setCheckable(true);
  connect(sortAscAct, SIGNAL(triggered()), this, SLOT(toggleSortOrder()));
  actionList->append(sortAscAct);

  sortMenu = new QMenu(tr("Sort By"));
  sortMenu->addAction(sortNameAct);
  sortMenu->addAction(sortDateAct);
  sortMenu->addAction(sortSizeAct);
  sortMenu->addSeparator();
  sortMenu->addAction(sortAscAct);

  hiddenAct = new QAction(tr("Hidden files"), this);
  hiddenAct->setStatusTip(tr("Toggle hidden files"));
  hiddenAct->setCheckable(true);
  connect(hiddenAct, SIGNAL(triggered()), this, SLOT(toggleHidden()));
  hiddenAct->setIcon(actionIcons->at(10));
  actionList->append(hiddenAct);

  // TODO: create filter act that will use grep like filtering
  /*filterAct = new QAction(tr("Filter..."), this);
  filterAct->setStatusTip(tr("Filter current directory"));
  connect(hiddenAct, SIGNAL(triggered()), this, SLOT(toggleFilter()));
  hiddenAct->setIcon(actionIcons->at(10));
  actionList->append(filterAct);*/

  addBookmarkAct = new QAction(tr("Add bookmark"), this);
  addBookmarkAct->setStatusTip(tr("Add this folder to bookmarks"));
  connect(addBookmarkAct, SIGNAL(triggered()), this, SLOT(addBookmarkAction()));
  addBookmarkAct->setIcon(actionIcons->at(12));
  actionList->append(addBookmarkAct);

  addSeparatorAct = new QAction(tr("Add separator"),this);
  addSeparatorAct->setStatusTip(tr("Add separator to bookmarks list"));
  connect(addSeparatorAct, SIGNAL(triggered()),this,SLOT(addSeparatorAction()));
  actionList->append(addSeparatorAct);

  delBookmarkAct = new QAction(tr("Remove bookmark"),this);
  delBookmarkAct->setStatusTip(tr("Remove this bookmark"));
  connect(delBookmarkAct, SIGNAL(triggered()),this,SLOT(delBookmark()));
  delBookmarkAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("window-close")));
  actionList->append(delBookmarkAct);

  removeSeparatorAct = new QAction(tr("Remove separator"), this);
  removeSeparatorAct->setStatusTip(tr("Remove this separator line"));
  connect(removeSeparatorAct, SIGNAL(triggered()), this, SLOT(removeSeparator()));
  removeSeparatorAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("window-close")));
  actionList->append(removeSeparatorAct);

  renameBookmarkAct = new QAction(tr("Rename bookmark"), this);
  renameBookmarkAct->setStatusTip(tr("Rename this bookmark"));
  connect(renameBookmarkAct, SIGNAL(triggered()), this, SLOT(renameBookmark()));
  renameBookmarkAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("rename")));
  actionList->append(renameBookmarkAct);

  editBookmarkAct = new QAction(tr("Edit icon"),this);
  editBookmarkAct->setStatusTip(tr("Change bookmark icon"));
  connect(editBookmarkAct, SIGNAL(triggered()),this,SLOT(editBookmark()));
  editBookmarkAct->setIcon(actionIcons->at(15));
  actionList->append(editBookmarkAct);

  wrapBookmarksAct = new QAction(tr("Wrap bookmarks"),this);
  wrapBookmarksAct->setCheckable(true);
  connect(wrapBookmarksAct, SIGNAL(triggered()),this,SLOT(toggleWrapBookmarks()));
  actionList->append(wrapBookmarksAct);

  trashAct = new QAction(tr("Move to Trash"), this);
  trashAct->setStatusTip(tr("Move selected to trash"));
  connect(trashAct, SIGNAL(triggered(bool)), this, SLOT(trashFile()));
  trashAct->setIcon(actionIcons->at(27));
  actionList->append(trashAct);

  deleteAct = new QAction(tr("Delete"), this);
  deleteAct->setStatusTip(tr("Delete selected"));
  connect(deleteAct, SIGNAL(triggered()), this, SLOT(deleteFile()));
  deleteAct->setIcon(actionIcons->at(14));
  actionList->append(deleteAct);

  settingsAct = new QAction(tr("Settings..."), this);
  settingsAct->setStatusTip(tr("Edit custom actions"));
  connect(settingsAct, SIGNAL(triggered()), this, SLOT(showEditDialog()));
  settingsAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("settings")));
  actionList->append(settingsAct);

  renameAct = new QAction(tr("Rename"), this);
  renameAct->setStatusTip(tr("Rename file"));
  renameAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("rename")));
  renameAct->setShortcut(QKeySequence(Qt::Key_F2));
  renameAct->setShortcutContext(Qt::ApplicationShortcut);
  connect(renameAct, SIGNAL(triggered()),this, SLOT(renameFile()));
  actionList->append(renameAct);

  terminalAct = new QAction(tr("Terminal"), this);
  terminalAct->setStatusTip(tr("Open virtual terminal"));
  connect(terminalAct, SIGNAL(triggered()), this, SLOT(terminalRun()));
  terminalAct->setIcon(actionIcons->at(17));
  actionList->append(terminalAct);

  openAct = new QAction(tr("Open"), this);
  openAct->setStatusTip(tr("Open the file"));
  connect(openAct, SIGNAL(triggered()), this, SLOT(openFile()));
  openAct->setIcon(actionIcons->at(18));
  actionList->append(openAct);

  openFolderAct = new QAction(tr("Enter folder"), this);
  connect(openFolderAct, SIGNAL(triggered()), this, SLOT(openFolderAction()));
  actionList->append(openFolderAct);

  runAct = new QAction(tr("Run"), this);
  runAct->setStatusTip(tr("Run this program"));
  connect(runAct, SIGNAL(triggered()), this, SLOT(runFile()));
  runAct->setIcon(actionIcons->at(19));
  actionList->append(runAct);

  exitAct = new QAction(tr("Quit"), this);
  exitAct->setStatusTip(tr("Quit %1").arg(APP_NAME));
  connect(exitAct, SIGNAL(triggered()), this, SLOT(exitAction()));
  exitAct->setIcon(actionIcons->at(20));
  actionList->append(exitAct);

  closeAct = new QAction(tr("Close"), this);
  closeAct->setStatusTip(tr("Close %1").arg(APP_NAME));
  connect(closeAct, SIGNAL(triggered()), this, SLOT(close()));
  closeAct->setIcon(actionIcons->at(25));
  actionList->append(closeAct);

  thumbsAct = new QAction(tr("Show thumbs"), this);
  thumbsAct->setStatusTip(tr("View thumbnails for image files"));
  thumbsAct->setCheckable(true);
  thumbsAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("show-thumbs")));
  connect(thumbsAct, SIGNAL(triggered()), this, SLOT(toggleThumbs()));
  actionList->append(thumbsAct);

  folderPropertiesAct = new QAction(tr("Properties"), this);
  folderPropertiesAct->setStatusTip(tr("View properties of selected items"));
  connect(folderPropertiesAct, SIGNAL(triggered()), this, SLOT(folderPropertiesLauncher()));
  folderPropertiesAct->setIcon(actionIcons->at(16));
  actionList->append(folderPropertiesAct);

  lockLayoutAct = new QAction(tr("Lock layout"), this);
  lockLayoutAct->setCheckable(true);
  connect(lockLayoutAct, SIGNAL(triggered()), this, SLOT(toggleLockLayout()));
  lockLayoutAct->setIcon(actionIcons->at(21));
  actionList->append(lockLayoutAct);

  /*escapeAct = new QAction(tr("Cancel"), this);
  connect(escapeAct, SIGNAL(triggered()), this, SLOT(refresh()));
  actionList->append(escapeAct);*/

  refreshAct = new QAction(tr("Refresh"), this);
  connect(refreshAct, SIGNAL(triggered()), this, SLOT(refresh()));
  refreshAct->setIcon(actionIcons->at(19));
  actionList->append(refreshAct);

  zoomInAct = new QAction(tr("Zoom in"), this);
  connect(zoomInAct, SIGNAL(triggered()), this, SLOT(zoomInAction()));
  zoomInAct->setIcon(actionIcons->at(23));
  actionList->append(zoomInAct);

  zoomOutAct = new QAction(tr("Zoom out"), this);
  connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(zoomOutAction()));
  zoomOutAct->setIcon(actionIcons->at(24));
  actionList->append(zoomOutAct);

  focusAddressAct = new QAction(tr("Focus address"), this);
  connect(focusAddressAct, SIGNAL(triggered()), this, SLOT(focusAction()));
  actionList->append(focusAddressAct);

  focusTreeAct = new QAction(tr("Focus tree"), this);
  connect(focusTreeAct, SIGNAL(triggered()), this, SLOT(focusAction()));
  actionList->append(focusTreeAct);

  focusBookmarksAct = new QAction(tr("Focus bookmarks"), this);
  connect(focusBookmarksAct, SIGNAL(triggered()), this, SLOT(focusAction()));
  actionList->append(focusBookmarksAct);

  focusListAct = new QAction(tr("Focus list"), this);
  connect(focusListAct, SIGNAL(triggered()), this, SLOT(focusAction()));
  actionList->append(focusListAct);

  aboutAct = new QAction(tr("About %1").arg(APP_NAME), this);
  aboutAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("about-qtfm")));
  connect(aboutAct, SIGNAL(triggered()), this, SLOT(showAboutBox()));
  actionList->append(aboutAct);

  aboutQtAct = new QAction(tr("About Qt"), this);
  aboutQtAct->setIcon(QIcon::fromTheme("qt-logo"));
  connect(aboutQtAct, SIGNAL(triggered(bool)), qApp, SLOT(aboutQt()));
  actionList->append(aboutQtAct);

#ifdef Q_OS_MAC
  macOpenWithHelpAct = new QAction(tr("macOS 打开方式设置…"), this);
  macOpenWithHelpAct->setStatusTip(tr("如何在设置里配置 Open with"));
  connect(macOpenWithHelpAct, SIGNAL(triggered()), this, SLOT(showMacOpenWithHelp()));
  actionList->append(macOpenWithHelpAct);
#endif

#ifndef NO_UDISKS
  mediaUnmountAct = new QAction(tr("Safely Remove"), this);
  mediaUnmountAct->setIcon(QIcon::fromTheme("media-eject"));
  connect(mediaUnmountAct, SIGNAL(triggered(bool)), this, SLOT(handleMediaUnmount()));

  mediaEjectAct = new QAction(tr("Eject"), this);
  mediaEjectAct->setIcon(QIcon::fromTheme("media-eject"));
  connect(mediaEjectAct, SIGNAL(triggered(bool)), this, SLOT(handleMediaEject()));
#endif
  clearCacheAct = new QAction(tr("Clear cache"), this);
  clearCacheAct->setIcon(BundledIcons::toolbarIcon(QStringLiteral("clear-cache")));
  connect(clearCacheAct, SIGNAL(triggered()), this, SLOT(clearCache()));

  // Icon list kept for refreshBundledUiIcons(); release after actions are built.
  delete actionIcons;
  actionIcons = nullptr;
}
//---------------------------------------------------------------------------

/**
 * @brief Reads shortcuts and registers actions
 */
void MainWindow::readShortcuts() {

  // This is called within the writing of bookmarks and QT can't handle writing
  // groups within groups cleanly. Save the current active group.
  settings->sync();
  QString startGroup = settings->group();
  if ( startGroup != "" ) { settings->endGroup(); }

  // Loads shortcuts
  QHash<QString, QString> shortcuts;
  settings->beginGroup("customShortcuts");
  QStringList keys = settings->childKeys();
  for (int i = 0; i < keys.count(); ++i) {
    QStringList temp(settings->value(keys.at(i)).toStringList());
    shortcuts.insert(temp.at(0),temp.at(1));
  }
  settings->endGroup();

  // Default shortcuts
  if (shortcuts.count() == 0) {
    shortcuts.insert(newWinAct->text(),"ctrl+n");
    shortcuts.insert(openTabAct->text(),"ctrl+t");
    shortcuts.insert(closeTabAct->text(),"ctrl+w");
    shortcuts.insert(cutAct->text(),"ctrl+x");
    shortcuts.insert(copyAct->text(),"ctrl+c");
    shortcuts.insert(pasteAct->text(),"ctrl+v");
    shortcuts.insert(upAct->text(),"alt+up");
    shortcuts.insert(backAct->text(),"backspace");
    //shortcuts.insert(homeAct->text(),"f1");
    shortcuts.insert(hiddenAct->text(),"ctrl+h");
    shortcuts.insert(trashAct->text(), "del");
    shortcuts.insert(deleteAct->text(),"shift+del");
    shortcuts.insert(terminalAct->text(),"f1");
    shortcuts.insert(exitAct->text(),"ctrl+q");
    shortcuts.insert(renameAct->text(),"f2");
    //shortcuts.insert(escapeAct->text(),"esc");
    shortcuts.insert(refreshAct->text(), "f5");
    shortcuts.insert(zoomOutAct->text(),"ctrl+-");
    shortcuts.insert(zoomInAct->text(),"ctrl++");
    shortcuts.insert(focusAddressAct->text(), "ctrl+l");
    shortcuts.insert(iconAct->text(), "f3");
    shortcuts.insert(listViewAct->text(), "f4");

    settings->beginGroup("customShortcuts");
    QHashIterator<QString, QString> i(shortcuts);
    int count = 0;
    while (i.hasNext()) {
        i.next();
        QStringList action;
        action << i.key() << i.value();
        settings->setValue(QString(count), action);
        ++count;
    }
    settings->endGroup();
    settings->sync();
  }

  // Remove all bookmarks from actions
  foreach (QAction* a, bookmarkActionList) {
    actionList->removeOne(a);
    delete a;
  }
  bookmarkActionList.clear();

  // Register bookmarks as actions
  QList<QStandardItem*> tmp = modelBookmarks->findItems("*", Qt::MatchWildcard);
  foreach (QStandardItem *item, tmp) {
    if (!item->text().isEmpty()) {
      QAction *tempAction = new QAction(item->icon(), item->text(), this);
      connect(tempAction, SIGNAL(triggered()), SLOT(bookmarkShortcutTrigger()));
      bookmarkActionList.append(tempAction);
      actionList->append(tempAction);
    }
  }

  // Add all actions to MainWindow so they work when menu is hidden
  // NOTE: QWidget can handle situation when two pointers to the same action
  // instance are added and holds only one of them
  foreach (QAction* action, *actionList) {
    QString text = shortcuts.value(action->text());
    if (!text.isEmpty()) {
      action->setShortcut(QKeySequence::fromString(text));
      addAction(action);
    }
  }

  // Restore the old group
  if ( settings->group() == "" ) {
    settings->beginGroup( startGroup );
  }
  else if ( settings->group() != startGroup ) {
    settings->endGroup();
    settings->sync();
    settings->beginGroup( startGroup );
  }

}
//---------------------------------------------------------------------------

void MainWindow::bookmarkShortcutTrigger() {
  QAction* sc = qobject_cast<QAction*>(sender());
  QModelIndex index = modelBookmarks->findItems(sc->text()).first()->index();
  bookmarksList->clearSelection();
  bookmarksList->setCurrentIndex(bookmarkListProxy->mapFromSource(index));
  bookmarkClicked(bookmarkListProxy->mapFromSource(index));
}
//---------------------------------------------------------------------------

void MainWindow::createMenus() {

  // File menu
  // ----------------------------------------------------------------------
  QMenu *fileMenu = new QMenu(tr("File"));
  fileMenu->addAction(newDirAct);
  fileMenu->addAction(newFileAct);
  fileMenu->addSeparator();
  fileMenu->addAction(newWinAct);
  fileMenu->addAction(openTabAct);
  fileMenu->addSeparator();
  fileMenu->addAction(closeAct);
  fileMenu->addAction(exitAct);

  // Edit menu
  // ----------------------------------------------------------------------
  QMenu *editMenu = new QMenu(tr("Edit"));
  editMenu->addAction(cutAct);
  editMenu->addAction(copyAct);
  editMenu->addAction(pasteAct);
  editMenu->addAction(renameAct);
  editMenu->addAction(trashAct);
  editMenu->addAction(deleteAct);
  editMenu->addSeparator();
  editMenu->addAction(addBookmarkAct);
  editMenu->addSeparator();
  editMenu->addAction(clearCacheAct);
  editMenu->addSeparator();
  editMenu->addAction(settingsAct);

  // View menu
  // ----------------------------------------------------------------------
  QMenu *viewMenu = new QMenu(tr("View"));

  QMenu *autoMenu = createPopupMenu();
  autoMenu->addSeparator();
  autoMenu->addAction(lockLayoutAct);
  autoMenu->setTitle(tr("Layout"));
  viewMenu->addMenu(autoMenu);

  viewMenu->addMenu(sortMenu);

  viewMenu->addSeparator();
  viewMenu->addAction(iconAct);
  viewMenu->addAction(listViewAct);
  viewMenu->addAction(hiddenAct);
  viewMenu->addSeparator();
  viewMenu->addAction(tabsOnTopAct);
  viewMenu->addAction(thumbsAct);
  viewMenu->addSeparator();
  viewMenu->addAction(zoomInAct);
  viewMenu->addAction(zoomOutAct);
  viewMenu->addSeparator();
  viewMenu->addAction(upAct);
  viewMenu->addAction(backAct);
  viewMenu->addAction(homeAct);
  viewMenu->addAction(refreshAct);

  // Help menu
  // ----------------------------------------------------------------------
  QMenu* helpMenu = new QMenu(tr("Help"));
#ifdef Q_OS_MAC
  helpMenu->addAction(macOpenWithHelpAct);
  helpMenu->addSeparator();
#endif
  helpMenu->addAction(aboutAct);
  helpMenu->addAction(aboutQtAct);

  // Place all menus on menu bar
  // ----------------------------------------------------------------------
#ifdef Q_OS_MAC
  QMenuBar *mb = menuBar();
  mb->addMenu(fileMenu);
  mb->addMenu(editMenu);
  mb->addMenu(viewMenu);
  mb->addMenu(helpMenu);
#else
  QMenuBar *menuBar = new QMenuBar;
  menuBar->addMenu(fileMenu);
  menuBar->addMenu(editMenu);
  menuBar->addMenu(viewMenu);
  menuBar->addMenu(helpMenu);
  menuToolBar->addWidget(menuBar);
#endif
}
//---------------------------------------------------------------------------

void MainWindow::createToolBars() {
#ifndef Q_OS_MAC
  menuToolBar = addToolBar(tr("Menu"));
  menuToolBar->setObjectName("Menu");
  addToolBarBreak();
#endif

  navToolBar = addToolBar(tr("Navigate"));
  navToolBar->setObjectName("Navigate");

#ifdef Q_OS_MAC
  navToolBar->addAction(settingsAct);
  settingsAct->setText(QString());
  settingsAct->setToolTip(tr("Settings..."));
#endif
  navToolBar->addAction(backAct);
  navToolBar->addAction(upAct);
  navToolBar->addAction(refreshAct);
  navToolBar->addAction(homeAct);
  navToolBar->addAction(iconAct);
  navToolBar->addAction(listViewAct);

  navToolBar->addWidget(pathEdit);
  navToolBar->addAction(newTabAct);
  navToolBar->addAction(terminalAct);

  addressToolBar = navToolBar;
#ifdef Q_OS_MAC
  applyMacNavToolBarLayout();
#endif
}
//---------------------------------------------------------------------------

void MainWindow::zoomInAction()
{
    int zoomLevel;

    if(focusWidget() == tree) {
        (zoomTree == 64) ? zoomTree=64 : zoomTree+= 8;
        tree->setIconSize(QSize(zoomTree,zoomTree));
        zoomLevel = zoomTree;
    } else if (focusWidget() == bookmarksList) {
        (zoomBook == 64) ? zoomBook=64 : zoomBook+= 8;
        bookmarksList->setIconSize(QSize(zoomBook,zoomBook));
        zoomLevel = zoomBook;
    } else {
        if (currentView == 1) {
            if (zoom == IconViewDelegate::iconZoomMax) {
                zoom = IconViewDelegate::iconZoomMax;
            } else {
                zoom += 8;
            }
            zoomLevel = zoom;
            applyIconView();
        } else {
            if (zoomDetail == 128) {
                zoomDetail = 128;
            } else {
                zoomDetail += 8;
            }
            zoomLevel = zoomDetail;
            applyListRowHeight();
        }
    }

    status->showMessage(QString(tr("Zoom: %1")).arg(zoomLevel));
    updateGrid();
}

//---------------------------------------------------------------------------
void MainWindow::zoomOutAction()
{
    int zoomLevel;

    if(focusWidget() == tree) {
        (zoomTree == 16) ? zoomTree=16 : zoomTree-= 8;
        tree->setIconSize(QSize(zoomTree,zoomTree));
        zoomLevel = zoomTree;
    } else if(focusWidget() == bookmarksList) {
        (zoomBook == 16) ? zoomBook=16 : zoomBook-= 8;
        bookmarksList->setIconSize(QSize(zoomBook,zoomBook));
        zoomLevel = zoomBook;
    } else {
        if (currentView == 1) {
            if (zoom == IconViewDelegate::iconZoomMin) {
                zoom = IconViewDelegate::iconZoomMin;
            } else {
                zoom -= 8;
            }
            zoomLevel = zoom;
            applyIconView();
        } else {
            if (zoomDetail == 18) {
                zoomDetail = 18;
            } else {
                zoomDetail -= 8;
            }
            zoomLevel = zoomDetail;
            applyListRowHeight();
        }
    }

    status->showMessage(QString(tr("Zoom: %1")).arg(zoomLevel));
    updateGrid();
}

//---------------------------------------------------------------------------
void MainWindow::focusAction()
{
    QAction *which = qobject_cast<QAction*>(sender());
    if(which)
    {
        if(which->text().contains("address")) pathEdit->setFocus(Qt::TabFocusReason);
        else if(which->text().contains("tree")) tree->setFocus(Qt::TabFocusReason);
        else if(which->text().contains("bookmarks")) bookmarksList->setFocus(Qt::TabFocusReason);
        else if(currentView == 2) detailTree->setFocus(Qt::TabFocusReason);
        else list->setFocus(Qt::TabFocusReason);
    }
    else
    {
        QApplication::clipboard()->blockSignals(0);
        pathEdit->setCompleter(customComplete);
    }
}
//---------------------------------------------------------------------------

void MainWindow::addressChanged(int old, int now)
{
    Q_UNUSED(old)

    if(!pathEdit->hasFocus()) return;
    QString temp = pathEdit->currentText();

    if(temp.contains("/."))
        if(!hiddenAct->isChecked())
        {
            hiddenAct->setChecked(1);
            toggleHidden();
        }

    if(temp.right(1) == "/")
    {
        modelList->index(temp);     //make sure model has walked this folder
        modelTree->invalidate();
    }

    if(temp.length() == now) return;
    int pos = temp.indexOf("/",now);

    pathEdit->lineEdit()->blockSignals(1);

    if(QApplication::keyboardModifiers() == Qt::ControlModifier)
    {
        tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(temp.left(pos))));
    }
    else
    if(QApplication::mouseButtons() == Qt::MiddleButton)
    {
        QApplication::clipboard()->blockSignals(1);
        QApplication::clipboard()->clear(QClipboard::Selection);        //don't paste stuff

        pathEdit->setCompleter(nullptr);
        tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(temp.left(pos))));

        QTimer::singleShot(500,this,SLOT(focusAction()));
    }
    else
    if(!pathEdit->lineEdit()->hasSelectedText())
    {
        pathEdit->completer()->setCompletionPrefix(temp.left(pos) + "/");
        pathEdit->completer()->complete();
    }

    pathEdit->lineEdit()->blockSignals(0);
}
//---------------------------------------------------------------------------
