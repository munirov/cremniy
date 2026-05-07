#include "shellcodegeneratordialog.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include "core/modules/ModuleManager.h"

static QString displayName() {
    return QCoreApplication::translate("ShellCodeGenerator","Shell code");
}

static bool registered = []() {
    ModuleManager::instance().registerModule<WindowBase>(
        &displayName, "", []() { return new ShellcodeGeneratorDialog(); });
    return true;
}();

struct ArchEntry {
    const char *label;
    const char *bits;
};
static const ArchEntry kArchEntries[] = {
    {"x86 (16-bit)", "16"},
    {"x86 (32-bit)", "32"},
    {"x86 (64-bit)", "64"},
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

QString ShellcodeGeneratorDialog::findTool(const QString &name) {
    QString sysPath = QStandardPaths::findExecutable(name);
    if (!sysPath.isEmpty()) {
        return sysPath;
    }

    // for win, macOS, linux
    const QStringList candidates = {
        QString("C:/Program Files/NASM/%1.exe").arg(name),
        QString("C:/Program Files (x86)/NASM/%1.exe").arg(name),
        QString("C:/nasm/%1.exe").arg(name),
        QString("/opt/homebrew/bin/%1").arg(name),
        QString("/usr/local/bin/%1").arg(name),
        QString("/usr/bin/%1").arg(name),
    };

    for (const QString &p : candidates) {
        if (QFile::exists(p))
            return p;
    }

    return name;
}

ShellcodeGeneratorDialog::ShellcodeGeneratorDialog(QWidget *parent) : WindowBase(parent) {
    setWindowTitle(tr("Shellcode Generator"));
    setModal(false);
    setMinimumSize(QSize(1400, 760));

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(12, 12, 12, 12);

    // --- Toolbar ---
    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);
    toolbar->addWidget(new QLabel(tr("Architecture:"), this));
    m_archCombo = new QComboBox(this);
    m_archCombo->setMinimumWidth(100);
    for (int i = 0; i < kArchCount; ++i)
        m_archCombo->addItem(kArchEntries[i].label);
    m_archCombo->setCurrentIndex(2);
    toolbar->addWidget(m_archCombo);

    toolbar->addSpacing(10);
    toolbar->addWidget(new QLabel(tr("Output style:"), this));
    m_shellcodeStyle = new QComboBox(this);
    m_shellcodeStyle->setMinimumWidth(80);
    m_shellcodeStyle->setMinimumContentsLength(10);
	for (int i = 0; i < kStyleCount; ++i) {
    	m_shellcodeStyle->addItem(QString::fromUtf8(kStyles[i].label), kStyles[i].id);
	}
    toolbar->addWidget(m_shellcodeStyle);

    toolbar->addStretch(1);
    m_byteCountLabel = new QLabel(tr("0 bytes"), this);
    m_byteCountLabel->setStyleSheet("font-weight: bold;");
    toolbar->addWidget(m_byteCountLabel);

    m_copyBtn = new QPushButton(tr("Copy"), this);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    toolbar->addWidget(m_copyBtn);

    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    toolbar->addWidget(m_clearBtn);
    root->addLayout(toolbar);

    auto *editorsLayout = new QHBoxLayout();
    editorsLayout->setSpacing(8);

    m_asmInput = new QTextEdit(this);
    m_asmInput->setPlaceholderText(tr("; Enter x86/x64 assembly here..."));
    m_asmInput->setFont(monoFont);
    m_asmInput->setTabStopDistance(32);
    m_asmInput->setStyleSheet("background-color: #202020; color: #ececec; border: 1px solid #505050;");

    m_shellcodeOutput = new QTextEdit(this);
    m_shellcodeOutput->setPlaceholderText(tr("// Shellcode output will appear here..."));
    m_shellcodeOutput->setFont(monoFont);
    m_shellcodeOutput->setReadOnly(true);
    m_shellcodeOutput->setStyleSheet("background-color: #202020; color: #ececec; border: 1px solid #505050;");

    editorsLayout->addWidget(m_asmInput, 1);
    editorsLayout->addWidget(m_shellcodeOutput, 1);

    root->addLayout(editorsLayout, 1);

    m_statusLabel = new QLabel(tr("Ready. Make sure nasm and ndisasm are in PATH."), this);
    m_statusLabel->setStyleSheet("color: gray; font-size: 11px;");
    root->addWidget(m_statusLabel);

    auto *debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(500);

    connect(m_asmInput, &QTextEdit::textChanged, this, [=]() { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, &ShellcodeGeneratorDialog::onAssemble);
    connect(m_copyBtn, &QPushButton::clicked, this, &ShellcodeGeneratorDialog::onCopyOutput);
    connect(m_clearBtn, &QPushButton::clicked, this, &ShellcodeGeneratorDialog::onClear);

    auto triggerReassemble = [this](int) {
        if (!m_shellcodeOutput->toPlainText().isEmpty())
            onAssemble();
    };
    connect(m_shellcodeStyle, qOverload<int>(&QComboBox::currentIndexChanged), this, triggerReassemble);
    connect(m_archCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, triggerReassemble);

    QTimer::singleShot(0, this, [this]() {
        if (!checkDependencies())
            close();
    });
}

void ShellcodeGeneratorDialog::onAssemble() {
    const QString asmText = m_asmInput->toPlainText().trimmed();
    if (asmText.isEmpty()) {
        m_shellcodeOutput->clear();
        m_byteCountLabel->setText("0 bytes");
        setStatus(tr("Ready."));
        return;
    }

    const int archIdx = m_archCombo->currentIndex();
    const QString bits = kArchEntries[archIdx].bits;

    const QString tmpAsm = QDir::tempPath() + "/shellgen_input.asm";
    const QString tmpBin = QDir::tempPath() + "/shellgen_output.bin";

    {
        QFile f(tmpAsm);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            setStatus(tr("Failed to create temp file."), true);
            return;
        }
        QTextStream s(&f);
        s << "BITS " << bits << "\n" << asmText << "\n";
    }

    const QString nasmExe = findTool("nasm");
    QProcess proc;
    proc.start(nasmExe, {"-f", "bin", "-o", tmpBin, tmpAsm});

    if (!proc.waitForStarted(3000)) {
        setStatus(tr("nasm not found. Ensure it is installed and in PATH."), true);
        QFile::remove(tmpAsm);
        return;
    }
    proc.waitForFinished(5000);

    if (proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed().replace(tmpAsm, "<input>");
        setStatus("Error: " + err, true);
        QFile::remove(tmpAsm);
        QFile::remove(tmpBin);
        return;
    }

    QFile binFile(tmpBin);
    if (!binFile.open(QIODevice::ReadOnly)) {
        setStatus(tr("Failed to read nasm output."), true);
        QFile::remove(tmpAsm);
        return;
    }

    const QByteArray raw = binFile.readAll();
    binFile.close();
    QFile::remove(tmpAsm);
    QFile::remove(tmpBin);

    if (raw.isEmpty()) {
        setStatus(tr("Assembled 0 bytes."), true);
        return;
    }

    m_byteCountLabel->setText(tr("%1 bytes").arg(raw.size()));

    const int styleId = m_shellcodeStyle->currentData().toInt();
    QString output;

    switch (styleId) {
    case 0:
        output = generateC(raw, bits);
        break;
    case 1:
        output = generateCpp(raw, bits);
        break;
    case 2:
        output = generateRaw(raw);
        break;
    default:
        output = generateC(raw, bits);
    }

    m_shellcodeOutput->setPlainText(output);
}

