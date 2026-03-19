// QCodeEditor
#include "QCECompleter.hpp"
#include <QLineNumberArea.hpp>
#include <QSyntaxStyle.hpp>
#include <QCodeEditor.hpp>
#include <QStyleSyntaxHighlighter.hpp>
#include <QFramedTextAttribute.hpp>
#include <QCXXHighlighter.hpp>


// Qt
#include <QTextBlock>
#include <QPaintEvent>
#include <QFontDatabase>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QTextCharFormat>
#include <QCursor>
#include <QCompleter>
#include <QAbstractItemView>
#include <QShortcut>
#include <QMimeData>

static QVector<QPair<QString, QString>> parentheses = {
    {"(", ")"},
    {"{", "}"},
    {"[", "]"},
    {"\"", "\""},
    {"'", "'"}
};

QCodeEditor::QCodeEditor(QString ext, QWidget* widget) :
    QPlainTextEdit(widget),
    m_highlighter(nullptr),
    m_syntaxStyle(nullptr),
    m_lineNumberArea(new QLineNumberArea(this)),
    m_completer(nullptr),
    m_framedAttribute(new QFramedTextAttribute(this)),
    m_autoIndentation(true),
    m_autoParentheses(true),
    m_replaceTab(true),
    m_tabReplace(QString(4, ' '))
{
    initLanguages();
    initDocumentLayoutHandlers();
    initFont();
    performConnections();

    this->setSyntaxStyle(m_styles["default"]);
    this->setCompleter  (m_completers[ext]);
    this->setHighlighter(m_highlighters[ext]);
}

void QCodeEditor::initLanguages(){
    // C / C++
    m_completers["c"]   = new QCECompleter(":/languages/c.xml");
    m_completers["h"]   = new QCECompleter(":/languages/c.xml");
    m_completers["cpp"] = new QCECompleter(":/languages/cpp.xml");
    m_completers["hpp"] = new QCECompleter(":/languages/cpp.xml");

    // Assembler (Intel / AT&T) — один словарь для .asm и .s
    m_completers["asm"] = new QCECompleter(":/languages/asm.xml");
    m_completers["s"]   = new QCECompleter(":/languages/asm.xml");

    // Rust
    m_completers["rs"]  = new QCECompleter(":/languages/rust.xml");

    // Build / config
    m_completers[""]        = new QCECompleter(":/languages/gnumake.xml"); // "Makefile" (no extension)
    m_completers["mk"]      = new QCECompleter(":/languages/gnumake.xml");
    m_completers["make"]    = new QCECompleter(":/languages/gnumake.xml");
    m_completers["cmake"]   = new QCECompleter(":/languages/cmake.xml");
    m_completers["txt"]     = new QCECompleter(":/languages/cmake.xml");    // CMakeLists.txt

    m_highlighters["c"]   = new QCXXHighlighter;
    m_highlighters["h"]   = new QCXXHighlighter;
    m_highlighters["cpp"] = new QCXXHighlighter;
    m_highlighters["hpp"] = new QCXXHighlighter;
    m_highlighters["asm"] = new QCXXHighlighter;
    m_highlighters["s"]   = new QCXXHighlighter;
    m_highlighters["rs"]  = new QCXXHighlighter;
    m_highlighters[""]    = new QCXXHighlighter;
    m_highlighters["mk"]  = new QCXXHighlighter;
    m_highlighters["make"]= new QCXXHighlighter;
    m_highlighters["cmake"]= new QCXXHighlighter;
    m_highlighters["txt"] = new QCXXHighlighter;

    m_styles["default"] = QSyntaxStyle::defaultStyle();
}

void QCodeEditor::initDocumentLayoutHandlers()
{
    document()
        ->documentLayout()
        ->registerHandler(
            QFramedTextAttribute::type(),
            m_framedAttribute
        );
}

void QCodeEditor::initFont()
{
    auto fnt = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fnt.setFixedPitch(true);
    fnt.setPointSize(12);

    setFont(fnt);
}

