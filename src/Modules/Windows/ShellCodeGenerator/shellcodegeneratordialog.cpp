#include "shellcodegeneratordialog.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "core/file/FileDataBuffer.h"
#include "core/modules/ModuleManager.h"
#include "widgets/CustomCodeEditor.h"

// --- Module registration ---

namespace {

    QString shellcodeGeneratorDisplayName() {
        return QCoreApplication::translate("ShellCodeGenerator", "Shell code");
    }

    void registerShellcodeGeneratorModule() {
        ModuleManager::instance().registerModule<WindowBase>(
            &shellcodeGeneratorDisplayName, "",
            [] {
                return new ShellcodeGeneratorDialog();
            });
    }

    const bool s_registered = (registerShellcodeGeneratorModule(), true);

    // --- Combo-box tables ---

    struct ArchEntry {
        const char* label;
        ShellcodeEngine::Architecture arch;
    };

    constexpr ArchEntry kArchEntries[] = {
        {"x86 (16-bit)", ShellcodeEngine::Architecture::X86_16},
        {"x86 (32-bit)", ShellcodeEngine::Architecture::X86_32},
        {"x86 (64-bit)", ShellcodeEngine::Architecture::X86_64},
    };

    struct StyleEntry {
        const char* label;
        ShellcodeEngine::OutputStyle style;
    };

    constexpr StyleEntry kStyleEntries[] = {
        {"C Array", ShellcodeEngine::OutputStyle::C},
        {"C++ Array", ShellcodeEngine::OutputStyle::Cpp},
        {"Raw Hex", ShellcodeEngine::OutputStyle::Raw},
    };

    constexpr int kDebounceMs = 500;
    constexpr int kToolbarHeight = 45;
    constexpr int kStatusBarHeight = 28;
    constexpr int kErrorPanelMinH = 80;
    constexpr int kErrorPanelMaxH = 200;

}// namespace

// --- Constructor / destructor ---

ShellcodeGeneratorDialog::ShellcodeGeneratorDialog(QWidget* parent)
    : WindowBase(parent), m_engine(new ShellcodeEngine(this)), m_asmBuffer(new FileDataBuffer(this)), m_outputBuffer(new FileDataBuffer(this)) {
    setWindowTitle(tr("Shellcode Generator"));
    setModal(false);
    setMinimumSize(1000, 600);
    resize(1200, 700);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    setupToolbar(root);
    setupEditors(root);
    setupStatusBar(root);

    auto* debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(kDebounceMs);
    setupConnections(debounce);

    QTimer::singleShot(0, this, [this] {
        if (!checkDependencies())
            close();
        else
            onAssemble();
    });
}

ShellcodeGeneratorDialog::~ShellcodeGeneratorDialog() = default;

// --- Setup helpers ---

void ShellcodeGeneratorDialog::setupToolbar(QVBoxLayout* root) {
    auto* container = new QWidget(this);
    container->setObjectName("shellcodeToolbar");
    container->setFixedHeight(kToolbarHeight);

    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(12);

    auto addLabel = [&](const QString& text) {
        auto* lbl = new QLabel(text, container);
        layout->addWidget(lbl);
    };

    addLabel(tr("Architecture:"));
    m_archCombo = new QComboBox(container);
    m_archCombo->setMinimumWidth(120);
    for (const auto& e: kArchEntries)
        m_archCombo->addItem(e.label, QVariant::fromValue(e.arch));
    m_archCombo->setCurrentIndex(2);
    layout->addWidget(m_archCombo);

    addLabel(tr("Output style:"));
    m_shellcodeStyle = new QComboBox(container);
    m_shellcodeStyle->setMinimumWidth(110);
    for (const auto& e: kStyleEntries)
        m_shellcodeStyle->addItem(tr(e.label), QVariant::fromValue(e.style));
    layout->addWidget(m_shellcodeStyle);

    layout->addStretch();

    m_clearBtn = new QPushButton(tr("Clear"), container);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_clearBtn);

    m_copyBtn = new QPushButton(tr("Copy Result"), container);
    m_copyBtn->setObjectName("copyResultBtn");
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_copyBtn);

    root->addWidget(container);
}

