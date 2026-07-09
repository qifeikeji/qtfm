
#include <QSortFilterProxyModel>
#include <QModelIndex>
#include <QDir>

//---------------------------------------------------------------------------------
// Subclass QSortFilterProxyModel and override 'filterAcceptsRow' to only show
// directories in tree and not files.
//---------------------------------------------------------------------------------
class mainTreeFilterProxyModel : public QSortFilterProxyModel
{
protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;
};


//---------------------------------------------------------------------------------
// Subclass QSortFilterProxyModel and override 'lessThan' for sorting in list/details views
//---------------------------------------------------------------------------------
class viewsSortProxyModel : public QSortFilterProxyModel
{
public:
    void setSingleFileFilter(const QString &absoluteFilePath);
    void clearSingleFileFilter();
    bool hasSingleFileFilter() const { return !m_singleFileCanonical.isEmpty(); }

    void setFoldersAlwaysFirstSetting(bool foldersFirst);
    bool foldersAlwaysFirstSetting() const { return m_foldersAlwaysFirstSetting; }
    void setFoldersAlwaysFirstIconSetting(bool foldersFirst);
    bool foldersAlwaysFirstIconSetting() const { return m_foldersAlwaysFirstIconSetting; }
    void setIconViewSortContext(bool iconView);
    void resetDirectorySortOverride();
    void toggleDirectorySortOverride();
    int directorySortOverride() const;
    bool directoriesFirst() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

private:
    QString m_singleFileCanonical;
    bool m_foldersAlwaysFirstSetting = true;
    bool m_foldersAlwaysFirstIconSetting = true;
    bool m_iconViewSortContext = false;
    /** -1 = use setting; 0 = files first; 1 = folders first */
    int m_directorySortOverride = -1;
};
