#include "sidebaritemdelegate.h"
#include "common.h"
#include "disksmodel.h"

#include <QPainter>
#include <QApplication>
#include <QFontMetrics>
#include <QLineEdit>
#include <QFrame>

namespace {
const int kMinItemHeight = 54;
const int kDiskItemHeight = 36;
const int kSeparatorHeight = 20;
const int kIconSize = 24;
const int kHPad = 8;
const int kVPad = 6;
const int kIconTextGap = 8;

QString bookmarkRenameEditorStyleSheet()
{
    const QPalette pal = QApplication::palette();
    return QStringLiteral(
               "QLineEdit {"
               " background: palette(base);"
               " color: palette(text);"
               " border: 1px solid %1;"
               " border-radius: 2px;"
               " padding: 4px 6px;"
               " selection-background-color: #0078d4;"
               " selection-color: #ffffff;"
               "}")
        .arg(pal.color(QPalette::Mid).name());
}

QString formatDiskSizeGbNumber(qint64 bytes)
{
    if (bytes <= 0) {
        return QStringLiteral("--");
    }
    const double gb = static_cast<double>(bytes) / 1000000000.0;
    if (gb >= 100.0) {
        return QString::number(gb, 'f', 0);
    }
    return QString::number(gb, 'f', 1);
}

QString diskUsageSizeLabel(qint64 usedBytes, qint64 totalBytes)
{
    return QStringLiteral("%1/%2G")
        .arg(formatDiskSizeGbNumber(usedBytes), formatDiskSizeGbNumber(totalBytes));
}
} // namespace

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
    const QString name = index.data(Qt::DisplayRole).toString();
    const QString path = index.data(BOOKMARK_PATH).toString();
    if (name.isEmpty() && path.isEmpty()) {
        return QSize(option.rect.width() > 0 ? option.rect.width() : 100, kSeparatorHeight);
    }
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), kMinItemHeight));
    return size;
}

void BookmarkItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    const QString name = index.data(Qt::DisplayRole).toString();
    const QString path = index.data(BOOKMARK_PATH).toString();
    if (name.isEmpty() && path.isEmpty()) {
        painter->save();
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        const QRect rect = opt.rect;
        QColor lineColor = opt.palette.text().color();
        lineColor.setAlpha(90);
        const int y = rect.center().y();
        const int margin = kHPad;
        painter->setPen(QPen(lineColor, 1));
        painter->drawLine(rect.left() + margin, y, rect.right() - margin, y);
        painter->restore();
        return;
    }

    if (m_editing && index == m_editingIndex) {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
        return;
    }

    painter->save();

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

    const QRect rect = opt.rect;
    if (opt.state & QStyle::State_Selected) {
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
    } else if (opt.state & QStyle::State_MouseOver) {
        QColor hoverBg = opt.palette.highlight().color();
        hoverBg.setAlpha(72);
        painter->fillRect(rect, hoverBg);
    } else {
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
    }

    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QRect iconRect(rect.left() + kHPad, rect.top() + (rect.height() - kIconSize) / 2,
                  kIconSize, kIconSize);
    if (!icon.isNull()) {
        icon.paint(painter, iconRect, Qt::AlignCenter,
                  (opt.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
    }

    const int textLeft = iconRect.right() + kIconTextGap;
    const int textWidth = rect.right() - textLeft - kHPad;

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

QWidget *BookmarkItemDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    auto *editor = new QLineEdit(parent);
    editor->setFrame(false);
    editor->setStyleSheet(bookmarkRenameEditorStyleSheet());
    QPalette pal = editor->palette();
    pal.setColor(QPalette::Highlight, QColor(0, 120, 212));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    editor->setPalette(pal);
    return editor;
}

void BookmarkItemDelegate::updateEditorGeometry(QWidget *editor,
                                                const QStyleOptionViewItem &option,
                                                const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect.adjusted(kHPad / 2, 2, -kHPad / 2, -2));
}

void BookmarkItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    m_editing = true;
    m_editingIndex = index;
    auto *line = qobject_cast<QLineEdit *>(editor);
    if (!line) {
        return;
    }
    line->setText(index.data(Qt::EditRole).toString());
    line->selectAll();
}

void BookmarkItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                        const QModelIndex &index) const
{
    auto *line = qobject_cast<QLineEdit *>(editor);
    if (line) {
        model->setData(index, line->text().trimmed(), Qt::EditRole);
    }
    m_editing = false;
    m_editingIndex = QModelIndex();
}

void BookmarkItemDelegate::destroyEditor(QWidget *editor, const QModelIndex &index) const
{
    m_editing = false;
    m_editingIndex = QModelIndex();
    QStyledItemDelegate::destroyEditor(editor, index);
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
    if (index.data(DISK_IS_SEPARATOR).toBool()) {
        return QSize(option.rect.width() > 0 ? option.rect.width() : 100, kSeparatorHeight);
    }
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), kDiskItemHeight));
    return size;
}

void DiskItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    if (index.data(DISK_IS_SEPARATOR).toBool()) {
        painter->save();
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        const QRect rect = opt.rect;
        QColor lineColor = opt.palette.text().color();
        lineColor.setAlpha(90);
        const int y = rect.center().y();
        painter->setPen(QPen(lineColor, 1));
        painter->drawLine(rect.left() + kHPad, y, rect.right() - kHPad, y);
        painter->restore();
        return;
    }

    painter->save();

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

    const QRect rect = opt.rect;
    if (opt.state & QStyle::State_Selected) {
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
    } else if (opt.state & QStyle::State_MouseOver) {
        QColor hoverBg = opt.palette.highlight().color();
        hoverBg.setAlpha(72);
        painter->fillRect(rect, hoverBg);
    } else {
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
    }

    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QRect iconRect(rect.left() + kHPad, rect.top() + (rect.height() - kIconSize) / 2,
                  kIconSize, kIconSize);
    if (!icon.isNull()) {
        icon.paint(painter, iconRect, Qt::AlignCenter,
                  (opt.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
    }

    const int textLeft = iconRect.right() + kIconTextGap;
    const int textRight = rect.right() - kHPad;

    const QString name = index.data(Qt::DisplayRole).toString();
    const qint64 used = index.data(DISK_USED_BYTES).toLongLong();
    const qint64 total = index.data(DISK_TOTAL_BYTES).toLongLong();
    const QString sizeText = diskUsageSizeLabel(used, total);

    const QColor textColor = (opt.state & QStyle::State_Selected)
                                 ? opt.palette.highlightedText().color()
                                 : opt.palette.text().color();

    const QFontMetrics fm(opt.font);
    const int sizeWidth = fm.horizontalAdvance(sizeText) + 4;
    const int nameWidth = qMax(0, textRight - textLeft - sizeWidth);

    painter->setFont(opt.font);
    painter->setPen(textColor);
    const int textY = rect.top() + (rect.height() + fm.ascent() - fm.descent()) / 2;
    painter->drawText(textLeft, textY, fm.elidedText(name, Qt::ElideRight, nameWidth));
    painter->drawText(textRight - fm.horizontalAdvance(sizeText), textY, sizeText);

    painter->restore();
}
