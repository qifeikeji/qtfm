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
    static QSize iconGridSize(int zoom, int cellGap, const QFontMetrics &fm);
    static QRect textLabelRect(const QRect &itemRect, int zoom, int cellGap,
                               const QFontMetrics &fm);

    void setCellGap(int gap);

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
    void setEditorData(QWidget *editor,
                       const QModelIndex &index) const override;
    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override;

private: // workaround for QTBUG
    mutable bool _isEditing;
    mutable QModelIndex _index;
    int _cellGap = 4;
protected: // workaround for QTBUG
    bool eventFilter(QObject * object,
                     QEvent * event) override;
};
