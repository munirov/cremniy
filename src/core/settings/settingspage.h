#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QString>
#include <QWidget>

class SettingsPage : public QWidget
{
public:
    explicit SettingsPage(QWidget* parent = nullptr) : QWidget(parent) {}
    ~SettingsPage() override = default;

    virtual void load() = 0;
    virtual bool validate(QString* errorMessage) const = 0;
    virtual void apply() = 0;
};

#endif // SETTINGSPAGE_H
