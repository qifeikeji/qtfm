/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#include <QObject>
#include <QItemDelegate>
#include <QStyledItemDelegate>
#include <QKeyEvent>
#include <QModelIndex>
#include <QPainter>

class IconViewDelegate : public QStyledItemDelegate
{
public:
    static constexpr int iconZoomMin = 16;
    static constexpr int iconZoomMax = 256;
    /** Icon-mode cell size: width ~ icon square + gap; height = icon + two text lines. */
    static QSize iconGridSize(int zoom, const QFontMetrics &fm);

private: // workaround for QTBUG
    mutable bool _isEditing;
    mutable QModelIndex _index;
protected: // workaround for QTBUG
    bool eventFilter(QObject * object,
                     QEvent * event);
public:
    void setEditorData(QWidget * editor,
                       const QModelIndex & index) const;
    void setModelData(QWidget * editor,
                      QAbstractItemModel * model,
                      const QModelIndex & index) const;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const;
};
