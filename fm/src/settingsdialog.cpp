#include "settingsdialog.h"
#include "mainwindow.h"
#include "fileutils.h"
#include "applicationdialog.h"
#include "properties.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QAction>
#include <QApplication>
#include <QTimer>

#include "openwithconfig.h"
#include "common.h"
#include "bundledicons.h"
#include "apptranslator.h"

namespace {

QIcon settingsPageIcon(const char *resourceName)
{
    const QIcon bundled = BundledIcons::settingsIcon(QLatin1String(resourceName));
    if (!bundled.isNull()) {
        return bundled;
    }
    return QIcon::fromTheme(QStringLiteral("preferences-system"));
}

} // namespace

/**
 * @brief Creates settings dialog
 * @param actionList
 * @param settings
 * @param mimeUtils
 * @param parent
 */
SettingsDialog::SettingsDialog(QList<QAction *> *actionList,
                               QSettings *settings,
                               MimeUtils *mimeUtils,
                               QWidget *parent) : QDialog(parent) {

  // Store pointer to custom action manager
  this->actionListPtr = actionList;
  this->settingsPtr = settings;
  this->mimeUtilsPtr = mimeUtils;

  // Main widgets of this dialog
  setWindowTitle(tr("Settings"));
  selector = new QListWidget(this);
  stack = new QStackedWidget(this);
  selector->setMaximumWidth(150);
  stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // Buttons
  dialogButtonBox = new QDialogButtonBox(this);
  dialogButtonBox->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  connect(dialogButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
  connect(dialogButtonBox, SIGNAL(rejected()), this, SLOT(reject()));

  // Size
  this->setMinimumWidth(640);
  this->setMinimumHeight(480);

  // Layouts
  QHBoxLayout* layoutMain = new QHBoxLayout(this);
  QVBoxLayout* layoutRight = new QVBoxLayout();
  layoutMain->addWidget(selector);
  layoutMain->addItem(layoutRight);
  layoutRight->addWidget(stack);
  layoutRight->addItem(new QSpacerItem(0, 10));
  layoutRight->addWidget(dialogButtonBox);

  // Icons (bundled SVGs in share/icons/settings/)
  const QIcon iconGeneral = settingsPageIcon("general");
  const QIcon iconAppearance = settingsPageIcon("appearance");
  const QIcon iconCustomActions = settingsPageIcon("custom-actions");
  const QIcon iconShortcuts = settingsPageIcon("shortcuts");
  const QIcon iconOpenWith = settingsPageIcon("open-with");
  const QIcon iconMimeTypes = settingsPageIcon("mime-types");
  const QIcon iconSystray = settingsPageIcon("systray");
  const QIcon iconAdvanced = settingsPageIcon("advanced");

  // Add widget with configurations
  selector->setMinimumWidth(160);
  selector->setViewMode(QListView::ListMode);
  selector->setIconSize(QSize(32, 32));
  selector->addItem(new QListWidgetItem(iconGeneral, tr("General"), selector));
  selector->addItem(new QListWidgetItem(iconAppearance, tr("Appearance"), selector));
  selector->addItem(new QListWidgetItem(iconCustomActions, tr("Custom Actions"), selector));
  selector->addItem(new QListWidgetItem(iconShortcuts, tr("Shortcuts"), selector));
  selector->addItem(new QListWidgetItem(iconOpenWith, tr("Open with"), selector));
#ifndef Q_OS_MAC
  selector->addItem(new QListWidgetItem(iconMimeTypes, tr("Mime Types"), selector));
  selector->addItem(new QListWidgetItem(iconSystray, tr("System Tray"), selector));
#endif
  selector->addItem(new QListWidgetItem(iconAdvanced, tr("Advanced"), selector));

  stack->addWidget(createGeneralSettings());
  stack->addWidget(createAppearanceSettings());
  stack->addWidget(createActionsSettings());
  stack->addWidget(createShortcutSettings());
  stack->addWidget(createOpenWithSettings());
#ifndef Q_OS_MAC
  stack->addWidget(createMimeSettings());
  stack->addWidget(createSystraySettings());
#endif
  stack->addWidget(createAdvSettings());

  connect(selector,
          SIGNAL(currentRowChanged(int)),
          stack,
          SLOT(setCurrentIndex(int)));
  connect(selector,
          SIGNAL(currentRowChanged(int)),
          SLOT(loadMimes(int)));

#if QT_VERSION >= 0x050000
  if (checkDarkTheme) {
      connect(checkDarkTheme, &QCheckBox::toggled, this, &SettingsDialog::updateDialogButtonIcons);
  }
#endif

  // Align items
  for (int i = 0; i < selector->count(); i++) {
    selector->item(i)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  }

  // Read settings
  QTimer::singleShot(100, this, SLOT(readSettings()));
}
//---------------------------------------------------------------------------

/**
 * @brief Creates widget with general settings
 * @return widget
 */
void SettingsDialog::previewDarkTheme(bool dark)
{
    Q_UNUSED(dark);
}

void SettingsDialog::updateDialogButtonIcons()
{
    if (!dialogButtonBox) {
        return;
    }
    bool darkForIcons = BundledIcons::uiDarkMode();
#if QT_VERSION >= 0x050000
    if (checkDarkTheme) {
        darkForIcons = checkDarkTheme->isChecked();
    }
#endif
    const bool prev = BundledIcons::uiDarkMode();
    BundledIcons::setUiDarkMode(darkForIcons);
    const QIcon saveIcon = BundledIcons::settingsIcon(QStringLiteral("save"));
    const QIcon cancelIcon = BundledIcons::settingsIcon(QStringLiteral("cancel"));
    BundledIcons::setUiDarkMode(prev);

    if (QPushButton *saveBtn = dialogButtonBox->button(QDialogButtonBox::Save)) {
        if (!saveIcon.isNull()) {
            saveBtn->setIcon(saveIcon);
        }
    }
    if (QPushButton *cancelBtn = dialogButtonBox->button(QDialogButtonBox::Cancel)) {
        if (!cancelIcon.isNull()) {
            cancelBtn->setIcon(cancelIcon);
        }
    }
}

QWidget *SettingsDialog::createGeneralSettings() {
  // Main widget and layout
  QWidget* widget = new QWidget();
  QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

  // Behaviour
  QGroupBox* grpBehav = new QGroupBox(tr("Behaviour"), widget);
  QFormLayout* layoutBehav = new QFormLayout(grpBehav);
  comboDAD = new QComboBox(grpBehav);
  comboDADctl = new QComboBox(grpBehav);
  comboDADshift = new QComboBox(grpBehav);
  comboDADalt = new QComboBox(grpBehav);
  QVector<QComboBox*> dads;
  dads.append(comboDAD);
  dads.append(comboDADalt);
  dads.append(comboDADctl);
  dads.append(comboDADshift);
  for (int i=0;i<dads.size();++i) {
      dads.at(i)->addItem(tr("Ask"),0);
      dads.at(i)->addItem(tr("Copy"),1);
      dads.at(i)->addItem(tr("Move"),2);
      dads.at(i)->addItem(tr("Link"),3);
  }
  layoutBehav->addRow(tr("Drag and Drop Default action: "), comboDAD);
  layoutBehav->addRow(tr("Drag and Drop CTRL action: "), comboDADctl);
  layoutBehav->addRow(tr("Drag and Drop SHIFT action: "), comboDADshift);
  layoutBehav->addRow(tr("Drag and Drop ALT action: "), comboDADalt);

  comboSingleClick = new QComboBox(grpBehav);
  comboSingleClick->addItem(tr("No"),0);
  comboSingleClick->addItem(tr("Directories only"),1);
  comboSingleClick->addItem(tr("Everything"),2);
  layoutBehav->addRow(tr("Enable Single Click"), comboSingleClick);

  checkPathHistory = new QCheckBox(grpBehav);
  layoutBehav->addRow(tr("Enable path history"), checkPathHistory);

  comboUiLanguage = new QComboBox(grpBehav);
  for (const QString &code : AppTranslator::availableLanguageCodes()) {
      comboUiLanguage->addItem(AppTranslator::languageDisplayName(code), code);
  }
  layoutBehav->addRow(tr("Language"), comboUiLanguage);

  // Confirmation
  QGroupBox* grpConfirm = new QGroupBox(tr("Confirmation"), widget);
  QFormLayout* layoutConfirm = new QFormLayout(grpConfirm);
  checkDelete = new QCheckBox(grpConfirm);
  layoutConfirm->addRow(tr("Ask before file is deleted: "), checkDelete);

  // Terminal emulator
  QGroupBox* grpTerm = new QGroupBox(tr("Terminal emulator"), widget);
  QFormLayout* layoutTerm = new QFormLayout(grpTerm);
  editTerm = new QLineEdit(grpTerm);
  layoutTerm->addRow(tr("Command: "), editTerm);

  // Layout of widget
  layoutWidget->addWidget(grpBehav);
  layoutWidget->addWidget(grpConfirm);
  layoutWidget->addWidget(grpTerm);
  layoutWidget->addSpacerItem(new QSpacerItem(0,
                                              0,
                                              QSizePolicy::Fixed,
                                              QSizePolicy::MinimumExpanding));
  return widget;
}

QWidget *SettingsDialog::createAppearanceSettings()
{
    // Main widget and layout
    QWidget* widget = new QWidget();
    QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

    // Appearance
    QGroupBox* grpAppear = new QGroupBox(tr("Appearance"), widget);
    QFormLayout* layoutAppear = new QFormLayout(grpAppear);
#if QT_VERSION >= 0x050000
    checkDarkTheme = new QCheckBox(grpAppear);
#endif
    checkWindowTitlePath = new QCheckBox(grpAppear);
    checkFileColor = new QCheckBox(grpAppear);
    showHomeButton = new QCheckBox(grpAppear);
    showNewTabButton = new QCheckBox(grpAppear);
    showTerminalButton = new QCheckBox(grpAppear);
  spinIconViewGapH = new QSpinBox(grpAppear);
  spinIconViewGapH->setRange(0, 48);
  spinIconViewGapH->setSuffix(tr(" px"));
  spinIconViewGapV = new QSpinBox(grpAppear);
  spinIconViewGapV->setRange(0, 48);
  spinIconViewGapV->setSuffix(tr(" px"));
  spinIconViewSize = new QSpinBox(grpAppear);
  spinIconViewSize->setRange(16, 256);
  spinIconViewSize->setSuffix(tr(" px"));
  spinListRowHeight = new QSpinBox(grpAppear);
  spinListRowHeight->setRange(18, 128);
  spinListRowHeight->setSuffix(tr(" px"));
  spinBookmarkGroupTabSize = new QSpinBox(grpAppear);
  spinBookmarkGroupTabSize->setRange(24, 128);
  spinBookmarkGroupTabSize->setSuffix(tr(" px"));
  checkFoldersAlwaysFirst = new QCheckBox(grpAppear);
  spinListColName = new QSpinBox(grpAppear);
  spinListColSize = new QSpinBox(grpAppear);
  spinListColDate = new QSpinBox(grpAppear);
  spinListColFormat = new QSpinBox(grpAppear);
  spinListColFolder = new QSpinBox(grpAppear);
  for (QSpinBox *spin : {spinListColName, spinListColSize, spinListColDate,
                         spinListColFormat, spinListColFolder}) {
      spin->setRange(40, 800);
      spin->setSuffix(tr(" px"));
  }

#if QT_VERSION >= 0x050000
    layoutAppear->addRow(tr("Use \"Dark Mode\""), checkDarkTheme);
#endif
    layoutAppear->addRow(tr("Colors on file names"), checkFileColor);
    layoutAppear->addRow(tr("Show path in window title"), checkWindowTitlePath);
    layoutAppear->addRow(tr("Show Home button"), showHomeButton);
    layoutAppear->addRow(tr("Show \"new tab\" button"), showNewTabButton);
    layoutAppear->addRow(tr("Show Terminal button"), showTerminalButton);
    layoutAppear->addRow(tr("Icon view horizontal gap"), spinIconViewGapH);
    layoutAppear->addRow(tr("Icon view vertical gap"), spinIconViewGapV);
    layoutAppear->addRow(tr("Icon view size"), spinIconViewSize);
    layoutAppear->addRow(tr("List row height"), spinListRowHeight);
    layoutAppear->addRow(tr("Bookmark group tab size"), spinBookmarkGroupTabSize);
    layoutAppear->addRow(tr("Folders always first (list)"), checkFoldersAlwaysFirst);
    layoutAppear->addRow(tr("List column: Name"), spinListColName);
    layoutAppear->addRow(tr("List column: Size"), spinListColSize);
    layoutAppear->addRow(tr("List column: Date"), spinListColDate);
    layoutAppear->addRow(tr("List column: Format"), spinListColFormat);
    layoutAppear->addRow(tr("List column: Folder"), spinListColFolder);

    // Layout widget
    layoutWidget->addWidget(grpAppear);
    layoutWidget->addSpacerItem(new QSpacerItem(0, 0,
                                                QSizePolicy::Fixed,
                                                QSizePolicy::MinimumExpanding));
    return widget;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates widget with custom actions settings
 * @return widget
 */
QWidget* SettingsDialog::createActionsSettings() {

  QWidget* widget = new QWidget(this);
  QVBoxLayout *outerLayout = new QVBoxLayout(widget);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  customActionsSettingsWidget = new CustomActionSettingsWidget(widget);
  connect(customActionsSettingsWidget, SIGNAL(entriesChanged()),
          this, SLOT(readShortcuts()));
  outerLayout->addWidget(customActionsSettingsWidget, 1);

  checkOutput = new QCheckBox(tr("Show dialog with action's output"), widget);
  outerLayout->addWidget(checkOutput);

  return widget;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates widget with shortcuts settings
 * @return widget
 */
QWidget* SettingsDialog::createShortcutSettings() {

  // Widget and its layout
  QWidget *widget = new QWidget();
  QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

  // Shortcuts group box
  QGroupBox* grpShortcuts = new QGroupBox(tr("Configure shortcuts"), widget);
  QVBoxLayout *layoutShortcuts = new QVBoxLayout(grpShortcuts);

  // Tree widget with list of shortcuts
  shortsWidget = new QTreeWidget(grpShortcuts);
  shortsWidget->setAlternatingRowColors(true);
  shortsWidget->setRootIsDecorated(false);
  shortsWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  QTreeWidgetItem *header = shortsWidget->headerItem();
  header->setText(0, tr("Action"));
  header->setText(1, tr("Shortcut"));
  shortsWidget->setColumnWidth(0, 220);
  layoutShortcuts->addWidget(shortsWidget);
  layoutWidget->addWidget(grpShortcuts);

  return widget;
}
//---------------------------------------------------------------------------

QWidget *SettingsDialog::createOpenWithSettings()
{
    openWithSettingsWidget = new OpenWithSettingsWidget(this);
    OpenWithConfig::load(settingsPtr);
    openWithSettingsWidget->loadFromConfig();
    return openWithSettingsWidget;
}

/**
 * @brief Creates widget with mime settings
 * @return widget
 */
QWidget* SettingsDialog::createMimeSettings() {

  // Widget and its layout
  QWidget *widget = new QWidget();
  QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

  // Shortcuts group box
  QGroupBox* grpMimes = new QGroupBox(tr("Mime types"), widget);
  QVBoxLayout *layoutMimes = new QVBoxLayout(grpMimes);

  // Editation of application list
  grpAssoc = new QGroupBox(tr("Applications"), widget);
  grpAssoc->setEnabled(false);
  QGridLayout* layoutAssoc = new QGridLayout(grpAssoc);
  listAssoc = new QListWidget(grpAssoc);
  QPushButton* btnAdd = new QPushButton(tr("Add.."), grpAssoc);
  QPushButton* btnRem = new QPushButton(tr("Remove"), grpAssoc);
  QPushButton* btnUp = new QPushButton(tr("Move up"), grpAssoc);
  QPushButton* btnDown = new QPushButton(tr("Move down"), grpAssoc);
  layoutAssoc->addWidget(listAssoc, 0, 0, 4, 1);
  layoutAssoc->addWidget(btnAdd, 0, 1);
  layoutAssoc->addWidget(btnRem, 1, 1);
  layoutAssoc->addWidget(btnUp, 2, 1);
  layoutAssoc->addWidget(btnDown, 3, 1);

  // tree filter
  QLineEdit *mimeSearch = new QLineEdit(grpMimes);
  mimeSearch->setPlaceholderText(tr("Filter ..."));
  mimeSearch->setClearButtonEnabled(true);
  connect(mimeSearch, SIGNAL(textChanged(QString)), this, SLOT(filterMimes(QString)));
  layoutMimes->addWidget(mimeSearch);

  // Tree widget with list of shortcuts
  mimesWidget = new QTreeWidget(grpMimes);
  mimesWidget->setAlternatingRowColors(true);
  mimesWidget->setRootIsDecorated(true);
  mimesWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  QTreeWidgetItem *header = mimesWidget->headerItem();
  header->setText(0, tr("Mime"));
  header->setText(1, tr("Application"));
  mimesWidget->setColumnWidth(0, 220);
  layoutMimes->addWidget(mimesWidget);

  // Default mime apps
  QGroupBox* grpDMime = new QGroupBox(tr("Default mime applications"), widget);
  QFormLayout* layoutDMime = new QFormLayout(grpDMime);
  cmbDefaultMimeApps = new QComboBox(grpDMime);
  cmbDefaultMimeApps->addItem("/.local/share/applications/mimeapps.list");
  cmbDefaultMimeApps->addItem("/.local/share/applications/defaults.list");
  cmbDefaultMimeApps->addItem("/.config/mimeapps.list");
  layoutDMime->addRow(tr("Configuration file: "), cmbDefaultMimeApps);

  layoutWidget->addWidget(grpMimes);
  layoutWidget->addWidget(grpAssoc);
  layoutWidget->addWidget(grpDMime);

  // Load application list
  /*QStringList apps = FileUtils::getApplicationNames();
  apps.replaceInStrings(".desktop", "");
  apps.sort();

  // Prepare source of icons
  QStringList iconFiles = Common::getPixmaps(qApp->applicationFilePath());
  //qDebug() << "icons" << iconFiles;
  QIcon defaultIcon = QIcon::fromTheme("application-x-executable");

  // Loads icon list
  QList<QIcon> icons;
  foreach (QString app, apps) {
    QApplication::processEvents();
    QPixmap temp = QIcon::fromTheme(app).pixmap(16, 16);
    if (!temp.isNull()) {
      icons.append(temp);
    } else {
      QStringList searchIcons = iconFiles.filter(app);
      if (searchIcons.count() > 0) {
        //qDebug() << "found icon" << searchIcons.at(0);
        icons.append(QIcon(searchIcons.at(0)));
      } else {
        icons.append(defaultIcon);
      }
    }
  }*/

  // Connect
  connect(mimesWidget,
          SIGNAL(currentItemChanged(QTreeWidgetItem*,
                                    QTreeWidgetItem*)),
          SLOT(onMimeSelected(QTreeWidgetItem*,
                              QTreeWidgetItem*)));
  connect(btnAdd,
          SIGNAL(clicked()),
          SLOT(showAppDialog()));
  connect(btnRem,
          SIGNAL(clicked()),
          SLOT(removeAppAssoc()));
  connect(btnUp,
          SIGNAL(clicked()),
          SLOT(moveAppAssocUp()));
  connect(btnDown,
          SIGNAL(clicked()),
          SLOT(moveAppAssocDown()));

  return widget;
}

QWidget *SettingsDialog::createSystraySettings()
{
    // Widget and its layout
    QWidget *widget = new QWidget();
    QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

    QGroupBox* trayGroup = new QGroupBox(tr("System Tray"), widget);
    QFormLayout* layoutTray = new QFormLayout(trayGroup);

    checkTrayNotify = new QCheckBox(trayGroup);
    layoutTray->addRow(tr("Show notifications"), checkTrayNotify);

    checkAutoMount = new QCheckBox(trayGroup);
    layoutTray->addRow(tr("Auto mount removable devices"), checkAutoMount);

    checkAudioCD = new QCheckBox(trayGroup);
    layoutTray->addRow(tr("Auto play audio CD's"), checkAudioCD);

    checkDVD = new QCheckBox(trayGroup);
    layoutTray->addRow(tr("Auto play audio/video DVD's"), checkDVD);

    layoutWidget->addWidget(trayGroup);

    return widget;
}

QWidget *SettingsDialog::createAdvSettings()
{
    // Widget and its layout
    QWidget *widget = new QWidget();
    QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

    // Custom Copy X of
    QGroupBox* grpCopyX = new QGroupBox(tr("Custom Copy of ..."), widget);
    QFormLayout* layoutCopyX = new QFormLayout(grpCopyX);
    editCopyX = new QLineEdit(grpCopyX);
    editCopyTS = new QLineEdit(grpCopyX);
    editCopyX->setToolTip(tr("Set a custom file name for 'Copy of ...'\n\n"
                             "%1 = num copy\n"
                             "%2 = orig filename (example.tar.gz)\n"
                             "%3 = timestamp (yyyyMMddHHmmss, set custom in 'Timestamp')\n"
                             "%4 = orig suffix (example.tar.gz=>tar.gz)\n"
                             "%5 = orig basename (example.tar.gz=>example)\n\n"
                             "Default is 'Copy (%1) of %2'"));
    editCopyTS->setToolTip(tr("Set a custom timestamp to use as %3.\n\n"
                              "See http://doc.qt.io/archives/qt-4.8/qdatetime.html#toString\n"
                              "for more information."));
    layoutCopyX->addRow(tr("Destination"), editCopyX);
    layoutCopyX->addRow(tr("Timestamp"), editCopyTS);

    layoutWidget->addWidget(grpCopyX);

    return widget;
}
//---------------------------------------------------------------------------

/**
 * @brief Updates default applications editor
 * @param current
 * @param previous
 */
void SettingsDialog::onMimeSelected(QTreeWidgetItem *current,
                                    QTreeWidgetItem *previous) {

  // Store previously used associations
  updateMimeAssoc(previous);

  // Clear previously used associations
  listAssoc->clear();

  // Check if current is editable
  if (current->childCount() > 0) {
    grpAssoc->setEnabled(false);
    return;
  }

  // Enable editation
  grpAssoc->setEnabled(true);

  // Prepare source of icons
  QIcon defaultIcon = QIcon::fromTheme("application-x-executable");

  QStringList apps = mimesWidget->currentItem()->text(1).remove(" ").split(";");
  foreach (QString app, apps) {

    // Skip empty string
    if (app.compare("") == 0) {
      continue;
    }

    // Finds icon
    QIcon temp = QIcon::fromTheme(app).pixmap(16, 16);
    if (temp.isNull()) {
        QString foundIcon = Common::findApplicationIcon(qApp->applicationFilePath(), QIcon::themeName(), app + ".desktop");
        if (!foundIcon.isEmpty()) {
            temp = QIcon(foundIcon);
        } else {
            temp = defaultIcon;
        }
    }

    // Add application
    listAssoc->addItem(new QListWidgetItem(temp, app, listAssoc));
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Updates mime association
 * @param item
 */
void SettingsDialog::updateMimeAssoc(QTreeWidgetItem* item) {
  if (item && item->childCount() == 0) {
    QStringList associations;
    for (int i = 0; i < listAssoc->count(); i++) {
      associations.append(listAssoc->item(i)->text());
    }
    item->setText(1, associations.join(";"));
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Displays application chooser dialog
 */
void SettingsDialog::showAppDialog() {

  // Choose application
  ApplicationDialog *dialog = new ApplicationDialog(false, this);
  if (dialog->exec()) {

    // If application name is empty, exit
    if (dialog->getCurrentLauncher().isEmpty()) {
      return;
    }

    // Retrieve launcher name
    QString name = dialog->getCurrentLauncher();

    // If application with same name is already used, exit
    for (int i = 0; i < listAssoc->count(); i++) {
      if (listAssoc->item(i)->text().compare(name) == 0) {
        return;
      }
    }

    // Add new launcher to the list of launchers
    if (dialog->getCurrentLauncher().compare("") != 0) {
      QIcon icon = QIcon::fromTheme(name).pixmap(16, 16);
      listAssoc->addItem(new QListWidgetItem(icon, name, listAssoc));
      updateMimeAssoc(mimesWidget->currentItem());
    }
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Removes association of application and mime type
 */
void SettingsDialog::removeAppAssoc() {
  qDeleteAll(listAssoc->selectedItems());
  updateMimeAssoc(mimesWidget->currentItem());
}
//---------------------------------------------------------------------------

/**
 * @brief Moves association up in list
 */
void SettingsDialog::moveAppAssocUp() {
  QListWidgetItem *current = listAssoc->currentItem();
  int currIndex = listAssoc->row(current);
  QListWidgetItem *prev = listAssoc->item(listAssoc->row(current) - 1);
  int prevIndex = listAssoc->row(prev);
  QListWidgetItem *temp = listAssoc->takeItem(prevIndex);
  listAssoc->insertItem(prevIndex, current);
  listAssoc->insertItem(currIndex, temp);
  updateMimeAssoc(mimesWidget->currentItem());
}
//---------------------------------------------------------------------------

/**
 * @brief Moves association down in list
 */
void SettingsDialog::moveAppAssocDown() {
  QListWidgetItem *current = listAssoc->currentItem();
  int currIndex = listAssoc->row(current);
  QListWidgetItem *next = listAssoc->item(listAssoc->row(current) + 1);
  int nextIndex = listAssoc->row(next);
  QListWidgetItem *temp = listAssoc->takeItem(nextIndex);
  listAssoc->insertItem(currIndex, temp);
  listAssoc->insertItem(nextIndex, current);
  updateMimeAssoc(mimesWidget->currentItem());
}

void SettingsDialog::restartToApply(int /*triggered*/)
{
    QMessageBox::information(this, tr("Settings information"), tr("You must re-start the application to apply this setting."));
}

void SettingsDialog::restartToApply(bool /*triggered*/)
{
    restartToApply(0);
}

void SettingsDialog::filterMimes(QString filter)
{
    mimesWidget->setUpdatesEnabled(false);
    for (int i=0;i<mimesWidget->topLevelItemCount();++i) {
        QTreeWidgetItem *topItem = mimesWidget->topLevelItem(i);
        if (!topItem) { continue; }
        for (int y=0;y<topItem->childCount();++y) {
            QTreeWidgetItem *item = topItem->child(y);
            if (!item) { continue; }
            if (item->text(0).contains(filter) || filter.isEmpty()) {
                item->setHidden(false);
            } else { item->setHidden(true); }
        }
    }
    if (filter.isEmpty()) { mimesWidget->collapseAll(); }
    else { mimesWidget->expandAll(); }
    mimesWidget->setUpdatesEnabled(true);
    mimesWidget->update();
}
//---------------------------------------------------------------------------

/**
 * @brief Reads settings
 */
void SettingsDialog::readSettings() {

  // Read general settings
  checkDelete->setChecked(settingsPtr->value("confirmDelete", true).toBool());
#ifdef Q_OS_MAC
  editTerm->setText(settingsPtr->value("term", "/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal").toString());
#else
  editTerm->setText(settingsPtr->value("term", "xterm").toString());
#endif
  editCopyX->setText(settingsPtr->value("copyXof", COPY_X_OF).toString());
  editCopyTS->setText(settingsPtr->value("copyXofTS", COPY_X_TS).toString());

  comboDAD->setCurrentIndex(settingsPtr->value("dad", 2).toInt());
  comboDADalt->setCurrentIndex(settingsPtr->value("dad_alt", 0).toInt());
  comboDADctl->setCurrentIndex(settingsPtr->value("dad_ctrl", 1).toInt());
  comboDADshift->setCurrentIndex(settingsPtr->value("dad_shift", 2).toInt());

  comboSingleClick->setCurrentIndex(settingsPtr->value("singleClick", 0).toInt());
  showHomeButton->setChecked(settingsPtr->value("home_button", true).toBool());
  showNewTabButton->setChecked(settingsPtr->value("newtab_button", false).toBool());
  showTerminalButton->setChecked(settingsPtr->value("terminal_button", true).toBool());
  const int legacyGap = settingsPtr->value("iconViewGap", 4).toInt();
  spinIconViewGapH->setValue(settingsPtr->value("iconViewGapH", legacyGap).toInt());
  spinIconViewGapV->setValue(settingsPtr->value("iconViewGapV", legacyGap).toInt());
  spinIconViewSize->setValue(settingsPtr->value("zoom", 48).toInt());
  spinListRowHeight->setValue(settingsPtr->value("zoomDetail", 24).toInt());
  spinBookmarkGroupTabSize->setValue(settingsPtr->value("bookmarkGroupTabSize", 40).toInt());
  checkFoldersAlwaysFirst->setChecked(settingsPtr->value("foldersAlwaysFirst", true).toBool());
  spinListColName->setValue(settingsPtr->value("listColumnWidth1", 220).toInt());
  spinListColSize->setValue(settingsPtr->value("listColumnWidth2", 90).toInt());
  spinListColDate->setValue(settingsPtr->value("listColumnWidth3", 130).toInt());
  spinListColFormat->setValue(settingsPtr->value("listColumnWidth4", 120).toInt());
  spinListColFolder->setValue(settingsPtr->value("listColumnWidth5", 80).toInt());
  OpenWithConfig::load(settingsPtr);
  if (openWithSettingsWidget) {
      openWithSettingsWidget->loadFromConfig();
  }
#if QT_VERSION >= 0x050000
#ifdef DEPLOY
  checkDarkTheme->blockSignals(true);
  checkDarkTheme->setChecked(settingsPtr->value("darkTheme", true).toBool());
  checkDarkTheme->blockSignals(false);
#else
  checkDarkTheme->blockSignals(true);
  checkDarkTheme->setChecked(settingsPtr->value("darkTheme", false).toBool());
  checkDarkTheme->blockSignals(false);
#endif
#endif
  checkFileColor->setChecked(settingsPtr->value("fileColor", false).toBool());
  checkPathHistory->setChecked(settingsPtr->value("pathHistory", true).toBool());
  const QString uiLang = AppTranslator::normalizedLanguageCode(
      settingsPtr->value(QStringLiteral("uiLanguage"), QStringLiteral("system")).toString());
  const int langIdx = comboUiLanguage->findData(uiLang);
  comboUiLanguage->setCurrentIndex(langIdx >= 0 ? langIdx : 0);
  checkWindowTitlePath->setChecked(settingsPtr->value("windowTitlePath", true).toBool());

#ifndef Q_OS_MAC
  checkTrayNotify->setChecked(settingsPtr->value("trayNotify", true).toBool());
  checkAudioCD->setChecked(settingsPtr->value("autoPlayAudioCD", false).toBool());
  checkAutoMount->setChecked(settingsPtr->value("trayAutoMount", false).toBool());
  checkDVD->setChecked(settingsPtr->value("autoPlayDVD", false).toBool());

  // Load default mime apps location
  QString tmp = settingsPtr->value("defMimeAppsFile", MIME_APPS).toString();
#if QT_VERSION >= 0x050000
  cmbDefaultMimeApps->setCurrentText(tmp);
#else
  cmbDefaultMimeApps->setEditText(tmp);
#endif
  mimeUtilsPtr->setDefaultsFileName(cmbDefaultMimeApps->currentText());

#endif

  // Read custom actions
  checkOutput->setChecked(settingsPtr->value("showActionOutput", true).toBool());
  if (customActionsSettingsWidget) {
      customActionsSettingsWidget->loadFromSettings(settingsPtr);
  }

  connect(comboSingleClick, SIGNAL(currentIndexChanged(int)), this, SLOT(restartToApply(int)));

  // Read shortcuts
  readShortcuts();
  updateDialogButtonIcons();
}
//---------------------------------------------------------------------------

/**
 * @brief Reads shortcuts
 */
void SettingsDialog::readShortcuts() {

  // Delete list of shortcuts
  shortsWidget->clear();

  // Icon configuration
  QPixmap pixmap(16, 16);
  pixmap.fill(Qt::transparent);
  QIcon blank(pixmap);

  // Read shortcuts
  QHash<QString, QString> shortcuts;
  settingsPtr->beginGroup("customShortcuts");
  QStringList keys = settingsPtr->childKeys();
  for (int i = 0; i < keys.count(); ++i) {
    QApplication::processEvents();
    QStringList temp(settingsPtr->value(keys.at(i)).toStringList());
    shortcuts.insert(temp.at(0), temp.at(1));
  }
  settingsPtr->endGroup();

  // Assign shortcuts to action and bookmarks
  for (int i = 0; i < actionListPtr->count(); ++i) {
    QApplication::processEvents();
    QAction* act = actionListPtr->at(i);
    QString text = shortcuts.value(act->text());
    text = text.isEmpty() ? text : QKeySequence::fromString(text).toString();
    QStringList list;
    list << act->text() << text;
    QTreeWidgetItem *item = new QTreeWidgetItem(shortsWidget, list);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    item->setIcon(0, act->icon());
    if (item->icon(0).isNull()) {
      item->setIcon(0, blank);
    }
  }

  // Assign shortcuts to custom actions
  if (customActionsSettingsWidget) {
    const QVector<CustomActionEntry> &customEntries = customActionsSettingsWidget->entries();
    for (int i = 0; i < customEntries.size(); i++) {
      QApplication::processEvents();
      const CustomActionEntry &entry = customEntries.at(i);
      QString text = shortcuts.value(entry.name);
      text = text.isEmpty() ? text : QKeySequence::fromString(text).toString();
      QStringList list;
      list << entry.name << text;
      QTreeWidgetItem *item = new QTreeWidgetItem(shortsWidget, list);
      item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
      QIcon actionIcon;
      if (!entry.iconPath.isEmpty()) {
          actionIcon = QIcon(entry.iconPath);
      }
      if (actionIcon.isNull()) {
          actionIcon = BundledIcons::iconByName(entry.bundledIconName);
      }
      item->setIcon(0, actionIcon);
      if (item->icon(0).isNull()) {
        item->setIcon(0, blank);
      }
    }
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Loads mime types
 * @param section
 */
void SettingsDialog::loadMimes(int section) {

#ifdef Q_OS_MAC
    return;
#endif

  // Mime progress section
  const int MIME_PROGRESS_SECTION = 5;

  // If section is not mime type configuration section exit
  if (section != MIME_PROGRESS_SECTION) {
    return;
  }

  // If mimes have been already loaded move to another section (mime config)
  if (mimesWidget->topLevelItemCount() > 0) {
    stack->setCurrentIndex(MIME_PROGRESS_SECTION /*+ 1*/);
    return;
  }

  // Load list of mimes
  QStringList mimes = mimeUtilsPtr->getMimeTypes();

  // Default icon
  QIcon defaultIcon = BundledIcons::iconByName(QStringLiteral("text"));

  // Mime categories and their icons
  QMap<QString, QTreeWidgetItem*> categories;
  QMap<QTreeWidgetItem*, QIcon> genericIcons;

  // Load mime settings
  foreach (QString mime, mimes) {

    QApplication::processEvents();

    // Skip all 'inode' nodes including 'inode/directory'
    if (mime.startsWith("inode")) {
      continue;
    }

    // Skip all 'x-content' and 'message' nodes
    if (/*mime.startsWith("x-content") ||*/ mime.startsWith("message")) {
      continue;
    }

    // Parse mime
    QStringList splitMime = mime.split("/");

    // Retrieve categories
    QIcon icon = defaultIcon;
    QString categoryName = splitMime.first();
    QTreeWidgetItem* category = categories.value(categoryName, nullptr);
    if (!category) {
      category = new QTreeWidgetItem(mimesWidget);
      category->setText(0, categoryName);
      category->setFlags(Qt::ItemIsEnabled);
      categories.insert(categoryName, category);
      genericIcons.insert(category, icon);
    }

    // Load icon and default application for current mime
    // NOTE: if icon is not found generic icon is used
    QString appNames = mimeUtilsPtr->getDefault(mime).join(";");

    // Create item from current mime
    QTreeWidgetItem *item = new QTreeWidgetItem(category);
    item->setIcon(0, icon);
    item->setText(0, splitMime.at(1));
    item->setText(1, appNames.remove(".desktop"));
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  }

  // Move to mimes
  stack->setCurrentIndex(MIME_PROGRESS_SECTION /*+ 1*/);
}
//---------------------------------------------------------------------------

/**
 * @brief Saves settings
 * @return true if successful
 */
bool SettingsDialog::saveSettings() {

  OpenWithConfig::save(settingsPtr);

  // General settings
  settingsPtr->setValue("confirmDelete", checkDelete->isChecked());
  settingsPtr->setValue("term", editTerm->text());
  settingsPtr->setValue("copyXof", editCopyX->text());
  settingsPtr->setValue("copyXofTS", editCopyTS->text());

  settingsPtr->setValue("dad", comboDAD->currentIndex());
  settingsPtr->setValue("dad_alt", comboDADalt->currentIndex());
  settingsPtr->setValue("dad_ctrl", comboDADctl->currentIndex());
  settingsPtr->setValue("dad_shift", comboDADshift->currentIndex());

  settingsPtr->setValue("singleClick", comboSingleClick->currentIndex());
  const QString newUiLang = comboUiLanguage->currentData().toString();
  if (AppTranslator::normalizedLanguageCode(newUiLang)
      != AppTranslator::normalizedLanguageCode(
          settingsPtr->value(QStringLiteral("uiLanguage"), QStringLiteral("system")).toString())) {
      QMessageBox::warning(this, tr("Restart to apply settings"),
                           tr("You must restart application to apply language settings."));
  }
  settingsPtr->setValue(QStringLiteral("uiLanguage"), newUiLang);
  settingsPtr->setValue("home_button", showHomeButton->isChecked());
  settingsPtr->setValue("newtab_button", showNewTabButton->isChecked());
  settingsPtr->setValue("terminal_button", showTerminalButton->isChecked());
  settingsPtr->setValue("iconViewGapH", spinIconViewGapH->value());
  settingsPtr->setValue("iconViewGapV", spinIconViewGapV->value());
  settingsPtr->setValue("iconViewGap", spinIconViewGapH->value());
  settingsPtr->setValue("zoom", spinIconViewSize->value());
  settingsPtr->setValue("zoomDetail", spinListRowHeight->value());
  settingsPtr->setValue("bookmarkGroupTabSize", spinBookmarkGroupTabSize->value());
  settingsPtr->setValue("foldersAlwaysFirst", checkFoldersAlwaysFirst->isChecked());
  settingsPtr->setValue("listColumnWidth1", spinListColName->value());
  settingsPtr->setValue("listColumnWidth2", spinListColSize->value());
  settingsPtr->setValue("listColumnWidth3", spinListColDate->value());
  settingsPtr->setValue("listColumnWidth4", spinListColFormat->value());
  settingsPtr->setValue("listColumnWidth5", spinListColFolder->value());
  settingsPtr->setValue("windowTitlePath", checkWindowTitlePath->isChecked());

#ifndef Q_OS_MAC
  settingsPtr->setValue("trayNotify", checkTrayNotify->isChecked());
  settingsPtr->setValue("autoPlayAudioCD", checkAudioCD->isChecked());
  settingsPtr->setValue("trayAutoMount", checkAutoMount->isChecked());
  settingsPtr->setValue("autoPlayDVD", checkDVD->isChecked());
  settingsPtr->setValue("defMimeAppsFile", cmbDefaultMimeApps->currentText());
#endif

#if QT_VERSION >= 0x050000
  settingsPtr->setValue("darkTheme", checkDarkTheme->isChecked());
#endif
  settingsPtr->setValue("fileColor", checkFileColor->isChecked());
  settingsPtr->setValue("pathHistory", checkPathHistory->isChecked());


  // Custom actions
  settingsPtr->setValue("showActionOutput", checkOutput->isChecked());
  if (customActionsSettingsWidget) {
      customActionsSettingsWidget->saveToSettings(settingsPtr);
  }

  // Shortcuts
  QStringList shortcuts, duplicates;
  settingsPtr->remove("customShortcuts");
  settingsPtr->beginGroup("customShortcuts");
  for (int i = 0; i < shortsWidget->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = shortsWidget->topLevelItem(i);
    if (!item->text(1).isEmpty()) {
      int existing = shortcuts.indexOf(item->text(1));
      if (existing != -1) {
        duplicates.append(QString("<b>%1</b> - %2").arg(shortcuts.at(existing))
                          .arg(item->text(0)));
      }
      shortcuts.append(item->text(1));
      QStringList temp;
      temp << item->text(0) << item->text(1);
      QString number = QString("%1").arg(shortcuts.count(), 4, 10, QChar('0'));
      settingsPtr->setValue(number, temp);
    }
  }
  settingsPtr->endGroup();

  // Mime types
#ifndef Q_OS_MAC
  for (int i = 0; i < mimesWidget->topLevelItemCount(); ++i) {
    QTreeWidgetItem* category = mimesWidget->topLevelItem(i);
    QString categoryName = category->text(0) + "/";
    for (int j = 0; j < category->childCount(); j++) {
      QString mime = categoryName + category->child(j)->text(0);
      QString appNames = category->child(j)->text(1);
      if (!appNames.isEmpty()) {
        QStringList temps = appNames.split(";");
        for (int i = 0; i < temps.size(); i++) {
          temps[i] = temps[i] + ".desktop";
        }
        mimeUtilsPtr->setDefault(mime, temps);
      }
    }
  }
  mimeUtilsPtr->saveDefaults();
#endif

  // Check for shortcuts duplicates
  if (duplicates.count()) {
    QString title = tr("Warning");
    QString msg = tr("Duplicate shortcuts detected:<p>%1");
    QMessageBox::information(this, title, msg.arg(duplicates.join("<p>")));
  }

  // Save succeeded
  return true;
}
//---------------------------------------------------------------------------

/**
 * @brief Accepts settings configuration
 */
void SettingsDialog::accept() {
  if (saveSettings()) this->done(1);
}
//---------------------------------------------------------------------------