void QCodeEditor::performConnections()
{
    connect(this,
            &QPlainTextEdit::updateRequest,
            this,
            &QCodeEditor::updateLineNumberArea);

    connect(
        document(),
        &QTextDocument::blockCountChanged,
        this,
        &QCodeEditor::updateLineNumberAreaWidth
    );

    connect(
        this,
        &QPlainTextEdit::cursorPositionChanged,
        this,
        &QCodeEditor::updateExtraSelection
    );

    connect(
        this,
        &QPlainTextEdit::selectionChanged,
        this,
        &QCodeEditor::onSelectionChanged
    );

}

void QCodeEditor::setHighlighter(QStyleSyntaxHighlighter* highlighter)
{
    if (m_highlighter)
    {
        m_highlighter->setDocument(nullptr);
    }

    m_highlighter = highlighter;

    if (m_highlighter)
    {
        m_highlighter->setSyntaxStyle(m_syntaxStyle);
        m_highlighter->setDocument(document());
    }
}

void QCodeEditor::setSyntaxStyle(QSyntaxStyle* style)
{
    m_syntaxStyle = style;

    m_framedAttribute->setSyntaxStyle(m_syntaxStyle);
    m_lineNumberArea->setSyntaxStyle(m_syntaxStyle);

    if (m_highlighter)
    {
        m_highlighter->setSyntaxStyle(m_syntaxStyle);
    }

    updateStyle();
}

void QCodeEditor::updateStyle()
{
    if (m_highlighter)
    {
        m_highlighter->rehighlight();
    }

    if (m_syntaxStyle)
    {
        auto currentPalette = palette();

        // Setting text format/color
        currentPalette.setColor(
            QPalette::ColorRole::Text,
            m_syntaxStyle->getFormat("Text").foreground().color()
        );

        // Setting common background
        currentPalette.setColor(
            QPalette::Base,
            m_syntaxStyle->getFormat("Text").background().color()
        );

        // Setting selection color
        currentPalette.setColor(
            QPalette::Highlight,
            m_syntaxStyle->getFormat("Selection").background().color()
        );

        setPalette(currentPalette);
    }

    updateExtraSelection();
}

void QCodeEditor::onSelectionChanged()
{
    auto selected = textCursor().selectedText();

    auto cursor = textCursor();

    // Cursor is null if setPlainText was called.
    if (cursor.isNull())
    {
        return;
    }

    cursor.movePosition(QTextCursor::MoveOperation::Left);
    cursor.select(QTextCursor::SelectionType::WordUnderCursor);

    QSignalBlocker blocker(this);
    m_framedAttribute->clear(cursor);

    if (selected.size() > 1 &&
        cursor.selectedText() == selected)
    {
        auto backup = textCursor();

        // Perform search selecting
        handleSelectionQuery(cursor);

        setTextCursor(backup);
    }
}

void QCodeEditor::resizeEvent(QResizeEvent* e)
{
    QPlainTextEdit::resizeEvent(e);

    updateLineGeometry();
}

void QCodeEditor::updateLineGeometry()
{
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(
        QRect(cr.left(),
              cr.top(),
              m_lineNumberArea->sizeHint().width(),
              cr.height()
        )
    );
}

void QCodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(m_lineNumberArea->sizeHint().width(), 0, 0, 0);
}

void QCodeEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy)
    {
        m_lineNumberArea->scroll(0, dy);
    }
    else
    {
        m_lineNumberArea->update(0,
                                 rect.y(),
                                 m_lineNumberArea->width(),
                                 rect.height());
    }

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void QCodeEditor::handleSelectionQuery(QTextCursor cursor)
{

    auto searchIterator = cursor;
    searchIterator.movePosition(QTextCursor::Start);
    searchIterator = document()->find(cursor.selectedText(), searchIterator);
    while (searchIterator.hasSelection())
    {
        m_framedAttribute->frame(searchIterator);

        searchIterator = document()->find(cursor.selectedText(), searchIterator);
    }
}

