#include "bookmarkgroupbar.h"
#include "bundledicons.h"

#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QSize iconSizeForTabButton(int tabButtonSize)
{
    const int inner = qMax(16, tabButtonSize - 12);
    return QSize(inner, inner);
}
} // namespace

BookmarkGroupBar::BookmarkGroupBar(QWidget *parent)
    : QWidget(parent)
    , m_buttonGroup(new QButtonGroup(this))
{
    m_buttonGroup->setExclusive(true);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(2, 4, 2, 4);
    m_layout->setSpacing(4);

    m_addButton = new QToolButton(this);
    m_addButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_addButton->setAutoRaise(false);
    m_addButton->setToolTip(tr("New bookmark group"));
    QIcon addIcon(QStringLiteral(":/icons/toolbar/tab-add.svg"));
    if (addIcon.isNull()) {
        addIcon = style()->standardIcon(QStyle::SP_FileDialogNewFolder);
    }
    m_addButton->setIcon(addIcon);
    connect(m_addButton, &QToolButton::clicked, this, &BookmarkGroupBar::addGroupRequested);
    m_layout->addWidget(m_addButton, 0, Qt::AlignHCenter | Qt::AlignTop);

    m_tabsHost = new QWidget(this);
    m_tabsLayout = new QVBoxLayout(m_tabsHost);
    m_tabsLayout->setContentsMargins(0, 0, 0, 0);
    m_tabsLayout->setSpacing(4);
    m_layout->addWidget(m_tabsHost, 0, Qt::AlignTop);

    m_layout->addStretch(1);

    applyButtonSizes();
}

void BookmarkGroupBar::applyButtonSizes()
{
    const QSize btnSize(m_tabButtonSize, m_tabButtonSize);
    const QSize iconSize = iconSizeForTabButton(m_tabButtonSize);
    setFixedWidth(m_tabButtonSize + 8);
    setMinimumWidth(m_tabButtonSize + 8);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    const QString btnStyle = QStringLiteral(
        "QToolButton { border: 1px solid palette(mid); border-radius: 4px; "
        "background: palette(button); }"
        "QToolButton:hover { background: palette(light); }"
        "QToolButton:checked { background: palette(highlight); }");

    if (m_addButton) {
        m_addButton->setIconSize(iconSize);
        m_addButton->setFixedSize(btnSize);
        m_addButton->setStyleSheet(btnStyle);
    }

    for (QToolButton *btn : m_tabButtons) {
        btn->setIconSize(iconSize);
        btn->setFixedSize(btnSize);
        btn->setStyleSheet(btnStyle);
    }
}

void BookmarkGroupBar::setTabButtonSize(int size)
{
    if (size < 24) {
        size = 24;
    }
    if (size > 128) {
        size = 128;
    }
    if (m_tabButtonSize == size) {
        return;
    }
    m_tabButtonSize = size;
    applyButtonSizes();
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

    const QSize btnSize(m_tabButtonSize, m_tabButtonSize);
    const QSize iconSize = iconSizeForTabButton(m_tabButtonSize);

    for (const BookmarkGroupInfo &g : m_groups) {
        auto *btn = new QToolButton(m_tabsHost);
        btn->setCheckable(true);
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setAutoRaise(false);
        btn->setProperty("groupId", g.id);
        btn->setToolTip(g.id);
        btn->setIcon(BundledIcons::iconByName(g.iconName.isEmpty() ? QStringLiteral("folder") : g.iconName));
        btn->setIconSize(iconSize);
        btn->setFixedSize(btnSize);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        m_buttonGroup->addButton(btn);
        m_tabButtons.insert(g.id, btn);
        m_tabsLayout->addWidget(btn, 0, Qt::AlignHCenter);
        connect(btn, &QToolButton::clicked, this, [this, id = g.id]() {
            selectGroup(id);
        });
        connect(btn, &QWidget::customContextMenuRequested, this, [this, btn, id = g.id](const QPoint &pos) {
            QMenu menu;
            QAction *setIcon = menu.addAction(tr("Set group icon…"));
            connect(setIcon, &QAction::triggered, this, [this, id]() {
                emit groupIconChangeRequested(id);
            });
            if (m_groups.size() > 1) {
                menu.addSeparator();
                QAction *delGroup = menu.addAction(tr("Delete group…"));
                connect(delGroup, &QAction::triggered, this, [this, id]() {
                    emit groupDeleteRequested(id);
                });
            }
            menu.exec(btn->mapToGlobal(pos));
        });
    }

    selectGroup(m_currentGroupId);
    applyButtonSizes();
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

void BookmarkGroupBar::contextMenuEvent(QContextMenuEvent *event)
{
    QWidget *child = childAt(event->pos());
    while (child && child != this) {
        if (qobject_cast<QToolButton *>(child)) {
            event->accept();
            return;
        }
        child = child->parentWidget();
    }
    event->accept();
}
