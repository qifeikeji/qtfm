#ifndef SETTINGSUISTYLES_H
#define SETTINGSUISTYLES_H

#include <QString>

class QPushButton;

namespace SettingsUiStyles {

QString moduleStyleSheet();
void styleAddButton(QPushButton *button);
void styleDeleteButton(QPushButton *button);

} // namespace SettingsUiStyles

#endif
