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

#include "bookmarkmodel.h"
#include "common.h"
#include "bundledicons.h"

#include <QApplication>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QModelIndex>

bookmarkmodel::bookmarkmodel(/*QHash<QString,
                             QIcon> *icons*/)
    : m_activeGroupId(QStringLiteral("default"))
{
    //folderIcons = icons;
}

void bookmarkmodel::setActiveGroupId(const QString &groupId)
{
    m_activeGroupId = groupId.isEmpty() ? QStringLiteral("default") : groupId;
}

QString bookmarkmodel::activeGroupId() const
{
    return m_activeGroupId;
}

void bookmarkmodel::addBookmark(QString name,
                                QString path,
                                QString isAuto,
                                QString icon,
                                QString mediaPath,
                                bool isMedia,
                                bool changed,
                                const QString &groupId)
{
    if (path.isEmpty() && !isMedia) { //add separator
        QStandardItem *item = new QStandardItem(BundledIcons::emptyIcon(), "");
        item->setData(QBrush(QColor(0, 0, 0, 40)), Qt::BackgroundRole);
        QFlags<Qt::ItemFlag> flags = item->flags();
        flags ^= Qt::ItemIsEditable; //not editable
        item->setFlags(flags);
        item->setFont(QFont("sans", 8)); //force size to prevent 2 rows of background tiling
        item->setData(m_activeGroupId, BOOKMARK_GROUP);
        this->appendRow(item);
        return;
    }

    QIcon theIcon;
    if (!icon.isEmpty()) {
        theIcon = BundledIcons::iconByName(icon);
    } else if (!path.isEmpty()) {
        theIcon = BundledIcons::iconForBookmarkPath(path);
    }
    if (theIcon.isNull()) {
        theIcon = BundledIcons::iconByName(QStringLiteral("folder"));
    }

    if (name.isEmpty()) name = QString("/");
    QStandardItem *item = new QStandardItem(theIcon, name);
    item->setData(path, BOOKMARK_PATH);
    item->setData(icon, BOOKMARK_ICON);
    item->setData(isAuto, BOOKMARKS_AUTO);
    item->setData(isMedia, MEDIA_MODEL);
    item->setToolTip(path);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    const QString gid = groupId.isEmpty() ? m_activeGroupId : groupId;
    item->setData(gid.isEmpty() ? QStringLiteral("default") : gid, BOOKMARK_GROUP);
    if (isMedia) { item->setData(mediaPath, MEDIA_PATH); }
    this->appendRow(item);
    if (changed) { emit bookmarksChanged(); }
}

bool bookmarkmodel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    const bool ok = QStandardItemModel::setData(index, value, role);
    if (ok && role == Qt::EditRole) {
        if (QStandardItem *item = itemFromIndex(index)) {
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        }
        emit bookmarksChanged();
    }
    return ok;
}

QStringList bookmarkmodel::mimeTypes() const
{
    return QStringList() << "application/x-qstandarditemmodeldatalist" << "text/uri-list";
}

bool bookmarkmodel::dropMimeData(const QMimeData * data,
                                 Qt::DropAction action,
                                 int row,
                                 int column,
                                 const QModelIndex &parent)
{
    // Reorder bookmarks within the list
    if (data->hasFormat("application/x-qstandarditemmodeldatalist")) {
        if (parent.column() == -1) {
            return QStandardItemModel::dropMimeData(data,
                                                    action,
                                                    row,
                                                    column,
                                                    parent);
        }
        return false;
    }

    if (!data->hasUrls()) {
        return false;
    }

    // External drags (e.g. from the file view): bookmark folders only — never move/copy
    // files into a bookmark path, including when dropped on a bookmark row.
    bool addedFolder = false;
    foreach (const QUrl &url, data->urls()) {
        const QFileInfo file(url.toLocalFile());
        if (!file.exists() || !file.isDir()) {
            continue;
        }
        addBookmark(file.fileName(), file.filePath(), QStringLiteral("0"), QString(),
                    QString(), false, true, m_activeGroupId);
        addedFolder = true;
    }
    return addedFolder;
}
