#ifndef INSTRUCTIONHELPSERVICE_H
#define INSTRUCTIONHELPSERVICE_H

#include <QString>
#include <QHash>
#include <QStringList>

struct InstructionHelpEntry {
    QString mnemonic;
    QString title;
    QString description;
    QStringList flags;
};

class InstructionHelpService
{
public:
    static InstructionHelpService &instance();

    QString tooltipForToken(const QString &token, const QString &lineContext = QString()) const;
    QString tooltipForLine(const QString &lineContext) const;

private:
    InstructionHelpService();
    void loadLocale(const QString &locale);
    static QString normalizeMnemonic(const QString &value);
    static QString extractMnemonicFromLine(const QString &line);
    static QStringList extractNumericTokens(const QString &text);
    static bool parseAsmNumber(const QString &token, qulonglong *out);
    static QString makeNumberRow(const QString &token);

    QHash<QString, InstructionHelpEntry> m_entries;
};

#endif // INSTRUCTIONHELPSERVICE_H
