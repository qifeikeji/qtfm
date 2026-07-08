#include "iconview.h"
#include "bundledicons.h"

#include <QListView>
#include <QPainterPath>
#include <QTextLayout>
#include <QTextOption>

namespace {

constexpr int kInterCellGap = 4;
constexpr int kIconTop = 6;
constexpr int kTextTopGap = 2;
constexpr int kFramePad = 2;

int iconPaintSize(const QStyleOptionViewItem &option)
{
    int zoom = option.decorationSize.width();
    if (const auto *view = qobject_cast<const QListView *>(option.widget)) {
        const QSize iconSize = view->iconSize();
        if (iconSize.width() > 0) {
            zoom = iconSize.width();
        }
    }
    return qMax(zoom, IconViewDelegate::iconZoomMin);
}

int twoLineTextHeight(const QFontMetrics &fm)
{
    return fm.height() * 2 + 2;
}

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
    const int textHeight = twoLineTextHeight(fm);
    const int cellWidth = zoom + kInterCellGap;
    const int cellHeight = kIconTop + zoom + kTextTopGap + textHeight + kFramePad;
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
    return iconGridSize(iconPaintSize(option), option.fontMetrics);
}

void IconViewDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));

    const int zoom = iconPaintSize(opt);
    QRect item = opt.rect;
    const int inset = kInterCellGap / 2;
    QRect iconRect(item.left() + (item.width() - zoom) / 2,
                   item.top() + kIconTop,
                   zoom, zoom);
    const QFontMetrics fm = opt.fontMetrics;
    const int textHeight = twoLineTextHeight(fm);
    QRect txtRect(item.left() + inset, iconRect.bottom() + kTextTopGap,
                  item.width() - 2 * inset, textHeight);
    QBrush txtBrush = qvariant_cast<QBrush>(index.data(Qt::ForegroundRole));
    bool isSelected = opt.state & QStyle::State_Selected;
    bool isEditing = _isEditing && index==_index;

    painter->setRenderHint(QPainter::Antialiasing);

    if (isSelected && !isEditing) {
        QPainterPath path;
        const int frameTop = iconRect.top() - kFramePad;
        const int frameBottom = txtRect.bottom() + kFramePad;
        QRect frame(item.left() + inset, frameTop,
                    item.width() - 2 * inset, frameBottom - frameTop);
        path.addRoundedRect(frame, 15, 15);
        painter->setOpacity(0.7);
        painter->fillPath(path, opt.palette.highlight());
        painter->setOpacity(1.0);
    }

    const QPixmap pm = BundledIcons::iconPixmap(icon, zoom);
    painter->drawPixmap(iconRect, pm);

    if (isEditing) { return; }

    const QColor textColor = isSelected
        ? opt.palette.highlightedText().color()
        : txtBrush.color();
    drawTwoLineFileName(painter, txtRect, index.data().toString(), opt.font, textColor);
}
