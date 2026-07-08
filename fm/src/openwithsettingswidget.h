#ifndef OPENWITHSETTINGSWIDGET_H
#define OPENWITHSETTINGSWIDGET_H

#include <QHash>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class QScrollArea;

class OpenWithSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit OpenWithSettingsWidget(QWidget *parent = nullptr);

    void loadFromConfig();
    void saveToConfig();

private slots:
    void addSuffixModule();

private:
    QWidget *buildSuffixSection();
    QWidget *buildCategorySection(const QString &categoryId);

    QScrollArea *scrollArea;
    QVBoxLayout *suffixModulesLayout;
    QHash<QString, QVBoxLayout *> categoryLayouts;
};

#endif
