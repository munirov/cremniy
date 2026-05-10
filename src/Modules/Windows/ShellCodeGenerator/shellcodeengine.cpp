#include "shellcodeengine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

static constexpr int kProcessStartTimeoutMs = 3000;
static constexpr int kProcessFinishTimeoutMs = 5000;
static constexpr int kRawFallbackCols = 12;
static constexpr int kRawHexCols = 16;

ShellcodeEngine::ShellcodeEngine(QObject* parent) : QObject(parent) {}

void ShellcodeEngine::assemble(const QString& asmText, Architecture arch, OutputStyle style) {
    if (asmText.trimmed().isEmpty()) {
        emit finished(QString(), 0);
        return;
    }

    const QString bits = archToBits(arch);
    const QString tmpAsm = QDir::tempPath() + "/shellgen_input.asm";
    const QString tmpBin = QDir::tempPath() + "/shellgen_output.bin";

    {
        QFile f(tmpAsm);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit errorOccurred({{-1, tr("Failed to create temp ASM file.")}});
            return;
        }
        QTextStream(&f) << "BITS " << bits << "\n"
                        << asmText << "\n";
    }

    auto cleanup = [tmpAsm, tmpBin]() {
        QFile::remove(tmpAsm);
        QFile::remove(tmpBin);
    };

    auto* proc = new QProcess(this);

    connect(proc, &QProcess::finished, this,
            [=](int exitCode, QProcess::ExitStatus exitStatus) {
                const bool crashed = (exitStatus == QProcess::CrashExit || exitCode != 0);
                if (crashed) {
                    const QString raw = QString::fromUtf8(proc->readAllStandardError())
                                            .trimmed()
                                            .replace(tmpAsm, "<input>");
                    emit errorOccurred(parseErrors(raw));
                }
                else {
                    QFile binFile(tmpBin);
                    if (!binFile.open(QIODevice::ReadOnly)) {
                        emit errorOccurred({{-1, tr("Failed to read nasm output.")}});
                    }
                    else {
                        const QByteArray raw = binFile.readAll();
                        if (raw.isEmpty())
                            emit errorOccurred({{-1, tr("Assembled 0 bytes.")}});
                        else
                            emit finished(buildOutput(raw, bits, style), raw.size());
                    }
                }

                cleanup();
                proc->deleteLater();
            });

    proc->start(findTool("nasm"), {"-f", "bin", "-o", tmpBin, tmpAsm});

    if (!proc->waitForStarted(kProcessStartTimeoutMs)) {
        emit errorOccurred({{-1, tr("nasm not found. Ensure it is installed and in PATH.")}});
        cleanup();
        proc->deleteLater();
    }
}

QStringList ShellcodeEngine::checkDependencies() const {
    QStringList missing;
    for (const char* tool: {"nasm", "ndisasm"}) {
        if (!isToolAvailable(tool))
            missing << tool;
    }
    return missing;
}

QString ShellcodeEngine::findTool(const QString& name) {
    const QString sysPath = QStandardPaths::findExecutable(name);
    if (!sysPath.isEmpty())
        return sysPath;

    const QString candidates[] = {
        QString("C:/Program Files/NASM/%1.exe").arg(name),
        QString("C:/Program Files (x86)/NASM/%1.exe").arg(name),
        QString("C:/nasm/%1.exe").arg(name),
        QString("/opt/homebrew/bin/%1").arg(name),
        QString("/usr/local/bin/%1").arg(name),
        QString("/usr/bin/%1").arg(name),
    };
    for (const QString& p: candidates) {
        if (QFile::exists(p))
            return p;
    }
    return name;
}

bool ShellcodeEngine::isToolAvailable(const QString& name) {
    const QString path = findTool(name);
    return !QStandardPaths::findExecutable(QFileInfo(path).fileName()).isEmpty()
           || QFile::exists(path);
}

QList<ShellcodeEngine::AsmError> ShellcodeEngine::parseErrors(const QString& stderrText) {
    QList<AsmError> result;
    if (stderrText.isEmpty()) {
        result.append({-1, tr("nasm failed to assemble.")});
        return result;
    }

    // nasm format: "<input>:LINE: error: MESSAGE" or "<input>:LINE: warning: MESSAGE"
    static const QRegularExpression kErrRe(
        R"(<input>:(\d+):\s*(?:error|warning):\s*(.+))");

    bool anyParsed = false;
    for (const QString& line: stderrText.split('\n', Qt::SkipEmptyParts)) {
        const auto m = kErrRe.match(line);
        if (m.hasMatch()) {
            const int lineNum = m.captured(1).toInt();
            // Subtract 1 because we prepend "BITS XX\n" in assemble()
            result.append({lineNum - 1, m.captured(2).trimmed()});
            anyParsed = true;
        }
    }

    if (!anyParsed)
        result.append({-1, stderrText});

    return result;
}

