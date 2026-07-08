#include "customactionsettingswidget.h"
#include "settingsuistyles.h"
#include "bundledicons.h"
#include "common.h"
#include "icondlg.h"

#include <QCheckBox>
#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QFrame *makeModuleFrame(QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("settingsModule"));
    frame->setFrameShape(QFrame::StyledPanel);
    return frame;
}

void pasteClipboardInto(QLineEdit *edit)
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (mime && mime->hasText()) {
        edit->setText(mime->text().trimmed());
    }
}

QIcon effectiveIcon(const CustomActionEntry &entry)
{
    if (!entry.iconPath.isEmpty()) {
        const QIcon pathIcon(entry.iconPath);
        if (!pathIcon.isNull()) {
            return pathIcon;
        }
    }
    if (!entry.bundledIconName.isEmpty()) {
        return BundledIcons::iconByName(entry.bundledIconName);
    }
    return BundledIcons::iconByName(QStringLiteral("empty"));
}

CustomActionEntry parseStoredRow(const QStringList &temp)
{
    CustomActionEntry entry;
    if (temp.size() < 4) {
        return entry;
    }
    entry.fileType = temp.at(0);
    entry.name = temp.at(1);
    entry.bundledIconName = temp.at(2);

    QString cmd;
    if (temp.size() >= 5) {
        entry.iconPath = temp.at(3);
        cmd = temp.at(4);
    } else {
        cmd = temp.at(3);
    }
    if (!cmd.isEmpty() && cmd.at(0) == QLatin1Char('|')) {
        cmd.remove(0, 1);
        entry.monitorOutput = true;
    }
    entry.command = cmd;
    return entry;
}

QStringList serializeRow(const CustomActionEntry &entry)
{
    QString cmd = entry.command;
    if (entry.monitorOutput && !cmd.isEmpty()) {
        cmd.prepend(QLatin1Char('|'));
    }
    return QStringList() << entry.fileType << entry.name << entry.bundledIconName
                         << entry.iconPath << cmd;
}

} // namespace

CustomActionSettingsWidget::CustomActionSettingsWidget(QWidget *parent) : QWidget(parent)
{
    setStyleSheet(SettingsUiStyles::moduleStyleSheet());

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto *container = new QWidget(scrollArea);
    auto *pageLayout = new QVBoxLayout(container);
    pageLayout->setSpacing(10);

    auto *hint = new QLabel(tr("Each module is one custom context-menu action. "
                               "Icon path overrides the bundled icon when set. "
                               "Use %f, %F, %n in commands. Enable capture output to show stdout/stderr."));
    hint->setWordWrap(true);
    pageLayout->addWidget(hint);

    auto *toolbar = new QHBoxLayout();
    auto *addBtn = new QPushButton(tr("Add action module"));
    SettingsUiStyles::styleAddButton(addBtn);
    connect(addBtn, &QPushButton::clicked, this, &CustomActionSettingsWidget::addActionModule);

    auto *infoBtn = new QPushButton(tr("Usage…"));
    connect(infoBtn, &QPushButton::clicked, this, &CustomActionSettingsWidget::showUsageInfo);

    toolbar->addWidget(addBtn);
    toolbar->addWidget(infoBtn);
    toolbar->addStretch(1);
    pageLayout->addLayout(toolbar);

    modulesLayout = new QVBoxLayout();
    modulesLayout->setSpacing(10);
    pageLayout->addLayout(modulesLayout);
    pageLayout->addStretch(1);

    scrollArea->setWidget(container);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scrollArea, 1);
}

void CustomActionSettingsWidget::loadFromSettings(QSettings *settings)
{
    actionEntries.clear();
    settings->beginGroup(QStringLiteral("customActions"));
    const QStringList keys = settings->childKeys();
    for (const QString &key : keys) {
        actionEntries.append(parseStoredRow(settings->value(key).toStringList()));
    }
    settings->endGroup();
    rebuildModules();
}

void CustomActionSettingsWidget::saveToSettings(QSettings *settings) const
{
    settings->remove(QStringLiteral("customActions"));
    settings->beginGroup(QStringLiteral("customActions"));
    for (int i = 0; i < actionEntries.size(); ++i) {
        const QString number = QStringLiteral("%1").arg(i, 4, 10, QChar('0'));
        settings->setValue(number, serializeRow(actionEntries.at(i)));
    }
    settings->endGroup();
}

void CustomActionSettingsWidget::setDefaults(const QVector<QStringList> &rows)
{
    actionEntries.clear();
    for (const QStringList &row : rows) {
        CustomActionEntry entry;
        if (row.size() >= 4) {
            entry.fileType = row.at(0);
            entry.name = row.at(1);
            entry.bundledIconName = row.at(2);
            entry.command = row.at(3);
        }
        actionEntries.append(entry);
    }
    rebuildModules();
    emit entriesChanged();
}

void CustomActionSettingsWidget::clearAll()
{
    actionEntries.clear();
    rebuildModules();
}

void CustomActionSettingsWidget::addActionModule()
{
    CustomActionEntry entry;
    entry.fileType = QStringLiteral("*");
    entry.bundledIconName = QStringLiteral("empty");
    actionEntries.append(entry);
    rebuildModules();
    emit entriesChanged();
}

