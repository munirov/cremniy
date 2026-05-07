#ifndef RADARE2BACKEND_H
#define RADARE2BACKEND_H

#include "../../disassemblerworker.h"

#include <QString>
#include <QVector>

class Radare2Backend
{
public:
    struct Options {
        int insnLimitPerSection = 4000;
        int analysisLevel = 0; // 0 none, 1 aa, 2 aaa
        int asmSyntax = 0;     // 0 intel, 1 att
        QString preCommands;   // semicolon-separated
    };

    struct Result {
        QVector<DisasmSection> sections;
        QVector<DisasmFunction> functions;
        QVector<DisasmString> strings;
        QString error;
    };

    /* r2Path: path to r2 executable (may be "r2" or absolute) */
    static Result disassembleFile(const QString &r2Path, const QString &filePath, const Options &opt, bool *cancelled = nullptr);

private:
    static QByteArray runR2JsonCommand(const QString &r2Path, const QString &filePath, const QString &cmd, QString *error, bool *cancelled);
};

#endif // RADARE2BACKEND_H

