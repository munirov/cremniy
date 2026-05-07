#include "disassemblerworker.h"
#include "utils/appsettings.h"
#include "disasm/backends/radare2backend.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
struct ObjdumpSectionHeader {
    QString name;
    quint64 size = 0;
    quint64 vma = 0;
    quint64 fileOffset = 0;
    bool valid = false;
};

static QHash<QString, DisasmSection> parseObjdumpSectionHeaders(const QString &objdumpExe,
                                                                const QString &filePath,
                                                                const QProcessEnvironment &env)
{
    QHash<QString, DisasmSection> sections;

    QProcess proc;
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(objdumpExe, {"-h", filePath});
    if (!proc.waitForStarted(5000))
        return sections;
    if (!proc.waitForFinished(10000) || proc.exitCode() != 0)
        return sections;

    static const QRegularExpression re(
        R"(^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+)");

    const QStringList lines = QString::fromUtf8(proc.readAllStandardOutput()).split('\n');
    for (const QString &line : lines) {
        const auto match = re.match(line);
        if (!match.hasMatch())
            continue;

        bool okSize = false;
        bool okVma = false;
        bool okOff = false;
        const quint64 size = match.captured(2).toULongLong(&okSize, 16);
        const quint64 vma = match.captured(3).toULongLong(&okVma, 16);
        const quint64 off = match.captured(4).toULongLong(&okOff, 16);
        if (!okSize || !okVma || !okOff)
            continue;

        DisasmSection sec;
        sec.name = match.captured(1);
        sec.vaddr = vma;
        sec.fileOffset = off;
        sec.size = size;
        sec.hasFileMapping = true;
        sections.insert(sec.name, sec);
    }

    return sections;
}
} // namespace

DisassemblerWorker::DisassemblerWorker(QObject *parent)
    : QObject{parent}
{}

void DisassemblerWorker::cancel()
{
    m_cancelled = true;
}

QString DisassemblerWorker::detectArch(const QString &filePath)
{
    QProcess fileProc;
    fileProc.start("file", {"-b", filePath});
    if (!fileProc.waitForFinished(3000))
        return {};

    QString desc = QString::fromUtf8(fileProc.readAllStandardOutput()).toLower();

    if (desc.contains("x86-64") || desc.contains("x86_64") || desc.contains("amd64"))
        return "i386:x86-64";
    if (desc.contains("80386") || desc.contains("ia-32"))
        return "i386";
    if (desc.contains("aarch64") || desc.contains("arm64"))
        return "aarch64";
    if (desc.contains("arm"))
        return "arm";
    if (desc.contains("mips") && desc.contains("64"))
        return "mips:isa64r2";
    if (desc.contains("mips"))
        return "mips:3000";
    if (desc.contains("riscv") && desc.contains("64"))
        return "riscv:rv64";
    if (desc.contains("riscv"))
        return "riscv:rv32";

    return {};
}

