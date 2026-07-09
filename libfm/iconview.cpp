#include "iconview.h"
#include "bundledicons.h"

#include <QListView>
#include <QFrame>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QShowEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <QApplication>

namespace {

constexpr int kIconTop = 6;
constexpr int kTextTopGap = 2;
constexpr int kFramePad = 2;

/** Text selection while renaming (distinct from app highlight / gray). */
constexpr QRgb kRenameSelectionBackground = 0xff0078d4;
constexpr QRgb kRenameSelectionForeground = 0xffffffff;

QString renameEditorStyleSheet()
{
    const QPalette pal = QApplication::palette();
    const QString border = pal.color(QPalette::Mid).name();
    return QStringLiteral(
               "QPlainTextEdit {"
               " background: palette(base);"
               " color: palette(text);"
               " border: 1px solid %1;"
               " border-radius: 2px;"
               " padding: 2px;"
               " selection-background-color: #0078d4;"
               " selection-color: #ffffff;"
               "}")
        .arg(border);
}

class RenameEditor : public QPlainTextEdit
{
public:
    explicit RenameEditor(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
    {
        setFrameStyle(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        setTabChangesFocus(true);
        document()->setDocumentMargin(0);
        setLineWrapMode(QPlainTextEdit::WidgetWidth);

        QPalette pal = palette();
        pal.setColor(QPalette::Highlight, QColor::fromRgb(kRenameSelectionBackground));
        pal.setColor(QPalette::HighlightedText, QColor::fromRgb(kRenameSelectionForeground));
        setPalette(pal);
        setStyleSheet(renameEditorStyleSheet());
    }

    void applyRenameLayout()
    {
        QTextCursor cursor(document());
        cursor.select(QTextCursor::Document);
        QTextBlockFormat blockFormat;
        blockFormat.setAlignment(Qt::AlignRight);
        cursor.mergeBlockFormat(blockFormat);
        cursor.clearSelection();
    }

    void keepCursorVisible()
    {
        ensureCursorVisible();
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QPlainTextEdit::showEvent(event);
        QTimer::singleShot(0, this, [this]() { keepCursorVisible(); });
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QPlainTextEdit::resizeEvent(event);
        keepCursorVisible();
    }
};

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
    setCellGaps(gap, gap);
}

void IconViewDelegate::setCellGaps(int horizontal, int vertical)
{
    _cellGapH = qBound(0, horizontal, 64);
    _cellGapV = qBound(0, vertical, 64);
}

QSize IconViewDelegate::iconGridSize(int zoom, int cellGapH, int cellGapV, const QFontMetrics &fm)
{
    const int gapH = qBound(0, cellGapH, 64);
    const int gapV = qBound(0, cellGapV, 64);
    const int textHeight = twoLineTextHeight(fm);
    const int cellWidth = zoom + gapH;
    const int cellHeight = kIconTop + zoom + kTextTopGap + textHeight + kFramePad + gapV;
    return QSize(cellWidth, cellHeight);
}

QRect IconViewDelegate::textLabelRect(const QRect &itemRect, int zoom, int cellGapH,
                                      const QFontMetrics &fm)
{
    const int gapH = qBound(0, cellGapH, 64);
    const int inset = qMax(1, gapH / 2);
    const int iconBottom = itemRect.top() + kIconTop + zoom;
    return QRect(itemRect.left() + inset, iconBottom + kTextTopGap,
                 itemRect.width() - 2 * inset, twoLineTextHeight(fm));
}

QRect IconViewDelegate::itemHighlightRect(const QRect &itemRect, int zoom, int cellGapH,
                                          int cellGapV, const QFontMetrics &fm)
{
    Q_UNUSED(cellGapV);
    const int gapH = qBound(0, cellGapH, 64);
    const int inset = qMax(1, gapH / 2);
    QRect iconRect(itemRect.left() + (itemRect.width() - zoom) / 2,
                   itemRect.top() + kIconTop,
                   zoom, zoom);
    const QRect txtRect = textLabelRect(itemRect, zoom, cellGapH, fm);
    const int frameTop = iconRect.top() - kFramePad;
    const int frameBottom = txtRect.bottom() + kFramePad;
    return QRect(itemRect.left() + inset, frameTop,
                 itemRect.width() - 2 * inset, frameBottom - frameTop);
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
    auto *editor = new RenameEditor(parent);
    editor->setFixedHeight(twoLineTextHeight(option.fontMetrics) + 4);
    return editor;
}

void IconViewDelegate::updateEditorGeometry(QWidget *editor,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const
{
    Q_UNUSED(index);
    const int zoom = iconPaintSize(option);
    const QRect txtRect = textLabelRect(option.rect, zoom, _cellGapH, option.fontMetrics);
    editor->setGeometry(txtRect);
}

void IconViewDelegate::setEditorData(QWidget *editor,
                                     const QModelIndex &index) const
{
    _isEditing = true;
    _index = index;
    auto *plain = static_cast<RenameEditor *>(editor);
    if (!plain) {
        return;
    }
    plain->setPlainText(index.data(Qt::EditRole).toString());
    plain->applyRenameLayout();
    plain->selectAll();
}

void IconViewDelegate::setModelData(QWidget *editor,
                                    QAbstractItemModel *model,
                                    const QModelIndex &index) const
{
    auto *plain = static_cast<RenameEditor *>(editor);
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
    return iconGridSize(iconPaintSize(option), _cellGapH, _cellGapV, option.fontMetrics);
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
    QRect iconRect(item.left() + (item.width() - zoom) / 2,
                   item.top() + kIconTop,
                   zoom, zoom);
    const QFontMetrics fm = opt.fontMetrics;
    QRect txtRect = textLabelRect(item, zoom, _cellGapH, fm);
    QBrush txtBrush = qvariant_cast<QBrush>(index.data(Qt::ForegroundRole));
    bool isSelected = opt.state & QStyle::State_Selected;
    bool isEditing = _isEditing && index==_index;

    painter->setRenderHint(QPainter::Antialiasing);

    if (isSelected && !isEditing) {
        QPainterPath path;
        const QRect frame = itemHighlightRect(item, zoom, _cellGapH, _cellGapV, fm);
        path.addRoundedRect(frame, 15, 15);
        painter->setOpacity(0.85);
        painter->fillPath(path, opt.palette.highlight());
        painter->setOpacity(1.0);
    } else if ((opt.state & QStyle::State_MouseOver) && !isEditing) {
        QPainterPath path;
        const QRect frame = itemHighlightRect(item, zoom, _cellGapH, _cellGapV, fm);
        path.addRoundedRect(frame, 15, 15);
        painter->setOpacity(0.38);
        painter->fillPath(path, opt.palette.highlight());
        painter->setOpacity(1.0);
    }

    const QPixmap pm = BundledIcons::iconPixmap(icon, zoom);
    painter->drawPixmap(iconRect, pm);

    if (isEditing) { return; }

    const QColor textColor = isSelected
        ? opt.palette.highlightedText().color()
        : (index.data(Qt::ForegroundRole).isValid()
               ? txtBrush.color()
               : opt.palette.text().color());
    drawTwoLineFileName(painter, txtRect, index.data().toString(), opt.font, textColor);
}
