#ifndef DISASSEMBLERWORKER_H
#define DISASSEMBLERWORKER_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

struct DisasmInstruction {
    QString address;
    QString bytes;
    QString mnemonic;
    QString operands;
    qint64 fileOffset = -1;
    qint64 size = 0;
};

struct DisasmSection {
    QString name;
    QVector<DisasmInstruction> instructions;
    quint64 vaddr = 0;
    quint64 fileOffset = 0;
    quint64 size = 0;
    bool hasFileMapping = false;
};

struct DisasmFunction {
    QString name;
    QString address; // string like 0x...
};

struct DisasmString {
    QString address; // string like 0x...
    QString value;   // decoded string
};

class DisassemblerWorker : public QObject
{
    Q_OBJECT

public:
    explicit DisassemblerWorker(QObject *parent = nullptr);

public slots:
    void disassemble(const QString &filePath, const QString &arch);
    void cancel();

signals:
    void sectionFound(const DisasmSection &section);
    void functionsFound(const QVector<DisasmFunction> &funcs);
    void stringsFound(const QVector<DisasmString> &strings);
    void finished();
    void errorOccurred(const QString &errorMsg);
    void progressUpdated(int percent);
    void logLine(const QString &line);   // diagnostic log line

private:
    bool m_cancelled = false;
    friend class Radare2Backend;

    QVector<DisasmSection> parseSections(const QByteArray &raw, const QHash<QString, DisasmSection> &sectionMap);
    static QString detectArch(const QString &filePath);
};

#endif // DISASSEMBLERWORKER_H
