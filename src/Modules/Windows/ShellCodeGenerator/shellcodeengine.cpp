#include "shellcodeengine.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QRegularExpression>

static QString findToolImpl(const QString &name) {
    QString sysPath = QStandardPaths::findExecutable(name);
    if (!sysPath.isEmpty())
        return sysPath;

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

QString ShellcodeEngine::findNasm() { return findToolImpl("nasm"); }
QString ShellcodeEngine::findNdisasm() { return findToolImpl("ndisasm"); }

bool ShellcodeEngine::checkDependencies(QString *errorOut) {
    auto isAvailable = [](const QString &path) {
        return !QStandardPaths::findExecutable(QFileInfo(path).fileName()).isEmpty()
            || QFile::exists(path);
    };

    QStringList missing;
    if (!isAvailable(findNasm()))
        missing << "nasm (assembler)";
    if (!isAvailable(findNdisasm()))
        missing << "ndisasm (disassembler)";

    if (!missing.isEmpty()) {
        if (errorOut)
            *errorOut = missing.join(", ");
        return false;
    }
    return true;
}

AssemblyResult ShellcodeEngine::assemble(const QString &source, int bits) {
    AssemblyResult result;

    const QString tmpAsm = QDir::tempPath() + "/shellgen_input.asm";
    const QString tmpBin = QDir::tempPath() + "/shellgen_output.bin";

    {
        QFile f(tmpAsm);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            result.error = QObject::tr("Failed to create temp file.");
            return result;
        }
        QTextStream s(&f);
        s << "BITS " << bits << "\n" << source << "\n";
    }

    const QString nasmExe = findNasm();
    QProcess proc;
    proc.start(nasmExe, {"-f", "bin", "-o", tmpBin, tmpAsm});

    if (!proc.waitForStarted(3000)) {
        result.error = QObject::tr("nasm not found. Ensure it is installed and in PATH.");
        QFile::remove(tmpAsm);
        return result;
    }
    proc.waitForFinished(5000);

    if (proc.exitCode() != 0) {
        result.error = QString::fromUtf8(proc.readAllStandardError())
                           .trimmed()
                           .replace(tmpAsm, "<input>");
        result.exitCode = proc.exitCode();
        QFile::remove(tmpAsm);
        QFile::remove(tmpBin);
        return result;
    }

    QFile binFile(tmpBin);
    if (!binFile.open(QIODevice::ReadOnly)) {
        result.error = QObject::tr("Failed to read nasm output.");
        QFile::remove(tmpAsm);
        return result;
    }

    result.binary = binFile.readAll();
    binFile.close();
    QFile::remove(tmpAsm);
    QFile::remove(tmpBin);

    return result;
}

QList<DisasmLine> ShellcodeEngine::disassemble(const QByteArray &binary, int bits) {
    QList<DisasmLine> result;

    const QString tmpBin = QDir::tempPath() + "/shellgen_disasm.bin";
    {
        QFile f(tmpBin);
        if (!f.open(QIODevice::WriteOnly))
            return result;
        f.write(binary);
    }

    const QString ndisasmExe = findNdisasm();
    QProcess proc;
    proc.start(ndisasmExe, {"-b", QString::number(bits), tmpBin});

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
        const quint64 offset = parts[0].toULongLong(&ok, 16);
        if (!ok)
            continue;

        const QByteArray rawBytes = QByteArray::fromHex(parts[1].toLatin1());
        if (rawBytes.isEmpty())
            continue;

        const QString mnemonic = parts.mid(2).join(' ');
        result.append({offset, rawBytes, mnemonic});
    }
    return result;
}

static QString formatAnnotated(const QByteArray &raw, const QList<DisasmLine> &lines) {
    if (!lines.isEmpty()) {
        int maxByteLength = 0;
        for (const auto &l : lines)
            maxByteLength = qMax(maxByteLength, l.hexBytes.size() * 5);

        QString result;
        for (int i = 0; i < lines.size(); ++i) {
            const auto &l = lines[i];
            const bool isLast = (i + 1 == lines.size());

            QString bytePart;
            for (int b = 0; b < l.hexBytes.size(); ++b) {
                bytePart += QString("0x%1").arg(static_cast<uint8_t>(l.hexBytes[b]), 2, 16, QChar('0'));
                if (!(isLast && b + 1 == l.hexBytes.size()))
                    bytePart += ", ";
            }

            QString comment;
            if (l.offset == 0)
                comment = QString("// %1").arg(l.mnemonic);
            else
                comment = QString("// %1 (+0x%2)").arg(l.mnemonic).arg(l.offset, 0, 16);

            int paddingLen = maxByteLength - bytePart.length() + 2;
            if (paddingLen < 2) paddingLen = 2;

            result += "    " + bytePart + QString(paddingLen, ' ') + comment + "\n";
        }
        return result;
    }

    // Fallback: plain hex dump, 12 bytes per line
    QString result;
    const int cols = 12;
    for (int i = 0; i < raw.size(); ++i) {
        if (i % cols == 0) result += "    ";
        result += QString("0x%1").arg(static_cast<uint8_t>(raw[i]), 2, 16, QChar('0'));
        if (i + 1 < raw.size()) result += ", ";
        if ((i + 1) % cols == 0 && i + 1 < raw.size()) result += "\n";
    }
    result += "\n";
    return result;
}

QString ShellcodeEngine::formatC(const QByteArray &raw, const QList<DisasmLine> &lines) {
    QString s = QObject::tr("unsigned char shellcode[] = {  // %1 bytes\n").arg(raw.size());
    s += formatAnnotated(raw, lines);
    s += "};\n";
    return s;
}

QString ShellcodeEngine::formatCpp(const QByteArray &raw, const QList<DisasmLine> &lines) {
    QString s = QObject::tr("std::array<std::uint8_t, %1> shellcode = {  // %1 bytes\n").arg(raw.size());
    s += formatAnnotated(raw, lines);
    s += "};\n";
    return s;
}

QString ShellcodeEngine::formatRaw(const QByteArray &raw) {
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
