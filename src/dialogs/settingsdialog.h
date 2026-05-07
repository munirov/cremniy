#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QPlainTextEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onBrowseObjdump();
    void onBrowseRadare2();
    void onTestTools();
    void onExportIni();
    void onImportIni();
    void onAccept();
    void onBackendChanged(int index);

private:
    void loadFromSettings();
    void updateUiEnabledState();
    void updateDependencyStatus();

    QComboBox   *m_backendCombo = nullptr;
    QLineEdit   *m_objdumpPath  = nullptr;
    QLineEdit   *m_radare2Path  = nullptr;
    QLabel      *m_objdumpStatus = nullptr;
    QLabel      *m_radare2Status = nullptr;
    QLabel      *m_fileStatus    = nullptr;

    /* Disassembler options */
    QSpinBox   *m_insnLimit = nullptr;
    QComboBox  *m_syntaxCombo = nullptr;

    /* radare2 options */
    QComboBox     *m_r2AnalysisCombo = nullptr;
    QPlainTextEdit *m_r2PreCommands = nullptr;

    QPushButton *m_testBtn      = nullptr;
    QPushButton *m_exportBtn    = nullptr;
    QPushButton *m_importBtn    = nullptr;
    QPushButton *m_okBtn        = nullptr;
    QPushButton *m_cancelBtn    = nullptr;
};

#endif // SETTINGSDIALOG_H

