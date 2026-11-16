#ifndef FILESSETTINGSPAGE_H
#define FILESSETTINGSPAGE_H

#include "core/settings/settingspage.h"

class QPlainTextEdit;

class FilesSettingsPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit FilesSettingsPage(QWidget* parent = nullptr);

    void load() override;
    bool validate(QString* errorMessage) const override;
    void apply() override;

private:
    QPlainTextEdit* m_excludedPatterns = nullptr;
};

#endif // FILESSETTINGSPAGE_H