void QCodeEditor::updateExtraSelection()
{
    QList<QTextEdit::ExtraSelection> extra;

    highlightCurrentLine(extra);
    highlightParenthesis(extra);

    setExtraSelections(extra);
}

void QCodeEditor::highlightParenthesis(QList<QTextEdit::ExtraSelection>& extraSelection)
{
    auto currentSymbol = charUnderCursor();
    auto prevSymbol = charUnderCursor(-1);

    for (auto& pair : parentheses)
    {
        int direction;
        QChar counterSymbol;
        QChar activeSymbol;
        int position = textCursor().position();

        if (pair.first == currentSymbol)
        {
            direction = 1;
            counterSymbol = pair.second[0];
            activeSymbol = currentSymbol;
        }
        else if (pair.second == prevSymbol)
        {
            direction = -1;
            counterSymbol = pair.first[0];
            activeSymbol = prevSymbol;
            position--;
        }
        else
        {
            continue;
        }

        int counter = 1;

        // Safe bounds for QPlainTextEdit
        const int lastPos = document()->characterCount() - 1;
        while (counter != 0 && position > 0 && position < lastPos)
        {
            position += direction;

            QChar character = document()->characterAt(position);

            if (character == activeSymbol)
                ++counter;
            else if (character == counterSymbol)
                --counter;
        }

        // If found
        if (counter == 0)
        {
            QTextCharFormat format = m_syntaxStyle->getFormat("Parentheses");
            format.setForeground(QBrush());

            auto makeSelection = [&](int pos) -> QTextEdit::ExtraSelection {
                QTextEdit::ExtraSelection sel;
                sel.format = format;

                QTextCursor c = textCursor();
                c.clearSelection();

                // Move to the symbol
                if (pos < textCursor().position())
                    c.setPosition(pos, QTextCursor::KeepAnchor);
                else
                    c.setPosition(pos, QTextCursor::MoveAnchor);

                // Select one character
                c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
                sel.cursor = c;

                return sel;
            };

            extraSelection.append(makeSelection(direction == 1 ? textCursor().position() : textCursor().position() - 1));
            extraSelection.append(makeSelection(position));
        }

        break; // Only first matching pair
    }
}

void QCodeEditor::highlightCurrentLine(QList<QTextEdit::ExtraSelection>& extraSelection)
{
    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection{};

        selection.format = m_syntaxStyle->getFormat("CurrentLine");
        selection.format.setForeground(QBrush());
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();

        extraSelection.append(selection);
    }
}

void QCodeEditor::paintEvent(QPaintEvent* e)
{
    // updateLineNumberArea(e->rect());
    QPlainTextEdit::paintEvent(e);
}

int QCodeEditor::getFirstVisibleBlock()
{
    // Detect the first block for which bounding rect - once translated
    // in absolute coordinated - is contained by the editor's text area

    // Costly way of doing but since "blockBoundingGeometry(...)" doesn't
    // exists for "QTextEdit"...

    QTextCursor curs = QTextCursor(document());
    curs.movePosition(QTextCursor::Start);
    for(int i=0; i < document()->blockCount(); ++i)
    {
        QTextBlock block = curs.block();

        QRect r1 = viewport()->geometry();
        QRect r2 = document()
            ->documentLayout()
            ->blockBoundingRect(block)
            .translated(
                viewport()->geometry().x(),
                viewport()->geometry().y() - verticalScrollBar()->sliderPosition()
            ).toRect();

        if (r1.intersects(r2))
        {
            return i;
        }

        curs.movePosition(QTextCursor::NextBlock);
    }

    return 0;
}

bool QCodeEditor::proceedCompleterBegin(QKeyEvent *e)
{
    if (m_completer &&
        m_completer->popup()->isVisible())
    {
        switch (e->key())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return true; // let the completer do default behavior
        default:
            break;
        }
    }

    // todo: Replace with modifiable QShortcut
    auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);

    return !(!m_completer || !isShortcut);

}

