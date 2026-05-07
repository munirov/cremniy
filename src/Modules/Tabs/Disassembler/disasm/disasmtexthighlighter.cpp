// SPDX-License-Identifier: MIT
#include "disasmtexthighlighter.h"

#include <QRegularExpression>

DisasmTextHighlighter::DisasmTextHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Restrict to red/green/blue for text (no gray/black).
    m_addr.setForeground(QColor("#2f7bff"));      // blue
    m_addr.setFontWeight(QFont::DemiBold);

    m_bytes.setForeground(QColor("#21c55d"));     // green
    m_bytes.setFontWeight(QFont::DemiBold);

    m_mnemonic.setForeground(QColor("#ef4444"));  // red
    m_mnemonic.setFontWeight(QFont::DemiBold);

    m_reg.setForeground(QColor("#22c55e"));       // green
    m_reg.setFontWeight(QFont::DemiBold);

    m_imm.setForeground(QColor("#fb7185"));       // red-ish

    m_sym.setForeground(QColor("#3b82f6"));       // blue
    m_sym.setFontWeight(QFont::DemiBold);

    m_comment.setForeground(QColor("#34d399"));   // green
}

void DisasmTextHighlighter::highlightBlock(const QString &t)
{
    // Listing format we render:
    // <addr>: <bytes padded>  <mnemonic> <operands...> [#comment]

    // Address at line start (hex)
    {
        static const QRegularExpression re(R"(^\s*(0x[0-9a-fA-F]+|[0-9a-fA-F]+)(?=\:))");
        auto m = re.match(t);
        if (m.hasMatch())
            setFormat(m.capturedStart(1), m.capturedLength(1), m_addr);
    }

    // Bytes after ":"
    {
        static const QRegularExpression re(R"(:\s+((?:[0-9a-fA-F]{2}\s+)+))");
        auto m = re.match(t);
        if (m.hasMatch()) {
            const int s = m.capturedStart(1);
            const int l = m.capturedLength(1);
            setFormat(s, l, m_bytes);
        }
    }

    // Mnemonic: first token after bytes padding (we find last two spaces gap)
    {
        // Typical: "ADDR: BYTES....  MNEM ..."
        static const QRegularExpression re(R"(^.*?:\s+(?:[0-9a-fA-F]{2}\s+)*\s{2,}(\S+))");
        auto m = re.match(t);
        if (m.hasMatch())
            setFormat(m.capturedStart(1), m.capturedLength(1), m_mnemonic);
    }

    // Comments: after '#' or ';'
    {
        const int hash = t.indexOf('#');
        const int semi = t.indexOf(';');
        int cut = -1;
        if (hash >= 0) cut = hash;
        if (semi >= 0) cut = (cut < 0) ? semi : qMin(cut, semi);
        if (cut >= 0)
            setFormat(cut, t.size() - cut, m_comment);
    }

    // Symbols <...>
    {
        static const QRegularExpression re(R"(<[^>]+>)");
        auto it = re.globalMatch(t);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_sym);
        }
    }

    // Registers (best-effort x86/x64)
    {
        static const QRegularExpression re(
            R"(\b(?:r(?:1[0-5]|[0-9])d?|r(?:1[0-5]|[0-9])w|r(?:1[0-5]|[0-9])b|r(?:ip|flags)|rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|eax|ebx|ecx|edx|esi|edi|ebp|esp|ax|bx|cx|dx|si|di|bp|sp|al|ah|bl|bh|cl|ch|dl|dh|xmm\d+|ymm\d+|zmm\d+|cs|ds|es|fs|gs|ss)\b)",
            QRegularExpression::CaseInsensitiveOption);
        auto it = re.globalMatch(t);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_reg);
        }
    }

    // Immediates / numbers
    {
        static const QRegularExpression re(R"((?:\$\s*)?(?:0x[0-9a-fA-F]+|\b\d+\b|\b-\d+\b))");
        auto it = re.globalMatch(t);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_imm);
        }
    }
}

