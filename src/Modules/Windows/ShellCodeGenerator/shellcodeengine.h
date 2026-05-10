#pragma once

#include <QByteArray>
#include <QObject>
#include <QStringList>

class ShellcodeEngine : public QObject {
    Q_OBJECT

public:
    enum class Architecture { X86_16,
                              X86_32,
                              X86_64 };
    enum class OutputStyle { C,
                             Cpp,
                             Raw };

    struct DisasmEntry {
        int offset;
        int size;
        QString mnemonic;
    };

    struct AsmError {
        int line;
        QString message;
    };

    explicit ShellcodeEngine(QObject* parent = nullptr);

    void assemble(const QString& asmText, Architecture arch, OutputStyle style);

    [[nodiscard]] QStringList checkDependencies() const;
    [[nodiscard]] static QString findTool(const QString& name);

signals:
    void finished(const QString& output, int byteCount);
    void errorOccurred(const QList<ShellcodeEngine::AsmError>& errors);

private:
    [[nodiscard]] QList<DisasmEntry> disassemble(const QByteArray& raw, const QString& bits) const;
    [[nodiscard]] QString formatAnnotated(const QByteArray& raw, const QList<DisasmEntry>& entries) const;
    [[nodiscard]] QString buildOutput(const QByteArray& raw, const QString& bits, OutputStyle style) const;

    [[nodiscard]] QString generateC(const QByteArray& raw, const QList<DisasmEntry>& entries) const;
    [[nodiscard]] QString generateCpp(const QByteArray& raw, const QList<DisasmEntry>& entries) const;
    [[nodiscard]] QString generateRaw(const QByteArray& raw) const;

    [[nodiscard]] static QString archToBits(Architecture arch);
    [[nodiscard]] static bool isToolAvailable(const QString& name);
    [[nodiscard]] static QList<AsmError> parseErrors(const QString& stderrText);
};
