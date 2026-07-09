#include "sortmodel.h"
#include "mymodel.h"
#include "dfmqstyleditemdelegate.h"

#include <QCollator>
#include <QDateTime>
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

void viewsSortProxyModel::setFoldersAlwaysFirstSetting(bool foldersFirst)
{
    m_foldersAlwaysFirstSetting = foldersFirst;
    invalidate();
}

void viewsSortProxyModel::setFoldersAlwaysFirstIconSetting(bool foldersFirst)
{
    m_foldersAlwaysFirstIconSetting = foldersFirst;
    invalidate();
}

void viewsSortProxyModel::setIconViewSortContext(bool iconView)
{
    if (m_iconViewSortContext == iconView) {
        return;
    }
    m_iconViewSortContext = iconView;
    invalidate();
}

void viewsSortProxyModel::resetDirectorySortOverride()
{
    m_directorySortOverride = -1;
    invalidate();
}

void viewsSortProxyModel::toggleDirectorySortOverride()
{
    const bool defaultDirsFirst = m_iconViewSortContext
                                      ? m_foldersAlwaysFirstIconSetting
                                      : m_foldersAlwaysFirstSetting;
    if (m_directorySortOverride < 0) {
        m_directorySortOverride = defaultDirsFirst ? 0 : 1;
    } else if (m_directorySortOverride == 1) {
        m_directorySortOverride = 0;
    } else {
        m_directorySortOverride = 1;
    }
    invalidate();
}

int viewsSortProxyModel::directorySortOverride() const
{
    return m_directorySortOverride;
}

bool viewsSortProxyModel::directoriesFirst() const
{
    if (m_directorySortOverride >= 0) {
        return m_directorySortOverride == 1;
    }
    return m_iconViewSortContext ? m_foldersAlwaysFirstIconSetting
                                 : m_foldersAlwaysFirstSetting;
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

static int compareDirEntries(bool leftIsDir, bool rightIsDir, bool dirsFirst)
{
    if (leftIsDir == rightIsDir) {
        return 0;
    }
    if (dirsFirst) {
        return leftIsDir ? -1 : 1;
    }
    return leftIsDir ? 1 : -1;
}

//---------------------------------------------------------------------------------
bool viewsSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    myModel *fsModel = dynamic_cast<myModel *>(sourceModel());
    if (fsModel == nullptr) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    const bool leftDir = fsModel->isDir(left);
    const bool rightDir = fsModel->isDir(right);
    const int dirCmp = compareDirEntries(leftDir, rightDir, directoriesFirst());
    if (dirCmp != 0) {
        return dirCmp < 0;
    }

    const int column = sortColumn();
    if (column == COLUMN_SIZE) {
        const qint64 ls = fsModel->size(left);
        const qint64 rs = fsModel->size(right);
        if (ls != rs) {
            return ls < rs;
        }
    } else if (column == COLUMN_DATE) {
        const QDateTime ld = fsModel->fileInfo(left).lastModified();
        const QDateTime rd = fsModel->fileInfo(right).lastModified();
        if (ld != rd) {
            return ld < rd;
        }
    } else if (column == COLUMN_FORMAT) {
        const QString lf = fsModel->getMimeType(left);
        const QString rf = fsModel->getMimeType(right);
        static QCollator collator;
        collator.setNumericMode(true);
        const int cmp = collator.compare(lf, rf);
        if (cmp != 0) {
            return cmp < 0;
        }
    } else if (column == COLUMN_FOLDER) {
        const int l = leftDir ? 1 : 0;
        const int r = rightDir ? 1 : 0;
        if (l != r) {
            return l < r;
        }
    }

    static QCollator nameCollator;
    nameCollator.setNumericMode(true);
    return nameCollator.compare(fsModel->fileName(left), fsModel->fileName(right)) < 0;
}
