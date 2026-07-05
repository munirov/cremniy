#include "shellcodegeneratordialog.h"
#include "shellcodeengine.h"

#include "core/file/FileDataBuffer.h"
#include "core/modules/ModuleManager.h"
#include "widgets/CustomCodeEditor.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

static QString displayName() {
    return QCoreApplication::translate("ShellCodeGenerator", "Shell code");
}

static bool registered = []() {
    ModuleManager::instance().registerModule<WindowBase>(
        &displayName, "", []() { return new ShellcodeGeneratorDialog(); });
    return true;
}();

struct ArchEntry {
    const char *label;
    int bits;
};
static const ArchEntry kArchEntries[] = {
    {"x86 (16-bit)", 16},
    {"x86 (32-bit)", 32},
    {"x86 (64-bit)", 64},
};
static constexpr int kArchCount = std::size(kArchEntries);

struct StyleEntry {
    const char *label;
    int id;
};
static const StyleEntry kStyles[] = {
    {"C", 0},
    {"C++", 1},
    {"RAW", 2},
};
static constexpr int kStyleCount = std::size(kStyles);

ShellcodeGeneratorDialog::ShellcodeGeneratorDialog(QWidget *parent)
    : WindowBase(parent)
{
    setWindowTitle(tr("Shellcode Generator"));
    setModal(false);
    setMinimumSize(QSize(900, 600));
    resize(1200, 750);

    setupUi();

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(500);

    connect(m_debounceTimer, &QTimer::timeout, this, &ShellcodeGeneratorDialog::onAssemble);
    connect(m_asmEditor, &CustomCodeEditor::contentsChanged, this, [this]() {
        m_debounceTimer->start();
    });

    auto triggerReassemble = [this](int) {
        if (!m_lastRawBinary.isEmpty())
            onAssemble();
    };
    connect(m_shellcodeStyle, qOverload<int>(&QComboBox::currentIndexChanged), this, triggerReassemble);
    connect(m_archCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, triggerReassemble);

    auto *assembleShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(assembleShortcut, &QShortcut::activated, this, &ShellcodeGeneratorDialog::onAssemble);

    QTimer::singleShot(0, this, [this]() {
        QString missing;
        if (!ShellcodeEngine::checkDependencies(&missing)) {
            QMessageBox::warning(this,
                tr("Missing dependencies"),
                tr("The following tools were not found:\n\n - %1\n\n"
                   "Please install them or add to PATH.\n"
                   "Download: https://www.nasm.us/pub/nasm/releasebuilds/").arg(missing));
            close();
        }
    });
}

void ShellcodeGeneratorDialog::setupUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    setupToolbar(this);
    root->addLayout(m_toolbarLayout);

    setupEditors(this);
    root->addWidget(m_splitter, 1);

    m_statusLabel = new QLabel(tr("Ready. Press F5 or start typing to assemble."), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #a1a1aa; font-size: 11px; padding: 2px 4px;"));
    root->addWidget(m_statusLabel);
}

