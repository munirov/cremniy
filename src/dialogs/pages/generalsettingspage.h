#ifndef GENERALSETTINGSPAGE_H
#define GENERALSETTINGSPAGE_H

#include "core/settings/settingspage.h"

class QComboBox;

class GeneralSettingsPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit GeneralSettingsPage(QWidget* parent = nullptr);

    void load() override;
    bool validate(QString* errorMessage) const override;
    void apply() override;

private:
    QComboBox* m_languageCombo = nullptr;
};

#endif // GENERALSETTINGSPAGE_H