void ShellcodeGeneratorDialog::setupEditors(QVBoxLayout* root) {
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    m_mainSplitter->setHandleWidth(1);
    m_mainSplitter->setChildrenCollapsible(false);

    auto* editorContainer = new QWidget(this);
    auto* editorLayout = new QHBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(1);

    m_asmInput = new CustomCodeEditor(this);
    m_asmInput->setBuffer(m_asmBuffer);
    m_asmInput->setFileExt("asm");
    m_asmInput->setWordWrapEnabled(false);

    m_shellcodeOutput = new CustomCodeEditor(this);
    m_shellcodeOutput->setBuffer(m_outputBuffer);
    m_shellcodeOutput->setFileExt("cpp");
    m_shellcodeOutput->setWordWrapEnabled(true);

    editorLayout->addWidget(m_asmInput, 1);
    editorLayout->addWidget(m_shellcodeOutput, 1);

    // Error panel
    m_errorPanel = new QWidget(this);
    m_errorPanel->setObjectName("errorPanel");
    m_errorPanel->setMinimumHeight(kErrorPanelMinH);
    m_errorPanel->setMaximumHeight(kErrorPanelMaxH);
    m_errorPanel->setVisible(false);

    auto* errorPanelLayout = new QVBoxLayout(m_errorPanel);
    errorPanelLayout->setContentsMargins(0, 0, 0, 0);
    errorPanelLayout->setSpacing(0);

    auto* errorTitle = new QLabel(tr("Problems"), m_errorPanel);
    errorTitle->setObjectName("errorTitle");
    errorPanelLayout->addWidget(errorTitle);

    m_errorList = new QListWidget(m_errorPanel);
    m_errorList->setObjectName("errorList");
    m_errorList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_errorList->setFocusPolicy(Qt::NoFocus);
    m_errorList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    errorPanelLayout->addWidget(m_errorList);

    m_mainSplitter->addWidget(editorContainer);
    m_mainSplitter->addWidget(m_errorPanel);
    m_mainSplitter->setStretchFactor(0, 4);
    m_mainSplitter->setStretchFactor(1, 1);

    root->addWidget(m_mainSplitter, 1);
}

void ShellcodeGeneratorDialog::setupStatusBar(QVBoxLayout* root) {
    auto* bar = new QWidget(this);
    bar->setObjectName("shellcodeStatusBar");
    bar->setFixedHeight(kStatusBarHeight);

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 0, 10, 0);

    m_statusLabel = new QLabel(tr("Ready"), bar);
    m_statusLabel->setObjectName("statusLabel");
    layout->addWidget(m_statusLabel);

    layout->addStretch();

    m_archInfoLabel = new QLabel(bar);
    m_archInfoLabel->setObjectName("archInfoLabel");
    layout->addWidget(m_archInfoLabel);

    m_byteCountLabel = new QLabel(tr("0 bytes"), bar);
    m_byteCountLabel->setObjectName("byteCountLabel");
    layout->addWidget(m_byteCountLabel);

    root->addWidget(bar);
}

void ShellcodeGeneratorDialog::setupConnections(QTimer* debounce) {
    connect(m_asmBuffer, &FileDataBuffer::dataChanged,
            debounce, qOverload<>(&QTimer::start));
    connect(debounce, &QTimer::timeout,
            this, &ShellcodeGeneratorDialog::onAssemble);

    connect(m_copyBtn, &QPushButton::clicked, this, &ShellcodeGeneratorDialog::onCopyOutput);
    connect(m_clearBtn, &QPushButton::clicked, this, &ShellcodeGeneratorDialog::onClear);

    auto onComboChanged = [this](int) {
        if (m_outputBuffer->size() > 0)
            onAssemble();
    };
    connect(m_shellcodeStyle, qOverload<int>(&QComboBox::currentIndexChanged), this, onComboChanged);
    connect(m_archCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, onComboChanged);

    connect(m_engine, &ShellcodeEngine::finished,
            this, &ShellcodeGeneratorDialog::onEngineFinished);
    connect(m_engine, &ShellcodeEngine::errorOccurred,
            this, &ShellcodeGeneratorDialog::onEngineError);

    connect(m_errorList, &QListWidget::currentRowChanged,
            this, &ShellcodeGeneratorDialog::onErrorItemClicked);
}

