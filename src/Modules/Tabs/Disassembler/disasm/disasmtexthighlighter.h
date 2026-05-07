// SPDX-License-Identifier: MIT
#ifndef DISASMTEXTHIGHLIGHTER_H
#define DISASMTEXTHIGHLIGHTER_H

#include <QSyntaxHighlighter>

class DisasmTextHighlighter final : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit DisasmTextHighlighter(QTextDocument *parent);

protected:
    void highlightBlock(const QString &text) override;

private:
    QTextCharFormat m_addr;
    QTextCharFormat m_bytes;
    QTextCharFormat m_mnemonic;
    QTextCharFormat m_reg;
    QTextCharFormat m_imm;
    QTextCharFormat m_sym;
    QTextCharFormat m_comment;
};

#endif // DISASMTEXTHIGHLIGHTER_H

