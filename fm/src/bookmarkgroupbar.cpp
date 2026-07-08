#include "bookmarkgroupbar.h"
#include "bundledicons.h"

#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

BookmarkGroupBar::BookmarkGroupBar(QWidget *parent)
    : QWidget(parent)
    , m_buttonGroup(new QButtonGroup(this))
{
    m_buttonGroup->setExclusive(true);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(2, 4, 2, 4);
    m_layout->setSpacing(4);

    m_tabsHost = new QWidget(this);
    m_tabsLayout = new QVBoxLayout(m_tabsHost);
    m_tabsLayout->setContentsMargins(0, 0, 0, 0);
    m_tabsLayout->setSpacing(4);
    m_layout->addWidget(m_tabsHost, 0, Qt::AlignTop);

    m_layout->addStretch(1);

    m_addButton = new QToolButton(this);
    m_addButton->setAutoRaise(true);
    m_addButton->setToolTip(tr("New bookmark group"));
    m_addButton->setIcon(QIcon(QStringLiteral(":/icons/toolbar/tab-add.svg")));
    m_addButton->setIconSize(QSize(m_iconSize, m_iconSize));
    m_addButton->setFixedSize(m_iconSize + 12, m_iconSize + 12);
    connect(m_addButton, &QToolButton::clicked, this, &BookmarkGroupBar::addGroupRequested);
    m_layout->addWidget(m_addButton, 0, Qt::AlignHCenter | Qt::AlignBottom);

    setFixedWidth(m_iconSize + 16);
}

void BookmarkGroupBar::setTabIconSize(int size)
{
    if (size < 16) {
        size = 16;
    }
    m_iconSize = size;
    m_addButton->setIconSize(QSize(m_iconSize, m_iconSize));
    m_addButton->setFixedSize(m_iconSize + 12, m_iconSize + 12);
    setFixedWidth(m_iconSize + 16);
    rebuildButtons();
}

void BookmarkGroupBar::setGroups(const QVector<BookmarkGroupInfo> &groups, const QString &currentGroupId)
{
    m_groups = groups;
    m_currentGroupId = currentGroupId;
    rebuildButtons();
}

void BookmarkGroupBar::rebuildButtons()
{
    qDeleteAll(m_tabButtons);
    m_tabButtons.clear();

    while (QLayoutItem *item = m_tabsLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    for (const BookmarkGroupInfo &g : m_groups) {
        auto *btn = new QToolButton(m_tabsHost);
        btn->setCheckable(true);
        btn->setAutoRaise(true);
        btn->setProperty("groupId", g.id);
        btn->setToolTip(g.id);
        btn->setIcon(BundledIcons::iconByName(g.iconName.isEmpty() ? QStringLiteral("folder") : g.iconName));
        btn->setIconSize(QSize(m_iconSize, m_iconSize));
        btn->setFixedSize(m_iconSize + 12, m_iconSize + 12);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        btn->installEventFilter(this);
        m_buttonGroup->addButton(btn);
        m_tabButtons.insert(g.id, btn);
        m_tabsLayout->addWidget(btn, 0, Qt::AlignHCenter);
        connect(btn, &QToolButton::clicked, this, [this, id = g.id]() {
            selectGroup(id);
        });
    }

    selectGroup(m_currentGroupId);
}

void BookmarkGroupBar::selectGroup(const QString &groupId)
{
    QString id = groupId;
    if (!m_tabButtons.contains(id) && !m_groups.isEmpty()) {
        id = m_groups.first().id;
    }
    if (id.isEmpty()) {
        return;
    }
    m_currentGroupId = id;
    if (QToolButton *btn = m_tabButtons.value(id)) {
        btn->setChecked(true);
    }
    emit currentGroupChanged(id);
}

bool BookmarkGroupBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ContextMenu) {
        auto *btn = qobject_cast<QToolButton *>(watched);
        if (btn) {
            const QString gid = btn->property("groupId").toString();
            if (!gid.isEmpty()) {
                QMenu menu;
                QAction *setIcon = menu.addAction(tr("Set group icon…"));
                connect(setIcon, &QAction::triggered, this, [this, gid]() {
                    emit groupIconChangeRequested(gid);
                });
                menu.exec(static_cast<QContextMenuEvent *>(event)->globalPos());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
