#include "widgets/LineNumberArea.h"
#include "widgets/CustomCodeEditor.h"
#include <QPainter>
#include <QTextBlock>

LineNumberArea::LineNumberArea(CustomCodeEditor* editor)
    : QWidget(editor)
    , m_editor(editor) {
}

QSize LineNumberArea::sizeHint() const {
    return QSize(calculateWidth(), 0);
}

int LineNumberArea::calculateWidth() const {
    if (!m_editor) {
        return 0;
    }
    
    qint64 lineCount = m_editor->lineCount();
    if (lineCount == 0) {
        lineCount = 1;
    }
    
    // Calculate number of digits needed
    int digits = 1;
    qint64 max = lineCount;
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    
    // Calculate width based on font metrics
    QFontMetrics fm(font());
    int space = 3 + fm.horizontalAdvance(QLatin1Char('9')) * digits + 3;
    
    return space;
}

void LineNumberArea::paintEvent(QPaintEvent* event) {
    if (m_editor) {
        m_editor->lineNumberAreaPaintEvent(event);
    }
}
