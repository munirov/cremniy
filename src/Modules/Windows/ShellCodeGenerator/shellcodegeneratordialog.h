#ifndef SHELLCODEGENERATORDIALOG_H
#define SHELLCODEGENERATORDIALOG_H

#include "core/modules/WindowBase.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>

class CustomCodeEditor;
class FileDataBuffer;
class QTimer;
class QToolButton;

class ShellcodeGeneratorDialog : public WindowBase {
    Q_OBJECT
public:
    explicit ShellcodeGeneratorDialog(QWidget *parent = nullptr);

private slots:
    void onAssemble();
    void onCopyActiveTab();
    void onClear();
    void togglePanel(int index);

private:
    void setupUi();
    void setupToolbar(QWidget *parent);
    void setupEditors(QWidget *parent);
    int currentBits() const;
    void setStatus(const QString &msg, bool error = false);

    QHBoxLayout *m_toolbarLayout = nullptr;
    QComboBox *m_archCombo = nullptr;
    QComboBox *m_shellcodeStyle = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_byteCountLabel = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;

    QSplitter *m_splitter = nullptr;
    QToolButton *m_toggleAsmBtn = nullptr;
    QToolButton *m_toggleShellBtn = nullptr;
    QToolButton *m_toggleDisasmBtn = nullptr;

    // Assembly input
    FileDataBuffer *m_asmBuffer = nullptr;
    CustomCodeEditor *m_asmEditor = nullptr;

    // Shellcode output
    FileDataBuffer *m_outputBuffer = nullptr;
    CustomCodeEditor *m_outputEditor = nullptr;

    // Disassembly output
    FileDataBuffer *m_disasmBuffer = nullptr;
    CustomCodeEditor *m_disasmEditor = nullptr;

    QTimer *m_debounceTimer = nullptr;

    QByteArray m_lastRawBinary;
};

#endif // SHELLCODEGENERATORDIALOG_H