void ShellcodeGeneratorDialog::setupToolbar(QWidget *parent) {
    m_toolbarLayout = new QHBoxLayout();
    m_toolbarLayout->setSpacing(6);

    m_toolbarLayout->addWidget(new QLabel(tr("Architecture:"), parent));
    m_archCombo = new QComboBox(parent);
    m_archCombo->setMinimumWidth(110);
    for (int i = 0; i < kArchCount; ++i)
        m_archCombo->addItem(kArchEntries[i].label);
    m_archCombo->setCurrentIndex(2);
    m_toolbarLayout->addWidget(m_archCombo);

    m_toolbarLayout->addSpacing(8);
    m_toolbarLayout->addWidget(new QLabel(tr("Output:"), parent));
    m_shellcodeStyle = new QComboBox(parent);
    m_shellcodeStyle->setMinimumWidth(70);
    for (int i = 0; i < kStyleCount; ++i)
        m_shellcodeStyle->addItem(kStyles[i].label, kStyles[i].id);
    m_toolbarLayout->addWidget(m_shellcodeStyle);

    m_toolbarLayout->addSpacing(12);

    // Toggle buttons for panel visibility
    m_toggleAsmBtn = new QToolButton(parent);
    m_toggleAsmBtn->setText(tr("ASM"));
    m_toggleAsmBtn->setCheckable(true);
    m_toggleAsmBtn->setChecked(true);
    m_toggleAsmBtn->setToolTip(tr("Toggle Assembly panel"));
    m_toolbarLayout->addWidget(m_toggleAsmBtn);
    connect(m_toggleAsmBtn, &QToolButton::toggled, this, &ShellcodeGeneratorDialog::togglePanel);

    m_toggleShellBtn = new QToolButton(parent);
    m_toggleShellBtn->setText(tr("Shell"));
    m_toggleShellBtn->setCheckable(true);
    m_toggleShellBtn->setChecked(true);
    m_toggleShellBtn->setToolTip(tr("Toggle Shellcode panel"));
    m_toolbarLayout->addWidget(m_toggleShellBtn);
    connect(m_toggleShellBtn, &QToolButton::toggled, this, &ShellcodeGeneratorDialog::togglePanel);

    m_toggleDisasmBtn = new QToolButton(parent);
    m_toggleDisasmBtn->setText(tr("Disasm"));
    m_toggleDisasmBtn->setCheckable(true);
    m_toggleDisasmBtn->setChecked(true);
    m_toggleDisasmBtn->setToolTip(tr("Toggle Disassembly panel"));
    m_toolbarLayout->addWidget(m_toggleDisasmBtn);
    connect(m_toggleDisasmBtn, &QToolButton::toggled, this, &ShellcodeGeneratorDialog::togglePanel);

    m_toolbarLayout->addStretch(1);

    m_byteCountLabel = new QLabel(tr("0 bytes"), parent);
    m_byteCountLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
    m_toolbarLayout->addWidget(m_byteCountLabel);

    m_copyBtn = new QPushButton(tr("Copy"), parent);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_toolbarLayout->addWidget(m_copyBtn);
    connect(m_copyBtn, &QPushButton::clicked, this, &ShellcodeGeneratorDialog::onCopyActiveTab);

    m_clearBtn = new QPushButton(tr("Clear"), parent);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_toolbarLayout->addWidget(m_clearBtn);
    connect(m_clearBtn, &QPushButton::clicked, this, &ShellcodeGeneratorDialog::onClear);
}

void ShellcodeGeneratorDialog::setupEditors(QWidget *parent) {
    m_splitter = new QSplitter(Qt::Horizontal, parent);

    // Assembly input
    m_asmBuffer = new FileDataBuffer(this);
    m_asmEditor = new CustomCodeEditor(parent);
    m_asmEditor->setBuffer(m_asmBuffer);
    m_asmEditor->setFileExt("asm");
    m_splitter->addWidget(m_asmEditor);

    // Shellcode output
    m_outputBuffer = new FileDataBuffer(this);
    m_outputEditor = new CustomCodeEditor(parent);
    m_outputEditor->setBuffer(m_outputBuffer);
    m_outputEditor->setFileExt("cpp");
    m_splitter->addWidget(m_outputEditor);

    // Disassembly output
    m_disasmBuffer = new FileDataBuffer(this);
    m_disasmEditor = new CustomCodeEditor(parent);
    m_disasmEditor->setBuffer(m_disasmBuffer);
    m_disasmEditor->setFileExt("asm");
    m_splitter->addWidget(m_disasmEditor);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 1);
}

int ShellcodeGeneratorDialog::currentBits() const {
    return kArchEntries[m_archCombo->currentIndex()].bits;
}

