#include "bookmarkgroupproxy.h"
#include "common.h"

BookmarkGroupProxy::BookmarkGroupProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_groupId(QStringLiteral("default"))
{
    setDynamicSortFilter(true);
}

void BookmarkGroupProxy::setGroupId(const QString &groupId)
{
    const QString next = groupId.isEmpty() ? QStringLiteral("default") : groupId;
    if (m_groupId == next) {
        return;
    }
    m_groupId = next;
    invalidateFilter();
}

bool BookmarkGroupProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);
    const QModelIndex idx = sourceModel()->index(sourceRow, 0);
    if (!idx.isValid()) {
        return false;
    }
    const QString gid = idx.data(BOOKMARK_GROUP).toString();
    if (gid.isEmpty()) {
        return m_groupId == QLatin1String("default");
    }
    return gid == m_groupId;
}
