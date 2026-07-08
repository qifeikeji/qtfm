#ifndef DFMQFILEITEMDELEGATE_H
#define DFMQFILEITEMDELEGATE_H

#include <QtWidgets>

#define COLUMN_ICON 0
#define COLUMN_NAME 1
#define COLUMN_SIZE 2
#define COLUMN_DATE 3
#define COLUMN_FORMAT 4
#define COLUMN_FOLDER 5
#define LIST_COLUMN_COUNT 6

/**
 * Extends QFileItemDelegate by a minimized selection way
 * for the name column of the details view.
 */
class DfmQStyledItemDelegate : public QStyledItemDelegate
{
public:
    explicit DfmQStyledItemDelegate(QObject* parent = 0);
    virtual ~DfmQStyledItemDelegate();

    /**
     * If minimized is true, the selection are
     * only drawn above the icon and text of an item.
     */
    void setMinimizedNameColumnSelection(bool minimized);
    bool hasMinimizedNameColumnSelection() const;

    virtual void paint(QPainter* painter,
                       const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;

    /**
     * Returns the minimized width of the name column for the name.
     */
    static int nameColumnWidth(const QString& name, const QStyleOptionViewItem& option);

private:
    bool m_hasMinimizedNameColumnSelection;
};

inline void DfmQStyledItemDelegate::setMinimizedNameColumnSelection(bool minimized)
{
    m_hasMinimizedNameColumnSelection = minimized;
}

inline bool DfmQStyledItemDelegate::hasMinimizedNameColumnSelection() const
{
    return m_hasMinimizedNameColumnSelection;
}

#endif // DFMQFILEITEMDELEGATE_H