void CustomActionSettingsWidget::showUsageInfo()
{
    const QString info = tr("Use 'folder' to match all folders.<br>"
                            "Use a folder name to match a specific folder.<br>"
                            "Set text to 'Open' to override xdg default."
                            "<p>%f - selected files<br>"
                            "%F - selected files with full path<br>"
                            "%n - current filename</p>"
                            "<p>Enable <b>Capture output</b> to monitor stdout and stderr.</p>");
    QMessageBox::information(this, tr("Usage"), info);
}

void CustomActionSettingsWidget::rebuildModules()
{
    while (QLayoutItem *item = modulesLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    if (actionEntries.isEmpty()) {
        auto *emptyLabel = new QLabel(tr("No custom actions yet. Click “Add action module”."));
        emptyLabel->setWordWrap(true);
        modulesLayout->addWidget(emptyLabel);
    } else {
        for (int i = 0; i < actionEntries.size(); ++i) {
            modulesLayout->addWidget(buildModuleFrame(i));
        }
    }
}

QFrame *CustomActionSettingsWidget::buildModuleFrame(int index)
{
    CustomActionEntry *entry = &actionEntries[index];
    auto *frame = makeModuleFrame(scrollArea->widget());
    auto *mainLay = new QVBoxLayout(frame);

    // Row 1: file type, name, bundled icon
    auto *row1 = new QHBoxLayout();
    auto *fileTypeEdit = new QLineEdit(entry->fileType);
    fileTypeEdit->setPlaceholderText(tr("File type(s), comma-separated"));
    auto *nameEdit = new QLineEdit(entry->name);
    nameEdit->setPlaceholderText(tr("Action name"));
    auto *iconBtn = new QToolButton();
    iconBtn->setIcon(effectiveIcon(*entry));
    iconBtn->setIconSize(QSize(28, 28));
    iconBtn->setToolTip(tr("Choose bundled icon"));
    iconBtn->setText(tr("Icon…"));
    iconBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    row1->addWidget(new QLabel(tr("File type")));
    row1->addWidget(fileTypeEdit, 2);
    row1->addWidget(new QLabel(tr("Action")));
    row1->addWidget(nameEdit, 2);
    row1->addWidget(iconBtn, 0);

    // Row 2: icon path + paste
    auto *row2 = new QHBoxLayout();
    auto *iconPathEdit = new QLineEdit(entry->iconPath);
    iconPathEdit->setPlaceholderText(tr("Paste icon file path (overrides bundled icon)"));
    auto *pasteIconBtn = new QPushButton(tr("Paste"));
    row2->addWidget(new QLabel(tr("Icon path")));
    row2->addWidget(iconPathEdit, 1);
    row2->addWidget(pasteIconBtn);

    // Row 3: command + paste + capture output
    auto *row3 = new QHBoxLayout();
    auto *cmdEdit = new QLineEdit(entry->command);
    cmdEdit->setPlaceholderText(tr("Shell command"));
    auto *pasteCmdBtn = new QPushButton(tr("Paste"));
    auto *monitorCheck = new QCheckBox(tr("Capture output"));
    monitorCheck->setChecked(entry->monitorOutput);
    row3->addWidget(new QLabel(tr("Command")));
    row3->addWidget(cmdEdit, 1);
    row3->addWidget(pasteCmdBtn);
    row3->addWidget(monitorCheck);

    mainLay->addLayout(row1);
    mainLay->addLayout(row2);
    mainLay->addLayout(row3);

    auto *delBtn = new QPushButton(tr("Delete module"));
    SettingsUiStyles::styleDeleteButton(delBtn);
    mainLay->addWidget(delBtn, 0, Qt::AlignRight);

    connect(fileTypeEdit, &QLineEdit::textChanged, [entry](const QString &t) {
        entry->fileType = t;
    });
    connect(nameEdit, &QLineEdit::textChanged, [this, entry](const QString &t) {
        entry->name = t;
        emit entriesChanged();
    });
    connect(iconPathEdit, &QLineEdit::textChanged, [entry, iconBtn](const QString &t) {
        entry->iconPath = t;
        iconBtn->setIcon(effectiveIcon(*entry));
    });
    connect(cmdEdit, &QLineEdit::textChanged, [entry](const QString &t) {
        entry->command = t;
    });
    connect(monitorCheck, &QCheckBox::toggled, [entry](bool on) {
        entry->monitorOutput = on;
    });
    connect(pasteIconBtn, &QPushButton::clicked, [iconPathEdit]() {
        pasteClipboardInto(iconPathEdit);
    });
    connect(pasteCmdBtn, &QPushButton::clicked, [cmdEdit]() {
        pasteClipboardInto(cmdEdit);
    });
    connect(iconBtn, &QToolButton::clicked, this, [this, entry, iconBtn]() {
        icondlg dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            entry->bundledIconName = dlg.result;
            iconBtn->setIcon(effectiveIcon(*entry));
        }
    });
    connect(delBtn, &QPushButton::clicked, this, [this, index]() {
        if (index >= 0 && index < actionEntries.size()) {
            actionEntries.removeAt(index);
            rebuildModules();
            emit entriesChanged();
        }
    });

    return frame;
}
