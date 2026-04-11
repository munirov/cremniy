#include "radare2backend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

static QString normalizeHexBytes(QString s)
{
    s.remove(' ');
    s.remove('\t');
    s.remove('\n');
    s.remove('\r');
    return s.trimmed();
}

static bool parseU64FromJson(const QJsonValue &v, quint64 *out)
{
    if (!out) return false;

    if (v.isDouble()) {
        // Note: JSON numbers are represented as double in Qt. This is safe for typical
        // in-file offsets, but may lose precision for very large values.
        const double d = v.toDouble();
        if (d < 0) return false;
        *out = static_cast<quint64>(d);
        return true;
    }

    if (v.isString()) {
        const QString s = v.toString().trimmed();
        if (s.isEmpty()) return false;
        bool ok = false;
        quint64 n = 0;
        if (s.startsWith("0x", Qt::CaseInsensitive))
            n = s.mid(2).toULongLong(&ok, 16);
        else
            n = s.toULongLong(&ok, 10);
        if (!ok) return false;
        *out = n;
        return true;
    }

    return false;
}

QByteArray Radare2Backend::runR2JsonCommand(const QString &r2Path,
                                           const QString &filePath,
                                           const QString &cmd,
                                           QString *error,
                                           bool *cancelled)
{
    if (cancelled && *cancelled) {
        if (error) *error = QObject::tr("Cancelled");
        return {};
    }

    // We run a fresh r2 for each command (MVP simplicity).
    // Use '-q' to reduce noise; add explicit 'q' to exit cleanly.
    const QStringList args = {
        "-q",
        "-c", cmd,
        "-c", "q",
        filePath
    };

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(r2Path, args);
    if (!proc.waitForStarted(5000)) {
        if (error) *error = QObject::tr("Failed to start radare2 (%1)").arg(r2Path);
        return {};
    }

    // Small polling loop to allow cancellation.
    while (!proc.waitForFinished(50)) {
        if (cancelled && *cancelled) {
            proc.kill();
            if (error) *error = QObject::tr("Cancelled");
            return {};
        }
    }

    const int exitCode = proc.exitCode();
    const QByteArray stdoutData = proc.readAllStandardOutput();
    const QByteArray stderrData = proc.readAllStandardError();

    if (exitCode != 0) {
        QString errMsg = QString::fromUtf8(stderrData).trimmed();
        if (errMsg.isEmpty())
            errMsg = QObject::tr("radare2 exited with code %1").arg(exitCode);
        if (error) *error = errMsg;
        return {};
    }

    if (!stderrData.isEmpty()) {
        // Some builds may output warnings to stderr even on success; keep them for debugging.
        // We don't fail on that.
    }

    return stdoutData;
}