QString ShellcodeEngine::buildOutput(const QByteArray& raw, const QString& bits, OutputStyle style) const {
    switch (style) {
        case OutputStyle::C:
            return generateC(raw, disassemble(raw, bits));
        case OutputStyle::Cpp:
            return generateCpp(raw, disassemble(raw, bits));
        case OutputStyle::Raw:
            return generateRaw(raw);
    }
    Q_UNREACHABLE();
}

QList<ShellcodeEngine::DisasmEntry> ShellcodeEngine::disassemble(
    const QByteArray& raw, const QString& bits) const {
    QList<DisasmEntry> result;

    const QString tmpBin = QDir::tempPath() + "/shellgen_disasm.bin";
    {
        QFile f(tmpBin);
        if (!f.open(QIODevice::WriteOnly))
            return result;
        f.write(raw);
    }

    QProcess proc;
    proc.start(findTool("ndisasm"), {"-b", bits, tmpBin});
    const bool ok = proc.waitForStarted(kProcessStartTimeoutMs)
                    && proc.waitForFinished(kProcessFinishTimeoutMs);
    QFile::remove(tmpBin);

    if (!ok)
        return result;

    static const QRegularExpression kWhitespace("\\s+");

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString& line: lines) {
        const QStringList parts = line.trimmed().split(kWhitespace, Qt::SkipEmptyParts);
        if (parts.size() < 3)
            continue;

        bool okHex = false;
        const int offset = parts[0].toInt(&okHex, 16);
        if (!okHex)
            continue;

        const QByteArray rawBytes = QByteArray::fromHex(parts[1].toLatin1());
        if (rawBytes.isEmpty())
            continue;

        result.append({offset, static_cast<int>(rawBytes.size()), parts.mid(2).join(' ').toLower()});
    }
    return result;
}

QString ShellcodeEngine::formatAnnotated(
    const QByteArray& raw, const QList<DisasmEntry>& entries) const {
    if (entries.isEmpty()) {
        QString out;
        out.reserve(raw.size() * 6);
        for (int i = 0; i < raw.size(); ++i) {
            if (i % kRawFallbackCols == 0)
                out += "    ";
            out += QString("0x%1").arg(static_cast<uint8_t>(raw[i]), 2, 16, QChar('0'));
            if (i + 1 < raw.size()) {
                out += ", ";
                if ((i + 1) % kRawFallbackCols == 0)
                    out += "\n";
            }
        }
        out += "\n";
        return out;
    }

    // 1: build byte strings and find max length for alignment
    QStringList byteStrings;
    byteStrings.reserve(entries.size());
    int maxLen = 0;

    const int lastIdx = entries.size() - 1;
    for (int i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        QString s;
        s.reserve(e.size * 6);
        for (int b = 0; b < e.size; ++b) {
            s += QString("0x%1").arg(static_cast<uint8_t>(raw[e.offset + b]), 2, 16, QChar('0'));
            if (i != lastIdx || b + 1 < e.size)
                s += ", ";
        }
        maxLen = qMax(maxLen, s.length());
        byteStrings.append(std::move(s));
    }

    // 2: emit padded lines with disasm comments
    QString out;
    out.reserve(entries.size() * (maxLen + 40));
    for (int i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        const QString& bs = byteStrings[i];

        const QString comment = (e.offset == 0)
                                    ? QString("// %1").arg(e.mnemonic)
                                    : QString("// %1 (+0x%2)").arg(e.mnemonic).arg(e.offset, 0, 16);

        out += "    ";
        out += bs;
        out += QString(maxLen - bs.length() + 2, ' ');
        out += comment;
        out += '\n';
    }
    return out;
}

QString ShellcodeEngine::generateC(const QByteArray& raw, const QList<DisasmEntry>& entries) const {
    return tr("unsigned char shellcode[] = {  // %1 bytes\n").arg(raw.size())
           + formatAnnotated(raw, entries)
           + "};\n";
}

QString ShellcodeEngine::generateCpp(const QByteArray& raw, const QList<DisasmEntry>& entries) const {
    return tr("std::array<std::uint8_t, %1> shellcode = {  // %1 bytes\n").arg(raw.size())
           + formatAnnotated(raw, entries)
           + "};\n";
}

QString ShellcodeEngine::generateRaw(const QByteArray& raw) const {
    QString out;
    out.reserve(raw.size() * 3);
    for (int i = 0; i < raw.size(); ++i) {
        out += QString("%1").arg(static_cast<uint8_t>(raw[i]), 2, 16, QChar('0'));
        if ((i + 1) % kRawHexCols == 0)
            out += '\n';
        else if (i + 1 < raw.size())
            out += ' ';
    }
    return out;
}

QString ShellcodeEngine::archToBits(Architecture arch) {
    switch (arch) {
        case Architecture::X86_16:
            return "16";
        case Architecture::X86_32:
            return "32";
        case Architecture::X86_64:
            return "64";
    }
    Q_UNREACHABLE();
}