void ShellcodeGeneratorDialog::onCopyOutput() {
    const QString text = m_shellcodeOutput->toPlainText();
    if (!text.isEmpty()) {
        QGuiApplication::clipboard()->setText(text);
        setStatus(tr("Copied to clipboard."));
    }
}

void ShellcodeGeneratorDialog::onClear() {
    m_asmInput->clear();
    m_shellcodeOutput->clear();
    m_byteCountLabel->setText("0 bytes");
    setStatus(tr("Ready."));
}

QList<ShellcodeGeneratorDialog::DisasmEntry> ShellcodeGeneratorDialog::disassemble(const QByteArray &raw, const QString &bits) const {
    QList<DisasmEntry> result;
    const QString ndisasmExe = findTool("ndisasm");

    const QString tmpBin = QDir::tempPath() + "/shellgen_disasm.bin";
    {
        QFile f(tmpBin);
        if (!f.open(QIODevice::WriteOnly))
            return result;
        f.write(raw);
    }

    QProcess proc;
    proc.start(ndisasmExe, {"-b", bits, tmpBin});

    if (!proc.waitForStarted(3000) || !proc.waitForFinished(5000)) {
        QFile::remove(tmpBin);
        return result;
    }

    QFile::remove(tmpBin);

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        const QStringList parts = line.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 3)
            continue;

        bool ok = false;
        const int offset = parts[0].toInt(&ok, 16);
        if (!ok)
            continue;

        const QByteArray rawBytes = QByteArray::fromHex(parts[1].toLatin1());
        if (rawBytes.isEmpty())
            continue;

        const QString mnemonic = parts.mid(2).join(' ').toLower();
        result.append({offset, static_cast<int>(rawBytes.size()), mnemonic});
    }
    return result;
}

