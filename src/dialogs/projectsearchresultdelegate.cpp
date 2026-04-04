#include "projectsearchresultdelegate.h"

#include <QApplication>
#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QTextLayout>

namespace {

QString buildDisplayedPreview(const QString &text, const QString &needle, int availWidth,
                              const QFontMetrics &fm)
{
    if (text.isEmpty() || availWidth <= 1)
        return text;
    if (fm.horizontalAdvance(text) <= availWidth)
        return text;

    const QString ell = QStringLiteral("…");
    const int ellW = fm.horizontalAdvance(ell);
    if (needle.isEmpty())
        return fm.elidedText(text, Qt::ElideRight, availWidth);

    const QString lowerText = text.toLower();
    const QString lowerNeedle = needle.toLower();
    const int hit = lowerText.indexOf(lowerNeedle);
    if (hit < 0)
        return fm.elidedText(text, Qt::ElideRight, availWidth);

    const bool prefixElide = hit > 0;
    const int innerBudget = availWidth - (prefixElide ? ellW : 0);
    if (innerBudget <= 0)
        return ell;

    constexpr int kMaxPrefixWindow = 56;
    const int minStart = qMax(0, hit - kMaxPrefixWindow);
    for (int s = hit; s >= minStart; --s) {
        const bool pe = s > 0;
        const int b = availWidth - (pe ? ellW : 0);
        if (b <= 0)
            break;
        const QString elided = fm.elidedText(text.mid(s), Qt::ElideRight, b);
        if (elided.toLower().contains(lowerNeedle)) {
            return (pe ? ell : QString()) + elided;
        }
    }

    const QString suffix = text.mid(hit);
    if (fm.horizontalAdvance(suffix) <= innerBudget)
        return (prefixElide ? ell : QString()) + suffix;

    return (prefixElide ? ell : QString()) + fm.elidedText(suffix, Qt::ElideRight, innerBudget);
}

} // namespace

ProjectSearchResultDelegate::ProjectSearchResultDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ProjectSearchResultDelegate::setNeedle(const QString &needle)
{
    m_needle = needle;
}

void ProjectSearchResultDelegate::setPreviewColumn(int column)
{
    m_previewColumn = column;
}

void ProjectSearchResultDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    if (index.column() != m_previewColumn || m_needle.isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    const QString fullText = index.data(Qt::DisplayRole).toString();
    const QRect textArea = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);

    painter->save();
    painter->setFont(option.font);

    const QColor normalPen = option.palette.color(QPalette::Text);
    const bool rowSelected = option.state.testFlag(QStyle::State_Selected);
    QColor hiBg(rowSelected ? QStringLiteral("#2f6fd4") : QStringLiteral("#2563c7"));
    hiBg.setAlpha(rowSelected ? 235 : 215);
    const QColor hiFg(QStringLiteral("#ffffff"));
    const QColor hiOutline(rowSelected ? QStringLiteral("#a8d4ff") : QStringLiteral("#7ec8ff"));

    const QFontMetrics fm(option.font);
    const QString text =
        buildDisplayedPreview(fullText, m_needle, textArea.width(), fm);

    if (text.isEmpty()) {
        painter->restore();
        return;
    }

    QTextLayout layout(text, option.font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    line.setLineWidth(1e6);
    layout.endLayout();

    if (!line.isValid()) {
        painter->restore();
        return;
    }

    const qreal lineH = line.height();
    const int rowTop = textArea.top() + (textArea.height() - qRound(lineH)) / 2;
    const qreal baselineY = rowTop + line.ascent();
    const qreal left = textArea.left();

    const QString lowerText = text.toLower();
    const QString lowerNeedle = m_needle.toLower();
    int last = 0;

    while (last < text.size()) {
        const int hit = lowerText.indexOf(lowerNeedle, last);
        if (hit < 0) {
            painter->setPen(normalPen);
            painter->drawText(QPointF(left + line.cursorToX(last), baselineY), text.mid(last));
            break;
        }
        if (hit > last) {
            const QString before = text.mid(last, hit - last);
            painter->setPen(normalPen);
            painter->drawText(QPointF(left + line.cursorToX(last), baselineY), before);
        }
        const int matchLen = m_needle.size();
        const qreal x1 = left + line.cursorToX(hit);
        const qreal x2 = left + line.cursorToX(hit + matchLen);
        const QRectF hlRect(x1, qreal(rowTop), qMax(1.0, x2 - x1), lineH);

        painter->setPen(Qt::NoPen);
        painter->setBrush(hiBg);
        painter->drawRoundedRect(hlRect, 2.0, 2.0);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(hiOutline, 1));
        painter->drawRoundedRect(hlRect.adjusted(0.5, 0.5, -0.5, -0.5), 2.0, 2.0);

        const QString matched = text.mid(hit, matchLen);
        painter->setPen(hiFg);
        painter->drawText(QPointF(left + line.cursorToX(hit), baselineY), matched);
        painter->setPen(normalPen);
        last = hit + matchLen;
    }

    painter->restore();
}
