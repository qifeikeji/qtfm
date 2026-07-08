#ifndef SIDEBARITEMDELEGATE_H
#define SIDEBARITEMDELEGATE_H

#include <QStyledItemDelegate>

class QLineEdit;

/**
 * @class BookmarkItemDelegate
 * @brief Renders a bookmark as two lines: name on top, path (grey, elided)
 *        below. Separator rows (empty display text) are drawn unchanged.
 */
class BookmarkItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit BookmarkItemDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
              const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                  const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void closeEditor(QWidget *editor, EndEditHint hint) override;

private:
    mutable bool m_editing = false;
    mutable QModelIndex m_editingIndex;
};

/**
 * @class DiskItemDelegate
 * @brief Renders a disk as two lines: name on top, usage progress bar below
 *        (used=highlight colour, free=grey). Unmounted disks show a plain
 *        placeholder instead of a bar.
 */
class DiskItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DiskItemDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
              const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                  const QModelIndex &index) const override;
};

#endif // SIDEBARITEMDELEGATE_H
