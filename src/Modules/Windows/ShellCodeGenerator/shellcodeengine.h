#ifndef SHELLCODEENGINE_H
#define SHELLCODEENGINE_H

#include <QByteArray>
#include <QList>
#include <QString>

struct DisasmLine {
    quint64 offset;
    QByteArray hexBytes;
    QString mnemonic;
};

struct AssemblyResult {
    QByteArray binary;
    QString error;
    int exitCode = -1;
};

class ShellcodeEngine {
public:
    AssemblyResult assemble(const QString &source, int bits);
    QList<DisasmLine> disassemble(const QByteArray &binary, int bits);

    static QString formatC(const QByteArray &raw, const QList<DisasmLine> &lines);
    static QString formatCpp(const QByteArray &raw, const QList<DisasmLine> &lines);
    static QString formatRaw(const QByteArray &raw);

    static QString findNasm();
    static QString findNdisasm();
    static bool checkDependencies(QString *errorOut = nullptr);
};

#endif // SHELLCODEENGINE_H
