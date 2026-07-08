#include "iconview.h"
#include "bundledicons.h"

#include <QListView>
#include <QFrame>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>

namespace {

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

void IconViewDelegate::setCellGap(int gap)
{
    _cellGap = qBound(0, gap, 64);
}

QSize IconViewDelegate::iconGridSize(int zoom, int cellGap, const QFontMetrics &fm)
{
    const int gap = qBound(0, cellGap, 64);
    const int textHeight = twoLineTextHeight(fm);
    const int cellWidth = zoom + gap;
    const int cellHeight = kIconTop + zoom + kTextTopGap + textHeight + kFramePad;
    return QSize(cellWidth, cellHeight);
}

QRect IconViewDelegate::textLabelRect(const QRect &itemRect, int zoom, int cellGap,
                                      const QFontMetrics &fm)
{
    const int gap = qBound(0, cellGap, 64);
    const int inset = qMax(1, gap / 2);
    const int iconBottom = itemRect.top() + kIconTop + zoom;
    return QRect(itemRect.left() + inset, iconBottom + kTextTopGap,
                 itemRect.width() - 2 * inset, twoLineTextHeight(fm));
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

QWidget *IconViewDelegate::createEditor(QWidget *parent,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    Q_UNUSED(index);
    QPlainTextEdit *editor = new QPlainTextEdit(parent);
    editor->setFrameStyle(QFrame::NoFrame);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    editor->setTabChangesFocus(true);
    editor->document()->setDocumentMargin(0);
    editor->setFixedHeight(twoLineTextHeight(option.fontMetrics) + 2);
    editor->setStyleSheet(QStringLiteral("background: palette(base);"));
    return editor;
}

void IconViewDelegate::updateEditorGeometry(QWidget *editor,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const
{
    Q_UNUSED(index);
    const int zoom = iconPaintSize(option);
    const QRect txtRect = textLabelRect(option.rect, zoom, _cellGap, option.fontMetrics);
    editor->setGeometry(txtRect);
}

void IconViewDelegate::setEditorData(QWidget *editor,
                                     const QModelIndex &index) const
{
    _isEditing = true;
    _index = index;
    auto *plain = qobject_cast<QPlainTextEdit *>(editor);
    if (!plain) {
        return;
    }
    plain->setPlainText(index.data(Qt::EditRole).toString());
    QTextCursor cursor = plain->textCursor();
    QTextBlockFormat blockFormat;
    blockFormat.setAlignment(Qt::AlignHCenter);
    cursor.select(QTextDocument::SelectDocument);
    cursor.mergeBlockFormat(blockFormat);
    cursor.movePosition(QTextCursor::End);
    plain->setTextCursor(cursor);
}

void IconViewDelegate::setModelData(QWidget *editor,
                                    QAbstractItemModel *model,
                                    const QModelIndex &index) const
{
    auto *plain = qobject_cast<QPlainTextEdit *>(editor);
    if (plain) {
        QString name = plain->toPlainText();
        name.replace(QLatin1Char('\n'), QLatin1Char(' '));
        name = name.trimmed();
        model->setData(index, name, Qt::EditRole);
    }
    _isEditing = false;
    _index = QModelIndex();
}

QSize IconViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    Q_UNUSED(index);
    return iconGridSize(iconPaintSize(option), _cellGap, option.fontMetrics);
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
    const int inset = qMax(1, _cellGap / 2);
    QRect iconRect(item.left() + (item.width() - zoom) / 2,
                   item.top() + kIconTop,
                   zoom, zoom);
    const QFontMetrics fm = opt.fontMetrics;
    QRect txtRect = textLabelRect(item, zoom, _cellGap, fm);
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
