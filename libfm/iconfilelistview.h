#ifndef ICONFILELISTVIEW_H
#define ICONFILELISTVIEW_H

#include <QListView>

class IconViewDelegate;

/**
 * Icon-mode file list: hit-testing matches icon+label chrome, not full grid cell.
 */
class IconFileListView : public QListView
{
    Q_OBJECT
public:
    explicit IconFileListView(QWidget *parent = nullptr);

    QModelIndex indexAt(const QPoint &point) const override;

    void setSortingEnabled(bool enable) { QListView::setSortingEnabled(enable); }

private:
    QRect contentRectForVisualRect(const QRect &cellRect) const;
};

#endif