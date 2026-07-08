#include "openwithsettingswidget.h"
#include "openwithconfig.h"
#include "settingsuistyles.h"

#include <QClipboard>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QObject>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVector>

namespace {

QFrame *makeModuleFrame(QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("settingsModule"));
    frame->setFrameShape(QFrame::StyledPanel);
    return frame;
}

void fillEntryForm(QFormLayout *form, OpenWithEntry *entry, const QStringList &suffixesHint)
{
    auto *nameEdit = new QLineEdit(entry->name);
    auto *cmd1Edit = new QLineEdit(entry->commandLine1);
    auto *cmd2Edit = new QLineEdit(entry->commandLine2);
    auto *iconEdit = new QLineEdit(entry->iconPath);
    auto *pasteBtn = new QPushButton(QObject::tr("Paste icon path"));

    form->addRow(QObject::tr("Application name"), nameEdit);
    if (!suffixesHint.isEmpty()) {
        form->addRow(QObject::tr("Extensions"),
                     new QLabel(suffixesHint.join(QStringLiteral(", "))));
    }
    form->addRow(QObject::tr("Command (line 1)"), cmd1Edit);
    form->addRow(QObject::tr("Command (line 2)"), cmd2Edit);
    auto *iconRow = new QHBoxLayout();
    iconRow->addWidget(iconEdit, 1);
    iconRow->addWidget(pasteBtn);
    QWidget *iconRowWidget = new QWidget();
    iconRowWidget->setLayout(iconRow);
    form->addRow(QObject::tr("Icon path"), iconRowWidget);

    QObject::connect(nameEdit, &QLineEdit::textChanged, [entry](const QString &t) {
        entry->name = t;
    });
    QObject::connect(cmd1Edit, &QLineEdit::textChanged, [entry](const QString &t) {
        entry->commandLine1 = t;
    });
    QObject::connect(cmd2Edit, &QLineEdit::textChanged, [entry](const QString &t) {
        entry->commandLine2 = t;
    });
    QObject::connect(iconEdit, &QLineEdit::textChanged, [entry](const QString &t) {
        entry->iconPath = t;
    });
    QObject::connect(pasteBtn, &QPushButton::clicked, [iconEdit]() {
        const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
        if (mime && mime->hasText()) {
            iconEdit->setText(mime->text().trimmed());
        }
    });
}

} // namespace

OpenWithSettingsWidget::OpenWithSettingsWidget(QWidget *parent) : QWidget(parent)
{
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto *container = new QWidget(scrollArea);
    auto *mainLayout = new QVBoxLayout(container);

    mainLayout->addWidget(buildSuffixSection());

    auto *catLabel = new QLabel(tr("Unified categories (first module = double-click default)"));
    catLabel->setWordWrap(true);
    mainLayout->addWidget(catLabel);
    mainLayout->addWidget(buildCategorySection(QStringLiteral("image")));
    mainLayout->addWidget(buildCategorySection(QStringLiteral("video")));
    mainLayout->addWidget(buildCategorySection(QStringLiteral("text")));
    mainLayout->addWidget(buildCategorySection(QStringLiteral("archive")));

    mainLayout->addStretch(1);
    scrollArea->setWidget(container);

    auto *outer = new QVBoxLayout(this);
#ifdef Q_OS_MAC
    auto *hint = new QLabel(tr("These handlers override system defaults when set. "
                               "Use %f or %F for the file path (QtFM placeholder, same on macOS). "
                               "On macOS use: /Applications/App.app %f , or open -a \"App Name\" %f . "
                               "A .app path alone also works; the file is added automatically."));
#else
    auto *hint = new QLabel(tr("These handlers override the \"Mime Types\" tab. "
                               "Use %f or %F for file path in commands."));
#endif
    hint->setWordWrap(true);
    outer->addWidget(hint);
    setStyleSheet(SettingsUiStyles::moduleStyleSheet());
    outer->addWidget(scrollArea, 1);
}

QWidget *OpenWithSettingsWidget::buildSuffixSection()
{
    auto *box = new QGroupBox(tr("Specific extensions"));
    auto *layout = new QVBoxLayout(box);

    auto *desc = new QLabel(tr("Each module applies to listed extensions (comma-separated), "
                                "e.g. pdf or glb,gltf. Higher priority than category rules."));
    desc->setWordWrap(true);
    layout->addWidget(desc);

    suffixModulesLayout = new QVBoxLayout();
    layout->addLayout(suffixModulesLayout);

    auto *addBtn = new QPushButton(tr("Add extension module"));
    SettingsUiStyles::styleAddButton(addBtn);
    connect(addBtn, &QPushButton::clicked, this, &OpenWithSettingsWidget::addSuffixModule);
    layout->addWidget(addBtn);
    return box;
}

