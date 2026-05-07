/* QCodeEditor */
#include <QLineNumberArea.hpp>
#include <QSyntaxStyle.hpp>
#include <QCodeEditor.hpp>

#include <QTextEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>

QLineNumberArea::QLineNumberArea(QCodeEditor* parent) :
    QWidget(parent),
    m_syntaxStyle(nullptr),
    m_codeEditParent(parent)
{

}

QSize QLineNumberArea::sizeHint() const
{
    if (m_codeEditParent == nullptr)
    {
        return QWidget::sizeHint();
    }

    /* Calculating width */
    int digits = 1;
    int max = qMax(1, m_codeEditParent->document()->blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

#if QT_VERSION >= 0x050B00
    int space = 13 + m_codeEditParent->fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
#else
    int space = 13 + m_codeEditParent->fontMetrics().width(QLatin1Char('9')) * digits;
#endif

    return {space, 0};
}

void QLineNumberArea::setSyntaxStyle(QSyntaxStyle* style)
{
    m_syntaxStyle = style;
}

QSyntaxStyle* QLineNumberArea::syntaxStyle() const
{
    return m_syntaxStyle;
}

void QLineNumberArea::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    painter.fillRect(
        event->rect(),
        m_syntaxStyle->getFormat("Text").background().color()
        );

    QTextBlock block = m_codeEditParent->firstVisibleBlock();
    int blockNumber = block.blockNumber();

    int top = m_codeEditParent->blockBoundingGeometry(block)
                  .translated(m_codeEditParent->contentOffset())
                  .top();

    int bottom = top + m_codeEditParent->blockBoundingRect(block).height();

    auto currentLine = m_syntaxStyle->getFormat("CurrentLineNumber").foreground().color();
    auto otherLines  = m_syntaxStyle->getFormat("LineNumber").foreground().color();

    painter.setFont(m_codeEditParent->font());

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(blockNumber + 1);

            bool isCurrentLine =
                m_codeEditParent->textCursor().blockNumber() == blockNumber;

            painter.setPen(isCurrentLine ? currentLine : otherLines);

            painter.drawText(
                0,
                top,
                width() - 4,
                m_codeEditParent->fontMetrics().height(),
                Qt::AlignRight,
                number
                );
        }

        block = block.next();
        top = bottom;
        bottom = top + m_codeEditParent->blockBoundingRect(block).height();
        ++blockNumber;
    }
}