void QCodeEditor::proceedCompleterEnd(QKeyEvent *e)
{
    auto ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);

    if (!m_completer ||
        (ctrlOrShift && e->text().isEmpty()) ||
        e->key() == Qt::Key_Delete)
    {
        return;
    }

    static QString eow(R"(~!@#$%^&*()_+{}|:"<>?,./;'[]\-=)");

    auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);
    auto completionPrefix = wordUnderCursor();

    if (!isShortcut &&
        (e->text().isEmpty() ||
         completionPrefix.length() < 2 ||
         eow.contains(e->text().right(1))))
    {
        m_completer->popup()->hide();
        return;
    }

    if (completionPrefix != m_completer->completionPrefix())
    {
        m_completer->setCompletionPrefix(completionPrefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    auto cursRect = cursorRect();
    cursRect.setWidth(
        m_completer->popup()->sizeHintForColumn(0) +
        m_completer->popup()->verticalScrollBar()->sizeHint().width()
    );

    m_completer->complete(cursRect);
}

void QCodeEditor::keyPressEvent(QKeyEvent* e) {
#if QT_VERSION >= 0x050A00
  const int defaultIndent = tabStopDistance() / fontMetrics().averageCharWidth();
#else
  const int defaultIndent = tabStopWidth() / fontMetrics().averageCharWidth();
#endif

  auto completerSkip = proceedCompleterBegin(e);

  if (!completerSkip) {

    // Ctrl+Plus / Ctrl+Minus zoom
    if ((e->modifiers() & Qt::ControlModifier) &&
        (e->key() == Qt::Key_Plus || e->key() == Qt::Key_Equal || e->key() == Qt::Key_Minus)) {
      double delta = (e->key() == Qt::Key_Minus) ? 1 / 1.1 : 1.1;
      scaleFactor *= delta;
      QFont f = font();
      f.setPointSizeF(12 * scaleFactor);
      setFont(f);
      QFontMetrics fm(f);
      double tabWidth = fm.horizontalAdvance(' ') * 4;
      QTextOption opt = document()->defaultTextOption();
      opt.setTabStopDistance(tabWidth);
      document()->setDefaultTextOption(opt);
      document()->markContentsDirty(0, document()->characterCount());
      viewport()->update();
      return;
    }

    // Replace: tab -> 4 space
    if (m_replaceTab && e->key() == Qt::Key_Tab &&
        e->modifiers() == Qt::NoModifier) {
      insertPlainText(m_tabReplace);
      return;
    }

    // Auto indentation
    int indentationLevel = getIndentationSpaces();

#if QT_VERSION >= 0x050A00
    int tabCounts =
        indentationLevel * fontMetrics().averageCharWidth() / tabStopDistance();
#else
    int tabCounts =
        indentationLevel * fontMetrics().averageCharWidth() / tabStopWidth();
#endif

    // Have Qt Edior like behaviour, if {|} and enter is pressed indent the two
    // parenthesis
    if (m_autoIndentation && 
       (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) &&
        charUnderCursor() == '}' && charUnderCursor(-1) == '{') 
    {
      int charsBack = 0;
      insertPlainText("\n");
      if (m_replaceTab)
        insertPlainText(m_tabReplace);
      else
        insertPlainText(QString('\t'));

      insertPlainText("\n");
      charsBack++;

      // if (m_replaceTab)
      // {
      //   insertPlainText(QString(indentationLevel, ' '));
      //   charsBack += indentationLevel;
      // }
      // else
      // {
      //   insertPlainText(QString(tabCounts, '\t'));
      //   charsBack += tabCounts;
      // }

      while (charsBack--)
        moveCursor(QTextCursor::MoveOperation::Left);
      return;
    }

    // Shortcut for moving line to left
    if (m_replaceTab && e->key() == Qt::Key_Backtab) {
      indentationLevel = qMin(indentationLevel, m_tabReplace.size());

      auto cursor = textCursor();

      cursor.movePosition(QTextCursor::MoveOperation::StartOfLine);
      cursor.movePosition(QTextCursor::MoveOperation::Right,
                          QTextCursor::MoveMode::KeepAnchor, indentationLevel);

      cursor.removeSelectedText();
      return;
    }

    QPlainTextEdit::keyPressEvent(e);

    if (m_autoIndentation && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
      if (m_replaceTab)
        insertPlainText(QString(indentationLevel, ' '));
      else
        insertPlainText(QString(tabCounts, '\t'));
    }

    if (m_autoParentheses) 
    {
      for (auto&& el : parentheses) 
      {
                // Inserting closed brace
                if (el.first == e->text()) 
                {
                  insertPlainText(el.second);
                  moveCursor(QTextCursor::MoveOperation::Left);
                  break;
                }

                // If it's close brace - check parentheses
                if (el.second == e->text())
                {
                    auto symbol = charUnderCursor();

                    if (symbol == el.second)
                    {
                        textCursor().deletePreviousChar();
                        moveCursor(QTextCursor::MoveOperation::Right);
                    }

                    break;
                }
            }
        }
    }

    proceedCompleterEnd(e);
}

void QCodeEditor::setAutoIndentation(bool enabled)
{
    m_autoIndentation = enabled;
}

bool QCodeEditor::autoIndentation() const
{
    return m_autoIndentation;
}

void QCodeEditor::setAutoParentheses(bool enabled)
{
    m_autoParentheses = enabled;
}

bool QCodeEditor::autoParentheses() const
{
    return m_autoParentheses;
}

void QCodeEditor::setTabReplace(bool enabled)
{
    m_replaceTab = enabled;
}

bool QCodeEditor::tabReplace() const
{
    return m_replaceTab;
}

void QCodeEditor::setTabReplaceSize(int val)
{
    m_tabReplace.clear();

    m_tabReplace.fill(' ', val);
}

int QCodeEditor::tabReplaceSize() const
{
    return m_tabReplace.size();
}

void QCodeEditor::setCompleter(QCompleter *completer)
{
    if (m_completer)
    {
        disconnect(m_completer, nullptr, this, nullptr);
    }

    m_completer = completer;

    if (!m_completer)
    {
        return;
    }

    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::CompletionMode::PopupCompletion);

    connect(
        m_completer,
        QOverload<const QString&>::of(&QCompleter::activated),
        this,
        &QCodeEditor::insertCompletion
    );
}

