#include "iconview.h"

#include <QPainterPath>
#include <QTextLayout>
#include <QTextOption>

namespace {

constexpr int kInterCellGap = 4;
constexpr int kIconTop = 8;
constexpr int kTextTopGap = 4;
constexpr int kBottomPad = 4;

void drawTwoLineFileName(QPainter *painter, const QRect &rect, const QString &text,
                         const QFont &font, const QColor &color)
{
    if (text.isEmpty()) {
        return;
    }
    QFontMetrics fm(font);
    const int lineHeight = fm.lineSpacing();

    QTextLayout layout(text, font);
    QTextOption opt;
    opt.setAlignment(Qt::AlignHCenter);
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opt);

    layout.beginLayout();
    QTextLine line1 = layout.createLine();
    if (!line1.isValid()) {
        layout.endLayout();
        return;
    }
    line1.setLineWidth(rect.width());

    QTextLine line2 = layout.createLine();
    bool truncated = false;
    if (line2.isValid()) {
        line2.setLineWidth(rect.width());
        QTextLine line3 = layout.createLine();
        truncated = line3.isValid();
    }
    layout.endLayout();

    painter->setPen(color);
    qreal y = rect.top();
    line1.draw(painter, QPointF(rect.left(), y));
    y += lineHeight;

    if (!line2.isValid()) {
        return;
    }
    if (truncated) {
        const QString rest = text.mid(line2.textStart());
        painter->drawText(QRect(rect.left(), int(y), rect.width(), lineHeight),
                          Qt::AlignHCenter | Qt::AlignTop,
                          fm.elidedText(rest, Qt::ElideRight, rect.width()));
    } else {
        line2.draw(painter, QPointF(rect.left(), y));
    }
}

} // namespace

QSize IconViewDelegate::iconGridSize(int zoom, const QFontMetrics &fm)
{
    const int textHeight = fm.lineSpacing() * 2;
    const int cellWidth = zoom + kInterCellGap;
    const int cellHeight = kIconTop + zoom + kTextTopGap + textHeight + kBottomPad;
    return QSize(cellWidth, cellHeight);
}

bool IconViewDelegate::eventFilter(QObject *object,
                                   QEvent *event)
{
    QWidget *editor = qobject_cast<QWidget*>(object);
    if(editor && event->type() == QEvent::KeyPress) {
        if(static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape){
            _isEditing = false;
            _index = QModelIndex();
        }
    }
    return QStyledItemDelegate::eventFilter(editor, event);
}

void IconViewDelegate::setEditorData(QWidget *editor,
                                     const QModelIndex &index) const
{ // workaround for QTBUG
    _isEditing = true;
    _index = index;
    QStyledItemDelegate::setEditorData(editor, index);
}

void IconViewDelegate::setModelData(QWidget *editor,
                                    QAbstractItemModel *model,
                                    const QModelIndex &index) const
{ // workaround for QTBUG
    QStyledItemDelegate::setModelData(editor, model, index);
    _isEditing = false;
    _index = QModelIndex();
}

QSize IconViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    Q_UNUSED(index);
    return iconGridSize(option.decorationSize.width(), option.fontMetrics);
}

void IconViewDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    const int zoom = option.decorationSize.width();
    QSize iconsize = icon.actualSize(option.decorationSize);
    if (iconsize.width() > zoom || iconsize.height() > zoom) {
        iconsize = QSize(zoom, zoom);
    }
    QRect item = option.rect;
    const int inset = kInterCellGap / 2;
    QRect iconRect(item.left() + (item.width() - iconsize.width()) / 2,
                   item.top() + kIconTop,
                   iconsize.width(), iconsize.height());
    const QFontMetrics fm = option.fontMetrics;
    const int textHeight = fm.lineSpacing() * 2;
    QRect txtRect(item.left() + inset, iconRect.bottom() + kTextTopGap,
                  item.width() - 2 * inset, textHeight);
    QBrush txtBrush = qvariant_cast<QBrush>(index.data(Qt::ForegroundRole));
    bool isSelected = option.state & QStyle::State_Selected;
    bool isEditing = _isEditing && index==_index;

    painter->setRenderHint(QPainter::Antialiasing);

    if (isSelected && !isEditing) {
        QPainterPath path;
        QRect frame(item.left() + inset, item.top() + kIconTop / 2,
                    item.width() - 2 * inset, item.height() - kIconTop / 2 - kBottomPad / 2);
        path.addRoundedRect(frame, 15, 15);
        painter->setOpacity(0.7);
        painter->fillPath(path, option.palette.highlight());
        painter->setOpacity(1.0);
    }

    painter->drawPixmap(iconRect, icon.pixmap(iconsize.width(), iconsize.height()));

    if (isEditing) { return; }

    const QColor textColor = isSelected
        ? option.palette.highlightedText().color()
        : txtBrush.color();
    drawTwoLineFileName(painter, txtRect, index.data().toString(), option.font, textColor);
}