Radare2Backend::Result Radare2Backend::disassembleFile(const QString &r2Path,
                                                       const QString &filePath,
                                                       const Options &opt,
                                                       bool *cancelled)
{
    Result res;

    // Build a prelude command that applies settings and optional analysis,
    // then runs the actual JSON command. We keep it per-invocation since
    // MVP uses a fresh r2 process per command.
    auto buildPrelude = [&](const QString &jsonCmd) -> QString {
        QStringList cmds;

        // Apply syntax preference
        if (opt.asmSyntax == 1)
            cmds << "e asm.syntax=att";
        else
            cmds << "e asm.syntax=intel";

        // Optional user pre-commands (semicolon-separated)
        if (!opt.preCommands.trimmed().isEmpty()) {
            const QStringList extra = opt.preCommands.split(';', Qt::SkipEmptyParts);
            for (const QString &c : extra)
                cmds << c.trimmed();
        }

        // Optional analysis
        if (opt.analysisLevel == 2)
            cmds << "aaa";
        else if (opt.analysisLevel == 1)
            cmds << "aa";

        cmds << jsonCmd;
        return cmds.join(';');
    };

    // Functions list (best-effort). We fetch it once so UI can show navigation like IDA/Ghidra.
    {
        QString err;
        const QByteArray funcJson = runR2JsonCommand(r2Path, filePath, buildPrelude("aflj"), &err, cancelled);
        if (!funcJson.isEmpty()) {
            QJsonParseError pef{};
            const QJsonDocument fdoc = QJsonDocument::fromJson(funcJson, &pef);
            if (pef.error == QJsonParseError::NoError && fdoc.isArray()) {
                const QJsonArray arr = fdoc.array();
                res.functions.reserve(arr.size());
                for (const QJsonValue &v : arr) {
                    if (!v.isObject()) continue;
                    const QJsonObject o = v.toObject();
                    const QString name = o.value("name").toString();
                    quint64 off = 0;
                    const bool got = parseU64FromJson(o.value("offset"), &off)
                                  || parseU64FromJson(o.value("addr"), &off)
                                  || parseU64FromJson(o.value("address"), &off);
                    if (name.isEmpty() || !got) continue;
                    DisasmFunction f;
                    f.name = name;
                    f.address = QString("0x%1").arg(QString::number(off, 16));
                    res.functions.append(std::move(f));
                }
            }
        }
        // If empty, keep going; disassembly still works.
    }

    // Strings list (best-effort) for auto-comments like ; "Hello world"
    {
        QString err;
        const QByteArray strJson = runR2JsonCommand(r2Path, filePath, buildPrelude("izj"), &err, cancelled);
        if (!strJson.isEmpty()) {
            QJsonParseError pes{};
            const QJsonDocument sdoc = QJsonDocument::fromJson(strJson, &pes);
            if (pes.error == QJsonParseError::NoError && sdoc.isArray()) {
                const QJsonArray arr = sdoc.array();
                res.strings.reserve(arr.size());
                for (const QJsonValue &v : arr) {
                    if (!v.isObject()) continue;
                    const QJsonObject o = v.toObject();
                    const QString s = o.value("string").toString();
                    if (s.isEmpty()) continue;
                    quint64 vaddr = 0;
                    const bool got = parseU64FromJson(o.value("vaddr"), &vaddr)
                                  || parseU64FromJson(o.value("paddr"), &vaddr)
                                  || parseU64FromJson(o.value("offset"), &vaddr);
                    if (!got) continue;
                    DisasmString ds;
                    ds.address = QString("0x%1").arg(QString::number(vaddr, 16));
                    ds.value = s;
                    res.strings.append(std::move(ds));
                }
            }
        }
    }

    QString err;
    const QByteArray secJson = runR2JsonCommand(r2Path, filePath, buildPrelude("iSj"), &err, cancelled);
    if (secJson.isEmpty()) {
        res.error = err.isEmpty() ? QObject::tr("radare2 returned empty section list") : err;
        return res;
    }

    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(secJson, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
        res.error = QObject::tr("Failed to parse radare2 JSON (iSj): %1").arg(pe.errorString());
        return res;
    }

    const QJsonArray sections = doc.array();
    res.sections.reserve(sections.size());

    const int insnLimit = (opt.insnLimitPerSection > 0) ? opt.insnLimitPerSection : 4000;

    for (const QJsonValue &v : sections) {
        if (cancelled && *cancelled) break;
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();

        const QString name = o.value("name").toString();
        const QString perm = o.value("perm").toString(); // e.g. "r-x"
        quint64 vaddr = 0;
        parseU64FromJson(o.value("vaddr"), &vaddr);
        quint64 paddr = 0;
        parseU64FromJson(o.value("paddr"), &paddr);
        quint64 vsize = 0;
        parseU64FromJson(o.value("vsize"), &vsize);
        if (vsize == 0)
            parseU64FromJson(o.value("size"), &vsize);

        if (name.isEmpty()) continue;
        // Disassemble only code-like sections to avoid garbage disassembly of data sections.
        // Some binaries / r2 configs may mark non-code sections oddly; keep a conservative whitelist.
        static const QRegularExpression reCodeSec(R"(^\.(text|init|fini|plt(\..*)?)$)");
        if (!reCodeSec.match(name).hasMatch())
            continue;
        if (!perm.contains('x'))
            continue;
        if (vaddr == 0 || vsize == 0)
            continue;

        DisasmSection sec;
        sec.name = name;
        sec.vaddr = vaddr;
        sec.fileOffset = paddr;
        sec.size = vsize;
        sec.hasFileMapping = true;

        // pDj returns array of objects: {offset, bytes, opcode, ...}
        // Use address of section as start.
        const QString cmd = QString("pDj %1 @ 0x%2")
                                .arg(insnLimit)
                                .arg(QString::number(vaddr, 16));

        QString cmdErr;
        const QByteArray insnJson = runR2JsonCommand(r2Path, filePath, buildPrelude(cmd), &cmdErr, cancelled);
        if (insnJson.isEmpty()) {
            // Not all sections are disassemblable; skip but keep going.
            continue;
        }

        QJsonParseError pe2{};
        QJsonDocument doc2 = QJsonDocument::fromJson(insnJson, &pe2);
        if (pe2.error != QJsonParseError::NoError || !doc2.isArray()) {
            // Skip on parse issues for this section.
            continue;
        }

        const QJsonArray arr = doc2.array();
        sec.instructions.reserve(arr.size());
        const quint64 endAddr = vaddr + vsize;
        for (const QJsonValue &iv : arr) {
            if (!iv.isObject()) continue;
            const QJsonObject io = iv.toObject();

            DisasmInstruction insn;

            quint64 off = 0;
            bool gotAddr = parseU64FromJson(io.value("offset"), &off)
                        || parseU64FromJson(io.value("addr"), &off)
                        || parseU64FromJson(io.value("address"), &off)
                        || parseU64FromJson(io.value("at"), &off);
            if (gotAddr) {
                // Stop once we leave section bounds; r2 can continue disassembling into next sections/data.
                if (off < vaddr)
                    continue;
                if (off >= endAddr)
                    break;
            }

            // Keep something visible even if JSON misses address field.
            insn.address = QString("0x%1").arg(QString::number(gotAddr ? off : 0, 16));

            // radare2 provides fields like "bytes" (hex string) and "opcode" (full asm)
            insn.bytes = io.value("bytes").toString();
            insn.size = normalizeHexBytes(insn.bytes).size() / 2;
            if (gotAddr)
                insn.fileOffset = static_cast<qint64>(paddr + (off - vaddr));

            const QString opcode = io.value("opcode").toString();
            if (!opcode.isEmpty()) {
                const int sp = opcode.indexOf(' ');
                if (sp > 0) {
                    insn.mnemonic = opcode.left(sp).trimmed();
                    insn.operands = opcode.mid(sp + 1).trimmed();
                } else {
                    insn.mnemonic = opcode.trimmed();
                }
            } else {
                insn.mnemonic = io.value("mnemonic").toString();
                insn.operands = io.value("operands").toString();
            }

            if (insn.mnemonic.isEmpty() && insn.operands.isEmpty() && insn.bytes.isEmpty())
                continue;

            sec.instructions.append(insn);
        }

        if (!sec.instructions.isEmpty())
            res.sections.append(sec);
    }

    return res;
}