void QCodeEditor::focusInEvent(QFocusEvent *e)
{
    if (m_completer)
    {
        m_completer->setWidget(this);
    }

    QPlainTextEdit::focusInEvent(e);
}

void QCodeEditor::insertCompletion(QString s)
{
    if (m_completer->widget() != this)
    {
        return;
    }

    auto tc = textCursor();
    tc.select(QTextCursor::SelectionType::WordUnderCursor);
    tc.insertText(s);
    setTextCursor(tc);
}

QCompleter *QCodeEditor::completer() const
{
    return m_completer;
}

QChar QCodeEditor::charUnderCursor(int offset) const
{
    auto block = textCursor().blockNumber();
    auto index = textCursor().positionInBlock();
    auto text = document()->findBlockByNumber(block).text();

    index += offset;

    if (index < 0 || index >= text.size())
    {
        return {};
    }

    return text[index];
}

QString QCodeEditor::wordUnderCursor() const
{
    auto tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

void QCodeEditor::insertFromMimeData(const QMimeData* source)
{
    insertPlainText(source->text());
}

int QCodeEditor::getIndentationSpaces()
{
    auto blockText = textCursor().block().text();

    int indentationLevel = 0;

    for (auto i = 0;
         i < blockText.size() && QString("\t ").contains(blockText[i]);
         ++i)
    {
        if (blockText[i] == ' ')
        {
            indentationLevel++;
        }
        else
        {
#if QT_VERSION >= 0x050A00
            indentationLevel += tabStopDistance() / fontMetrics().averageCharWidth();
#else
            indentationLevel += tabStopWidth() / fontMetrics().averageCharWidth();
#endif
        }
    }

    return indentationLevel;
}
