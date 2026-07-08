#ifndef PATHCOMBODELEGATE_H
#define PATHCOMBODELEGATE_H

#include <QStyledItemDelegate>

/** Flat path-history row: rounded square icon cell + elided path text. */
class PathComboItemDelegate : public QStyledItemDelegate
{
public:
    explicit PathComboItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

#endif
