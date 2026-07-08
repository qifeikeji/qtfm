/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#include "fm.h"
#include <QVBoxLayout>
#include <QApplication>

FM::FM(MimeUtils *mimeUtils,
       QString startPath,
       QWidget *parent) : QWidget(parent)
  , mimeUtilsPtr(mimeUtils)
  , modelList(nullptr)
  , list(nullptr)
  , modelView(nullptr)
  , modelViewDelegate(nullptr)
  , listSelectionModel(nullptr)
  , zoom(48)
  , history(nullptr)
  , customComplete(nullptr)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);

    modelList = new myModel(true, mimeUtilsPtr);
    connect(modelList, SIGNAL(reloadDir(QString)), this, SLOT(dirLoaded()));

    modelView = new viewsSortProxyModel();
    modelView->setSourceModel(modelList);
    modelView->setSortCaseSensitivity(Qt::CaseInsensitive);

    modelViewDelegate = new IconViewDelegate();

    list = new QListView(this);
    list->setWrapping(true);
    list->setWordWrap(true);
    list->setModel(modelView);
    list->setTextElideMode(Qt::ElideNone);
    list->setViewMode(QListView::IconMode);
    list->setItemDelegate(modelViewDelegate);
    list->setGridSize(QSize(zoom, zoom));
    list->setIconSize(QSize(zoom, zoom));
    list->setFlow(QListView::LeftToRight);
    list->setMouseTracking(true);
    list->setDragDropMode(QAbstractItemView::DragDrop);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setResizeMode(QListView::Adjust);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setSelectionRectVisible(true);
    list->setFocus();
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(list, SIGNAL(doubleClicked(QModelIndex))
            ,this, SLOT(listDoubleClicked(QModelIndex)));
    connect(list, SIGNAL(clicked(QModelIndex))
            ,this, SLOT(listClicked(QModelIndex)));

    listSelectionModel = list->selectionModel();

    layout->addWidget(list);

    history = new QStringList();

    customComplete = new myCompleter;
    customComplete->setModel(modelList);
    customComplete->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    customComplete->setMaxVisibleItems(10);

    setPath(startPath);
}

FM::~FM()
{
    delete history;
    qDebug() << "bye, bye!";
}

void FM::setPath(QString path)
{
    QFileInfo name(path);
    if (!name.exists() || path == getPath()) { return; }
    if (modelList->setRootPath(path)) { modelView->invalidate(); }

    QModelIndex baseIndex = modelView->mapFromSource(modelList->index(path));
    list->setRootIndex(baseIndex);

    if (path != QString("/")) { addHistory(path); }
    emit newPath(path);
    emit newWindowTitle(path==QString("/")?path:path.split("/", Qt::SkipEmptyParts).takeLast());
    updateGrid();

    //dirLoaded();
}

QString FM::getPath()
{
    return modelList->getRootPath();
}

QStringList *FM::getHistory()
{
    return history;
}

QCompleter *FM::getCompleter()
{
    return customComplete;
}


void FM::dirLoaded()
{
    // TODO
    qDebug() << "dirLoaded" << getPath();
    emit updatedDir(getPath());
}

void FM::updateGrid()
{
    if (list->viewMode() != QListView::IconMode) { return; }
    const QSize grid = IconViewDelegate::iconGridSize(zoom, 4, fontMetrics());
    list->setIconSize(QSize(zoom, zoom));
    if (list->gridSize() != grid) {
        list->setGridSize(grid);
    }
}

void FM::listDoubleClicked(QModelIndex current)
{
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if (mods == Qt::ControlModifier || mods == Qt::ShiftModifier) {
        return;
    }
    QModelIndex index = modelView->mapToSource(current);
    if (modelList->isDir(index)) {
        qDebug() << "move further down the rabbit hole...";
        setPath(modelList->filePath(index));
    } else {
        qDebug() << "is file";
        emit openFile(modelList->filePath(index));
    }
}

void FM::listClicked(QModelIndex current)
{
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if (mods == Qt::ControlModifier || mods == Qt::ShiftModifier) {
        return;
    }
    QModelIndex index = modelView->mapToSource(current);
    qDebug() << "preview";
    emit previewFile(modelList->filePath(index));
}

void FM::addHistory(QString path)
{
    if (path.isEmpty()) { return; }
    qDebug() << "add to history" << path;
    history->insert(0, path);
}

void FM::remHistory()
{
    qDebug() << "remove from history";
    history->removeFirst();
}