void DisassemblerWorker::disassemble(const QString &filePath, const QString &arch)
{
    m_cancelled = false;

  /*  ── backend selection ─────────────────────────────────────────────────*/
    if (AppSettings::disasmBackend() == AppSettings::DisasmBackend::Radare2) {
        QString r2 = AppSettings::radare2Path();
        if (r2.isEmpty())
            r2 = QStandardPaths::findExecutable("r2");

        emit logLine("[disasm] backend   : radare2");
        emit logLine("[disasm] r2        : " + (r2.isEmpty() ? QString("(not found)") : r2));

        if (r2.isEmpty()) {
            emit errorOccurred(tr("radare2 (r2) not found. Set its path in Settings."));
            emit finished();
            return;
        }

        Radare2Backend::Options opt;
        opt.insnLimitPerSection = AppSettings::disasmInsnLimitPerSection();
        opt.analysisLevel = static_cast<int>(AppSettings::radare2AnalysisLevel());
        opt.asmSyntax = static_cast<int>(AppSettings::asmSyntax());
        opt.preCommands = AppSettings::radare2PreCommands();

        emit logLine(QString("[disasm] insnLimit : %1").arg(opt.insnLimitPerSection));
        emit logLine(QString("[disasm] r2Analyze : %1").arg(opt.analysisLevel == 2 ? "aaa" : (opt.analysisLevel == 1 ? "aa" : "none")));
        emit logLine(QString("[disasm] syntax    : %1").arg(opt.asmSyntax == 1 ? "att" : "intel"));

        auto result = Radare2Backend::disassembleFile(r2, filePath, opt, &m_cancelled);
        if (!result.error.isEmpty()) {
            emit errorOccurred(result.error);
            emit finished();
            return;
        }

        if (!result.functions.isEmpty())
            emit functionsFound(result.functions);
        if (!result.strings.isEmpty())
            emit stringsFound(result.strings);

        emit logLine(QString("[disasm] sections parsed: %1").arg(result.sections.size()));
        for (const auto &s : result.sections)
            emit logLine(QString("[disasm]   section '%1': %2 instructions")
                             .arg(s.name).arg(s.instructions.size()));

        const int total = result.sections.size();
        for (int i = 0; i < total; ++i) {
            if (m_cancelled) break;
            emit sectionFound(result.sections[i]);
            emit progressUpdated(total == 0 ? 100 : (i + 1) * 100 / total);
        }

        emit finished();
        return;
    }

    emit logLine("[disasm] backend   : objdump");

   /* ── arch detection ────────────────────────────────────────────────────*/
    QString effectiveArch = arch.isEmpty() ? detectArch(filePath) : arch;

    if (effectiveArch.isEmpty()) {
        effectiveArch = "i386:x86-64";
    }

    emit logLine("[disasm] file      : " + filePath);
    emit logLine("[disasm] arch-hint : " + (effectiveArch.isEmpty() ? "(auto)" : effectiveArch));

    /*── build objdump command ─────────────────────────────────────────────*/
    QString objdumpExe = AppSettings::objdumpPath();
    if (objdumpExe.isEmpty())
        objdumpExe = QStandardPaths::findExecutable("objdump");
    if (objdumpExe.isEmpty())
        objdumpExe = "objdump";

    QStringList args;
    args << "-d";
    if (filePath.endsWith(".bin", Qt::CaseInsensitive)) {
        args << "-D" << "-b" << "binary" << "-m" << effectiveArch;
    } else {
        args << "-d";
        if (!effectiveArch.isEmpty()) args << "-m" << effectiveArch;
    }
   if (effectiveArch.contains("i386") || effectiveArch.contains("x86")) {
        args << "-M" << (AppSettings::asmSyntax() == AppSettings::AsmSyntax::Att ? "att" : "intel");
    }
    args << filePath;

    emit logLine("[disasm] objdump   : " + objdumpExe);
    emit logLine("[disasm] command   : " + objdumpExe + " " + args.join(' '));

    /*── launch process ────────────────────────────────────────────────────
    Force C locale: ensures English section headers ("Disassembly of section")
    and standard tab-separated format regardless of system locale. */
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LANG",   "C");
    env.insert("LC_ALL", "C");
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(objdumpExe, args);

    if (!proc.waitForStarted(5000)) {
        emit logLine("[disasm] ERROR: failed to start objdump");
        emit errorOccurred(tr("Failed to start objdump. Make sure it is installed and in PATH."));
        emit finished();
        return;
    }

    emit logLine("[disasm] objdump started (pid " + QString::number(proc.processId()) + ")");

    const QHash<QString, DisasmSection> sectionHeaders = parseObjdumpSectionHeaders(objdumpExe, filePath, env);

    QByteArray output;
    while (!proc.waitForFinished(200)) {
        if (m_cancelled) {
            proc.kill();
            emit logLine("[disasm] cancelled by user");
            emit finished();
            return;
        }
        output += proc.readAllStandardOutput();
    }
    output += proc.readAllStandardOutput();

    QByteArray stderrData = proc.readAllStandardError();
    int exitCode = proc.exitCode();

    emit logLine(QString("[disasm] exit code : %1").arg(exitCode));
    emit logLine(QString("[disasm] stdout    : %1 bytes").arg(output.size()));

    if (!stderrData.isEmpty())
        emit logLine("[disasm] stderr    : " + QString::fromUtf8(stderrData).trimmed());

    if (exitCode != 0) {
        QString errMsg = QString::fromUtf8(stderrData).trimmed();
        if (errMsg.isEmpty())
            errMsg = tr("objdump exited with code %1").arg(exitCode);
        emit errorOccurred(errMsg);
        emit finished();
        return;
    }

    if (m_cancelled) { emit finished(); return; }

    /*── dump first 30 lines of raw output to log ──────────────────────────*/
    emit logLine("[disasm] --- raw output (first 30 lines) ---");
    const QStringList rawLines = QString::fromUtf8(output).split('\n');
    int preview = qMin(30, rawLines.size());
    for (int i = 0; i < preview; ++i)
        emit logLine(rawLines[i]);
    if (rawLines.size() > 30)
        emit logLine(QString("[disasm] ... (%1 more lines)").arg(rawLines.size() - 30));
    emit logLine("[disasm] --- end raw output ---");

    /*── parse ─────────────────────────────────────────────────────────────*/
    QVector<DisasmSection> sections = parseSections(output, sectionHeaders);

    emit logLine(QString("[disasm] sections parsed: %1").arg(sections.size()));
    for (const auto &s : sections)
        emit logLine(QString("[disasm]   section '%1': %2 instructions")
                         .arg(s.name).arg(s.instructions.size()));

    int total = sections.size();
    for (int i = 0; i < total; ++i) {
        if (m_cancelled) break;
        emit sectionFound(sections[i]);
        emit progressUpdated((i + 1) * 100 / total);
    }

    emit finished();
}

