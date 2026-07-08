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

#include "icondlg.h"
#include "iconview.h"
#include "bundledicons.h"

#include <QScrollArea>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

icondlg::icondlg(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select icon"));
    resize(720, 520);

    iconModel = new QStandardItemModel(this);
    iconView = new QListView(this);
    iconView->setModel(iconModel);
    iconView->setSelectionMode(QAbstractItemView::SingleSelection);
    iconView->setWrapping(true);
    iconView->setFlow(QListView::LeftToRight);
    iconView->setResizeMode(QListView::Adjust);
    iconView->setViewMode(QListView::IconMode);
    iconView->setMovement(QListView::Static);
    iconView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    iconView->setTextElideMode(Qt::ElideNone);
    iconView->setWordWrap(true);

    iconDelegate = new IconViewDelegate(this);
    iconDelegate->setCellGap(kCellGap);
    iconView->setItemDelegate(iconDelegate);
    applyGridLayout();

    connect(iconView, SIGNAL(activated(QModelIndex)), this, SLOT(onIconActivated(QModelIndex)));
    connect(iconView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(onIconActivated(QModelIndex)));

    buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(iconView, 1);
    layout->addWidget(buttons);
    setLayout(layout);

    pendingNames = BundledIcons::availableIconBaseNames();
    thread.setFuture(QtConcurrent::run(this, &icondlg::scanTheme));
    connect(&thread, SIGNAL(finished()), this, SLOT(loadIcons()));
}

void icondlg::applyGridLayout()
{
    iconDelegate->setCellGap(kCellGap);
    const QSize grid = IconViewDelegate::iconGridSize(
        kPickerZoom, kCellGap, iconView->fontMetrics());
    iconView->setIconSize(QSize(kPickerZoom, kPickerZoom));
    iconView->setGridSize(grid);
}

void icondlg::scanTheme()
{
    // Names collected synchronously; hook kept for async batching.
}

void icondlg::loadIcons()
{
    int counter = 0;
    while (!pendingNames.isEmpty() && counter < 24) {
        const QString name = pendingNames.takeFirst();
        QStandardItem *item = new QStandardItem(BundledIcons::iconByName(name), name);
        item->setToolTip(name);
        iconModel->appendRow(item);
        counter++;
    }
    if (!pendingNames.isEmpty()) {
        QTimer::singleShot(10, this, SLOT(loadIcons()));
    }
}

void icondlg::onIconActivated(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    iconView->setCurrentIndex(index);
    accept();
}

void icondlg::accept()
{
    const QModelIndex index = iconView->currentIndex();
    if (!index.isValid()) {
        return;
    }
    result = index.data(Qt::DisplayRole).toString();
    done(QDialog::Accepted);
}
