/*
 * This file is part of QtFM <https://qtfm.eu>
 *
 * Copyright (C) 2013-2019 QtFM developers (see AUTHORS)
 * Copyright (C) 2010-2012 Wittfella <wittfella@qtfm.org>
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
 */

#include "mainwindow.h"
#include "common.h"
#include "icondlg.h"
#include "bundledicons.h"

#include <QStatusBar>
#include <QApplication>
#include <QPushButton>
#include <QMessageBox>
#include <QStandardItem>

void MainWindow::addBookmarkAction()
{
    modelBookmarks->addBookmark(curIndex.fileName(),
                                curIndex.filePath(),
                                "0",
                                "");
}

void MainWindow::addSeparatorAction()
{
    modelBookmarks->addBookmark("",
                                "",
                                "",
                                "");
}

void MainWindow::removeSeparator()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Remove separator"),
        tr("Remove this separator line?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QModelIndexList list = bookmarksList->selectionModel()->selectedIndexes();
    while (!list.isEmpty()) {
        const QModelIndex src = bookmarkListProxy->mapToSource(list.first());
        modelBookmarks->removeRow(src.row());
        list = bookmarksList->selectionModel()->selectedIndexes();
    }
    handleBookmarksChanged();
}

void MainWindow::delBookmark()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Remove bookmark"),
        tr("Remove the selected bookmark(s)?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QModelIndexList list = bookmarksList->selectionModel()->selectedIndexes();
    while (!list.isEmpty()) {
        const QModelIndex src = bookmarkListProxy->mapToSource(list.first());
        if (src.data(BOOKMARKS_AUTO).toString() == "1") { //automount, add to dontShowList
            QStringList temp = settings->value("hideBookmarks", 0).toStringList();
            temp.append(src.data(BOOKMARK_PATH).toString());
            settings->setValue("hideBookmarks", temp);
        }
        modelBookmarks->removeRow(src.row());
        list = bookmarksList->selectionModel()->selectedIndexes();
    }
    handleBookmarksChanged();
}

void MainWindow::renameBookmark()
{
    const QModelIndex idx = bookmarkListProxy->mapToSource(bookmarksList->currentIndex());
    if (!idx.isValid()) { return; }
    if (idx.data(BOOKMARK_PATH).toString().isEmpty()) { return; }

    QStandardItem *item = modelBookmarks->itemFromIndex(idx);
    if (!item) { return; }

    item->setFlags(item->flags() | Qt::ItemIsEditable);
    bookmarksList->setFocus(Qt::OtherFocusReason);
    bookmarksList->setCurrentIndex(bookmarkListProxy->mapFromSource(idx));
    bookmarksList->edit(bookmarkListProxy->mapFromSource(idx));
}

void MainWindow::editBookmark()
{
    icondlg * themeIcons = new icondlg;
    if (themeIcons->exec() == QDialog::Accepted) {
        QStandardItem * item = modelBookmarks->itemFromIndex(
            bookmarkListProxy->mapToSource(bookmarksList->currentIndex()));
        item->setData(themeIcons->result,
                      BOOKMARK_ICON);
        item->setIcon(BundledIcons::iconByName(themeIcons->result));
        handleBookmarksChanged();
    }
    delete themeIcons;
}

void MainWindow::toggleWrapBookmarks()
{
    bookmarksList->setWrapping(wrapBookmarksAct->isChecked());
    settings->setValue("wrapBookmarks",
                       wrapBookmarksAct->isChecked());
}

void MainWindow::bookmarkPressed(QModelIndex current)
{
    if (QApplication::mouseButtons() == Qt::MiddleButton) {
        tabs->setCurrentIndex(addTab(current.data(BOOKMARK_PATH).toString()));
    }
}

void MainWindow::bookmarkClicked(QModelIndex item)
{
    if (item.data(BOOKMARK_PATH).toString() == pathEdit->currentText()) { return; }

    QString info(item.data(BOOKMARK_PATH).toString());
    if (info.isEmpty()) { return; } //separator
    if (info.contains("/.")) { modelList->setRootPath(info); } //hidden folders

    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(item
                                                                    .data(BOOKMARK_PATH)
                                                                    .toString())));
    status->showMessage(Common::getDriveInfo(curIndex.filePath()));
}
