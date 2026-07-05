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
#include <QTabWidget>
#include <QTimer>
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
    setMinimumSize(QSize(1200, 700));
    resize(1400, 800);

    setupUi();

    // Debounce timer for auto-assembly
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

    // Shortcuts
    auto *assembleShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(assembleShortcut, &QShortcut::activated, this, &ShellcodeGeneratorDialog::onAssemble);

    // Check dependencies on startup
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
    root->addWidget(m_tabWidget, 1);

    // Status bar
    m_statusLabel = new QLabel(tr("Ready. Press F5 or start typing to assemble."), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #a1a1aa; font-size: 11px; padding: 2px 4px;"));
    root->addWidget(m_statusLabel);
}

void ShellcodeGeneratorDialog::setupToolbar(QWidget *parent) {
    m_toolbarLayout = new QHBoxLayout();
    m_toolbarLayout->setSpacing(8);

    m_toolbarLayout->addWidget(new QLabel(tr("Architecture:"), parent));
    m_archCombo = new QComboBox(parent);
    m_archCombo->setMinimumWidth(110);
    for (int i = 0; i < kArchCount; ++i)
        m_archCombo->addItem(kArchEntries[i].label);
    m_archCombo->setCurrentIndex(2); // default 64-bit
    m_toolbarLayout->addWidget(m_archCombo);

    m_toolbarLayout->addSpacing(10);
    m_toolbarLayout->addWidget(new QLabel(tr("Output:"), parent));
    m_shellcodeStyle = new QComboBox(parent);
    m_shellcodeStyle->setMinimumWidth(70);
    for (int i = 0; i < kStyleCount; ++i)
        m_shellcodeStyle->addItem(kStyles[i].label, kStyles[i].id);
    m_toolbarLayout->addWidget(m_shellcodeStyle);

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
    m_tabWidget = new QTabWidget(parent);

    // Assembly input tab
    m_asmBuffer = new FileDataBuffer(this);
    m_asmEditor = new CustomCodeEditor(parent);
    m_asmEditor->setBuffer(m_asmBuffer);
    m_asmEditor->setFileExt("asm");
    m_tabWidget->addTab(m_asmEditor, tr("Assembly"));

    // Shellcode output tab
    m_outputBuffer = new FileDataBuffer(this);
    m_outputEditor = new CustomCodeEditor(parent);
    m_outputEditor->setBuffer(m_outputBuffer);
    m_outputEditor->setFileExt("cpp");
    m_tabWidget->addTab(m_outputEditor, tr("Shellcode"));

    // Disassembly tab
    m_disasmBuffer = new FileDataBuffer(this);
    m_disasmEditor = new CustomCodeEditor(parent);
    m_disasmEditor->setBuffer(m_disasmBuffer);
    m_disasmEditor->setFileExt("asm");
    m_tabWidget->addTab(m_disasmEditor, tr("Disassembly"));

    m_tabWidget->setCurrentIndex(0);
}

int ShellcodeGeneratorDialog::currentBits() const {
    return kArchEntries[m_archCombo->currentIndex()].bits;
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

    // Generate shellcode output
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

    // Generate disassembly listing
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
    switch (m_tabWidget->currentIndex()) {
    case 0: activeBuffer = m_asmBuffer; break;
    case 1: activeBuffer = m_outputBuffer; break;
    case 2: activeBuffer = m_disasmBuffer; break;
    }

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
