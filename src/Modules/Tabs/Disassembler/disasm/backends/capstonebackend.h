#ifndef CAPSTONEBACKEND_H
#define CAPSTONEBACKEND_H

#include "../../disassemblerworker.h"

#include <QString>
#include <QVector>
#include <atomic>

class CapstoneBackend
{
public:
    struct Options {
        int asmSyntax = 0; // 0 intel, 1 att
    };

    struct Result {
        QVector<DisasmSection> sections;
        QVector<DisasmString> strings;
        QString error;
    };

    static Result disassembleFile(const QString &filePath, const Options &opt, std::atomic<bool> *cancelled = nullptr);
};

#endif // CAPSTONEBACKEND_H