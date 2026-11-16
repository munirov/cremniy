#ifndef CODEEDITORSETTINGSPAGE_H
#define CODEEDITORSETTINGSPAGE_H

#include "core/settings/settingspage.h"

class QCheckBox;
class QComboBox;
class QSpinBox;
class QWidget;

class CodeEditorSettingsPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit CodeEditorSettingsPage(QWidget* parent = nullptr);

    void load() override;
    bool validate(QString* errorMessage) const override;
    void apply() override;

private:
    void chooseCustomColor(int index);

    QCheckBox* m_gitBlameEnabled = nullptr;
    QComboBox* m_gitBlameColor = nullptr;
    QSpinBox* m_gitBlamePadding = nullptr;
    QWidget* m_options = nullptr;
};

#endif // CODEEDITORSETTINGSPAGE_H
