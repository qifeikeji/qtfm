#include "customactionsmanager.h"
#include "processdialog.h"
#include "bundledicons.h"
#include <QMessageBox>
#include <QMenu>
#include <QTimer>
#include <QDebug>
#include <QProcess>
#include <QFileInfo>
#include "common.h"

namespace {

QIcon iconForCustomAction(const QStringList &temp)
{
    QString bundledName;
    QString iconPath;
    if (temp.size() >= 5) {
        bundledName = temp.at(2);
        iconPath = temp.at(3);
    } else if (temp.size() >= 3) {
        bundledName = temp.at(2);
    }
    if (!iconPath.isEmpty()) {
        const QIcon fileIcon(iconPath);
        if (!fileIcon.isNull()) {
            return fileIcon;
        }
    }
    if (!bundledName.isEmpty()) {
        const QIcon bundled = BundledIcons::iconByName(bundledName);
        if (!bundled.isNull()) {
            return bundled;
        }
    }
    return BundledIcons::iconByName(QStringLiteral("empty"));
}

QString commandFromStored(const QStringList &temp)
{
    if (temp.size() >= 5) {
        return temp.at(4);
    }
    if (temp.size() >= 4) {
        return temp.at(3);
    }
    return QString();
}

} // namespace

/**
 * @brief Creates custom action manager
 * @param settings
 * @param actionList
 * @param parent
 */
CustomActionsManager::CustomActionsManager(QSettings* settings,
                                           QList<QAction*> *actionList,
                                           QObject *parent) : QObject(parent) {
  // Store pointers
  this->settingsPtr = settings;
  this->actionListPtr = actionList;

  // Create data
  this->actions = new QMultiHash<QString,QAction*>;
  this->menus = new QMultiHash<QString,QMenu*>;
  this->mapper = new QSignalMapper(this);
  connect(mapper, SIGNAL(mapped(QString)), SIGNAL(actionMapped(QString)));
}
//---------------------------------------------------------------------------

/**
 * @brief Removes all custom actions
 */
void CustomActionsManager::freeActions() {

  // Delete actions
  foreach (QAction *action, *actionListPtr) {
    if (actions->values().contains(action)) {
      actionListPtr->removeOne(action);
      delete action;
    }
  }

  // Delete entries from custom menus
  QList<QMenu*> temp = menus->values();
  foreach (QMenu* menu, temp) {
    delete menu;
  }

  // Clear lists
  actions->clear();
  menus->clear();
  emit actionsDeleted();
}
//---------------------------------------------------------------------------

/**
 * @brief Reads actions from QSettings
 */
void CustomActionsManager::readActions() {

    //qDebug() << "read actions";
  // Read keys
  settingsPtr->beginGroup("customActions");
  QStringList keys = settingsPtr->childKeys();

  // Read actions
  for (int i = 0; i < keys.count(); ++i) {

    // Add action key in reverse order
    keys.insert(i, keys.takeLast());

    // temp.at(0) - FileType
    // temp.at(1) - Text
    // temp.at(2) - Bundled icon name
    // temp.at(3) - Icon path (optional, v5+)
    // temp.at(4) - Command (optional | prefix); v4: command at(3)
    QStringList temp(settingsPtr->value(keys.at(i)).toStringList());
    //qDebug() << "loaded custom action" << temp;

    // Create new action and read it
    const QString cmd = commandFromStored(temp);
    QAction *act = new QAction(iconForCustomAction(temp), temp.at(1), this);
    mapper->setMapping(act, cmd);
    connect(act, SIGNAL(triggered()), mapper, SLOT(map()));
    actionListPtr->append(act);

    // Parse types which are connected with current action
    QStringList types = temp.at(0).split(",");
    foreach (QString type, types) {
      QStringList children(temp.at(1).split(" / "));
      if (children.count() > 1) {
        QMenu* parent = nullptr;
        act->setText(children.at(1));
        foreach (QMenu *subMenu, menus->values(type)) {
          if (subMenu->title() == children.at(0)) {
            parent = subMenu;
          }
        }
        if (parent == nullptr) {
          parent = new QMenu(children.at(0));
          menus->insert(type, parent);
        }
        parent->addAction(act);
        actions->insert("null", act);
      } else {
        actions->insert(type, act);
      }
    }
  }
  settingsPtr->endGroup();
  emit actionsLoaded();
}
//---------------------------------------------------------------------------

/**
 * @brief Returns custom actions
 * @return actions
 */
QMultiHash<QString, QAction*>* CustomActionsManager::getActions() const {
  return actions;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns custom menus
 * @return menus
 */
QMultiHash<QString, QMenu*>* CustomActionsManager::getMenus() const {
  return menus;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns action list pointer
 * @return
 */
QList<QAction*>* CustomActionsManager::getActionList() const {
  return actionListPtr;
}
//---------------------------------------------------------------------------

/**
 * @brief Executes action
 * @param cmd command
 * @param path path to file
 */
void CustomActionsManager::execAction(const QString &cmd, const QString &path) {

    //qDebug() << "custom action" << cmd << path;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList temp = QProcess::splitCommand(cmd);
#else
    QStringList temp = cmd.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
    if (temp.isEmpty()) {
        return;
    }
    QString exec = temp.takeAt(0);

  // Create new custom process
  QProcess *p = new QProcess();
  p->setWorkingDirectory(path);
  //p->setProcessChannelMode(QProcess::MergedChannels);

  // Create process dialog
  if (settingsPtr->value("showActionOutput", true).toBool()) {
    new ProcessDialog(p, exec, qobject_cast<QWidget*>(parent()));
  }

  // Connect process
  connect(p, SIGNAL(finished(int)), this, SLOT(onActionFinished(int)));
  connect(p, SIGNAL(error(QProcess::ProcessError)), this,
          SLOT(onActionError(QProcess::ProcessError)));

  // Execute process
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  if (exec.at(0) == '|') {
    exec.remove(0, 1);
    env.insert(APP, "1");
    p->setProcessEnvironment(env);
  }
  p->start(exec, temp);
}
//---------------------------------------------------------------------------

/**
 * @brief Displays error message
 * @param error
 */
void CustomActionsManager::onActionError(QProcess::ProcessError error) {
    Q_UNUSED(error)
  QProcess* process = qobject_cast<QProcess*>(sender());
  QMessageBox::warning(nullptr, "Error", process->errorString());
  onActionFinished(0);
}
//---------------------------------------------------------------------------

/**
 * @brief Triggered when action is finished
 * @param ret
 */
void CustomActionsManager::onActionFinished(int ret) {
    Q_UNUSED(ret)

  QProcess* process = qobject_cast<QProcess*>(sender());
  if (process->processEnvironment().contains(APP)) {
    QString output = process->readAllStandardError();
    if (!output.isEmpty()) {
      QMessageBox::warning(nullptr, tr("Error - Custom action"), output);
    }
    output = process->readAllStandardOutput();
    if (!output.isEmpty()) {
      QMessageBox::information(nullptr, tr("Output - Custom action"), output);
    }
  }

  // Updates file sizes
  QTimer::singleShot(100, this, SIGNAL(actionFinished()));
  process->deleteLater();
}

//---------------------------------------------------------------------------