/*---------------------------------------------------------------------------
Parser
---------------------------------------------------------------------------
Verified objdump -d format (cat -A shows ^I for TAB):

Disassembly of section .text:

0000000000001040 <_start>:
    1040:^If3 0f 1e fa          ^Iendbr64
    1004:^I48 83 ec 08          ^Isub    $0x8,%rsp
    1008:^I48 8b 05 d9 2f 00 00 ^Imov    0x2fd9(%rip),%rax   # 3fe8 <sym>

---------------------------------------------------------------------------
Parser
---------------------------------------------------------------------------*/
QVector<DisasmSection> DisassemblerWorker::parseSections(const QByteArray &raw, const QHash<QString, DisasmSection> &sectionMap)
{
    QVector<DisasmSection> sections;

    /* Section: allow leading spaces and spaces before colons. */
    static const QRegularExpression reSectionHdr(
        R"(^\s*Disassembly of section\s+(\S+)\s*:)", 
        QRegularExpression::MultilineOption);

    /* Function label: allow any whitespace */
    static const QRegularExpression reFuncLabel(
        R"(^\s*([0-9a-fA-F]+)\s+<([^>]+)>:)",
        QRegularExpression::MultilineOption);

    /*INSTRUCTION(Crucial): replase \t with \s+ (any whitespace characters)
    GROUP 1: Adress
    GROUP 2: Bytes (hex pairs)
    GROUP 3: Mnemonic
    GROUP 4: Operands*/
    static const QRegularExpression reInsn(
        R"(^\s*([0-9a-fA-F]+):\s+((?:[0-9a-fA-F]{2}\s+)+)\s*(\S+)(?:[ \t]+([^#\n]*))?)",
        QRegularExpression::MultilineOption);

    const QString text = QString::fromUtf8(raw);
    const QStringList lines = text.split('\n');
    int curSectionIdx = -1;

    for (const QString &line : lines) {
        if (m_cancelled) break;
        if (line.trimmed().isEmpty()) continue;

        /* 1. Searct for section header */
        QRegularExpressionMatch m = reSectionHdr.match(line);
        if (m.hasMatch()) {
            const QString name = m.captured(1);
            if (sectionMap.contains(name)) {
                sections.append(sectionMap.value(name));
            } else {
                sections.append(DisasmSection{ name, {} });
            }
            curSectionIdx = sections.size() - 1;
            continue;
        }

        /* 2. Instruction lookup (check BEFORE function labelm as they're similar) */
        m = reInsn.match(line);
        if (m.hasMatch()) {
            if (curSectionIdx == -1) {
                sections.append(DisasmSection{ ".text (auto)", {} });
                curSectionIdx = sections.size() - 1;
            }

            DisasmInstruction insn;
            insn.address  = m.captured(1);
            insn.bytes    = m.captured(2).trimmed();
            insn.mnemonic = m.captured(3);
            insn.operands = m.captured(4).trimmed();

            /* Calculate size in bytes. Every two characters prepresent one byte (accouring for spaces) */
            QString b = insn.bytes;
            b.remove(' ');
            insn.size = b.size() / 2;

            DisasmSection &cur = sections[curSectionIdx];
            if (cur.hasFileMapping) {
                bool okAddr = false;
                const quint64 addr = insn.address.toULongLong(&okAddr, 16);
                if (okAddr && addr >= cur.vaddr) {
                    const quint64 delta = addr - cur.vaddr;
                    if (delta < cur.size)
                        insn.fileOffset = static_cast<qint64>(cur.fileOffset + delta);
                }
            }
            cur.instructions.append(insn);
            continue;
        }

        /* Function labe lookup (e.g. <main>:) */
        m = reFuncLabel.match(line);
        if (m.hasMatch()) {
            if (curSectionIdx == -1) {
                sections.append(DisasmSection{ ".text (auto)", {} });
                curSectionIdx = sections.size() - 1;
            }
            DisasmInstruction lbl;
            lbl.address  = m.captured(1);
            lbl.mnemonic = "<" + m.captured(2) + ">";
            sections[curSectionIdx].instructions.append(lbl);
            continue;
        }
    }
    return sections;
}
