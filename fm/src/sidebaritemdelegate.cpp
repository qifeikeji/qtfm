#include "sidebaritemdelegate.h"
#include "common.h"
#include "disksmodel.h"

#include <QPainter>
#include <QApplication>
#include <QFontMetrics>

namespace {
const int kMinItemHeight = 54;
const int kDiskProgressMinWidth = 200;
const int kIconSize = 24;
const int kHPad = 8;
const int kVPad = 6;
const int kIconTextGap = 8;
}

// ------------------------------------------------------------------------
// BookmarkItemDelegate
// ------------------------------------------------------------------------

BookmarkItemDelegate::BookmarkItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize BookmarkItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    const QString name = index.data(Qt::DisplayRole).toString();
    if (name.isEmpty()) {
        return size; // separator row, keep the compact default height
    }
    size.setHeight(qMax(size.height(), kMinItemHeight));
    return size;
}

void BookmarkItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    const QString name = index.data(Qt::DisplayRole).toString();
    if (name.isEmpty()) {
        // Separator row: keep default rendering (background tile etc).
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    painter->save();

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    const QRect rect = opt.rect;
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QRect iconRect(rect.left() + kHPad, rect.top() + (rect.height() - kIconSize) / 2,
                  kIconSize, kIconSize);
    if (!icon.isNull()) {
        icon.paint(painter, iconRect, Qt::AlignCenter,
                  (opt.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
    }

    const int textLeft = iconRect.right() + kIconTextGap;
    const int textWidth = rect.right() - textLeft - kHPad;
    const QString path = index.data(BOOKMARK_PATH).toString();

    QFont nameFont = opt.font;
    QFont pathFont = opt.font;
    pathFont.setPointSizeF(qMax(7.0, opt.font.pointSizeF() - 1.5));

    const QFontMetrics nameFm(nameFont);
    const QFontMetrics pathFm(pathFont);

    const QColor textColor = (opt.state & QStyle::State_Selected)
                                 ? opt.palette.highlightedText().color()
                                 : opt.palette.text().color();
    QColor pathColor = textColor;
    pathColor.setAlpha(150); // grey / dimmed path line

    const int nameY = rect.top() + kVPad + nameFm.ascent();
    const int pathY = rect.top() + kVPad + nameFm.height() + pathFm.ascent() + 2;

    painter->setFont(nameFont);
    painter->setPen(textColor);
    painter->drawText(textLeft, nameY,
                      nameFm.elidedText(name, Qt::ElideRight, textWidth));

    if (!path.isEmpty()) {
        painter->setFont(pathFont);
        painter->setPen(pathColor);
        painter->drawText(textLeft, pathY,
                          pathFm.elidedText(path, Qt::ElideMiddle, textWidth));
    }

    painter->restore();
}

// ------------------------------------------------------------------------
// DiskItemDelegate
// ------------------------------------------------------------------------

DiskItemDelegate::DiskItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize DiskItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    Q_UNUSED(index);
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), kMinItemHeight));
    const int minRowWidth = kHPad + kIconSize + kIconTextGap + kDiskProgressMinWidth + kHPad;
    size.setWidth(qMax(size.width(), minRowWidth));
    return size;
}

void DiskItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    painter->save();

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    const QRect rect = opt.rect;
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QRect iconRect(rect.left() + kHPad, rect.top() + (rect.height() - kIconSize) / 2,
                  kIconSize, kIconSize);
    if (!icon.isNull()) {
        icon.paint(painter, iconRect, Qt::AlignCenter,
                  (opt.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
    }

    const int textLeft = iconRect.right() + kIconTextGap;
    const int textWidth = rect.right() - textLeft - kHPad;

    const QString name = index.data(Qt::DisplayRole).toString();
    const QString mountpoint = index.data(DISK_MOUNTPOINT).toString();
    const qint64 used = index.data(DISK_USED_BYTES).toLongLong();
    const qint64 total = index.data(DISK_TOTAL_BYTES).toLongLong();

    const QColor textColor = (opt.state & QStyle::State_Selected)
                                 ? opt.palette.highlightedText().color()
                                 : opt.palette.text().color();

    const QFontMetrics nameFm(opt.font);
    painter->setFont(opt.font);
    painter->setPen(textColor);
    const int nameY = rect.top() + kVPad + nameFm.ascent();
    painter->drawText(textLeft, nameY,
                      nameFm.elidedText(name, Qt::ElideRight, textWidth));

    // Second line: usage progress bar, or a muted "not mounted" placeholder.
    const int barHeight = 8;
    const int barTop = rect.top() + kVPad + nameFm.height() + 6;
    const int barWidth = qMax(kDiskProgressMinWidth, textWidth);
    const QRect barRect(textLeft, barTop, qMax(0, barWidth), barHeight);

    QColor freeColor = textColor;
    freeColor.setAlpha(60);

    painter->setPen(Qt::NoPen);
    painter->setBrush(freeColor);
    painter->drawRoundedRect(barRect, barHeight / 2, barHeight / 2);

    if (!mountpoint.isEmpty() && total > 0) {
        const double ratio = qBound(0.0, static_cast<double>(used) / static_cast<double>(total), 1.0);
        QRect usedRect(barRect);
        usedRect.setWidth(qMax(barHeight, static_cast<int>(barRect.width() * ratio)));
        QColor usedColor = opt.palette.highlight().color();
        painter->setBrush(usedColor);
        painter->drawRoundedRect(usedRect, barHeight / 2, barHeight / 2);
    }

    painter->restore();
}