// --- Error panel ---

void ShellcodeGeneratorDialog::showErrorPanel(const QList<ShellcodeEngine::AsmError>& errors) {
    m_errorList->clear();

    for (const auto& e: errors) {
        const QString text = (e.line > 0)
                                 ? QString("Line %1:  %2").arg(e.line).arg(e.message)
                                 : e.message;

        auto* item = new QListWidgetItem(QString("⚠  %1").arg(text), m_errorList);
        item->setData(Qt::UserRole, e.line);
        if (e.message.contains("warning", Qt::CaseInsensitive))
            item->setForeground(QColor("#e3b341"));
    }

    m_errorPanel->setVisible(true);
}

void ShellcodeGeneratorDialog::hideErrorPanel() {
    m_errorPanel->setVisible(false);
    m_errorList->clear();
}

// --- Slots ---

void ShellcodeGeneratorDialog::onAssemble() {
    const QString asmText = QString::fromUtf8(m_asmBuffer->data()).trimmed();
    if (asmText.isEmpty()) {
        m_outputBuffer->loadData({});
        m_byteCountLabel->setText(tr("0 bytes"));
        hideErrorPanel();
        setStatus(tr("Ready."));
        return;
    }

    setStatus(tr("Assembling…"));

    m_engine->assemble(
        asmText,
        m_archCombo->currentData().value<ShellcodeEngine::Architecture>(),
        m_shellcodeStyle->currentData().value<ShellcodeEngine::OutputStyle>());
}

void ShellcodeGeneratorDialog::onEngineFinished(const QString& output, int byteCount) {
    hideErrorPanel();
    m_outputBuffer->loadData(output.toUtf8());
    m_byteCountLabel->setText(tr("%1 bytes").arg(byteCount));
    m_archInfoLabel->setText(m_archCombo->currentText());
    setStatus(tr("Assembly successful."));
}

void ShellcodeGeneratorDialog::onEngineError(const QList<ShellcodeEngine::AsmError>& errors) {
    const QString brief = errors.isEmpty()
                              ? tr("Assembly failed.")
                              : (errors.first().line > 0
                                     ? tr("Line %1: %2").arg(errors.first().line).arg(errors.first().message)
                                     : errors.first().message);

    setStatus(brief, /*isError=*/true);
    showErrorPanel(errors);
}

void ShellcodeGeneratorDialog::onErrorItemClicked(int row) {
    if (row < 0 || !m_errorList->item(row))
        return;
    const int line = m_errorList->item(row)->data(Qt::UserRole).toInt();
    if (line > 0)
        m_asmInput->goToLine(line);
}

void ShellcodeGeneratorDialog::onCopyOutput() {
    const QByteArray data = m_outputBuffer->data();
    if (data.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(QString::fromUtf8(data));
    setStatus(tr("Copied to clipboard."));
}

void ShellcodeGeneratorDialog::onClear() {
    m_asmBuffer->loadData({});
    m_outputBuffer->loadData({});
    m_byteCountLabel->setText(tr("0 bytes"));
    m_archInfoLabel->clear();
    hideErrorPanel();
    setStatus(tr("Ready."));
}

void ShellcodeGeneratorDialog::setStatus(const QString& msg, bool isError) {
    m_statusLabel->setText(msg);
    m_statusLabel->setProperty("error", isError);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);

    if (isError)
        QApplication::beep();
}

bool ShellcodeGeneratorDialog::checkDependencies() {
    const QStringList missing = m_engine->checkDependencies();
    if (missing.isEmpty())
        return true;

    QMessageBox::warning(
        this,
        tr("Missing dependencies"),
        tr("The following tools were not found:\n\n - %1\n\n"
           "Install them or add their location to PATH.\n\n"
           "Download: https://www.nasm.us/pub/nasm/releasebuilds/")
            .arg(missing.join("\n - ")));
    return false;
}
