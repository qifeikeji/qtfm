#include "mainwindow.h"
#include "settingsdialog.h"
#include "openwithconfig.h"
#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QDockWidget>
#include <QStatusBar>
#include <QHeaderView>
#include <QToolBar>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#include <fcntl.h>

/**
 * @brief Executes a file
 * @param index
 * @param run
 */
void MainWindow::executeFile(QModelIndex index, bool run) {

  // Index of file
  QModelIndex srcIndex = modelView->mapToSource(index);

   QString filePath = modelList->filePath(srcIndex);
   QString type = mimeUtils->getMimeType(filePath);
   if (type.endsWith("executable") || type.endsWith("appimage") || filePath.endsWith(".desktop")) { run = true; }

  // Run or open
  if (run) {
#ifdef Q_OS_MAC
    QProcess::startDetached(QString("open \"%1\"").arg(filePath));
#else
    if (filePath.endsWith(".desktop")) {
        DesktopFile df(filePath);
        if (!df.getExec().isEmpty()) {
            filePath = df.getExec();
            if (filePath.toLower().contains("%f")) {
              filePath.replace("%f", "", Qt::CaseInsensitive);
            } else if (filePath.toLower().contains("%u")) {
              filePath.replace("%u", "", Qt::CaseInsensitive);
            }
            filePath = filePath.trimmed();
        } else { return; }
    }
    if (filePath.contains(" ")) {
        filePath.prepend("\"");
        filePath.append("\"");
    }
    qDebug() << "RUN" << filePath;
    QProcess::startDetached(filePath, QStringList());
#endif
  } else {
    const QString customCmd = OpenWithConfig::defaultCommandFor(QFileInfo(filePath));
    if (!customCmd.isEmpty()) {
      mimeUtils->openInApp(customCmd, QFileInfo(filePath), QString());
      return;
    }
    mimeUtils->openInApp(QFileInfo(filePath), QString());
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Runs a file
 */
void MainWindow::runFile() {
    qDebug() << "runFile";
  executeFile(listSelectionModel->currentIndex(), 1);
}
//---------------------------------------------------------------------------

/**
 * @brief Opens folder
 */
void MainWindow::openFolderAction() {
  QModelIndex i = listSelectionModel->currentIndex();
  tree->setCurrentIndex(modelTree->mapFromSource(i));
}
//---------------------------------------------------------------------------

/**
 * @brief Opens file or files
 */
void MainWindow::openFile()
{
    qDebug() << "openFile(s)";

    // get selection
    QModelIndexList items;
    if (listSelectionModel->selectedRows(0).count()) {
        items = listSelectionModel->selectedRows(0);
    } else {
        items = listSelectionModel->selectedIndexes();
    }

    // get files and mimes
    QMap<QString,QString> files;
    QMap<QString,QString> mimes;
    foreach (QModelIndex index, items) {
        QModelIndex srcIndex = modelView->mapToSource(index);
        QString filePath = modelList->filePath(srcIndex);
        QFileInfo fileInfo(filePath);
        if (fileInfo.isDir()) { continue; }
        const QString customCmd = OpenWithConfig::defaultCommandFor(fileInfo);
        if (!customCmd.isEmpty()) {
            mimeUtils->openInApp(customCmd, fileInfo, QString());
            continue;
        }
        QString mime = mimeUtils->getMimeType(filePath);
        if (mime.isEmpty()) { continue; }
        files[filePath] = mime;
        mimes[mime] = "";
    }
    qDebug() << "selected files" << items.size() << files << mimes;

    // get apps for mimes
    QMapIterator<QString, QString> i_mimes(mimes);
    while (i_mimes.hasNext()) {
        i_mimes.next();
        QString app = mimeUtils->getAppForMimeType(i_mimes.key());
        if (app.isEmpty()) { continue; }
        mimes[i_mimes.key()] = app;
    }
    qDebug() << "selected files apps" << mimes;

    // match apps and files
    QMap<QString,QStringList> launch;
    QMapIterator<QString, QString> i_apps(mimes);
    while (i_apps.hasNext()) {
        i_apps.next();
        QString app = i_apps.value();
        QString mime = i_apps.key();
        if (app.isEmpty()) { continue; }
        QMapIterator<QString, QString> i_files(files);
        while (i_files.hasNext()) {
            i_files.next();
            if (mime == i_files.value()) { launch[app] << i_files.key(); }
        }
    }
    qDebug() << "launch" << launch;

    // launch
    QMapIterator<QString, QStringList> i_launch(launch);
    while (i_launch.hasNext()) {
        i_launch.next();
        QString desktop = Common::findApplication(qApp->applicationFilePath(), i_launch.key());
        if (desktop.isEmpty()) { continue; }
        DesktopFile df = DesktopFile(desktop);
        if (df.getExec().isEmpty()) { continue; }
        QStringList fileList = i_launch.value();
        if (df.getExec().contains("%F") || df.getExec().contains("%U")) { // app supports multiple files
            mimeUtils->openFilesInApp(df.getExec(), fileList, df.isTerminal()?term:"");
        } else { // launch new instance for each file
            for (int i=0;i<i_launch.value().size();++i) {
                QFileInfo fileInfo(fileList.at(i));
                mimeUtils->openInApp(df.getExec(), fileInfo, df.isTerminal()?term:"");
            }
        }
    }
}

//---------------------------------------------------------------------------

/**
 * @brief Goes up in directory tree
 */
void MainWindow::goUpDir() {
  tree->setCurrentIndex(tree->currentIndex().parent());
}
//---------------------------------------------------------------------------

/**
 * @brief Goes back in directory tree
 */
void MainWindow::goBackDir() {

  // If there is only one item in path edit, we cannot go back
  if (pathEdit->count() == 1) return;

  // Retrieve current index
  QString current = pathEdit->currentText();
  if (current.contains(pathEdit->itemText(1))) {
    backIndex = modelList->index(current);
  }

  // Remove history
  do {
    pathEdit->removeItem(0);
    if (tabs->count()) tabs->remHistory();
  } while (!QFileInfo(pathEdit->itemText(0)).exists()
           || pathEdit->itemText(0) == current);

  // Sets new dir index
  QModelIndex i = modelList->index(pathEdit->itemText(0));
  tree->setCurrentIndex(modelTree->mapFromSource(i));
}
//---------------------------------------------------------------------------

/**
 * @brief Goes to home directory
 */
void MainWindow::goHomeDir() {
  QModelIndex i = modelTree->mapFromSource(modelList->index(QDir::homePath()));
  tree->setCurrentIndex(i);
}
//---------------------------------------------------------------------------

/**
 * @brief Starts terminal
 */
void MainWindow::terminalRun() {

  // If terminal was not specified, asks user for terminal command
  if (term.isEmpty()) {
    QString title = tr("Setting");
    QString label = tr("Set default terminal:");
    QString def = "xterm";
#ifdef Q_OS_MAC
    def = "/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal";
#endif
    term = QInputDialog::getText(this, title, label, QLineEdit::Normal, def);
    settings->setValue("term", term);
  }

  // Starts terminal
  QStringList args(term.split(" "));
  QString name = args.at(0);
  args.removeAt(0);
  QProcess::startDetached(name, args, pathEdit->itemText(0));
}
//---------------------------------------------------------------------------

/**
 * @brief Creates a new directory
 */
void MainWindow::newDir() {

  // Check whether current directory is writeable
  QModelIndex newDir;
  if (!QFileInfo(pathEdit->itemText(0)).isWritable()) {
    status->showMessage(tr("The current directory is not writable, unable to create new folder."));
    return;
  }

  // Create new directory
  QModelIndex i = modelList->index(pathEdit->itemText(0));
  newDir = modelView->mapFromSource(modelList->insertFolder(i));
  listSelectionModel->setCurrentIndex(newDir,
                                      QItemSelectionModel::ClearAndSelect);

  // Editation of name of new directory
  if (stackWidget->currentIndex() == 0) list->edit(newDir);
  else detailTree->edit(newDir);
}
//---------------------------------------------------------------------------

/**
 * @brief Creates a new file
 */
void MainWindow::newFile() {

  // Check whether current directory is writeable
  QModelIndex fileIndex;
  if (!QFileInfo(pathEdit->itemText(0)).isWritable()) {
    status->showMessage(tr("The current directory is not writable, unable to create new file."));
    return;
  }

  // Create new file
  QModelIndex i = modelList->index(pathEdit->itemText(0));
  fileIndex = modelView->mapFromSource(modelList->insertFile(i));
  listSelectionModel->setCurrentIndex(fileIndex,
                                      QItemSelectionModel::ClearAndSelect);

   // Editation of name of new file
  if (stackWidget->currentIndex() == 0) list->edit(fileIndex);
  else detailTree->edit(fileIndex);
}

void MainWindow::newMdFile()
{
  if (!QFileInfo(pathEdit->itemText(0)).isWritable()) {
    status->showMessage(tr("The current directory is not writable, unable to create new file."));
    return;
  }
  QModelIndex i = modelList->index(pathEdit->itemText(0));
  const QModelIndex fileIndex =
      modelView->mapFromSource(modelList->insertFileWithSuffix(i, QStringLiteral("md")));
  if (!fileIndex.isValid()) {
    return;
  }
  listSelectionModel->setCurrentIndex(fileIndex, QItemSelectionModel::ClearAndSelect);
  if (stackWidget->currentIndex() == 0) {
    list->edit(fileIndex);
  } else {
    detailTree->edit(fileIndex);
  }
}

void MainWindow::newTxtFile()
{
  if (!QFileInfo(pathEdit->itemText(0)).isWritable()) {
    status->showMessage(tr("The current directory is not writable, unable to create new file."));
    return;
  }
  QModelIndex i = modelList->index(pathEdit->itemText(0));
  const QModelIndex fileIndex =
      modelView->mapFromSource(modelList->insertFileWithSuffix(i, QStringLiteral("txt")));
  if (!fileIndex.isValid()) {
    return;
  }
  listSelectionModel->setCurrentIndex(fileIndex, QItemSelectionModel::ClearAndSelect);
  if (stackWidget->currentIndex() == 0) {
    list->edit(fileIndex);
  } else {
    detailTree->edit(fileIndex);
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Deletes file
 */
void MainWindow::deleteFile() {

  // Temporary selection info
  QModelIndexList selList;
  bool yesToAll = false;

  // Retrieves selection
  if (focusWidget() == tree) {
    selList << modelList->index(pathEdit->itemText(0));
  } else {
    QModelIndexList proxyList;
    if (listSelectionModel->selectedRows(0).count()) {
      proxyList = listSelectionModel->selectedRows(0);
    } else {
      proxyList = listSelectionModel->selectedIndexes();
    }
    foreach (QModelIndex proxyItem, proxyList) {
      selList.append(modelView->mapToSource(proxyItem));
    }
  }

  bool ok = false;
  bool confirm;

  // Display confirmation message box
  if (settings->value("confirmDelete").isNull()) {
    QString title = tr("Delete confirmation");
    QString msg = tr("Confirm all delete operations?");
    QMessageBox::StandardButtons btns = QMessageBox::Yes | QMessageBox::No;
    if (QMessageBox::question(this, title, msg, btns) == QMessageBox::Yes) {
      confirm = 1;
    } else {
      confirm = 0;
    }
    settings->setValue("confirmDelete",confirm);
  } else {
    confirm = settings->value("confirmDelete").toBool();
  }

  // Delete selected file(s)
  for (int i = 0; i < selList.count(); ++i) {
    QFileInfo file(modelList->filePath(selList.at(i)));
    if (file.isWritable() || file.isSymLink()) {
        if (yesToAll == false) {
          if (confirm) {
            QString title = tr("Careful");
            QString msg = tr("Are you sure you want to delete <p><b>\"") +
                          file.filePath() + "</b>?";
            int ret = QMessageBox::information(this, title, msg,
                QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll);
            if (ret == QMessageBox::YesToAll) yesToAll = true;
            if (ret == QMessageBox::No) return;
          }
        }
        if (file.isSymLink()) {
            ok = QFile::remove(file.filePath());
        } else {
            ok = modelList->remove(selList.at(i));
        }
    }
  }

  // Display error message if deletion failed
  if(!ok) {
    QString title = tr("Failed");
    QString msg = tr("Some files where not deleted. You may not have the proper permissions or maybe the file system is read-only.");
    QMessageBox::warning(this, title, msg);
  }

  return;
}

void MainWindow::trashFile()
{
    // Temporary selection files
    QModelIndexList selList;
    QStringList fileList;

    // Selection
    if (focusWidget() == tree) {
      selList << modelView->mapFromSource(modelList->index(pathEdit->itemText(0)));
    } else if (listSelectionModel->selectedRows(0).count()) {
      selList = listSelectionModel->selectedRows(0);
    } else {
      selList = listSelectionModel->selectedIndexes();
    }

    // Retrieve selected indices
    foreach (QModelIndex item, selList) {
      fileList.append(modelList->filePath(modelView->mapToSource(item)));
    }

    bool ok = true;
    for (int i=0;i<fileList.size();++i) {
        QFileInfo file(fileList.at(i));
        if (!file.isWritable()) {
            ok = false;
            continue;
        }
        QString trashPath = QString("%1/%2").arg(trashDir).arg(file.absoluteFilePath().split("/").takeLast());
        while (QFile::exists(trashPath)) {
            trashPath = QString("%1/%2.%3").arg(trashDir).arg(file.absoluteFilePath().split("/").takeLast()).arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmssz"));
        }
        if (file.isDir()) {
            QDir origDir(file.absoluteFilePath());
            bool movedDir = origDir.rename(file.absoluteFilePath(), trashPath);
            if (!movedDir) { ok = false; }
        } else if (file.isFile()) {
            QFile origFile(file.absoluteFilePath());
            bool movedFile = origFile.rename(file.absoluteFilePath(), trashPath);
            if (!movedFile) { ok = false; }
        }
    }
    if(!ok) {
      QString title = tr("Failed");
      QString msg = tr("Some files where not moved. You may not have the proper permissions or maybe the file system is read-only.");
      QMessageBox::warning(this, title, msg);
    }
}
//---------------------------------------------------------------------------

/**
 * @brief Cuts file
 */
void MainWindow::cutFile() {

  // Temporary selection files
  QModelIndexList selList;
  QStringList fileList;

  // Selection
  if (focusWidget() == tree) {
    selList << modelView->mapFromSource(modelList->index(pathEdit->itemText(0)));
  } else if (listSelectionModel->selectedRows(0).count()) {
    selList = listSelectionModel->selectedRows(0);
  } else {
    selList = listSelectionModel->selectedIndexes();
  }

  // Retrieve selected indices
  foreach (QModelIndex item, selList) {
    fileList.append(modelList->filePath(modelView->mapToSource(item)));
  }

  clearCutItems();
  modelList->addCutItems(fileList);

  // Save a temp file to allow pasting in a different instance
  const QString clipboardFile = Common::getTempClipboardFile();
  if (!clipboardFile.isEmpty()) {
      QFile tempFile(clipboardFile);
      tempFile.open(QIODevice::WriteOnly);
      QDataStream out(&tempFile);
      out << fileList;
      tempFile.close();
  }

  QApplication::clipboard()->setMimeData(modelView->mimeData(selList));

  modelTree->invalidate();
  listSelectionModel->clear();
}
//---------------------------------------------------------------------------

/**
 * @brief Copies a file
 */
void MainWindow::copyFile() {

  // Selection
  QModelIndexList selList;
  if (listSelectionModel->selectedRows(0).count()) {
    selList = listSelectionModel->selectedRows(0);
  } else {
    selList = listSelectionModel->selectedIndexes();
  }

  if (selList.count() == 0) {
    if (focusWidget() == tree) {
      QModelIndex i = modelList->index(pathEdit->itemText(0));
      selList << modelView->mapFromSource(i);
    } else {
      return;
    }
  }

  clearCutItems();

  QStringList text;
  foreach (QModelIndex item,selList) {
    text.append(modelList->filePath(modelView->mapToSource(item)));
  }

  QApplication::clipboard()->setText(text.join("\n"), QClipboard::Selection);
  QApplication::clipboard()->setMimeData(modelView->mimeData(selList));

  cutAct->setData(0);
}
//---------------------------------------------------------------------------

/**
 * @brief Renames file
 */
void MainWindow::renameFile() {
  if (focusWidget() == tree) {
    tree->edit(treeSelectionModel->currentIndex());
  } else if(focusWidget() == list) {
    list->edit(listSelectionModel->currentIndex());
  } else if(focusWidget() == detailTree) {
    detailTree->edit(listSelectionModel->currentIndex());
  }
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------


//---------------------------------------------------------------------------


//---------------------------------------------------------------------------

/**
 * @brief Creates symbolic links to files
 * @param files
 * @param newPath
 * @return true if link creation was successful
 */
bool MainWindow::linkFiles(const QList<QUrl> &files, const QString &newPath) {

  // Quit if folder not writable
  if (!QFileInfo(newPath).isWritable()
      || newPath == QDir(files.at(0).toLocalFile()).path())
  {
      QMessageBox::warning(this, tr("Folder not writable"), tr("The destination folder (%1) is not writable").arg(newPath));
      return false;
  }

  // TODO: even if symlinks are small we have to make sure that we have space
  // available for links

  // Main loop
  for (int i = 0; i < files.count(); ++i) {

    // Choose destination file name and url
    QFile file(files.at(i).toLocalFile());
    QFileInfo temp(file);
    QString destName = temp.fileName();
    QString destUrl = newPath + QDir::separator() + destName;

    // Only do 'Link(x) of' if same folder
    if (temp.path() == newPath) {
      int num = 1;
      while (QFile(destUrl).exists()) {
        destName = QString("Link (%1) of %2").arg(num).arg(temp.fileName());
        destUrl = newPath + QDir::separator() + destName;
        num++;
      }
    }

    // If file does not exists then create link
    QFileInfo dName(destUrl);
    if (!dName.exists()) {
      file.link(destUrl);
    }
  }
  return true;
}
//---------------------------------------------------------------------------

/**
 * @brief Locks/Unlocks editation of layout
 */
void MainWindow::toggleLockLayout() {

  if (lockLayoutAct->isChecked()) {
    QFrame *newTitle = new QFrame();
    newTitle->setFrameShape(QFrame::StyledPanel);
    newTitle->setMinimumSize(0,1);
    dockTree->setTitleBarWidget(newTitle);

    newTitle = new QFrame();
    newTitle->setFrameShape(QFrame::StyledPanel);
    newTitle->setMinimumSize(0,1);
    dockBookmarks->setTitleBarWidget(newTitle);

    newTitle = new QFrame();
    newTitle->setFrameShape(QFrame::StyledPanel);
    newTitle->setMinimumSize(0,1);

    menuToolBar->setMovable(0);
    //editToolBar->setMovable(0);
    //viewToolBar->setMovable(0);
    navToolBar->setMovable(0);
    addressToolBar->setMovable(0);
    lockLayoutAct->setText(tr("Unlock layout"));
  } else {
    dockTree->setTitleBarWidget(nullptr);
    dockBookmarks->setTitleBarWidget(nullptr);

    menuToolBar->setMovable(1);
    //editToolBar->setMovable(1);
    //viewToolBar->setMovable(1);
    navToolBar->setMovable(1);
    addressToolBar->setMovable(1);

    lockLayoutAct->setText(tr("Lock layout"));
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Switches to icon view
 */
void MainWindow::applyIconView() {

  if (list->rootIndex() != modelList->index(pathEdit->currentText())) {
    QModelIndex i = modelList->index(pathEdit->currentText());
    list->setRootIndex(modelView->mapFromSource(i));
  }

  iconAct->setChecked(true);
  listViewAct->setChecked(false);
  currentView = 1;
  list->setViewMode(QListView::IconMode);
  list->setItemDelegate(ivdelegate);
  list->setGridSize(QSize(zoom, zoom));
  list->setIconSize(QSize(zoom, zoom));
  list->setFlow(QListView::LeftToRight);

  modelList->setShowListDecorations(true);
  modelList->setMode(thumbsAct->isChecked());
  stackWidget->setCurrentIndex(0);
  detailTree->setMouseTracking(false);
  list->setMouseTracking(true);

  if (tabs->count()) { tabs->setType(1); }
  updateGrid();
  modelView->invalidate();
  list->viewport()->update();

  list->setDragDropMode(QAbstractItemView::DragDrop);
  list->setDefaultDropAction(Qt::MoveAction);
  settings->setValue(QStringLiteral("fileViewMode"), QStringLiteral("icon"));
}
//---------------------------------------------------------------------------

/**
 * @brief Switches to column list view
 */
void MainWindow::applyListView() {

  iconAct->setChecked(false);
  listViewAct->setChecked(true);
  currentView = 2;
  modelList->setShowListDecorations(false);

  QModelIndex i = modelList->index(pathEdit->currentText());
  const QModelIndex proxyRoot = modelView->mapFromSource(i);
  if (detailTree->rootIndex() != proxyRoot) {
    detailTree->setRootIndex(proxyRoot);
  }

  modelList->setMode(thumbsAct->isChecked());
  detailTree->setMouseTracking(true);
  list->setMouseTracking(false);
  stackWidget->setCurrentIndex(1);

  if (tabs->count()) { tabs->setType(2); }

  applyListRowHeight();
  modelView->sort(currentSortColumn, currentSortOrder);
  settings->setValue(QStringLiteral("fileViewMode"), QStringLiteral("list"));
}
//---------------------------------------------------------------------------

void MainWindow::setupFileListHeader()
{
    QHeaderView *header = detailTree->header();
    header->setStretchLastSection(true);
    header->setSectionsClickable(true);
    header->setSectionsMovable(false);
    header->setHighlightSections(true);
    header->setSectionResizeMode(COLUMN_ICON, QHeaderView::Fixed);
    for (int col = COLUMN_NAME; col < COLUMN_FOLDER; ++col) {
        header->setSectionResizeMode(col, QHeaderView::Interactive);
    }
    header->setSectionResizeMode(COLUMN_FOLDER, QHeaderView::Stretch);
    connect(header, SIGNAL(sectionClicked(int)), this, SLOT(listHeaderClicked(int)));
}

void MainWindow::listHeaderClicked(int logicalIndex)
{
    if (logicalIndex == COLUMN_ICON) {
        return;
    }
    if (logicalIndex == COLUMN_FOLDER) {
        modelView->toggleDirectorySortOverride();
        modelView->sort(currentSortColumn, currentSortOrder);
        return;
    }

    if (logicalIndex == currentSortColumn) {
        toggleSortOrder();
    } else {
        currentSortColumn = logicalIndex;
        switch (logicalIndex) {
        case COLUMN_NAME: sortNameAct->setChecked(true); break;
        case COLUMN_SIZE: sortSizeAct->setChecked(true); break;
        case COLUMN_DATE: sortDateAct->setChecked(true); break;
        default: break;
        }
        settings->setValue(QStringLiteral("sortBy"), currentSortColumn);
    }
    modelView->sort(currentSortColumn, currentSortOrder);
}

void MainWindow::applyListRowHeight()
{
    const int h = qMax(18, zoomDetail);
    const int iconSz = qMax(16, h - 4);
    detailTree->setIconSize(QSize(iconSz, iconSz));
    detailTree->setIndentation(0);
    detailTree->setStyleSheet(QStringLiteral("QTreeView::item { height: %1px; }").arg(h));
    QHeaderView *header = detailTree->header();
    const int iconCol = settings->value(QStringLiteral("listColumnWidth0"), 0).toInt();
    header->resizeSection(COLUMN_ICON, iconCol > 0 ? iconCol : iconSz + 8);
}

void MainWindow::applyListColumnWidths()
{
    QHeaderView *header = detailTree->header();
    const int defaults[] = {0, 220, 90, 130, 120, 80};
    for (int col = 0; col < LIST_COLUMN_COUNT; ++col) {
        const QString key = QStringLiteral("listColumnWidth%1").arg(col);
        const int w = settings->value(key, defaults[col]).toInt();
        if (col == COLUMN_ICON) {
            if (w > 0) {
                header->resizeSection(col, w);
            }
            continue;
        }
        if (col < COLUMN_FOLDER) {
            header->resizeSection(col, w);
        }
    }
}

/**
 * @brief Sets sort column
 * @param columnAct
 */
void MainWindow::setSortColumn(QAction *columnAct) {

  // Set root index
  if (list->rootIndex() != modelList->index(pathEdit->currentText())) {
    QModelIndex i = modelList->index(pathEdit->currentText());
    list->setRootIndex(modelView->mapFromSource(i));
  }

  columnAct->setChecked(true);

  if (columnAct == sortNameAct) {
    currentSortColumn =  COLUMN_NAME;
  } else if (columnAct == sortDateAct) {
    currentSortColumn =  COLUMN_DATE;
  } else if (columnAct == sortSizeAct) {
    currentSortColumn = COLUMN_SIZE;
  }
  settings->setValue("sortBy", currentSortColumn);
}
//---------------------------------------------------------------------------

/**
 * @brief Sets sort column
 * @param action
 */
void MainWindow::toggleSortBy(QAction *action) {
  setSortColumn(action);
  modelView->sort(currentSortColumn, currentSortOrder);
}
//---------------------------------------------------------------------------

/**
 * @brief Sets sort order
 * @param order
 */
void MainWindow::setSortOrder(Qt::SortOrder order) {

  // Set root index
  if (list->rootIndex() != modelList->index(pathEdit->currentText())) {
    QModelIndex i = modelList->index(pathEdit->currentText());
    list->setRootIndex(modelView->mapFromSource(i));
  }

  // Change sort order
  currentSortOrder = order;
  sortAscAct->setChecked(!((bool) currentSortOrder));
  settings->setValue("sortOrder", currentSortOrder);
}
//---------------------------------------------------------------------------

/**
 * @brief Changes sort order
 */
void MainWindow::toggleSortOrder() {
  setSortOrder(currentSortOrder == Qt::AscendingOrder ? Qt::DescendingOrder
                                                      : Qt::AscendingOrder);
  modelView->sort(currentSortColumn, currentSortOrder);
}
//---------------------------------------------------------------------------

/**
 * @brief Switches from thumbs to details and vice versa
 */
void MainWindow::toggleThumbs() {
  modelList->setMode(thumbsAct->isChecked());
}
//---------------------------------------------------------------------------

/**
 * @brief Hides/Shows hidden files
 */
void MainWindow::toggleHidden() {

  if (hiddenAct->isChecked() == false) {
    if (curIndex.isHidden()) {
      listSelectionModel->clear();
    }
    modelView->setFilterRegExp("no");
    modelTree->setFilterRegExp("no");
  } else {
    modelView->setFilterRegExp("");
    modelTree->setFilterRegExp("");
  }

  modelView->invalidate();
  dirLoaded();
}
//---------------------------------------------------------------------------

/**
 * @brief Displays about box
 */
void MainWindow::showAboutBox()
{
    QMessageBox box;
    box.setWindowTitle(tr("About %1").arg(APP_NAME));
    box.setWindowIcon(QIcon::fromTheme("qtfm", QIcon(":/icons/app.svg")));
    box.setIconPixmap(QPixmap(":/icons/app.svg").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    box.setText(QString("<h1>%1 %2</h1>"
                        "<h3 style=\"font-weight:normal;\">Qt File Manager</h3>").arg(APP_NAME).arg(APP_VERSION));
    box.setInformativeText(QString("<p style=\"text-align:justify;font-size:small;\">"
                                   "This program is free software; you can redistribute it and/or modify"
                                   " it under the terms of the GNU General Public License as published by"
                                   " the Free Software Foundation; either version 2 of the License, or"
                                   " (at your option) any later version.</p>"
                                   "<p style=\"font-size:small;\">Copyright &copy;2010-2019 The QtFM Developers."
                                   "<br>All rights reserved.</p>"
                                   "<p style=\"font-weight:bold;\">"
                                   "<a href=\"https://qtfm.eu\">"
                                   "https://qtfm.eu</a></p>"));
    QString details;
    QFile authorsFile(":/AUTHORS");
    if (authorsFile.open(QIODevice::Text|QIODevice::ReadOnly)) {
        details.append(authorsFile.readAll());
        authorsFile.close();
    }
    if (!details.isEmpty()) { box.setDetailedText(details); }
    box.exec();
}
//---------------------------------------------------------------------------

#ifdef Q_OS_MAC
void MainWindow::showMacOpenWithHelp()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("macOS 打开方式设置"));
    box.setIcon(QMessageBox::Information);
    box.setText(tr("在 QtFM 中配置默认打开与「Open with」"));
    box.setInformativeText(
        tr("<p>打开 <b>设置 (Settings) → Open with</b>。这里的规则优先于系统默认关联。</p>"
           "<p><b>扩展名模块</b>：填写后缀，逗号分隔（如 <code>pdf</code>、"
           "<code>glb,gltf</code>），优先级最高。</p>"
           "<p><b>分类</b>（图片 / 视频 / 文本与代码 / 压缩包）：在大框内添加应用模块；"
           "<b>第一个模块</b> 用于双击打开文件。</p>"
           "<p><b>占位符</b>（QtFM 在启动前替换，macOS 与 Linux 相同）：</p>"
           "<ul><li><code>%f</code> 或 <code>%F</code> — 文件的完整路径</li></ul>"
           "<p><b>macOS 命令示例</b></p>"
           "<ul>"
           "<li><code>/Applications/Preview.app %f</code></li>"
           "<li><code>/System/Applications/TextEdit.app</code> "
           "（不写 %f 也会自动带上当前文件）</li>"
           "<li><code>open -a Preview %f</code></li>"
           "<li><code>open -a \"Visual Studio Code\" %f</code></li>"
           "</ul>"
           "<p>不要指望单独输入 <code>xxx.app</code> 当 shell 命令；"
           "QtFM 会对 <code>.app</code> 包自动使用 <code>open -a</code>。</p>"
           "<p><b>图标路径</b>：可选，用于 Open with 菜单（PNG/ICNS，"
           "例如 Preview 的 AppIcon.icns）。</p>"
           "<p>改完后在设置窗口点 <b>Save</b> 保存。</p>"));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}
#endif

//---------------------------------------------------------------------------
/**
 * @brief Displays settings dialog
 */
void MainWindow::showEditDialog() {

  // Deletes current list of custom actions
  customActManager->freeActions();

  // save settings
  writeSettings();

  // Creates settings dialog
  SettingsDialog *d = new SettingsDialog(actionList, settings, mimeUtils, this);
  if (d->exec()) {

    // Reload settings
    loadSettings(false /* don't reload window state/geo */,
                 false /* don't reload hidden state */,
                 false /* don't reload tabs state */,
                 false /* don't reload thumb state */);

    modelList->clearIconCache();
    modelList->refreshItems();
    OpenWithConfig::load(settings);
    customActManager->readActions();
  }

  // Reads custom actions
  customActManager->readActions();
  delete d;
}
//---------------------------------------------------------------------------
