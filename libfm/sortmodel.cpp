#include "sortmodel.h"
#include "mymodel.h"

#include <QFileInfo>

static QString canonicalFilePathIfExists(const QString &path)
{
    const QFileInfo fi(path);
    if (fi.exists()) {
        return fi.canonicalFilePath();
    }
    return QDir::cleanPath(path);
}

void viewsSortProxyModel::setSingleFileFilter(const QString &absoluteFilePath)
{
    m_singleFileCanonical = canonicalFilePathIfExists(absoluteFilePath);
    invalidateFilter();
}

void viewsSortProxyModel::clearSingleFileFilter()
{
    if (m_singleFileCanonical.isEmpty()) {
        return;
    }
    m_singleFileCanonical.clear();
    invalidateFilter();
}

//---------------------------------------------------------------------------------
bool mainTreeFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (sourceModel() == nullptr) { return false; }
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    myModel* fileModel = qobject_cast<myModel*>(sourceModel());
    if (fileModel == nullptr) { return false; }
    if (fileModel->isDir(index0)) {
        if (this->filterRegExp().isEmpty() || fileModel->fileInfo(index0).isHidden() == 0) { return true; }
    }

    return false;
}

//---------------------------------------------------------------------------------
bool viewsSortProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (sourceModel() == nullptr) { return false; }

    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    myModel* fileModel = qobject_cast<myModel*>(sourceModel());
    if (fileModel == nullptr) { return false; }

    if (!m_singleFileCanonical.isEmpty()) {
        return canonicalFilePathIfExists(fileModel->filePath(index0))
               == m_singleFileCanonical;
    }

    if (this->filterRegExp().isEmpty()) { return true; }

    if (fileModel->fileInfo(index0).isHidden()) { return false; }
    return true;
}

//---------------------------------------------------------------------------------
bool viewsSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    myModel* fsModel = dynamic_cast<myModel*>(sourceModel());

    if ((fsModel->isDir(left) && !fsModel->isDir(right))) {
        return sortOrder() == Qt::AscendingOrder;
    } else if(!fsModel->isDir(left) && fsModel->isDir(right)) {
        return sortOrder() == Qt::DescendingOrder;
    }

    if(left.column() == 1) { // size
        if (fsModel->size(left) > fsModel->size(right)) { return true; }
        else { return false; }
    } else if (left.column() == 3) { // date
        if (fsModel->fileInfo(left).lastModified() > fsModel->fileInfo(right).lastModified()) { return true; }
        else { return false; }
    }

    return QSortFilterProxyModel::lessThan(left,right);
}