QWidget *OpenWithSettingsWidget::buildCategorySection(const QString &categoryId)
{
    auto *outer = new QFrame(this);
    outer->setObjectName(QStringLiteral("settingsCategoryBox"));
    auto *layout = new QVBoxLayout(outer);

    auto *title = new QLabel(
        tr("Category: %1")
            .arg(categoryId == QLatin1String("image") ? tr("Image")
                 : categoryId == QLatin1String("video") ? tr("Video")
                 : categoryId == QLatin1String("text") ? tr("Text and code")
                 : categoryId == QLatin1String("archive") ? tr("Archive")
                 : categoryId));
    title->setToolTip(OpenWithConfig::suffixesForCategory(categoryId).join(QStringLiteral(", ")));
    title->setWordWrap(true);
    layout->addWidget(title);

    auto *modulesLayout = new QVBoxLayout();
    categoryLayouts.insert(categoryId, modulesLayout);
    layout->addLayout(modulesLayout);

    auto *addBtn = new QPushButton(tr("Add application module"));
    SettingsUiStyles::styleAddButton(addBtn);
    connect(addBtn, &QPushButton::clicked, this, [this, categoryId]() {
        OpenWithConfig::categoryModules(categoryId).append(OpenWithEntry());
        loadFromConfig();
    });
    layout->addWidget(addBtn);
    return outer;
}

void OpenWithSettingsWidget::loadFromConfig()
{
    while (QLayoutItem *item = suffixModulesLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    for (auto it = categoryLayouts.constBegin(); it != categoryLayouts.constEnd(); ++it) {
        QVBoxLayout *lay = it.value();
        while (QLayoutItem *item = lay->takeAt(0)) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
    }

    for (int i = 0; i < OpenWithConfig::suffixModules().size(); ++i) {
        SuffixOpenModule *mod = &OpenWithConfig::suffixModules()[i];
        auto *frame = makeModuleFrame(this);
        auto *vlay = new QVBoxLayout(frame);
        auto *form = new QFormLayout();
        auto *suffixEdit = new QLineEdit(mod->suffixesText);
        form->addRow(tr("Extensions"), suffixEdit);
        connect(suffixEdit, &QLineEdit::textChanged, [mod](const QString &t) {
            mod->suffixesText = t;
        });
        fillEntryForm(form, &mod->entry, QStringList());
        vlay->addLayout(form);
        const int moduleIndex = i;
        auto *del = new QPushButton(tr("Delete module"));
        SettingsUiStyles::styleDeleteButton(del);
        connect(del, &QPushButton::clicked, this, [this, moduleIndex]() {
            OpenWithConfig::suffixModules().removeAt(moduleIndex);
            loadFromConfig();
        });
        vlay->addWidget(del, 0, Qt::AlignRight);
        suffixModulesLayout->addWidget(frame);
    }

    for (const QString &cat : OpenWithConfig::categoryIds()) {
        QVBoxLayout *lay = categoryLayouts.value(cat);
        if (!lay) {
            continue;
        }
        QVector<OpenWithEntry> &list = OpenWithConfig::categoryModules(cat);
        for (int i = 0; i < list.size(); ++i) {
            auto *frame = makeModuleFrame(this);
            auto *vlay = new QVBoxLayout(frame);
            if (i == 0) {
                vlay->addWidget(new QLabel(tr("Default for double-click (first module)")));
            }
            auto *form = new QFormLayout();
            fillEntryForm(form, &list[i], OpenWithConfig::suffixesForCategory(cat));
            vlay->addLayout(form);
            const int moduleIndex = i;
            auto *del = new QPushButton(tr("Delete module"));
            SettingsUiStyles::styleDeleteButton(del);
            connect(del, &QPushButton::clicked, this, [this, cat, moduleIndex]() {
                OpenWithConfig::categoryModules(cat).removeAt(moduleIndex);
                loadFromConfig();
            });
            vlay->addWidget(del, 0, Qt::AlignRight);
            lay->addWidget(frame);
        }
    }
}

void OpenWithSettingsWidget::saveToConfig()
{
    // Data already live in OpenWithConfig vectors via signal edits
}

void OpenWithSettingsWidget::addSuffixModule()
{
    SuffixOpenModule mod;
    OpenWithConfig::suffixModules().append(mod);
    loadFromConfig();
}
