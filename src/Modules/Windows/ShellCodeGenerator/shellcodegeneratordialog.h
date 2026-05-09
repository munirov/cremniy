#pragma once

#include "core/modules/WindowBase.h"
#include "shellcodeengine.h"

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QListWidget;
class QSplitter;
class CustomCodeEditor;
class FileDataBuffer;

class ShellcodeGeneratorDialog : public WindowBase {
    Q_OBJECT

public:
    explicit ShellcodeGeneratorDialog(QWidget* parent = nullptr);
    ~ShellcodeGeneratorDialog() override;

private slots:
    void onAssemble();
    void onCopyOutput();
    void onClear();
    void onEngineFinished(const QString& output, int byteCount);
    void onEngineError(const QList<ShellcodeEngine::AsmError>& errors);
    void onErrorItemClicked(int row);

private:
    void setStatus(const QString& msg, bool isError = false);
    [[nodiscard]] bool checkDependencies();

    void setupToolbar(QVBoxLayout* root);
    void setupEditors(QVBoxLayout* root);
    void setupStatusBar(QVBoxLayout* root);
    void setupConnections(QTimer* debounce);

    void showErrorPanel(const QList<ShellcodeEngine::AsmError>& errors);
    void hideErrorPanel();

    CustomCodeEditor* m_asmInput = nullptr;
    CustomCodeEditor* m_shellcodeOutput = nullptr;
    QListWidget* m_errorList = nullptr;
    QWidget* m_errorPanel = nullptr;
    QSplitter* m_mainSplitter = nullptr;

    QComboBox* m_archCombo = nullptr;
    QComboBox* m_shellcodeStyle = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_byteCountLabel = nullptr;
    QLabel* m_archInfoLabel = nullptr;

    FileDataBuffer* m_asmBuffer = nullptr;
    FileDataBuffer* m_outputBuffer = nullptr;
    ShellcodeEngine* m_engine = nullptr;
};
