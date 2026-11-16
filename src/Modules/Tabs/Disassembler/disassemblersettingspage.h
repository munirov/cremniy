#ifndef DISASSEMBLERSETTINGSPAGE_H
#define DISASSEMBLERSETTINGSPAGE_H

#include "core/settings/settingspage.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QWidget;

class DisassemblerSettingsPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit DisassemblerSettingsPage(QWidget* parent = nullptr);

    void load() override;
    bool validate(QString* errorMessage) const override;
    void apply() override;

private:
    void updateUiState();
    void updateDependencyStatus();
    void browseObjdump();
    void browseRadare2();
    void testTools();

    QComboBox* m_backendCombo = nullptr;
    QSpinBox* m_insnLimit = nullptr;
    QComboBox* m_syntaxCombo = nullptr;
    QLineEdit* m_objdumpPath = nullptr;
    QLineEdit* m_radare2Path = nullptr;
    QLabel* m_objdumpStatus = nullptr;
    QLabel* m_radare2Status = nullptr;
    QLabel* m_fileStatus = nullptr;
    QComboBox* m_r2AnalysisCombo = nullptr;
    QPlainTextEdit* m_r2PreCommands = nullptr;
    QWidget* m_r2Options = nullptr;
};

#endif // DISASSEMBLERSETTINGSPAGE_H