void ShellcodeGeneratorDialog::togglePanel(int) {
    m_asmEditor->setVisible(m_toggleAsmBtn->isChecked());
    m_outputEditor->setVisible(m_toggleShellBtn->isChecked());
    m_disasmEditor->setVisible(m_toggleDisasmBtn->isChecked());
}

void ShellcodeGeneratorDialog::onAssemble() {
    const QString asmText = QString::fromUtf8(m_asmBuffer->data()).trimmed();

    if (asmText.isEmpty()) {
        m_outputBuffer->loadData(QByteArray());
        m_disasmBuffer->loadData(QByteArray());
        m_lastRawBinary.clear();
        m_byteCountLabel->setText(tr("0 bytes"));
        setStatus(tr("Ready."));
        return;
    }

    ShellcodeEngine engine;
    const int bits = currentBits();
    auto result = engine.assemble(asmText, bits);

    if (!result.error.isEmpty()) {
        m_outputBuffer->loadData(result.error.toUtf8());
        m_disasmBuffer->loadData(QByteArray());
        m_lastRawBinary.clear();
        m_byteCountLabel->setText(tr("0 bytes"));
        setStatus("Error: " + result.error, true);
        return;
    }

    if (result.binary.isEmpty()) {
        m_outputBuffer->loadData(QByteArray());
        m_disasmBuffer->loadData(QByteArray());
        m_lastRawBinary.clear();
        m_byteCountLabel->setText(tr("0 bytes"));
        setStatus(tr("Assembled 0 bytes."), true);
        return;
    }

    m_lastRawBinary = result.binary;
    m_byteCountLabel->setText(tr("%1 bytes").arg(result.binary.size()));

    const auto lines = engine.disassemble(result.binary, bits);
    const int styleId = m_shellcodeStyle->currentData().toInt();

    QString output;
    switch (styleId) {
    case 0: output = ShellcodeEngine::formatC(result.binary, lines); break;
    case 1: output = ShellcodeEngine::formatCpp(result.binary, lines); break;
    case 2: output = ShellcodeEngine::formatRaw(result.binary); break;
    default: output = ShellcodeEngine::formatC(result.binary, lines); break;
    }

    m_outputBuffer->loadData(output.toUtf8());

    QString disasmText;
    for (const auto &l : lines) {
        disasmText += QStringLiteral("%1  %2  %3\n")
            .arg(l.offset, 8, 16, QChar('0'))
            .arg(QString::fromLatin1(l.hexBytes.toHex(' ')).leftJustified(24))
            .arg(l.mnemonic);
    }
    m_disasmBuffer->loadData(disasmText.toUtf8());

    setStatus(tr("Assembled %1 bytes successfully.").arg(result.binary.size()));
}

void ShellcodeGeneratorDialog::onCopyActiveTab() {
    FileDataBuffer *activeBuffer = nullptr;

    if (m_outputEditor->isVisible())
        activeBuffer = m_outputBuffer;
    else if (m_disasmEditor->isVisible())
        activeBuffer = m_disasmBuffer;
    else
        activeBuffer = m_asmBuffer;

    if (activeBuffer) {
        const QByteArray data = activeBuffer->data();
        if (!data.isEmpty()) {
            QGuiApplication::clipboard()->setText(QString::fromUtf8(data));
            setStatus(tr("Copied to clipboard."));
        }
    }
}

void ShellcodeGeneratorDialog::onClear() {
    m_asmBuffer->loadData(QByteArray());
    m_outputBuffer->loadData(QByteArray());
    m_disasmBuffer->loadData(QByteArray());
    m_lastRawBinary.clear();
    m_byteCountLabel->setText(tr("0 bytes"));
    setStatus(tr("Ready."));
}

void ShellcodeGeneratorDialog::setStatus(const QString &msg, bool error) {
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(error
        ? QStringLiteral("color: #dc3545; font-size: 11px; padding: 2px 4px;")
        : QStringLiteral("color: #a1a1aa; font-size: 11px; padding: 2px 4px;"));
    if (error)
        QApplication::beep();
}
