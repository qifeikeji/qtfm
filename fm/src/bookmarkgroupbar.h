#ifndef BOOKMARKGROUPBAR_H
#define BOOKMARKGROUPBAR_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QMap>

struct BookmarkGroupInfo {
    QString id;
    QString iconName;
};

class QButtonGroup;
class QVBoxLayout;
class QToolButton;

class BookmarkGroupBar : public QWidget
{
    Q_OBJECT
public:
    explicit BookmarkGroupBar(QWidget *parent = nullptr);

    void setGroups(const QVector<BookmarkGroupInfo> &groups, const QString &currentGroupId);
    /** Side length of group tab and “+” buttons (default 40 px). */
    void refreshToolbarIcons();
    void applyButtonSizes();
    void setTabButtonSize(int size);
    int tabButtonSize() const { return m_tabButtonSize; }
    QString currentGroupId() const { return m_currentGroupId; }

signals:
    void currentGroupChanged(const QString &groupId);
    void addGroupRequested();
    void groupIconChangeRequested(const QString &groupId);
    void groupDeleteRequested(const QString &groupId);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void rebuildButtons();
    void selectGroup(const QString &groupId);

    QVector<BookmarkGroupInfo> m_groups;
    QString m_currentGroupId;
    int m_tabButtonSize = 40;

    QVBoxLayout *m_layout = nullptr;
    QButtonGroup *m_buttonGroup = nullptr;
    QWidget *m_tabsHost = nullptr;
    QVBoxLayout *m_tabsLayout = nullptr;
    QToolButton *m_addButton = nullptr;
    QMap<QString, QToolButton *> m_tabButtons;
};

#endif
