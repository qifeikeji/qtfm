#ifndef BOOKMARKGROUPPROXY_H
#define BOOKMARKGROUPPROXY_H

#include <QSortFilterProxyModel>

class BookmarkGroupProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit BookmarkGroupProxy(QObject *parent = nullptr);

    void setGroupId(const QString &groupId);
    QString groupId() const { return m_groupId; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_groupId;
};

#endif
