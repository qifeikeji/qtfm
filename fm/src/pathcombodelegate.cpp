#include "pathcombodelegate.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>

namespace {
constexpr int kRowHeight = 32;
constexpr int kIconCell = 26;
constexpr int kHPad = 6;
constexpr int kGap = 8;
constexpr int kRadius = 4;
} // namespace

PathComboItemDelegate::PathComboItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize PathComboItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    Q_UNUSED(index);
    return QSize(option.rect.width() > 0 ? option.rect.width() : 200, kRowHeight);
}

void PathComboItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const QRect rect = opt.rect;
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

    if (opt.state & QStyle::State_Selected) {
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
    } else if (opt.state & QStyle::State_MouseOver) {
        QColor hover = opt.palette.highlight().color();
        hover.setAlpha(72);
        painter->fillRect(rect, hover);
    } else {
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
    }

    const QColor border = opt.palette.color(QPalette::Mid);
    const int cellTop = rect.top() + (rect.height() - kIconCell) / 2;
    QRect iconCell(rect.left() + kHPad, cellTop, kIconCell, kIconCell);

    painter->setPen(border);
    painter->setBrush(opt.palette.base());
    painter->drawRoundedRect(iconCell, kRadius, kRadius);

    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (!icon.isNull()) {
        const QPixmap pm = icon.pixmap(kIconCell - 8, kIconCell - 8);
        const QPoint topLeft(iconCell.left() + (iconCell.width() - pm.width()) / 2,
                             iconCell.top() + (iconCell.height() - pm.height()) / 2);
        painter->drawPixmap(topLeft, pm);
    }

    const int textLeft = iconCell.right() + kGap;
    const int textWidth = rect.right() - textLeft - kHPad;
    const QString text = index.data(Qt::DisplayRole).toString();

    painter->setPen(opt.state & QStyle::State_Selected
                        ? opt.palette.highlightedText().color()
                        : opt.palette.text().color());
    painter->drawText(QRect(textLeft, rect.top(), textWidth, rect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      opt.fontMetrics.elidedText(text, Qt::ElideMiddle, textWidth));
}
