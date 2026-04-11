// SPDX-License-Identifier: MIT
#include "disasmsyntaxdelegate.h"

#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QTextDocument>

DisasmSyntaxDelegate::DisasmSyntaxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

static QString wrapSpan(const QString &s, const QString &color, bool bold = false)
{
    if (s.isEmpty()) return s;
    if (bold)
        return QString("<span style=\"color:%1; font-weight:600;\">%2</span>").arg(color, s);
    return QString("<span style=\"color:%1;\">%2</span>").arg(color, s);
}

static QString highlightWithRegex(const QString &src,
                                  const QRegularExpression &re,
                                  const QString &color,
                                  bool bold = false)
{
    QString out;
    out.reserve(src.size() + 32);

    int last = 0;
    auto it = re.globalMatch(src);
    while (it.hasNext()) {
        const auto m = it.next();
        const int start = m.capturedStart();
        const int len = m.capturedLength();
        if (start < last) continue;
        out += src.mid(last, start - last);
        out += wrapSpan(m.captured(0), color, bold);
        last = start + len;
    }
    out += src.mid(last);
    return out;
}

QString DisasmSyntaxDelegate::htmlForCell(int column, const QString &text, bool selected)
{
    // Palette: strictly avoid black/gray for *text*.
    // Use only red/green/blue for syntax highlighting (white when selected for readability).
    const QString cText   = selected ? "#ffffff" : "#4aa3ff"; // blue base text
    const QString cAddr   = selected ? "#ffffff" : "#2f7bff"; // blue
    const QString cBytes  = selected ? "#ffffff" : "#21c55d"; // green
    const QString cMnem   = selected ? "#ffffff" : "#ef4444"; // red
    const QString cReg    = selected ? "#ffffff" : "#22c55e"; // green
    const QString cImm    = selected ? "#ffffff" : "#fb7185"; // red-ish
    const QString cBrack  = selected ? "#ffffff" : "#60a5fa"; // blue
    const QString cSym    = selected ? "#ffffff" : "#3b82f6"; // blue
    const QString cComm   = selected ? "#ffffff" : "#34d399"; // green

    const QString base = text.toHtmlEscaped();

    // Columns in DisassemblerTab: 0 addr, 1 bytes, 2 mnemonic, 3 operands
    if (column == 0) {
        return wrapSpan(base, cAddr, true);
    }
    if (column == 1) {
        // Highlight hex bytes (pairs)
        static const QRegularExpression reByte(R"(\b[0-9a-fA-F]{2}\b)");
        // Also wrap the whole cell so spaces don't fall back to default (black) text color.
        const QString s = highlightWithRegex(base, reByte, cBytes, true);
        return QString("<span style=\"color:%1;\">%2</span>").arg(cBytes, s);
    }
    if (column == 2) {
        return wrapSpan(base, cMnem, true);
    }

    // Operands: build HTML from raw text to avoid corrupting tags by later regex passes.
    const QString raw = text;

    // Split comment (first '#' or ';')
    int cut = -1;
    {
        const int hash = raw.indexOf('#');
        const int semi = raw.indexOf(';');
        if (hash >= 0) cut = hash;
        if (semi >= 0) cut = (cut < 0) ? semi : qMin(cut, semi);
    }

    const QString codePart = (cut >= 0) ? raw.left(cut) : raw;
    const QString commPart = (cut >= 0) ? raw.mid(cut) : QString();

    // One-pass token matcher over the *raw* code part.
    // Order matters: symbols, registers, immediates, punctuation.
    static const QRegularExpression reTok(
        R"(<[^>]+>|(?:\$\s*)?(?:0x[0-9a-fA-F]+|\b\d+\b|\b-\d+\b)|\b(?:r(?:1[0-5]|[0-9])d?|r(?:1[0-5]|[0-9])w|r(?:1[0-5]|[0-9])b|r(?:ip|flags)|rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|eax|ebx|ecx|edx|esi|edi|ebp|esp|ax|bx|cx|dx|si|di|bp|sp|al|ah|bl|bh|cl|ch|dl|dh|xmm\d+|ymm\d+|zmm\d+|st\(\d+\)|cs|ds|es|fs|gs|ss)\b|[\[\]\(\)\+\-\*\:])",
        QRegularExpression::CaseInsensitiveOption);

    QString out;
    out.reserve(raw.size() + 64);

    int last = 0;
    auto it = reTok.globalMatch(codePart);
    while (it.hasNext()) {
        const auto m = it.next();
        const int start = m.capturedStart();
        const int len = m.capturedLength();
        if (start < last) continue;

        out += codePart.mid(last, start - last).toHtmlEscaped();

        const QString tok = m.captured(0);
        QString htmlTok = tok.toHtmlEscaped();

        // Classify token
        if (tok.startsWith('<') && tok.endsWith('>')) {
            htmlTok = wrapSpan(htmlTok, cSym, true);
        } else if (tok.size() == 1 && QStringLiteral("[]()+-*:")
                                     .contains(tok)) {
            htmlTok = wrapSpan(htmlTok, cBrack, false);
        } else if (tok.startsWith("0x", Qt::CaseInsensitive)
                   || tok.startsWith('$')
                   || tok == "-" /* won't happen alone but keep safe */
                   || tok.at(0).isDigit()
                   || (tok.startsWith('-') && tok.size() > 1 && tok.at(1).isDigit())) {
            htmlTok = wrapSpan(htmlTok, cImm, false);
        } else {
            // Assume register-like
            htmlTok = wrapSpan(htmlTok, cReg, true);
        }

        out += htmlTok;
        last = start + len;
    }
    out += codePart.mid(last).toHtmlEscaped();

    if (!commPart.isEmpty()) {
        out += wrapSpan(commPart.toHtmlEscaped(), cComm, false);
    }

    // Wrap whole string so any non-highlighted chars never fall back to default (black).
    return QString("<span style=\"color:%1;\">%2</span>").arg(cText, out);
}

void DisasmSyntaxDelegate::paint(QPainter *painter,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool selected = (opt.state & QStyle::State_Selected);

    painter->save();

    // Draw default background (incl. selection)
    if (opt.widget)
        opt.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
    else
        QStyledItemDelegate::paint(painter, option, index);

    // Rich text paint on top
    QTextDocument doc;
    doc.setDefaultFont(opt.font);
    doc.setHtml(htmlForCell(index.column(), opt.text, selected));
    doc.setTextWidth(opt.rect.width());

    painter->translate(opt.rect.topLeft());
    QRect clip(0, 0, opt.rect.width(), opt.rect.height());
    painter->setClipRect(clip);

    QAbstractTextDocumentLayout::PaintContext ctx;
    if (selected) {
        // Ensure selection keeps readable foreground; background already drawn.
        ctx.palette.setColor(QPalette::Text, Qt::white);
    }
    doc.documentLayout()->draw(painter, ctx);

    painter->restore();
}

QSize DisasmSyntaxDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    QTextDocument doc;
    doc.setDefaultFont(opt.font);
    doc.setHtml(htmlForCell(index.column(), opt.text, false));
    doc.setTextWidth(opt.rect.width());
    return QSize(static_cast<int>(doc.idealWidth()), static_cast<int>(doc.size().height()));
}

