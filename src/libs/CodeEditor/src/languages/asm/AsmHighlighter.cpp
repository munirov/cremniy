#include "AsmHighlighter.h"

#include "QLanguage.hpp"
#include "QSyntaxStyle.hpp"

#include <QFile>

AsmHighlighter::AsmHighlighter(QTextDocument* document)
    : QStyleSyntaxHighlighter(document)
{
    Q_INIT_RESOURCE(codeeditor_res);
    QFile fl(QStringLiteral(":/languages/asm.xml"));
    if (fl.open(QIODevice::ReadOnly)) {
        QLanguage language(&fl);
        if (language.isLoaded()) {
            for (const QString& key : language.keys()) {
                QString formatName;
                if (key == QStringLiteral("Instruction"))
                    formatName = QStringLiteral("Keyword");
                else if (key == QStringLiteral("Register"))
                    formatName = QStringLiteral("PrimitiveType");
                else if (key == QStringLiteral("Directive"))
                    formatName = QStringLiteral("Preprocessor");
                else
                    formatName = key;

                for (const QString& name : language.names(key)) {
                    const QString escaped = QRegularExpression::escape(name);
                    if (name.startsWith(QLatin1Char('%'))) {
                        m_rules.append({
                            QRegularExpression(QStringLiteral("(?<=^|[^A-Za-z0-9_])%1\\b").arg(escaped)),
                            formatName
                        });
                    } else {
                        m_rules.append({
                            QRegularExpression(QStringLiteral("\\b%1\\b").arg(escaped)),
                            formatName
                        });
                    }
                }
            }
        }
    }

    // Size specifiers
    m_rules.append({
        QRegularExpression(QStringLiteral("\\b(BYTE|WORD|DWORD|QWORD|OWORD|TBYTE|XMMWORD|YMMWORD|ZMMWORD|PTR)\\b"), QRegularExpression::CaseInsensitiveOption),
        QStringLiteral("Type")
    });

    // Macros
    m_rules.append({
        QRegularExpression(QStringLiteral("\\b(macro|endmacro|%macro|%endmacro|%rep|%endrep|%assign|%deftok|%substr)\\b"), QRegularExpression::CaseInsensitiveOption),
        QStringLiteral("Keyword")
    });

    // Preprocessor directives (NASM: %include, GAS: .include)
    m_rules.append({
        QRegularExpression(QStringLiteral("(?:%%?)(include|define|ifdef|ifndef|endif|if|elif|else|macro|endmacro|assign|rotate|rep|endrep|exitmacro)")),
        QStringLiteral("Preprocessor")
    });
    m_rules.append({
        QRegularExpression(QStringLiteral("\\.(include|asciz|ascii|string|text|data|bss|rodata|section|globl|global|extern|type|size|align|equ|set|equiv|macro|endm|rept|endr|if|else|endif|elif)")),
        QStringLiteral("Preprocessor")
    });

    // Numbers — comprehensive ASM formats
    m_numberPattern = QRegularExpression(
        QStringLiteral(
            "\\b(?:0[xX][0-9a-fA-F]+"
            "|0[bB][01]+"
            "|0[oO][0-7]+"
            "|[0-9][0-9a-fA-F]*[hH]"
            "|[01]+[bB]"
            "|[0-7]+[oO]"
            "|[0-9]+[tT]"
            "|[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)"
            "\\b"
        )
    );

    // Strings
    m_stringPattern = QRegularExpression(QStringLiteral("\"[^\"\\\\]*(?:\\\\.[^\"\\\\]*)*\""));
    m_rules.append({
        QRegularExpression(QStringLiteral("'[^'\\\\]*(?:\\\\.[^'\\\\]*)*'")),
        QStringLiteral("String")
    });

    // Labels — identifier at line start followed by colon
    m_labelPattern = QRegularExpression(
        QStringLiteral("^\\s*[A-Za-z_$.@][A-Za-z0-9_$.@]*\\s*:"),
        QRegularExpression::MultilineOption
    );

    // Operators
    m_rules.append({
        QRegularExpression(QStringLiteral("[\\[\\]+\\-*,]")),
        QStringLiteral("Operator")
    });

    // Single-line comments: ; (NASM/MASM), # (GAS), // (some assemblers)
    m_commentPattern = QRegularExpression(QStringLiteral(";[^\\n]*|#[^\\n]*|//[^\\n]*"));

    // Block comments: /* ... */
    m_blockRules.append({
        QRegularExpression(QStringLiteral("/\\*")),
        QRegularExpression(QStringLiteral("\\*/")),
        QStringLiteral("Comment")
    });
}

void AsmHighlighter::highlightBlock(const QString& text)
{
    // 1. Block comments (multi-line state)
    for (const auto& blockRule : m_blockRules) {
        if (!blockRule.startPattern.isValid())
            continue;

        int startIndex = 0;
        if (previousBlockState() != 1)
            startIndex = text.indexOf(blockRule.startPattern);

        while (startIndex >= 0) {
            auto endMatch = blockRule.endPattern.match(text, startIndex);
            int commentLength;
            if (endMatch.capturedStart() == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endMatch.capturedStart() - startIndex + endMatch.capturedLength();
            }
            setFormat(startIndex, commentLength, syntaxStyle()->getFormat(blockRule.formatName));
            startIndex = text.indexOf(blockRule.startPattern, startIndex + commentLength);
        }
    }

    // 2. Numbers
    if (m_numberPattern.isValid()) {
        auto matches = m_numberPattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Number")));
        }
    }

    // 3. Strings
    if (m_stringPattern.isValid()) {
        auto matches = m_stringPattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("String")));
        }
    }

    // 4. Labels
    if (m_labelPattern.isValid()) {
        auto matches = m_labelPattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Label")));
        }
    }

    // 5. Keyword rules (instructions, registers, directives, size specifiers, etc.)
    for (const auto& rule : m_rules) {
        if (!rule.pattern.isValid())
            continue;
        auto matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(rule.formatName));
        }
    }

    // 6. Single-line comments (last — overrides everything)
    if (m_commentPattern.isValid()) {
        auto matches = m_commentPattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Comment")));
        }
    }
}