QString ShellcodeGeneratorDialog::formatAnnotated(const QByteArray &raw, const QList<DisasmEntry> &entries) {
    if (!entries.isEmpty()) {
        int maxByteLength = 0;
        for (const auto &e : entries) {
            // calc max block width with bytes for alignment
            maxByteLength = qMax(maxByteLength, e.size * 6);
        }

        QString result;
        for (int i = 0; i < entries.size(); ++i) {
            const auto &e = entries[i];
            const bool isLastInstruction = (i + 1 == entries.size());

            QString bytePart;
            for (int b = 0; b < e.size; ++b) {
                const uint8_t byte = static_cast<uint8_t>(raw[e.offset + b]);
                bytePart += QString("0x%1").arg(byte, 2, 16, QChar('0'));

                if (!(isLastInstruction && b + 1 == e.size)) {
                    bytePart += ", ";
                }
            }

            QString comment = (e.offset == 0) ? QString("// %1").arg(e.mnemonic) : QString("// %1 (+0x%2)").arg(e.mnemonic).arg(e.offset, 0, 16);

            // dynamic cacl of spaces for alignment
            int paddingLen = maxByteLength - bytePart.length() + 2;
            if (paddingLen < 2)
                paddingLen = 2;

            result += "    " + bytePart + QString(paddingLen, ' ') + comment + "\n";
        }
        return result;
    }

    // FALLBACK: if NDISASM fails, print raw lines (12 bytes each)
    QString result;
    const int cols = 12;
    for (int i = 0; i < raw.size(); ++i) {
        if (i % cols == 0)
            result += "    ";
        result += QString("0x%1").arg(static_cast<uint8_t>(raw[i]), 2, 16, QChar('0'));

        if (i + 1 < raw.size()) {
            result += ", ";
        }
        if ((i + 1) % cols == 0 && i + 1 < raw.size()) {
            result += "\n";
        }
    }
    result += "\n";
    return result;
}

QString ShellcodeGeneratorDialog::generateC(const QByteArray &raw, const QString &bits) const {
    const auto entries = disassemble(raw, bits);
    QString s = tr("unsigned char shellcode[] = {  // %1 bytes\n").arg(raw.size());
    s += formatAnnotated(raw, entries);
    s += "};\n";
    return s;
}

QString ShellcodeGeneratorDialog::generateCpp(const QByteArray &raw, const QString &bits) const {
    const auto entries = disassemble(raw, bits);
    QString s = tr("std::array<std::uint8_t, %1> shellcode = {  // %1 bytes\n").arg(raw.size());
    s += formatAnnotated(raw, entries);
    s += "};\n";
    return s;
}

QString ShellcodeGeneratorDialog::generateRaw(const QByteArray &raw) const {
    QString s;
    for (int i = 0; i < raw.size(); ++i) {
        s += QString("%1").arg(static_cast<uint8_t>(raw[i]), 2, 16, QChar('0'));
        if ((i + 1) % 16 == 0)
            s += "\n";
        else if (i + 1 < raw.size())
            s += " ";
    }
    return s;
}

void ShellcodeGeneratorDialog::setStatus(const QString &msg, bool error) {
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(error ? "color: #dc3545; font-size: 11px;" : "color: gray; font-size: 11px;");

    if (error) {
        QApplication::beep();
    }
}

bool ShellcodeGeneratorDialog::checkDependencies() {
    const QString nasmPath = findTool("nasm");
    const QString ndisasmPath = findTool("ndisasm");

    auto isAvailable = [](const QString &path) { return !QStandardPaths::findExecutable(QFileInfo(path).fileName()).isEmpty() || QFile::exists(path); };

    QStringList missing;
    if (!isAvailable(nasmPath))
        missing << "nasm (assembler)";
    if (!isAvailable(ndisasmPath))
        missing << "ndisasm (disassembler)";

    if (!missing.isEmpty()) {
        QMessageBox::warning(this,
            tr("Missing dependencies"),
            tr("The following tools were not found on your system:\n\n"
            " - %1\n\n"
            "Please install them or add their location to PATH.\n\n"
            "Download: https://www.nasm.us/pub/nasm/releasebuilds/").arg(missing.join("\n - ")));
        return false;
    }
    return true;
}
