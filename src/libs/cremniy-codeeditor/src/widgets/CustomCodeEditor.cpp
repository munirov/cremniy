#include "widgets/CustomCodeEditor.h"

#include "widgets/EditorEditOperations.h"
#include "widgets/EditorLanguageSupport.h"
#include "widgets/EditorTypingBehaviors.h"
#include "widgets/LineNumberArea.h"
#include "QCXXHighlighter.hpp"
#include "QJSONHighlighter.hpp"
#include "QLanguage.hpp"
#include "QSyntaxStyle.hpp"
#include "QStyleSyntaxHighlighter.hpp"
#include "core/FileDataBuffer.h"
#include "core/LineCache.h"
#include "core/LineIndex.h"
#include "core/UTF8Decoder.h"

#include <QApplication>
#include <QActionGroup>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QMenu>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTimer>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>

namespace {
const QByteArray kUtf8Bom("\xEF\xBB\xBF", 3);
constexpr bool kLogInputEvents = true;
constexpr int kTextLeftPadding = 4;

QString eventTypeName(QEvent::Type type)
{
    switch (type) {
    case QEvent::ShortcutOverride:
        return QStringLiteral("ShortcutOverride");
    case QEvent::KeyPress:
        return QStringLiteral("KeyPress");
    case QEvent::KeyRelease:
        return QStringLiteral("KeyRelease");
    case QEvent::InputMethod:
        return QStringLiteral("InputMethod");
    case QEvent::MouseButtonPress:
        return QStringLiteral("MouseButtonPress");
    case QEvent::MouseButtonDblClick:
        return QStringLiteral("MouseButtonDblClick");
    default:
        return QStringLiteral("Event(%1)").arg(static_cast<int>(type));
    }
}

QString modifiersToString(Qt::KeyboardModifiers modifiers)
{
    QStringList parts;
    if (modifiers.testFlag(Qt::ShiftModifier))
        parts.append(QStringLiteral("Shift"));
    if (modifiers.testFlag(Qt::ControlModifier))
        parts.append(QStringLiteral("Ctrl"));
    if (modifiers.testFlag(Qt::AltModifier))
        parts.append(QStringLiteral("Alt"));
    if (modifiers.testFlag(Qt::MetaModifier))
        parts.append(QStringLiteral("Meta"));
    return parts.isEmpty() ? QStringLiteral("None") : parts.join(QLatin1Char('|'));
}

void logKeyEvent(const char* origin, QEvent* event, const QKeyEvent* keyEvent, qint64 cursorBytePos)
{
    if (!kLogInputEvents || !keyEvent)
        return;

    qDebug().noquote()
        << QStringLiteral("[CustomCodeEditor][%1] type=%2 key=%3 text='%4' mods=%5 accepted=%6 cursor=%7")
              .arg(QString::fromLatin1(origin),
                   eventTypeName(event->type()),
                   QString::number(keyEvent->key()),
                   keyEvent->text(),
                   modifiersToString(keyEvent->modifiers()),
                   event->isAccepted() ? QStringLiteral("true") : QStringLiteral("false"),
                   QString::number(cursorBytePos));
}

void logEditRange(const char* origin, qint64 start, qint64 length,
                  const QByteArray& removedBytes, const QByteArray& replacement)
{
    if (!kLogInputEvents)
        return;

    qDebug().noquote()
        << QStringLiteral("[CustomCodeEditor][%1] start=%2 length=%3 removedHex=%4 removedUtf8='%5' replacementHex=%6 replacementUtf8='%7'")
              .arg(QString::fromLatin1(origin),
                   QString::number(start),
                   QString::number(length),
                   QString::fromLatin1(removedBytes.toHex(' ')),
                   QString::fromUtf8(removedBytes),
                   QString::fromLatin1(replacement.toHex(' ')),
                   QString::fromUtf8(replacement));
}

} // namespace

namespace {

struct RegexRule {
    QRegularExpression pattern;
    QString formatName;
};

struct WrappedSegment {
    int startColumn;
    int length;
    qreal x;
    qreal width;
};

struct DisplayTextLayout {
    QString text;
    QVector<int> rawToVisual;
    QVector<int> visualToRaw;
};

static bool isWordCharacter(QChar ch)
{
    return ch.isLetterOrNumber() || ch == QLatin1Char('_');
}

DisplayTextLayout buildDisplayTextLayout(const QString& text, int tabSize)
{
    DisplayTextLayout layout;
    layout.rawToVisual.reserve(text.size() + 1);
    layout.visualToRaw.reserve(text.size() + 1);

    layout.rawToVisual.append(0);
    int visualColumn = 0;
    for (int rawColumn = 0; rawColumn < text.size(); ++rawColumn) {
        const QChar ch = text.at(rawColumn);
        if (ch == QLatin1Char('\t')) {
            const int spaces = qMax(1, tabSize - (visualColumn % qMax(1, tabSize)));
            for (int i = 0; i < spaces; ++i) {
                layout.text.append(QLatin1Char(' '));
                layout.visualToRaw.append(rawColumn);
                ++visualColumn;
            }
        } else {
            layout.text.append(ch);
            layout.visualToRaw.append(rawColumn);
            ++visualColumn;
        }
        layout.rawToVisual.append(visualColumn);
    }

    layout.visualToRaw.append(text.size());
    return layout;
}

QVector<WrappedSegment> buildWrappedSegments(const QString& text, const QFont& font, int width, bool wordWrapEnabled)
{
    QVector<WrappedSegment> segments;
    if (text.isEmpty()) {
        segments.append({0, 0, 0.0, 0.0});
        return segments;
    }

    if (!wordWrapEnabled || width <= 0) {
        const qreal fullWidth = QFontMetricsF(font).horizontalAdvance(text);
        segments.append({0, static_cast<int>(text.size()), 0.0, fullWidth});
        return segments;
    }

    QTextLayout layout(text, font);
    layout.beginLayout();
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;

        line.setLineWidth(width);
        line.setPosition(QPointF(0.0, 0.0));
        segments.append({line.textStart(), line.textLength(), line.x(), line.naturalTextWidth()});
    }
    layout.endLayout();

    if (segments.isEmpty())
        segments.append({0, static_cast<int>(text.size()), 0.0, 0.0});

    return segments;
}

class RuleBasedHighlighter : public QStyleSyntaxHighlighter {
public:
    RuleBasedHighlighter(QVector<RegexRule> rules,
                         const QRegularExpression& commentPattern,
                         QTextDocument* document = nullptr)
        : QStyleSyntaxHighlighter(document)
        , m_rules(rules)
        , m_commentPattern(commentPattern)
    {
    }

protected:
    void highlightBlock(const QString& text) override
    {
        for (const auto& rule : m_rules) {
            auto matches = rule.pattern.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(rule.formatName));
            }
        }

        if (m_commentPattern.isValid()) {
            auto matches = m_commentPattern.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Comment")));
            }
        }
    }

private:
    QVector<RegexRule> m_rules;
    QRegularExpression m_commentPattern;
};

class XmlLanguageHighlighter : public QStyleSyntaxHighlighter {
public:
    XmlLanguageHighlighter(const QString& resourcePath,
                           const QRegularExpression& singleLineComment,
                           const QRegularExpression& stringPattern,
                           const QRegularExpression& numberPattern,
                           QTextDocument* document = nullptr)
        : QStyleSyntaxHighlighter(document)
        , m_singleLineComment(singleLineComment)
        , m_stringPattern(stringPattern)
        , m_numberPattern(numberPattern)
    {
        QFile file(resourcePath);
        if (!file.open(QIODevice::ReadOnly))
            return;

        QLanguage language(&file);
        if (!language.isLoaded())
            return;

        for (const QString& key : language.keys()) {
            const QString formatName = key == "Directive" || key == "Command" || key == "Variable" || key == "BuiltinFunction"
                                           ? QStringLiteral("Keyword")
                                           : key;
            for (const QString& name : language.names(key)) {
                const QString escaped = QRegularExpression::escape(name);
                m_rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(escaped)), formatName});
            }
        }
    }

protected:
    void highlightBlock(const QString& text) override
    {
        if (m_numberPattern.isValid()) {
            auto matches = m_numberPattern.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Number")));
            }
        }

        if (m_stringPattern.isValid()) {
            auto matches = m_stringPattern.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("String")));
            }
        }

        for (const auto& rule : m_rules) {
            auto matches = rule.pattern.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(rule.formatName));
            }
        }

        if (m_singleLineComment.isValid()) {
            auto matches = m_singleLineComment.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Comment")));
            }
        }
    }

private:
    struct Rule {
        QRegularExpression pattern;
        QString formatName;
    };

    QVector<Rule> m_rules;
    QRegularExpression m_singleLineComment;
    QRegularExpression m_stringPattern;
    QRegularExpression m_numberPattern;
};

class MarkdownHighlighter : public QStyleSyntaxHighlighter {
public:
    explicit MarkdownHighlighter(QTextDocument* document = nullptr)
        : QStyleSyntaxHighlighter(document)
    {
    }

protected:
    void highlightBlock(const QString& text) override
    {
        static const QRegularExpression headingPattern(QStringLiteral("^\\s*#{1,6}\\s.*$"));
        static const QRegularExpression listPattern(QStringLiteral("^\\s*([-*+]\\s|\\d+\\.\\s).*$"));
        static const QRegularExpression codeFencePattern(QStringLiteral("^\\s*```.*$"));
        static const QRegularExpression inlineCodePattern(QStringLiteral("`[^`]+`"));
        static const QRegularExpression emphasisPattern(QStringLiteral("(\\*\\*[^*]+\\*\\*|__[^_]+__|\\*[^*]+\\*|_[^_]+_)") );
        static const QRegularExpression linkPattern(QStringLiteral("\\[[^\\]]+\\]\\([^)]+\\)"));

        auto applyAll = [this, &text](const QRegularExpression& pattern, const QString& styleName) {
            auto matches = pattern.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(styleName));
            }
        };

        applyAll(headingPattern, QStringLiteral("Keyword"));
        applyAll(listPattern, QStringLiteral("Preprocessor"));
        applyAll(codeFencePattern, QStringLiteral("Comment"));
        applyAll(inlineCodePattern, QStringLiteral("String"));
        applyAll(emphasisPattern, QStringLiteral("Type"));
        applyAll(linkPattern, QStringLiteral("Function"));
    }
};

class MakefileHighlighter : public QStyleSyntaxHighlighter {
public:
    explicit MakefileHighlighter(QTextDocument* document = nullptr)
        : QStyleSyntaxHighlighter(document)
    {
    }

protected:
    void highlightBlock(const QString& text) override
    {
        static const QRegularExpression commentPattern(QStringLiteral("#[^\\n]*"));
        static const QRegularExpression stringPattern(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'"));
        static const QRegularExpression variablePattern(QStringLiteral("\\$\\((?:[^()\\\\]|\\\\.)+\\)|\\$\\{(?:[^{}\\\\]|\\\\.)+\\}|\\$[@%<?^+*|]"));
        static const QRegularExpression directivePattern(QStringLiteral("^\\s*(include|ifdef|ifndef|else|endif|ifeq|ifneq|define|endef|override|export|unexport)\\b"));
        static const QRegularExpression assignmentPattern(QStringLiteral("^[^:#=\\s][^:=#]*\\s*(?:[:+?]?=)"));
        static const QRegularExpression targetPattern(QStringLiteral("^(?:\\.[A-Z_][A-Z0-9_]*|[^:#=\\s][^:=#]*)\\s*:"));
        static const QRegularExpression functionPattern(QStringLiteral("\\$\\((subst|patsubst|strip|findstring|filter|filter-out|sort|word|wordlist|words|firstword|lastword|dir|notdir|suffix|basename|addsuffix|addprefix|join|foreach|if|or|and|call|value|eval|file|shell|wildcard|error|warn|info)\\b"));

        auto applyAll = [this, &text](const QRegularExpression& pattern, const QString& styleName, int captureGroup = 0) {
            auto matches = pattern.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                const int start = captureGroup == 0 ? match.capturedStart() : match.capturedStart(captureGroup);
                const int length = captureGroup == 0 ? match.capturedLength() : match.capturedLength(captureGroup);
                if (start >= 0 && length > 0)
                    setFormat(start, length, syntaxStyle()->getFormat(styleName));
            }
        };

        applyAll(stringPattern, QStringLiteral("String"));
        applyAll(variablePattern, QStringLiteral("PrimitiveType"));
        applyAll(directivePattern, QStringLiteral("Keyword"), 1);
        applyAll(functionPattern, QStringLiteral("Function"), 1);

        const auto assignmentMatch = assignmentPattern.match(text);
        if (assignmentMatch.hasMatch())
            setFormat(assignmentMatch.capturedStart(), assignmentMatch.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Function")));

        const auto targetMatch = targetPattern.match(text);
        if (targetMatch.hasMatch())
            setFormat(targetMatch.capturedStart(), targetMatch.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Keyword")));

        applyAll(commentPattern, QStringLiteral("Comment"));
    }
};

QVector<RegexRule> buildWordRules(const QStringList& words, const QString& formatName)
{
    QVector<RegexRule> rules;
    for (const QString& word : words)
        rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(word))), formatName});
    return rules;
}

RuleBasedHighlighter* createCommonLanguageHighlighter(const QString& syntaxKey, QTextDocument* document)
{
    QVector<RegexRule> rules = {
        {QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")), QStringLiteral("String")},
        {QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")), QStringLiteral("Number")}
    };
    QRegularExpression commentPattern;

    if (syntaxKey == QStringLiteral("js") || syntaxKey == QStringLiteral("ts")) {
        rules += buildWordRules({QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"), QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("else"), QStringLiteral("export"), QStringLiteral("extends"), QStringLiteral("finally"), QStringLiteral("for"), QStringLiteral("from"), QStringLiteral("function"), QStringLiteral("if"), QStringLiteral("import"), QStringLiteral("in"), QStringLiteral("instanceof"), QStringLiteral("let"), QStringLiteral("new"), QStringLiteral("return"), QStringLiteral("super"), QStringLiteral("switch"), QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("try"), QStringLiteral("typeof"), QStringLiteral("var"), QStringLiteral("while"), QStringLiteral("yield"), QStringLiteral("async"), QStringLiteral("await")}, QStringLiteral("Keyword"));
        rules += buildWordRules({QStringLiteral("string"), QStringLiteral("number"), QStringLiteral("boolean"), QStringLiteral("void"), QStringLiteral("null"), QStringLiteral("undefined"), QStringLiteral("any"), QStringLiteral("unknown"), QStringLiteral("never")}, QStringLiteral("PrimitiveType"));
        commentPattern = QRegularExpression(QStringLiteral("//[^\\n]*"));
    } else if (syntaxKey == QStringLiteral("java") || syntaxKey == QStringLiteral("cs") || syntaxKey == QStringLiteral("go") || syntaxKey == QStringLiteral("php")) {
        const QStringList keywords = syntaxKey == QStringLiteral("go")
            ? QStringList{QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("chan"), QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("defer"), QStringLiteral("else"), QStringLiteral("fallthrough"), QStringLiteral("for"), QStringLiteral("func"), QStringLiteral("go"), QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("import"), QStringLiteral("interface"), QStringLiteral("map"), QStringLiteral("package"), QStringLiteral("range"), QStringLiteral("return"), QStringLiteral("select"), QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("type"), QStringLiteral("var")}
            : syntaxKey == QStringLiteral("php")
                ? QStringList{QStringLiteral("class"), QStringLiteral("function"), QStringLiteral("public"), QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("elseif"), QStringLiteral("return"), QStringLiteral("foreach"), QStringLiteral("while"), QStringLiteral("namespace"), QStringLiteral("use"), QStringLiteral("extends"), QStringLiteral("implements"), QStringLiteral("trait"), QStringLiteral("static"), QStringLiteral("new")}
                : QStringList{QStringLiteral("abstract"), QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("else"), QStringLiteral("enum"), QStringLiteral("extends"), QStringLiteral("finally"), QStringLiteral("for"), QStringLiteral("if"), QStringLiteral("implements"), QStringLiteral("import"), QStringLiteral("interface"), QStringLiteral("namespace"), QStringLiteral("new"), QStringLiteral("package"), QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("return"), QStringLiteral("static"), QStringLiteral("switch"), QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("try"), QStringLiteral("using"), QStringLiteral("while")};
        rules += buildWordRules(keywords, QStringLiteral("Keyword"));
        rules += buildWordRules({QStringLiteral("int"), QStringLiteral("long"), QStringLiteral("short"), QStringLiteral("float"), QStringLiteral("double"), QStringLiteral("bool"), QStringLiteral("boolean"), QStringLiteral("string"), QStringLiteral("char"), QStringLiteral("byte"), QStringLiteral("void")}, QStringLiteral("PrimitiveType"));
        commentPattern = QRegularExpression(QStringLiteral("//[^\\n]*"));
    } else if (syntaxKey == QStringLiteral("sh")) {
        rules += buildWordRules({QStringLiteral("if"), QStringLiteral("then"), QStringLiteral("else"), QStringLiteral("elif"), QStringLiteral("fi"), QStringLiteral("for"), QStringLiteral("do"), QStringLiteral("done"), QStringLiteral("while"), QStringLiteral("case"), QStringLiteral("esac"), QStringLiteral("function"), QStringLiteral("in"), QStringLiteral("export"), QStringLiteral("local"), QStringLiteral("readonly")}, QStringLiteral("Keyword"));
        rules += buildWordRules({QStringLiteral("echo"), QStringLiteral("cd"), QStringLiteral("test"), QStringLiteral("printf"), QStringLiteral("source")}, QStringLiteral("Function"));
        commentPattern = QRegularExpression(QStringLiteral("#[^\\n]*"));
    } else if (syntaxKey == QStringLiteral("yaml")) {
        rules += buildWordRules({QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"), QStringLiteral("yes"), QStringLiteral("no"), QStringLiteral("on"), QStringLiteral("off")}, QStringLiteral("Keyword"));
        rules.append({QRegularExpression(QStringLiteral("^\\s*[^:#\\n]+:(?=\\s|$)")), QStringLiteral("Function")});
        commentPattern = QRegularExpression(QStringLiteral("#[^\\n]*"));
    } else if (syntaxKey == QStringLiteral("toml")) {
        rules += buildWordRules({QStringLiteral("true"), QStringLiteral("false")}, QStringLiteral("Keyword"));
        rules.append({QRegularExpression(QStringLiteral("^\\s*\\[[^\\]]+\\]")), QStringLiteral("Type")});
        rules.append({QRegularExpression(QStringLiteral("^\\s*[A-Za-z0-9_.-]+(?=\\s*=)")), QStringLiteral("Function")});
        commentPattern = QRegularExpression(QStringLiteral("#[^\\n]*"));
    } else if (syntaxKey == QStringLiteral("ini")) {
        rules.append({QRegularExpression(QStringLiteral("^\\s*\\[[^\\]]+\\]")), QStringLiteral("Type")});
        rules.append({QRegularExpression(QStringLiteral("^\\s*[A-Za-z0-9_.-]+(?=\\s*=)")), QStringLiteral("Function")});
        commentPattern = QRegularExpression(QStringLiteral("^[;#][^\\n]*"));
    } else if (syntaxKey == QStringLiteral("sql")) {
        rules += buildWordRules({QStringLiteral("select"), QStringLiteral("from"), QStringLiteral("where"), QStringLiteral("insert"), QStringLiteral("into"), QStringLiteral("update"), QStringLiteral("delete"), QStringLiteral("join"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("inner"), QStringLiteral("outer"), QStringLiteral("group"), QStringLiteral("by"), QStringLiteral("order"), QStringLiteral("limit"), QStringLiteral("create"), QStringLiteral("table"), QStringLiteral("alter"), QStringLiteral("drop"), QStringLiteral("index"), QStringLiteral("values"), QStringLiteral("set"), QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("not"), QStringLiteral("null")}, QStringLiteral("Keyword"));
        commentPattern = QRegularExpression(QStringLiteral("--[^\\n]*"));
    } else if (syntaxKey == QStringLiteral("xml")) {
        rules.append({QRegularExpression(QStringLiteral("</?[A-Za-z_:][A-Za-z0-9:._-]*")), QStringLiteral("Keyword")});
        rules.append({QRegularExpression(QStringLiteral("\\b[A-Za-z_:][A-Za-z0-9:._-]*(?=\\=)")), QStringLiteral("Function")});
        rules.append({QRegularExpression(QStringLiteral("<!DOCTYPE[^>]*>|<\\?xml[^?]*\\?>")), QStringLiteral("Preprocessor")});
        commentPattern = QRegularExpression(QStringLiteral("<!--[^>]*-->"));
    } else if (syntaxKey == QStringLiteral("sln")) {
        rules += buildWordRules({QStringLiteral("Project"), QStringLiteral("EndProject"), QStringLiteral("Global"), QStringLiteral("EndGlobal"), QStringLiteral("GlobalSection"), QStringLiteral("EndGlobalSection")}, QStringLiteral("Keyword"));
        rules.append({QRegularExpression(QStringLiteral("\"[^\"]+\"")), QStringLiteral("String")});
        commentPattern = QRegularExpression(QStringLiteral("^#.*$"));
    }

    return new RuleBasedHighlighter(rules, commentPattern, document);
}
}

CustomCodeEditor::CustomCodeEditor(QWidget* parent)
    : QAbstractScrollArea(parent)
    , m_buffer(nullptr)
    , m_lineIndex(new LineIndex())
    , m_lineCache(new LineCache())
    , m_utf8Decoder(new UTF8Decoder())
    , m_highlighter(nullptr)
    , m_lineNumberArea(new LineNumberArea(this))
    , m_cursorBytePos(0)
    , m_selectionStart(0)
    , m_selectionLength(0)
    , m_updatingSelection(false)
    , m_applyingBufferEdit(false)
    , m_firstVisibleLine(0)
    , m_visibleLineCount(0)
    , m_scaleFactor(1.0)
    , m_font("Monospace", 10)
    , m_fontMetrics(m_font)
    , m_tabReplace(false)
    , m_tabReplaceSize(4)
    , m_tabDisplaySize(4)
    , m_hasUtf8Bom(false)
    , m_selectionAnchor(-1)
    , m_mouseSelecting(false)
    , m_clickCount(0)
    , m_lastClickTimestamp(0)
    , m_lastClickPosition()
    , m_pendingTripleClick(false)
    , m_highlightDocument(new QTextDocument(this))
    , m_syntaxStyle(QSyntaxStyle::defaultStyle())
    , m_savedVerticalScrollValue(0)
    , m_savedHorizontalScrollValue(0)
    , m_savedCursorBytePos(0)
    , m_restoreViewStatePending(false)
    , m_wordWrapEnabled(true)
    , m_wrapCacheWidth(-1)
    , m_layoutCacheValid(false)
    , m_layoutCacheComputedLines(0)
    , m_layoutCacheComputedVisualLines(0)
    , m_totalVisualLineCount(0)
    , m_maxExpandedLineLength(0)
    , m_highlightCacheStartLine(-1)
    , m_highlightCacheEndLine(-1)
    , m_editGroupTimer(new QTimer(this))
    , m_currentEditGroupType(EditGroupNone)
{
    initSyntaxSupport();
    setFont(m_font);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    viewport()->setAutoFillBackground(true);
    m_editGroupTimer->setSingleShot(true);
    connect(m_editGroupTimer, &QTimer::timeout, this, [this]() { endEditGrouping(); });
    m_lineNumberArea->setFont(m_font);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        viewport()->update();
        m_lineNumberArea->update();
    });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        viewport()->update();
    });

    updateLineNumberAreaWidth();
    rebuildHighlighterForCurrentExtension();
    applyEditorPalette();
}

CustomCodeEditor::~CustomCodeEditor()
{
    delete m_lineIndex;
    delete m_lineCache;
    delete m_utf8Decoder;
}

bool CustomCodeEditor::event(QEvent* event)
{
    if (event && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        logKeyEvent("event", event, keyEvent, m_cursorBytePos);
        if ((keyEvent->key() == Qt::Key_Space || keyEvent->text() == QStringLiteral(" "))
            && !keyEvent->modifiers().testFlag(Qt::ControlModifier)
            && !keyEvent->modifiers().testFlag(Qt::AltModifier)
            && !keyEvent->modifiers().testFlag(Qt::MetaModifier)) {
            if (event->type() == QEvent::KeyPress)
                insertText(QStringLiteral(" "));
            if (kLogInputEvents)
                qDebug() << "[CustomCodeEditor][event] handled space in event()";
            keyEvent->accept();
            return true;
        }
    }

    return QAbstractScrollArea::event(event);
}

QString CustomCodeEditor::syntaxKeyForPath(const QString& filePath)
{
    const QFileInfo info(filePath);
    const QString fileName = info.fileName().toLower();
    const QString suffix = info.suffix().toLower();

    if (fileName == QStringLiteral("makefile") || fileName.endsWith(QStringLiteral(".mk")))
        return QStringLiteral("make");
    if (fileName == QStringLiteral("cmakelists.txt") || fileName == QStringLiteral("cmakecache.txt"))
        return QStringLiteral("cmake");
    if (fileName == QStringLiteral("dockerfile"))
        return QStringLiteral("sh");
    if (fileName.endsWith(QStringLiteral(".vcxproj")) || fileName.endsWith(QStringLiteral(".vcproj")) ||
        fileName.endsWith(QStringLiteral(".csproj")) || fileName.endsWith(QStringLiteral(".fsproj")) ||
        fileName.endsWith(QStringLiteral(".props")) || fileName.endsWith(QStringLiteral(".targets")) ||
        fileName.endsWith(QStringLiteral(".filters")) || fileName.endsWith(QStringLiteral(".xml")) ||
        fileName.endsWith(QStringLiteral(".xaml")) || fileName.endsWith(QStringLiteral(".svg")))
        return QStringLiteral("xml");
    if (fileName.endsWith(QStringLiteral(".sln")))
        return QStringLiteral("sln");
    if (fileName == QStringLiteral(".gitignore") || fileName == QStringLiteral(".dockerignore"))
        return QStringLiteral("ini");
    if (fileName == QStringLiteral(".env"))
        return QStringLiteral("ini");
    if (suffix == QStringLiteral("yml"))
        return QStringLiteral("yaml");
    if (suffix == QStringLiteral("bash") || suffix == QStringLiteral("zsh") || suffix == QStringLiteral("fish"))
        return QStringLiteral("sh");
    if (suffix == QStringLiteral("conf") || suffix == QStringLiteral("cfg") || suffix == QStringLiteral("properties"))
        return QStringLiteral("ini");
    if (suffix == QStringLiteral("mjs") || suffix == QStringLiteral("cjs") || suffix == QStringLiteral("jsx"))
        return QStringLiteral("js");
    if (suffix == QStringLiteral("tsx"))
        return QStringLiteral("ts");
    return suffix;
}

void CustomCodeEditor::initSyntaxSupport()
{
    // Reuse the old editor's language assets where they already exist, and
    // fall back to rule-based highlighters for common formats that were not
    // previously covered by QCodeEditor.
    m_languageResourceByExt.insert(QStringLiteral("c"), QStringLiteral(":/languages/c.xml"));
    m_languageResourceByExt.insert(QStringLiteral("h"), QStringLiteral(":/languages/c.xml"));
    m_languageResourceByExt.insert(QStringLiteral("cpp"), QStringLiteral(":/languages/cpp.xml"));
    m_languageResourceByExt.insert(QStringLiteral("hpp"), QStringLiteral(":/languages/cpp.xml"));
    m_languageResourceByExt.insert(QStringLiteral("cc"), QStringLiteral(":/languages/cpp.xml"));
    m_languageResourceByExt.insert(QStringLiteral("cxx"), QStringLiteral(":/languages/cpp.xml"));
    m_languageResourceByExt.insert(QStringLiteral("asm"), QStringLiteral(":/languages/asm.xml"));
    m_languageResourceByExt.insert(QStringLiteral("s"), QStringLiteral(":/languages/asm.xml"));
    m_languageResourceByExt.insert(QStringLiteral("rs"), QStringLiteral(":/languages/rust.xml"));
    m_languageResourceByExt.insert(QStringLiteral("mk"), QStringLiteral(":/languages/gnumake.xml"));
    m_languageResourceByExt.insert(QStringLiteral("make"), QStringLiteral(":/languages/gnumake.xml"));
    m_languageResourceByExt.insert(QStringLiteral("txt"), QStringLiteral(":/languages/plain"));
    m_languageResourceByExt.insert(QStringLiteral("cmake"), QStringLiteral(":/languages/cmake.xml"));
    m_languageResourceByExt.insert(QStringLiteral("py"), QStringLiteral(":/languages/python.xml"));
    m_languageResourceByExt.insert(QStringLiteral("lua"), QStringLiteral(":/languages/lua.xml"));
    m_languageResourceByExt.insert(QStringLiteral("glsl"), QStringLiteral(":/languages/glsl.xml"));
    m_languageResourceByExt.insert(QStringLiteral("vert"), QStringLiteral(":/languages/glsl.xml"));
    m_languageResourceByExt.insert(QStringLiteral("frag"), QStringLiteral(":/languages/glsl.xml"));
    m_languageResourceByExt.insert(QStringLiteral("md"), QStringLiteral(":/languages/markdown"));
    m_languageResourceByExt.insert(QStringLiteral("markdown"), QStringLiteral(":/languages/markdown"));
    m_languageResourceByExt.insert(QStringLiteral("json"), QStringLiteral(":/languages/json"));
    m_languageResourceByExt.insert(QStringLiteral("yaml"), QStringLiteral(":/languages/yaml"));
    m_languageResourceByExt.insert(QStringLiteral("yml"), QStringLiteral(":/languages/yaml"));
    m_languageResourceByExt.insert(QStringLiteral("toml"), QStringLiteral(":/languages/toml"));
    m_languageResourceByExt.insert(QStringLiteral("ini"), QStringLiteral(":/languages/ini"));
    m_languageResourceByExt.insert(QStringLiteral("cfg"), QStringLiteral(":/languages/ini"));
    m_languageResourceByExt.insert(QStringLiteral("conf"), QStringLiteral(":/languages/ini"));
    m_languageResourceByExt.insert(QStringLiteral("properties"), QStringLiteral(":/languages/ini"));
    m_languageResourceByExt.insert(QStringLiteral("env"), QStringLiteral(":/languages/ini"));
    m_languageResourceByExt.insert(QStringLiteral("sh"), QStringLiteral(":/languages/sh"));
    m_languageResourceByExt.insert(QStringLiteral("bash"), QStringLiteral(":/languages/sh"));
    m_languageResourceByExt.insert(QStringLiteral("zsh"), QStringLiteral(":/languages/sh"));
    m_languageResourceByExt.insert(QStringLiteral("fish"), QStringLiteral(":/languages/sh"));
    m_languageResourceByExt.insert(QStringLiteral("js"), QStringLiteral(":/languages/js"));
    m_languageResourceByExt.insert(QStringLiteral("mjs"), QStringLiteral(":/languages/js"));
    m_languageResourceByExt.insert(QStringLiteral("cjs"), QStringLiteral(":/languages/js"));
    m_languageResourceByExt.insert(QStringLiteral("jsx"), QStringLiteral(":/languages/js"));
    m_languageResourceByExt.insert(QStringLiteral("ts"), QStringLiteral(":/languages/ts"));
    m_languageResourceByExt.insert(QStringLiteral("tsx"), QStringLiteral(":/languages/ts"));
    m_languageResourceByExt.insert(QStringLiteral("java"), QStringLiteral(":/languages/java"));
    m_languageResourceByExt.insert(QStringLiteral("cs"), QStringLiteral(":/languages/cs"));
    m_languageResourceByExt.insert(QStringLiteral("go"), QStringLiteral(":/languages/go"));
    m_languageResourceByExt.insert(QStringLiteral("php"), QStringLiteral(":/languages/php"));
    m_languageResourceByExt.insert(QStringLiteral("sql"), QStringLiteral(":/languages/sql"));
    m_languageResourceByExt.insert(QStringLiteral("xml"), QStringLiteral(":/languages/xml"));
    m_languageResourceByExt.insert(QStringLiteral("xaml"), QStringLiteral(":/languages/xml"));
    m_languageResourceByExt.insert(QStringLiteral("svg"), QStringLiteral(":/languages/xml"));
    m_languageResourceByExt.insert(QStringLiteral("sln"), QStringLiteral(":/languages/sln"));
}

QString CustomCodeEditor::normalizedFileExt(const QString& ext) const
{
    return EditorLanguageSupport::normalizedFileExt(ext);
}

void CustomCodeEditor::rebuildHighlighterForCurrentExtension()
{
    const QString ext = normalizedFileExt(m_fileExt);
    const QString resource = EditorLanguageSupport::languageResourceForExtension(ext);
    m_languageResource = resource;

    if (resource == QStringLiteral(":/languages/markdown")) {
        setSyntaxHighlighter(new MarkdownHighlighter(m_highlightDocument));
    } else if (resource == QStringLiteral(":/languages/json")) {
        setSyntaxHighlighter(new QJSONHighlighter(m_highlightDocument));
    } else if (resource == QStringLiteral(":/languages/c.xml") ||
               resource == QStringLiteral(":/languages/cpp.xml") ||
               resource == QStringLiteral(":/languages/asm.xml")) {
        setSyntaxHighlighter(new QCXXHighlighter(m_highlightDocument));
    } else if (resource == QStringLiteral(":/languages/gnumake.xml")) {
        setSyntaxHighlighter(new MakefileHighlighter(m_highlightDocument));
    } else if (resource == QStringLiteral(":/languages/cmake.xml") || resource == QStringLiteral(":/languages/python.xml")) {
        setSyntaxHighlighter(new XmlLanguageHighlighter(resource,
                                                        QRegularExpression(QStringLiteral("#[^\\n]*")),
                                                        QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")),
                                                        QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")),
                                                        m_highlightDocument));
    } else if (resource == QStringLiteral(":/languages/lua.xml")) {
        setSyntaxHighlighter(new XmlLanguageHighlighter(resource,
                                                        QRegularExpression(QStringLiteral("--[^\\n]*")),
                                                        QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")),
                                                        QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")),
                                                        m_highlightDocument));
    } else if (EditorLanguageSupport::isCommonRuleLanguageResource(resource)) {
        setSyntaxHighlighter(createCommonLanguageHighlighter(ext, m_highlightDocument));
    } else if (resource == QStringLiteral(":/languages/plain")) {
        setSyntaxHighlighter(nullptr);
    } else {
        setSyntaxHighlighter(new XmlLanguageHighlighter(resource,
                                                        QRegularExpression(QStringLiteral("//[^\\n]*")),
                                                        QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")),
                                                        QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")),
                                                        m_highlightDocument));
    }

    applyEditorPalette();
}

void CustomCodeEditor::applyEditorPalette()
{
    if (!m_syntaxStyle)
        return;

    auto currentPalette = palette();
    currentPalette.setColor(QPalette::Text, m_syntaxStyle->getFormat(QStringLiteral("Text")).foreground().color());
    currentPalette.setColor(QPalette::Base, m_syntaxStyle->getFormat(QStringLiteral("Text")).background().color());
    currentPalette.setColor(QPalette::Highlight, m_syntaxStyle->getFormat(QStringLiteral("Selection")).background().color());
    currentPalette.setColor(QPalette::AlternateBase, palette().base().color().darker(115));
    setPalette(currentPalette);
    viewport()->setPalette(currentPalette);
    m_lineNumberArea->setPalette(currentPalette);
}

QVector<QTextLayout::FormatRange> CustomCodeEditor::highlightFormatsForVisibleLine(qint64 lineNum, const QString& text) const
{
    if (!m_highlighter)
        return {};

    if (const auto cached = m_highlightFormatCache.constFind(lineNum); cached != m_highlightFormatCache.constEnd())
        return cached.value();

    // QSyntaxHighlighter computes state line-by-line, so the custom editor
    // rebuilds a small rolling window around the visible line instead of
    // materializing the whole file in a QTextDocument.
    const int contextBefore = 16;
    const qint64 startLine = qMax<qint64>(0, lineNum - contextBefore);
    const qint64 endLine = qMin(lineNum + 16, m_lineIndex->lineCount() - 1);

    m_highlightFormatCache.clear();
    m_highlightCacheStartLine = startLine;
    m_highlightCacheEndLine = endLine;

    QStringList lines;
    lines.reserve(static_cast<int>(endLine - startLine + 1));
    for (qint64 current = startLine; current <= endLine; ++current)
        lines.append(const_cast<CustomCodeEditor*>(this)->displayTextForLine(current));

    try {
        m_highlightDocument->setPlainText(lines.join(QLatin1Char('\n')));
        m_highlighter->rehighlight();
    } catch (...) {
        const_cast<CustomCodeEditor*>(this)->setSyntaxHighlighter(nullptr);
        return {};
    }

    QTextBlock block = m_highlightDocument->firstBlock();
    for (qint64 current = startLine; current <= endLine && block.isValid(); ++current) {
        const auto ranges = block.layout()->formats();
        m_highlightFormatCache.insert(current, ranges);
        block = block.next();
    }

    const auto ranges = m_highlightFormatCache.value(lineNum);
    if (ranges.isEmpty() && !text.isEmpty())
        return {};
    return ranges;
}

qint64 CustomCodeEditor::clampToUtf8Boundary(qint64 bytePos) const
{
    if (!m_buffer)
        return 0;

    const qint64 clamped = qBound<qint64>(0, bytePos, m_buffer->size());
    const qint64 lineNum = m_lineIndex->lineCount() > 0 ? lineFromBytePos(clamped) : 0;
    const qint64 lineStart = m_lineIndex->lineCount() > 0 ? lineVisibleStart(lineNum) : 0;
    const qint64 lineEnd = m_lineIndex->lineCount() > 0 ? lineVisibleEnd(lineNum) : 0;
    const QByteArray lineBytes = m_buffer->read(lineStart, lineEnd - lineStart);
    return lineStart + m_utf8Decoder->findCharBoundary(lineBytes, clamped - lineStart);
}

void CustomCodeEditor::setBData(const QByteArray& data)
{
    if (!m_buffer)
        return;

    m_buffer->loadData(data);
}

QByteArray CustomCodeEditor::getBData()
{
    return m_buffer ? m_buffer->data() : QByteArray();
}

void CustomCodeEditor::setBuffer(FileDataBuffer* buffer)
{
    if (m_buffer)
        disconnect(m_buffer, nullptr, this, nullptr);

    m_buffer = buffer;
    m_lineCache->clear();
    invalidateWrapCache();
    m_lineIndex->clear();
    m_hasUtf8Bom = false;
    m_cursorBytePos = 0;
    m_selectionStart = 0;
    m_selectionLength = 0;
    m_selectionAnchor = -1;

    if (!m_buffer) {
        updateScrollbars();
        viewport()->update();
        m_lineNumberArea->update();
        return;
    }

    connect(m_buffer, &FileDataBuffer::byteChanged, this, &CustomCodeEditor::onBufferByteChanged);
    connect(m_buffer, &FileDataBuffer::bytesChanged, this, &CustomCodeEditor::onBufferBytesChanged);
    connect(m_buffer, &FileDataBuffer::dataChanged, this, &CustomCodeEditor::onBufferDataChanged);
    connect(m_buffer, &FileDataBuffer::selectionChanged, this, &CustomCodeEditor::onBufferSelectionChanged);

    buildLineIndex();
    m_cursorBytePos = firstTextByte();

    qint64 selectionPos = 0;
    qint64 selectionLength = 0;
    m_buffer->getSelection(selectionPos, selectionLength);
    updateSelection(selectionPos, selectionLength);
    clampCursorToBuffer();
    updateScrollbars();
    viewport()->update();
    m_lineNumberArea->update();
}

FileDataBuffer* CustomCodeEditor::getBuffer() const
{
    return m_buffer;
}

void CustomCodeEditor::setFileExt(const QString& ext)
{
    m_fileExt = normalizedFileExt(ext);
    m_highlightFormatCache.clear();
    m_highlightCacheStartLine = -1;
    m_highlightCacheEndLine = -1;
    rebuildHighlighterForCurrentExtension();
    viewport()->update();
}

void CustomCodeEditor::setSyntaxHighlighter(QStyleSyntaxHighlighter* highlighter)
{
    if (m_highlighter == highlighter)
        return;

    if (m_highlighter)
        delete m_highlighter;

    m_highlighter = highlighter;
    m_highlightFormatCache.clear();
    m_highlightCacheStartLine = -1;
    m_highlightCacheEndLine = -1;
    if (m_highlighter) {
        m_highlighter->setDocument(m_highlightDocument);
        m_highlighter->setSyntaxStyle(m_syntaxStyle);
    }
    viewport()->update();
}

void CustomCodeEditor::setTabReplaceSize(int spaces)
{
    m_tabReplaceSize = qMax(1, spaces);
}

void CustomCodeEditor::setTabReplace(bool enabled)
{
    m_tabReplace = enabled;
}

bool CustomCodeEditor::tabReplaceEnabled() const
{
    return m_tabReplace;
}

void CustomCodeEditor::setTabDisplaySize(int spaces)
{
    const int newSize = qMax(1, spaces);
    if (m_tabDisplaySize == newSize)
        return;

    m_tabDisplaySize = newSize;
    if (!m_tabReplace)
        m_tabReplaceSize = newSize;
    invalidateDisplayLayoutCache();
    invalidateWrapCache();
    updateScrollbars();
    viewport()->update();
}

int CustomCodeEditor::tabDisplaySize() const
{
    return m_tabDisplaySize;
}

void CustomCodeEditor::setWordWrapEnabled(bool enabled)
{
    if (m_wordWrapEnabled == enabled)
        return;

    m_wordWrapEnabled = enabled;
    invalidateDisplayLayoutCache();
    invalidateWrapCache();
    horizontalScrollBar()->setValue(0);
    updateScrollbars();
    ensureCursorVisible();
    viewport()->update();
    m_lineNumberArea->update();
}

bool CustomCodeEditor::wordWrapEnabled() const
{
    return m_wordWrapEnabled;
}

bool CustomCodeEditor::isModified() const
{
    return m_buffer ? m_buffer->isModified() : false;
}

qint64 CustomCodeEditor::cursorPosition() const
{
    return m_cursorBytePos;
}

qint64 CustomCodeEditor::cursorLineNumber() const
{
    if (!m_buffer)
        return 1;

    return lineFromBytePos(m_cursorBytePos) + 1;
}

qint64 CustomCodeEditor::cursorColumnNumber() const
{
    if (!m_buffer)
        return 1;

    return columnForBytePos(lineFromBytePos(m_cursorBytePos), m_cursorBytePos) + 1;
}

qint64 CustomCodeEditor::lineCount() const
{
    return m_lineIndex->lineCount();
}

bool CustomCodeEditor::hasSelection() const
{
    return m_selectionLength > 0;
}

QString CustomCodeEditor::selectedText() const
{
    if (!m_buffer || !hasSelection())
        return {};

    return decodeBytesForDisplay(m_selectionStart, m_buffer->read(m_selectionStart, m_selectionLength));
}

QString CustomCodeEditor::highlightedSelectionText() const
{
    const QString text = selectedText();
    if (text.isEmpty() || text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\r')))
        return {};
    if (text.trimmed().isEmpty())
        return {};
    return text;
}

QString CustomCodeEditor::syntaxKey() const
{
    return normalizedFileExt(m_fileExt);
}

QString CustomCodeEditor::lineCommentPrefix() const
{
    return EditorLanguageSupport::lineCommentPrefix(syntaxKey());
}

bool CustomCodeEditor::findText(const QString& text, bool forward, Qt::CaseSensitivity caseSensitivity)
{
    if (!m_buffer || text.isEmpty() || m_lineIndex->lineCount() == 0)
        return false;

    const qint64 startPos = hasSelection() ? (forward ? m_selectionStart + m_selectionLength : m_selectionStart) : m_cursorBytePos;
    const qint64 startLine = lineFromBytePos(startPos);

    auto searchInLine = [&](qint64 lineNum, int fromColumn) -> bool {
        const QString lineText = displayTextForLine(lineNum);
        const int index = forward
            ? lineText.indexOf(text, qMax(0, fromColumn), caseSensitivity)
            : lineText.lastIndexOf(text, qMin(fromColumn, lineText.length()), caseSensitivity);

        if (index < 0)
            return false;

        m_selectionStart = bytePosForColumn(lineNum, index);
        m_selectionLength = bytePosForColumn(lineNum, index + text.length()) - m_selectionStart;
        m_cursorBytePos = forward ? m_selectionStart + m_selectionLength : m_selectionStart;
        m_selectionAnchor = m_selectionStart;
        syncSelectionToBuffer();
        ensureCursorVisible();
        emit cursorPositionChanged();
        viewport()->update();
        return true;
    };

    if (forward) {
        if (searchInLine(startLine, columnForBytePos(startLine, startPos)))
            return true;
        for (qint64 lineNum = startLine + 1; lineNum < m_lineIndex->lineCount(); ++lineNum) {
            if (searchInLine(lineNum, 0))
                return true;
        }
        for (qint64 lineNum = 0; lineNum <= startLine; ++lineNum) {
            if (searchInLine(lineNum, 0))
                return true;
        }
    } else {
        if (searchInLine(startLine, columnForBytePos(startLine, startPos) - 1))
            return true;
        for (qint64 lineNum = startLine - 1; lineNum >= 0; --lineNum) {
            if (searchInLine(lineNum, displayTextForLine(lineNum).length()))
                return true;
        }
        for (qint64 lineNum = m_lineIndex->lineCount() - 1; lineNum >= startLine; --lineNum) {
            if (searchInLine(lineNum, displayTextForLine(lineNum).length()))
                return true;
        }
    }

    return false;
}

bool CustomCodeEditor::goToLine(qint64 oneBasedLineNumber)
{
    if (!m_buffer || m_lineIndex->lineCount() == 0)
        return false;

    const qint64 lineNum = qBound<qint64>(0, oneBasedLineNumber - 1, m_lineIndex->lineCount() - 1);
    m_cursorBytePos = lineVisibleStart(lineNum);
    clearSelection();
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
    return true;
}

int CustomCodeEditor::countMatches(const QString& text, Qt::CaseSensitivity caseSensitivity) const
{
    if (!m_buffer || text.isEmpty())
        return 0;

    int count = 0;
    for (qint64 lineNum = 0; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const QString lineText = const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum);
        int index = lineText.indexOf(text, 0, caseSensitivity);
        while (index >= 0) {
            ++count;
            index = lineText.indexOf(text, index + text.length(), caseSensitivity);
        }
    }

    return count;
}

int CustomCodeEditor::currentMatchIndex(const QString& text, Qt::CaseSensitivity caseSensitivity) const
{
    if (!m_buffer || text.isEmpty() || !hasSelection())
        return 0;

    int currentIndex = 0;
    for (qint64 lineNum = 0; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const QString lineText = const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum);
        int index = lineText.indexOf(text, 0, caseSensitivity);
        while (index >= 0) {
            ++currentIndex;
            const qint64 matchStart = bytePosForColumn(lineNum, index);
            const qint64 matchEnd = bytePosForColumn(lineNum, index + text.length());
            if (matchStart == m_selectionStart && matchEnd - matchStart == m_selectionLength)
                return currentIndex;
            index = lineText.indexOf(text, index + text.length(), caseSensitivity);
        }
    }

    return 0;
}

bool CustomCodeEditor::replaceCurrentSelection(const QString& text, const QString& replacement, Qt::CaseSensitivity caseSensitivity)
{
    if (!m_buffer || text.isEmpty() || !hasSelection())
        return false;

    const QString currentSelection = selectedText();
    const bool matches = caseSensitivity == Qt::CaseSensitive
        ? currentSelection == text
        : currentSelection.compare(text, Qt::CaseInsensitive) == 0;

    if (!matches)
        return false;

    replaceRange(m_selectionStart, m_selectionLength, replacement.toUtf8());
    return true;
}

int CustomCodeEditor::replaceAllMatches(const QString& text, const QString& replacement, Qt::CaseSensitivity caseSensitivity)
{
    if (!m_buffer || text.isEmpty() || m_lineIndex->lineCount() == 0)
        return 0;

    struct MatchRange {
        qint64 start = 0;
        qint64 length = 0;
    };

    QVector<MatchRange> matches;

    for (qint64 lineNum = 0; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const QString lineText = displayTextForLine(lineNum);
        int searchFrom = 0;

        while (true) {
            const int index = lineText.indexOf(text, searchFrom, caseSensitivity);
            if (index < 0)
                break;

            const qint64 start = bytePosForColumn(lineNum, index);
            const qint64 end = bytePosForColumn(lineNum, index + text.length());
            matches.push_back({start, end - start});

            searchFrom = index + text.length();
        }
    }

    if (matches.isEmpty())
        return 0;

    const QByteArray replacementBytes = replacement.toUtf8();

    for (qsizetype i = matches.size() - 1; i >= 0; --i)
        replaceRange(matches[i].start, matches[i].length, replacementBytes);

    return static_cast<int>(matches.size());
}

void CustomCodeEditor::setScaleFactor(double factor)
{
    const double boundedFactor = qBound(0.5, factor, 4.0);
    if (qFuzzyCompare(m_scaleFactor, boundedFactor))
        return;

    const qint64 anchorBytePos = m_buffer ? bytePosFromPoint(QPoint(viewport()->width() / 2, viewport()->height() / 2))
                                          : m_cursorBytePos;
    m_scaleFactor = boundedFactor;
    m_font.setPointSizeF(10.0 * m_scaleFactor);
    setFont(m_font);
    m_fontMetrics = QFontMetricsF(m_font);
    m_lineNumberArea->setFont(m_font);
    invalidateDisplayLayoutCache();
    invalidateWrapCache();
    updateLineNumberAreaWidth();
    updateScrollbars();
    if (m_buffer)
        centerViewOnBytePos(anchorBytePos);
    viewport()->update();
    m_lineNumberArea->update();
}

double CustomCodeEditor::scaleFactor() const
{
    return m_scaleFactor;
}

int CustomCodeEditor::lineNumberAreaWidth() const
{
    return m_lineNumberArea->calculateWidth();
}

void CustomCodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), palette().alternateBase());

    if (!m_buffer)
        return;

    const int lineHeight = qRound(m_fontMetrics.height());
    const int scrollY = verticalScrollBar()->value();
    const qint64 cursorLine = lineFromBytePos(m_cursorBytePos);
    const qint64 firstVisibleVisualLine = scrollY / lineHeight;
    const qint64 maxVisibleVisualLines = (height() / lineHeight) + 2;
    const qint64 lastVisibleVisualLine = firstVisibleVisualLine + maxVisibleVisualLines;

    painter.setFont(m_lineNumberArea->font());
    qint64 lineNum = logicalLineFromVisualLine(firstVisibleVisualLine);
    qint64 visualIndex = visualLineIndexForLogicalLine(lineNum);
    for (; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const int lineWrapCount = wrappedLineCount(lineNum);
        if (visualIndex > lastVisibleVisualLine)
            break;

        const int y = static_cast<int>(visualIndex) * lineHeight - scrollY;
        QRectF rect(0, y, m_lineNumberArea->width() - 4, lineHeight);

        painter.setPen(lineNum == cursorLine ? palette().highlight().color() : palette().mid().color());
        painter.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, QString::number(lineNum + 1));
        visualIndex += lineWrapCount;
    }
}

void CustomCodeEditor::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), palette().base());

    if (!m_buffer)
        return;

    renderVisibleLines(&painter);
    renderSelectionMatches(&painter);
    if (hasSelection())
        renderSelection(&painter);
    if (hasFocus())
        renderCursor(&painter);
}

void CustomCodeEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    invalidateWrapCache();
    updateScrollbars();
}

void CustomCodeEditor::keyPressEvent(QKeyEvent* event)
{
    if (!m_buffer) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    logKeyEvent("keyPressEvent-enter", event, event, m_cursorBytePos);

    const bool shiftPressed = event->modifiers().testFlag(Qt::ShiftModifier);
    const bool controlPressed = event->modifiers().testFlag(Qt::ControlModifier);
    const qint64 oldCursorPos = m_cursorBytePos;

    auto handleMove = [&](auto movement) {
        if (shiftPressed && m_selectionAnchor < 0)
            m_selectionAnchor = oldCursorPos;
        movement();
        if (shiftPressed)
            updateSelectionAfterMove(oldCursorPos);
        else
            clearSelection();
        event->accept();
    };

    switch (event->key()) {
    case Qt::Key_Left:
        handleMove([this, controlPressed]() {
            if (controlPressed)
                moveCursorWordLeft();
            else
                moveCursorLeft();
        });
        return;
    case Qt::Key_Right:
        handleMove([this, controlPressed]() {
            if (controlPressed)
                moveCursorWordRight();
            else
                moveCursorRight();
        });
        return;
    case Qt::Key_Up:
        if (event->modifiers() == Qt::AltModifier) {
            endEditGrouping();
            moveSelectedLines(-1);
            event->accept();
            return;
        }
        if (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)) {
            endEditGrouping();
            duplicateSelectionOrLine(true);
            event->accept();
            return;
        }
        handleMove([this]() { moveCursorUp(); });
        return;
    case Qt::Key_Down:
        if (event->modifiers() == Qt::AltModifier) {
            endEditGrouping();
            moveSelectedLines(1);
            event->accept();
            return;
        }
        if (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)) {
            endEditGrouping();
            duplicateSelectionOrLine(false);
            event->accept();
            return;
        }
        handleMove([this]() { moveCursorDown(); });
        return;
    case Qt::Key_Home:
        handleMove([this, controlPressed]() {
            if (controlPressed)
                moveCursorDocumentStart();
            else
                moveCursorHome();
        });
        return;
    case Qt::Key_End:
        handleMove([this, controlPressed]() {
            if (controlPressed)
                moveCursorDocumentEnd();
            else
                moveCursorEnd();
        });
        return;
    case Qt::Key_PageUp:
        handleMove([this]() { moveCursorPageUp(); });
        return;
    case Qt::Key_PageDown:
        handleMove([this]() { moveCursorPageDown(); });
        return;
    default:
        break;
    }

    if (event->matches(QKeySequence::Copy)) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=Copy";
        copySelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste) || (event->key() == Qt::Key_Insert && shiftPressed && !controlPressed)) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=Paste";
        pasteFromClipboard();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Insert && controlPressed && !shiftPressed) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=CopyCtrlInsert";
        copySelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete && shiftPressed && !controlPressed) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=CutShiftDelete";
        cutSelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Cut)) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=Cut";
        cutSelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=SelectAll";
        selectAll();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=Undo";
        undo();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Redo)) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=Redo";
        redo();
        event->accept();
        return;
    }
    if (controlPressed && (event->key() == Qt::Key_Slash || event->text() == QStringLiteral("/"))) {
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=ToggleLineComment";
        toggleLineComment();
        event->accept();
        return;
    }
    if (controlPressed && !shiftPressed && event->key() == Qt::Key_D) {
        endEditGrouping();
        duplicateSelectionOrLine(false);
        event->accept();
        return;
    }
    if (controlPressed && shiftPressed && event->key() == Qt::Key_K) {
        endEditGrouping();
        deleteCurrentLine();
        event->accept();
        return;
    }
    const QString text = event->text();
    if (text == QStringLiteral(" ") && !event->modifiers().testFlag(Qt::ControlModifier)) {
        beginEditGrouping(EditGroupTyping);
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=InsertSpaceByText";
        insertText(text);
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Backtab:
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=OutdentSelectionByBacktab";
        outdentSelection();
        event->accept();
        return;
    case Qt::Key_Space:
        beginEditGrouping(EditGroupTyping);
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=InsertSpaceByKey";
        insertText(QStringLiteral(" "));
        event->accept();
        return;
    case Qt::Key_Backspace:
        beginEditGrouping(EditGroupBackspace);
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=" << (controlPressed ? "DeleteWordBackward" : "DeleteBackward");
        if (controlPressed)
            deleteWordBackward();
        else
            deleteBackward();
        event->accept();
        return;
    case Qt::Key_Delete:
        beginEditGrouping(EditGroupDelete);
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=" << (controlPressed ? "DeleteWordForward" : "DeleteForward");
        if (controlPressed)
            deleteWordForward();
        else
            deleteForward();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=InsertNewline";
        if (controlPressed && shiftPressed) {
            insertLineAbove();
        } else if (controlPressed) {
            insertLineBelow();
        } else if (shiftPressed) {
            replaceRange(hasSelection() ? m_selectionStart : m_cursorBytePos,
                         hasSelection() ? m_selectionLength : 0,
                         QByteArray("\n"));
        } else {
            insertNewline();
        }
        event->accept();
        return;
    case Qt::Key_Tab:
        endEditGrouping();
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][keyPressEvent] action=" << (shiftPressed ? "OutdentSelection" : "InsertTab");
        if (shiftPressed)
            outdentSelection();
        else
            insertTab();
        event->accept();
        return;
    default:
        break;
    }

    if (!text.isEmpty() && !event->modifiers().testFlag(Qt::ControlModifier) && text.front().isPrint()) {
        beginEditGrouping(EditGroupTyping);
        if (kLogInputEvents)
            qDebug().noquote() << QStringLiteral("[CustomCodeEditor][keyPressEvent] action=InsertPrintable text='%1'").arg(text);
        insertText(text);
        event->accept();
        return;
    }

    endEditGrouping();
    if (kLogInputEvents)
        qDebug() << "[CustomCodeEditor][keyPressEvent] fell back to base implementation";
    QAbstractScrollArea::keyPressEvent(event);
}

void CustomCodeEditor::mousePressEvent(QMouseEvent* event)
{
    if (!m_buffer || event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }

    setFocus();
    endEditGrouping();

    const QPoint clickPoint = event->position().toPoint();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    const bool tripleClick = m_pendingTripleClick
        && (now - m_lastClickTimestamp < QApplication::doubleClickInterval())
        && (clickPoint - m_lastClickPosition).manhattanLength() <= QApplication::startDragDistance();

    m_clickCount = tripleClick ? 3 : 1;
    m_lastClickTimestamp = now;
    m_lastClickPosition = clickPoint;
    m_pendingTripleClick = false;

    m_mouseSelecting = true;
    m_cursorBytePos = bytePosFromPoint(clickPoint);

    if (tripleClick) {
        const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
        const qint64 lineStart = lineVisibleStart(lineNum);
        m_selectionStart = lineStart;
        m_selectionLength = lineVisibleEnd(lineNum) - lineStart;
        m_selectionAnchor = lineStart;
        m_cursorBytePos = lineVisibleEnd(lineNum);
        m_mouseSelecting = false;
        syncSelectionToBuffer();
        emit cursorPositionChanged();
        viewport()->update();
        return;
    }

    m_selectionAnchor = m_cursorBytePos;
    m_selectionStart = m_cursorBytePos;
    m_selectionLength = 0;
    syncSelectionToBuffer();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_buffer || !m_mouseSelecting || !(event->buttons() & Qt::LeftButton)) {
        QAbstractScrollArea::mouseMoveEvent(event);
        return;
    }

    const QPoint point = event->position().toPoint();
    if (point.y() < 0)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + point.y());
    else if (point.y() > viewport()->height())
        verticalScrollBar()->setValue(verticalScrollBar()->value() + (point.y() - viewport()->height()));

    if (!m_wordWrapEnabled) {
        const int textStartX = lineNumberAreaWidth() + kTextLeftPadding;
        if (point.x() < textStartX)
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() + point.x() - textStartX);
        else if (point.x() > viewport()->width())
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() + (point.x() - viewport()->width()));
    }

    m_cursorBytePos = bytePosFromPoint(point);
    updateSelectionAfterMove(m_selectionAnchor);
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::mouseReleaseEvent(QMouseEvent* event)
{
    m_mouseSelecting = false;
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void CustomCodeEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!m_buffer || event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mouseDoubleClickEvent(event);
        return;
    }

    const qint64 bytePos = bytePosFromPoint(event->position().toPoint());
    m_clickCount = 2;
    m_lastClickTimestamp = QDateTime::currentMSecsSinceEpoch();
    m_lastClickPosition = event->position().toPoint();
    m_pendingTripleClick = true;
    const qint64 lineNum = lineFromBytePos(bytePos);
    const QString text = expandedDisplayText(displayTextForLine(lineNum));
    const qint64 lineStart = lineVisibleStart(lineNum);
    const qint64 column = columnForBytePos(lineNum, bytePos);

    int start = static_cast<int>(qBound<qint64>(0, column, text.length()));
    int end = start;
    const bool selectWhitespace = start < text.length() && text.at(start).isSpace();
    if (selectWhitespace) {
        while (start > 0 && text.at(start - 1).isSpace())
            --start;
        while (end < text.length() && text.at(end).isSpace())
            ++end;
    } else {
        while (start > 0 && !text.at(start - 1).isSpace())
            --start;
        while (end < text.length() && !text.at(end).isSpace())
            ++end;
    }
    m_selectionStart = bytePosForColumn(lineNum, start);
    m_selectionLength = bytePosForColumn(lineNum, end) - m_selectionStart;
    m_cursorBytePos = bytePosForColumn(lineNum, end);

    syncSelectionToBuffer();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_buffer) {
        QAbstractScrollArea::contextMenuEvent(event);
        return;
    }

    setFocus();

    const qint64 clickPos = bytePosFromPoint(viewport()->mapFromGlobal(event->globalPos()));
    if (!hasSelection() || clickPos < m_selectionStart || clickPos > m_selectionStart + m_selectionLength) {
        m_cursorBytePos = clickPos;
        m_selectionStart = clickPos;
        m_selectionLength = 0;
        m_selectionAnchor = -1;
        syncSelectionToBuffer();
        emit cursorPositionChanged();
        viewport()->update();
    }

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral("QMenu::item:disabled { color: #6f6f6f; }"));
    QAction* undoAction = menu.addAction(tr("Undo"));
    QAction* redoAction = menu.addAction(tr("Redo"));
    menu.addSeparator();
    QAction* cutAction = menu.addAction(tr("Cut"));
    QAction* copyAction = menu.addAction(tr("Copy"));
    QAction* pasteAction = menu.addAction(tr("Paste"));
    QAction* deleteAction = menu.addAction(tr("Delete"));
    menu.addSeparator();
    QAction* selectAllAction = menu.addAction(tr("Select All"));

    const bool hasEditorSelection = hasSelection();
    const QMimeData* mimeData = QApplication::clipboard()->mimeData();
    const bool hasClipboardText = mimeData && mimeData->hasText() && !mimeData->text().isEmpty();

    undoAction->setEnabled(m_buffer->canUndo());
    redoAction->setEnabled(m_buffer->canRedo());
    cutAction->setEnabled(hasEditorSelection);
    copyAction->setEnabled(hasEditorSelection);
    deleteAction->setEnabled(hasEditorSelection);
    pasteAction->setEnabled(hasClipboardText);
    selectAllAction->setEnabled(m_buffer->size() > firstTextByte());

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == undoAction)
        undo();
    else if (chosen == redoAction)
        redo();
    else if (chosen == cutAction)
        cutSelection();
    else if (chosen == copyAction)
        copySelection();
    else if (chosen == pasteAction)
        pasteFromClipboard();
    else if (chosen == deleteAction)
        deleteForward();
    else if (chosen == selectAllAction)
        selectAll();

    event->accept();
}

void CustomCodeEditor::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const double delta = event->angleDelta().y() > 0 ? 1.1 : (1.0 / 1.1);
        setScaleFactor(m_scaleFactor * delta);
        event->accept();
        return;
    }

    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->angleDelta().y());
        event->accept();
        return;
    }

    QAbstractScrollArea::wheelEvent(event);
}

void CustomCodeEditor::focusInEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusInEvent(event);
    viewport()->update();
}

void CustomCodeEditor::focusOutEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusOutEvent(event);
    viewport()->update();
}

void CustomCodeEditor::hideEvent(QHideEvent* event)
{
    saveViewState();

    // Hidden editors should not keep decoded text around unnecessarily.
    // The buffer stays authoritative, so the cache can be rebuilt cheaply.
    m_lineCache->clear();
    invalidateWrapCache();
    m_restoreViewStatePending = true;

    QAbstractScrollArea::hideEvent(event);
}

void CustomCodeEditor::showEvent(QShowEvent* event)
{
    QAbstractScrollArea::showEvent(event);

    if (m_restoreViewStatePending)
        restoreViewState();
}

bool CustomCodeEditor::focusNextPrevChild(bool next)
{
    Q_UNUSED(next);
    return false;
}

void CustomCodeEditor::onBufferByteChanged(qint64 pos)
{
    const qint64 line = lineFromBytePos(pos);
    buildLineIndex();
    invalidateLineCache(line, line);
    updateScrollbars();
    viewport()->update();
    m_lineNumberArea->update();
}

void CustomCodeEditor::onBufferBytesChanged(qint64 pos, qint64 length)
{
    buildLineIndex();
    invalidateLineCache(lineFromBytePos(pos), lineFromBytePos(pos + length));
    updateScrollbars();
    viewport()->update();
    m_lineNumberArea->update();
}

void CustomCodeEditor::onBufferDataChanged()
{
    const bool preserveViewport = !m_applyingBufferEdit;
    const int previousVerticalScroll = verticalScrollBar()->value();
    const int previousHorizontalScroll = horizontalScrollBar()->value();

    buildLineIndex();
    m_lineCache->clear();
    invalidateDisplayLayoutCache();
    m_highlightFormatCache.clear();
    m_highlightCacheStartLine = -1;
    m_highlightCacheEndLine = -1;
    invalidateWrapCache();
    m_cursorBytePos = clampToUtf8Boundary(m_cursorBytePos);
    clampCursorToBuffer();
    if (m_selectionLength > 0) {
        const qint64 bufferSize = m_buffer ? m_buffer->size() : 0;
        m_selectionStart = qBound<qint64>(0, m_selectionStart, bufferSize);
        m_selectionLength = qBound<qint64>(0, m_selectionLength, bufferSize - m_selectionStart);
    }
    updateScrollbars();
    if (preserveViewport) {
        verticalScrollBar()->setValue(qBound(verticalScrollBar()->minimum(), previousVerticalScroll, verticalScrollBar()->maximum()));
        horizontalScrollBar()->setValue(qBound(horizontalScrollBar()->minimum(), previousHorizontalScroll, horizontalScrollBar()->maximum()));
    } else {
        ensureCursorVisible();
    }
    updateModificationState();
    viewport()->update();
    m_lineNumberArea->update();
}


void CustomCodeEditor::onBufferSelectionChanged(qint64 pos, qint64 length)
{
    if (m_updatingSelection)
        return;

    // Ignore zero-length selection echoes while this editor is actively
    // applying its own edit; other views may still be catching up.
    if (m_applyingBufferEdit && length == 0)
        return;

    updateSelection(pos, length);

    // A zero-length external selection should only clear the selection state.
    // Moving the caret to that position causes the first local edit to jump to
    // the start of the file when another view echoes back an empty selection.
    if (length > 0)
        m_cursorBytePos = clampToUtf8Boundary(pos + length);

    clampCursorToBuffer();
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::updateScrollbars()
{
    ensureLineIndexValid();
    if (!m_buffer) {
        verticalScrollBar()->setRange(0, 0);
        horizontalScrollBar()->setRange(0, 0);
        return;
    }

    ensureLayoutCache();
    const int lineHeight = qRound(m_fontMetrics.height());
    const int totalHeight = static_cast<int>(m_totalVisualLineCount) * lineHeight;
    const int viewportHeight = viewport()->height();
    verticalScrollBar()->setRange(0, qMax(0, totalHeight - viewportHeight));
    verticalScrollBar()->setPageStep(viewportHeight);
    verticalScrollBar()->setSingleStep(lineHeight);

    if (m_wordWrapEnabled) {
        horizontalScrollBar()->setRange(0, 0);
        horizontalScrollBar()->setPageStep(viewport()->width());
    } else {
        const int approxWidth = qRound(m_fontMetrics.horizontalAdvance(QLatin1Char('M')) * m_maxExpandedLineLength);
        horizontalScrollBar()->setRange(0, qMax(0, approxWidth - viewport()->width() + lineNumberAreaWidth()));
        horizontalScrollBar()->setPageStep(viewport()->width());
        horizontalScrollBar()->setSingleStep(qRound(m_fontMetrics.horizontalAdvance(QLatin1Char(' ')) * 4));
    }
}

void CustomCodeEditor::buildLineIndex()
{
    if (!m_buffer) {
        m_lineIndex->clear();
        m_hasUtf8Bom = false;
        m_layoutCacheValid = false;
        m_layoutCacheComputedLines = 0;
        m_layoutCacheComputedVisualLines = 0;
        m_visualLineOffsets.clear();
        m_totalVisualLineCount = 0;
        m_maxExpandedLineLength = 0;
        updateLineNumberAreaWidth();
        return;
    }

    m_lineIndex->build(m_buffer);
    m_hasUtf8Bom = m_buffer->read(0, 3) == kUtf8Bom;
    m_layoutCacheValid = false;
    m_layoutCacheComputedLines = 0;
    m_layoutCacheComputedVisualLines = 0;
    m_visualLineOffsets.clear();
    m_totalVisualLineCount = m_lineIndex->lineCount();
    m_maxExpandedLineLength = 0;
    updateLineNumberAreaWidth();
}

void CustomCodeEditor::ensureLineIndexValid()
{
    if (!m_buffer || m_lineIndex->lineCount() <= 0)
        return;

    const qint64 bufferSize = m_buffer->size();
    const qint64 lastLine = m_lineIndex->lineCount() - 1;
    const qint64 start = m_lineIndex->lineStartPos(lastLine);
    const qint64 length = m_lineIndex->lineLength(lastLine);
    if (start < 0 || start > bufferSize || length < 0 || start + length > bufferSize)
        buildLineIndex();
}

void CustomCodeEditor::saveViewState()
{
    m_savedVerticalScrollValue = verticalScrollBar()->value();
    m_savedHorizontalScrollValue = horizontalScrollBar()->value();
    m_savedCursorBytePos = m_cursorBytePos;
}

void CustomCodeEditor::restoreViewState()
{
    if (!m_buffer) {
        m_restoreViewStatePending = false;
        return;
    }

    ensureLineIndexValid();
    m_cursorBytePos = m_savedCursorBytePos;
    clampCursorToBuffer();
    updateScrollbars();
    verticalScrollBar()->setValue(qBound(verticalScrollBar()->minimum(), m_savedVerticalScrollValue, verticalScrollBar()->maximum()));
    horizontalScrollBar()->setValue(qBound(horizontalScrollBar()->minimum(), m_savedHorizontalScrollValue, horizontalScrollBar()->maximum()));
    m_restoreViewStatePending = false;
    viewport()->update();
    m_lineNumberArea->update();
}

int CustomCodeEditor::availableTextWidth() const
{
    return qMax(1, viewport()->width() - lineNumberAreaWidth() - kTextLeftPadding - 2);
}

int CustomCodeEditor::wrappedLineCount(qint64 lineNum) const
{
    if (!m_buffer || lineNum < 0 || lineNum >= m_lineIndex->lineCount())
        return 1;

    const int currentWidth = availableTextWidth();
    if (m_wrapCacheWidth != currentWidth) {
        m_wrapCountCache.clear();
        m_wrapCacheWidth = currentWidth;
    }

    if (const auto it = m_wrapCountCache.constFind(lineNum); it != m_wrapCountCache.constEnd())
        return it.value();

    int count = 1;
    try {
        count = buildWrappedSegments(expandedDisplayText(const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum)), m_font, currentWidth, m_wordWrapEnabled).size();
    } catch (...) {
        count = 1;
    }

    m_wrapCountCache.insert(lineNum, qMax(1, count));
    return qMax(1, count);
}

qint64 CustomCodeEditor::visualLineCount() const
{
    if (!m_buffer)
        return 0;

    ensureLayoutCache();
    return qMax<qint64>(1, m_totalVisualLineCount);
}

qint64 CustomCodeEditor::visualLineIndexForLogicalLine(qint64 lineNum) const
{
    if (!m_buffer || m_lineIndex->lineCount() == 0)
        return 0;

    if (!m_wordWrapEnabled)
        return qBound<qint64>(0, lineNum, m_lineIndex->lineCount());

    if (lineNum <= 0)
        return 0;

    ensureLayoutCacheForLine(lineNum);
    if (lineNum < m_layoutCacheComputedLines)
        return m_visualLineOffsets.at(static_cast<int>(lineNum));

    return m_layoutCacheComputedVisualLines + qMax<qint64>(0, lineNum - m_layoutCacheComputedLines);
}

qint64 CustomCodeEditor::logicalLineFromVisualLine(qint64 visualLine) const
{
    if (!m_buffer || m_lineIndex->lineCount() == 0)
        return 0;

    if (!m_wordWrapEnabled)
        return qBound<qint64>(0, visualLine, m_lineIndex->lineCount() - 1);

    ensureLayoutCacheForVisualLine(visualLine);
    const qint64 clampedVisualLine = qMax<qint64>(0, visualLine);

    if (m_layoutCacheComputedLines <= 0)
        return 0;

    if (clampedVisualLine >= m_layoutCacheComputedVisualLines && m_layoutCacheComputedLines < m_lineIndex->lineCount()) {
        const qint64 estimated = m_layoutCacheComputedLines + (clampedVisualLine - m_layoutCacheComputedVisualLines);
        return qBound<qint64>(0, estimated, m_lineIndex->lineCount() - 1);
    }

    auto endIt = m_visualLineOffsets.cbegin() + static_cast<qsizetype>(m_layoutCacheComputedLines);
    auto it = std::upper_bound(m_visualLineOffsets.cbegin(), endIt, clampedVisualLine);
    if (it == m_visualLineOffsets.cbegin())
        return 0;
    return static_cast<qint64>(std::distance(m_visualLineOffsets.cbegin(), it) - 1);
}

void CustomCodeEditor::ensureLayoutCache() const
{
    if (!m_buffer) {
        m_visualLineOffsets.clear();
        m_layoutCacheComputedLines = 0;
        m_layoutCacheComputedVisualLines = 0;
        m_totalVisualLineCount = 0;
        m_maxExpandedLineLength = 0;
        m_layoutCacheValid = true;
        return;
    }

    const int currentWidth = availableTextWidth();
    if (m_wrapCacheWidth != currentWidth) {
        m_wrapCountCache.clear();
        m_wrapCacheWidth = currentWidth;
        m_layoutCacheValid = false;
        m_layoutCacheComputedLines = 0;
        m_layoutCacheComputedVisualLines = 0;
        m_visualLineOffsets.clear();
        m_totalVisualLineCount = m_lineIndex->lineCount();
        m_maxExpandedLineLength = 0;
    }

    if (!m_wordWrapEnabled) {
        m_layoutCacheComputedLines = m_lineIndex->lineCount();
        m_layoutCacheComputedVisualLines = m_lineIndex->lineCount();
        m_totalVisualLineCount = qMax<qint64>(1, m_lineIndex->lineCount());
        m_layoutCacheValid = true;
        return;
    }

    const int lineHeight = qMax(1, qRound(m_fontMetrics.height()));
    const qint64 currentVisualLine = qMax(0, verticalScrollBar()->value() / lineHeight);
    const qint64 visibleVisualLines = qMax<qint64>(1, viewport()->height() / lineHeight + 64);
    ensureLayoutCacheForVisualLine(currentVisualLine + visibleVisualLines);
}

void CustomCodeEditor::ensureLayoutCacheForLine(qint64 lineNum) const
{
    if (!m_buffer || !m_wordWrapEnabled)
        return;

    const int currentWidth = availableTextWidth();
    if (m_wrapCacheWidth != currentWidth) {
        m_wrapCountCache.clear();
        m_wrapCacheWidth = currentWidth;
        m_layoutCacheValid = false;
        m_layoutCacheComputedLines = 0;
        m_layoutCacheComputedVisualLines = 0;
        m_visualLineOffsets.clear();
        m_totalVisualLineCount = m_lineIndex->lineCount();
        m_maxExpandedLineLength = 0;
    }

    appendLayoutCacheUntilLine(lineNum);
}

void CustomCodeEditor::ensureLayoutCacheForVisualLine(qint64 visualLine) const
{
    if (!m_buffer || !m_wordWrapEnabled)
        return;

    const int currentWidth = availableTextWidth();
    if (m_wrapCacheWidth != currentWidth) {
        m_wrapCountCache.clear();
        m_wrapCacheWidth = currentWidth;
        m_layoutCacheValid = false;
        m_layoutCacheComputedLines = 0;
        m_layoutCacheComputedVisualLines = 0;
        m_visualLineOffsets.clear();
        m_totalVisualLineCount = m_lineIndex->lineCount();
        m_maxExpandedLineLength = 0;
    }

    appendLayoutCacheUntilVisual(visualLine);
}

void CustomCodeEditor::appendLayoutCacheUntilLine(qint64 inclusiveLine) const
{
    if (!m_buffer || !m_wordWrapEnabled)
        return;

    const qint64 lineCount = m_lineIndex->lineCount();
    if (lineCount <= 0)
        return;

    const qint64 targetLine = qBound<qint64>(0, inclusiveLine + 64, lineCount - 1);
    if (m_layoutCacheComputedLines > targetLine)
        return;

    if (m_visualLineOffsets.size() != lineCount)
        m_visualLineOffsets.resize(static_cast<int>(lineCount));

    while (m_layoutCacheComputedLines <= targetLine && m_layoutCacheComputedLines < lineCount) {
        const qint64 lineNum = m_layoutCacheComputedLines;
        m_visualLineOffsets[static_cast<int>(lineNum)] = m_layoutCacheComputedVisualLines;
        const QString text = expandedDisplayText(const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum));
        m_maxExpandedLineLength = qMax<qint64>(m_maxExpandedLineLength, text.length());
        m_layoutCacheComputedVisualLines += wrappedLineCount(lineNum);
        ++m_layoutCacheComputedLines;
    }

    const qint64 remainingLines = qMax<qint64>(0, lineCount - m_layoutCacheComputedLines);
    m_totalVisualLineCount = qMax<qint64>(1, m_layoutCacheComputedVisualLines + remainingLines);
    m_layoutCacheValid = (m_layoutCacheComputedLines >= lineCount);
}

void CustomCodeEditor::appendLayoutCacheUntilVisual(qint64 inclusiveVisualLine) const
{
    if (!m_buffer || !m_wordWrapEnabled)
        return;

    const qint64 lineCount = m_lineIndex->lineCount();
    if (lineCount <= 0)
        return;

    const qint64 targetVisualLine = qMax<qint64>(0, inclusiveVisualLine + 64);
    if (m_visualLineOffsets.size() != lineCount)
        m_visualLineOffsets.resize(static_cast<int>(lineCount));

    while (m_layoutCacheComputedLines < lineCount && m_layoutCacheComputedVisualLines <= targetVisualLine) {
        const qint64 lineNum = m_layoutCacheComputedLines;
        m_visualLineOffsets[static_cast<int>(lineNum)] = m_layoutCacheComputedVisualLines;
        const QString text = expandedDisplayText(const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum));
        m_maxExpandedLineLength = qMax<qint64>(m_maxExpandedLineLength, text.length());
        m_layoutCacheComputedVisualLines += wrappedLineCount(lineNum);
        ++m_layoutCacheComputedLines;
    }

    const qint64 remainingLines = qMax<qint64>(0, lineCount - m_layoutCacheComputedLines);
    m_totalVisualLineCount = qMax<qint64>(1, m_layoutCacheComputedVisualLines + remainingLines);
    m_layoutCacheValid = (m_layoutCacheComputedLines >= lineCount);
}

void CustomCodeEditor::trimVisibleCaches(qint64 firstVisibleLine, qint64 lastVisibleLine) const
{
    static constexpr qint64 kCacheMargin = 256;

    const qint64 keepStart = qMax<qint64>(0, firstVisibleLine - kCacheMargin);
    const qint64 keepEnd = qMax(keepStart, lastVisibleLine + kCacheMargin);

    for (auto it = m_displayLayoutCache.begin(); it != m_displayLayoutCache.end();) {
        const qint64 lineNum = it.key();
        if (lineNum < keepStart || lineNum > keepEnd)
            it = m_displayLayoutCache.erase(it);
        else
            ++it;
    }

    for (auto it = m_highlightFormatCache.begin(); it != m_highlightFormatCache.end();) {
        const qint64 lineNum = it.key();
        if (lineNum < keepStart || lineNum > keepEnd)
            it = m_highlightFormatCache.erase(it);
        else
            ++it;
    }

    if (m_wordWrapEnabled) {
        for (auto it = m_wrapCountCache.begin(); it != m_wrapCountCache.end();) {
            const qint64 lineNum = it.key();
            if (lineNum < keepStart || lineNum > keepEnd)
                it = m_wrapCountCache.erase(it);
            else
                ++it;
        }
    }
}

qint64 CustomCodeEditor::lineSegmentStartByte(qint64 lineNum, int segmentIndex) const
{
    const auto& layout = cachedLineLayout(lineNum);
    const auto& segments = layout.segments;
    if (segmentIndex < 0 || segmentIndex >= segments.size())
        return lineVisibleStart(lineNum);

    return bytePosForColumn(lineNum, segments[segmentIndex].startColumn);
}

qint64 CustomCodeEditor::lineSegmentEndByte(qint64 lineNum, int segmentIndex) const
{
    const auto& layout = cachedLineLayout(lineNum);
    const auto& segments = layout.segments;
    if (segmentIndex < 0 || segmentIndex >= segments.size())
        return lineVisibleEnd(lineNum);

    const auto& segment = segments[segmentIndex];
    return bytePosForColumn(lineNum, segment.startColumn + segment.length);
}

void CustomCodeEditor::invalidateLineCache(qint64 startLine, qint64 endLine)
{
    m_lineCache->invalidate(startLine, endLine);
    invalidateDisplayLayoutCache(startLine, endLine);
    invalidateWrapCache(startLine, endLine);
}

void CustomCodeEditor::invalidateWrapCache(qint64 startLine, qint64 endLine)
{
    const int currentWidth = availableTextWidth();
    if (m_wrapCacheWidth != currentWidth) {
        m_wrapCountCache.clear();
        m_wrapCacheWidth = currentWidth;
    }

    if (startLine < 0 || endLine < 0 || startLine > endLine) {
        m_wrapCountCache.clear();
        invalidateDisplayLayoutCache();
        m_layoutCacheValid = false;
        m_layoutCacheComputedLines = 0;
        m_layoutCacheComputedVisualLines = 0;
        m_visualLineOffsets.clear();
        m_totalVisualLineCount = m_lineIndex->lineCount();
        m_maxExpandedLineLength = 0;
        return;
    }

    for (qint64 lineNum = startLine; lineNum <= endLine; ++lineNum)
        m_wrapCountCache.remove(lineNum);
    invalidateDisplayLayoutCache(startLine, endLine);
    m_layoutCacheValid = false;
    if (startLine < m_layoutCacheComputedLines) {
        m_layoutCacheComputedLines = qMax<qint64>(0, startLine);
        m_layoutCacheComputedVisualLines = m_layoutCacheComputedLines > 0
            ? m_visualLineOffsets.value(static_cast<int>(m_layoutCacheComputedLines - 1)) + wrappedLineCount(m_layoutCacheComputedLines - 1)
            : 0;
        m_totalVisualLineCount = qMax<qint64>(1, m_layoutCacheComputedVisualLines + qMax<qint64>(0, m_lineIndex->lineCount() - m_layoutCacheComputedLines));
    }
}

void CustomCodeEditor::renderVisibleLines(QPainter* painter)
{
    if (!m_buffer || m_lineIndex->lineCount() == 0)
        return;

    const int lineHeight = qRound(m_fontMetrics.height());
    const int scrollY = verticalScrollBar()->value();
    const int viewportHeight = viewport()->height();
    const int lineNumberWidth = lineNumberAreaWidth();
    const int scrollX = horizontalScrollBar()->value();

    m_firstVisibleLine = logicalLineFromVisualLine(scrollY / lineHeight);
    m_visibleLineCount = (viewportHeight / lineHeight) + 2;
    const qint64 firstVisibleVisualLine = scrollY / lineHeight;
    const qint64 lastVisibleVisualLine = firstVisibleVisualLine + m_visibleLineCount;
    trimVisibleCaches(m_firstVisibleLine, logicalLineFromVisualLine(lastVisibleVisualLine + 64));

    qint64 visualIndex = visualLineIndexForLogicalLine(m_firstVisibleLine);
    for (qint64 lineNum = m_firstVisibleLine; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const auto& layout = cachedLineLayout(lineNum);
        const QString& text = layout.rawText;
        const auto& segments = layout.segments;
        const qint64 baseVisualLine = visualIndex;

        if (baseVisualLine + segments.size() <= firstVisibleVisualLine) {
            visualIndex += segments.size();
            continue;
        }
        if (baseVisualLine > lastVisibleVisualLine)
            break;

        for (int segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
            const qint64 visualLine = baseVisualLine + segmentIndex;
            if (visualLine < firstVisibleVisualLine)
                continue;
            if (visualLine > lastVisibleVisualLine)
                break;

            const int y = static_cast<int>(visualLine) * lineHeight - scrollY;
            const int x = lineNumberWidth + kTextLeftPadding - (m_wordWrapEnabled ? 0 : scrollX);
            const QRectF lineRect(x, y, qMax(1, viewport()->width() - lineNumberWidth - kTextLeftPadding + (m_wordWrapEnabled ? 0 : scrollX)), lineHeight);
            renderLine(painter, lineNum, text, lineRect, segments[segmentIndex].startColumn, segments[segmentIndex].length);
        }

        visualIndex += segments.size();
    }
}

void CustomCodeEditor::renderLineNumber(QPainter* painter, qint64 lineNum, const QRectF& rect)
{
    Q_UNUSED(painter);
    Q_UNUSED(lineNum);
    Q_UNUSED(rect);
}

void CustomCodeEditor::renderLine(QPainter* painter, qint64 lineNum, const QString& text, const QRectF& rect, int segmentStartColumn, int segmentLength)
{
    painter->save();
    painter->setClipRect(rect);
    painter->setFont(m_font);

    const auto& cachedLayout = cachedLineLayout(lineNum);
    const QString& displayText = cachedLayout.displayText;

    // Paint the line in segments so syntax formats can be applied without
    // handing ownership of the document text to QTextEdit/QPlainTextEdit.
    const auto formats = highlightFormatsForVisibleLine(lineNum, text);
    QVector<QTextLayout::FormatRange> displayFormats;
    displayFormats.reserve(formats.size());
    for (const auto& range : formats) {
        const int rawStart = qBound(0, range.start, text.length());
        const int rawEnd = qBound(rawStart, range.start + range.length, text.length());
        const int visualStart = cachedLayout.rawToVisual.value(rawStart, displayText.length());
        const int visualEnd = cachedLayout.rawToVisual.value(rawEnd, displayText.length());
        if (visualEnd <= visualStart)
            continue;

        QTextLayout::FormatRange visualRange = range;
        visualRange.start = visualStart;
        visualRange.length = visualEnd - visualStart;
        displayFormats.append(visualRange);
    }
    qreal x = rect.left();
    int cursor = 0;
    const QString segmentText = displayText.mid(segmentStartColumn, segmentLength);

    auto drawSegment = [&](const QString& segment, const QTextCharFormat& format) {
        if (segment.isEmpty())
            return;

        QFont font = m_font;
        if (format.fontItalic())
            font.setItalic(true);
        if (format.fontWeight() != QFont::Normal)
            font.setWeight(static_cast<QFont::Weight>(format.fontWeight()));

        painter->setFont(font);
        painter->setPen(format.foreground().style() == Qt::NoBrush ? palette().text().color()
                                                                   : format.foreground().color());
        painter->drawText(QPointF(x, rect.top() + m_fontMetrics.ascent() + (rect.height() - m_fontMetrics.height()) / 2.0), segment);
        x += QFontMetricsF(font).horizontalAdvance(segment);
    };

    for (const auto& range : displayFormats) {
        const int rangeStart = qMax(range.start, segmentStartColumn);
        const int rangeEnd = qMin(range.start + range.length, segmentStartColumn + segmentLength);
        if (rangeEnd <= rangeStart)
            continue;

        if (rangeStart > segmentStartColumn + cursor)
            drawSegment(displayText.mid(segmentStartColumn + cursor, rangeStart - (segmentStartColumn + cursor)), QTextCharFormat());
        drawSegment(displayText.mid(rangeStart, rangeEnd - rangeStart), range.format);
        cursor = rangeEnd - segmentStartColumn;
    }

    if (cursor < segmentText.length())
        drawSegment(segmentText.mid(cursor), QTextCharFormat());

    if (formats.isEmpty()) {
        painter->setPen(palette().text().color());
        painter->drawText(rect.adjusted(0, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, segmentText);
    }

    painter->restore();
}

qint64 CustomCodeEditor::lineFromBytePos(qint64 bytePos) const
{
    return m_lineIndex->lineFromBytePos(bytePos);
}

qint64 CustomCodeEditor::bytePosFromLine(qint64 lineNum) const
{
    return m_lineIndex->lineStartPos(lineNum);
}

qint64 CustomCodeEditor::bytePosFromPoint(const QPoint& point) const
{
    if (!m_buffer || m_lineIndex->lineCount() == 0)
        return firstTextByte();

    const int lineHeight = qRound(m_fontMetrics.height());
    const int scrollY = verticalScrollBar()->value();
    const int scrollX = horizontalScrollBar()->value();
    const qint64 visualLine = qMax<qint64>(0, (point.y() + scrollY) / lineHeight);
    const qint64 lineNum = logicalLineFromVisualLine(visualLine);
    const auto& layout = cachedLineLayout(lineNum);
    const QString& text = layout.displayText;
    const auto& segments = layout.segments;
    const qint64 segmentIndex = qBound<qint64>(0, visualLine - visualLineIndexForLogicalLine(lineNum), segments.size() - 1);
    const auto& segment = segments[segmentIndex];
    const int localX = qMax(0, point.x() + (m_wordWrapEnabled ? 0 : scrollX) - lineNumberAreaWidth() - kTextLeftPadding);

    int bestColumn = segment.startColumn + segment.length;
    int previousWidth = 0;
    for (int column = 0; column <= segment.length; ++column) {
        const int width = qRound(displayAdvanceForRange(lineNum, segment.startColumn, column));
        const int snapThreshold = previousWidth + (width - previousWidth) / 2;
        if (localX <= snapThreshold) {
            bestColumn = segment.startColumn + qMax(0, column - 1);
            break;
        }
        previousWidth = width;
    }

    return bytePosForColumn(lineNum, bestColumn);
}

qreal CustomCodeEditor::displayAdvanceForRange(qint64 lineNum, int startColumn, int length) const
{
    if (length <= 0)
        return 0;

    const auto& cachedLayout = cachedLineLayout(lineNum);
    const QString& displayText = cachedLayout.displayText;
    const QString& rawText = cachedLayout.rawText;
    const int clampedStart = qBound(0, startColumn, displayText.length());
    const int clampedEnd = qBound(clampedStart, startColumn + length, displayText.length());
    if (clampedEnd <= clampedStart)
        return 0;

    const auto formats = highlightFormatsForVisibleLine(lineNum, rawText);
    QVector<QTextLayout::FormatRange> displayFormats;
    displayFormats.reserve(formats.size());
    for (const auto& range : formats) {
        const int rawStart = qBound(0, range.start, rawText.length());
        const int rawEnd = qBound(rawStart, range.start + range.length, rawText.length());
        const int visualStart = cachedLayout.rawToVisual.value(rawStart, displayText.length());
        const int visualEnd = cachedLayout.rawToVisual.value(rawEnd, displayText.length());
        if (visualEnd <= visualStart)
            continue;

        QTextLayout::FormatRange visualRange = range;
        visualRange.start = visualStart;
        visualRange.length = visualEnd - visualStart;
        displayFormats.append(visualRange);
    }

    qreal advance = 0;
    int cursor = clampedStart;
    for (const auto& range : displayFormats) {
        const int rangeStart = qMax(range.start, clampedStart);
        const int rangeEnd = qMin(range.start + range.length, clampedEnd);
        if (rangeEnd <= rangeStart)
            continue;

        if (rangeStart > cursor)
            advance += m_fontMetrics.horizontalAdvance(displayText.mid(cursor, rangeStart - cursor));

        QFont font = m_font;
        if (range.format.fontItalic())
            font.setItalic(true);
        if (range.format.fontWeight() != QFont::Normal)
            font.setWeight(static_cast<QFont::Weight>(range.format.fontWeight()));

        advance += QFontMetricsF(font).horizontalAdvance(displayText.mid(rangeStart, rangeEnd - rangeStart));
        cursor = rangeEnd;
    }

    if (cursor < clampedEnd)
        advance += m_fontMetrics.horizontalAdvance(displayText.mid(cursor, clampedEnd - cursor));

    return advance;
}

void CustomCodeEditor::ensureCursorVisible()
{
    if (!m_buffer)
        return;

    const qint64 cursorLine = lineFromBytePos(m_cursorBytePos);
    const int lineHeight = qRound(m_fontMetrics.height());
    const auto& layout = cachedLineLayout(cursorLine);
    const QString& text = layout.displayText;
    const auto& segments = layout.segments;
    const qint64 column = columnForBytePos(cursorLine, m_cursorBytePos);
    int segmentIndex = 0;
    for (int i = 0; i < segments.size(); ++i) {
        if (column >= segments[i].startColumn && column <= segments[i].startColumn + segments[i].length) {
            segmentIndex = i;
            break;
        }
    }
    const int cursorY = static_cast<int>(visualLineIndexForLogicalLine(cursorLine) + segmentIndex) * lineHeight;
    const int scrollY = verticalScrollBar()->value();
    const int viewportHeight = viewport()->height();

    if (cursorY < scrollY)
        verticalScrollBar()->setValue(cursorY);
    else if (cursorY + lineHeight > scrollY + viewportHeight)
        verticalScrollBar()->setValue(cursorY - viewportHeight + lineHeight);

    const int xPrefixColumn = static_cast<int>(column - segments[segmentIndex].startColumn);
    const int cursorX = lineNumberAreaWidth() + kTextLeftPadding + qRound(displayAdvanceForRange(cursorLine, segments[segmentIndex].startColumn, xPrefixColumn));
    const int scrollX = horizontalScrollBar()->value();
    const int viewportWidth = viewport()->width();
    if (m_wordWrapEnabled)
        return;
    if (cursorX < scrollX)
        horizontalScrollBar()->setValue(cursorX);
    else if (cursorX > scrollX + viewportWidth - qRound(m_fontMetrics.horizontalAdvance(QLatin1Char('M'))))
        horizontalScrollBar()->setValue(cursorX - viewportWidth + qRound(m_fontMetrics.horizontalAdvance(QLatin1Char('M'))));
}

void CustomCodeEditor::updateSelection(qint64 byteStart, qint64 byteLength)
{
    if (!m_buffer) {
        m_selectionStart = 0;
        m_selectionLength = 0;
        return;
    }

    const qint64 bufferSize = m_buffer->size();
    m_selectionStart = qBound<qint64>(0, byteStart, bufferSize);
    m_selectionLength = qBound<qint64>(0, byteLength, bufferSize - m_selectionStart);
    if (!hasSelection())
        m_selectionAnchor = -1;
}

void CustomCodeEditor::updateLineNumberAreaWidth()
{
    if (m_lineNumberArea) {
        const QRect cr = contentsRect();
        m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
        m_lineNumberArea->updateGeometry();
    }
}

void CustomCodeEditor::updateSelectionAfterMove(qint64 oldCursorPos)
{
    if (m_selectionAnchor < 0)
        m_selectionAnchor = oldCursorPos;

    m_selectionStart = qMin(m_selectionAnchor, m_cursorBytePos);
    m_selectionLength = qAbs(m_cursorBytePos - m_selectionAnchor);
    syncSelectionToBuffer();
}

void CustomCodeEditor::clearSelection()
{
    m_selectionStart = m_cursorBytePos;
    m_selectionLength = 0;
    m_selectionAnchor = -1;
    syncSelectionToBuffer();
}

void CustomCodeEditor::copySelection()
{
    if (hasSelection())
        QApplication::clipboard()->setText(selectedText());
}

void CustomCodeEditor::cutSelection()
{
    if (!hasSelection())
        return;
    copySelection();
    replaceRange(m_selectionStart, m_selectionLength, QByteArray());
}

void CustomCodeEditor::pasteFromClipboard()
{
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty())
        return;

    if (!hasSelection()) {
        const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
        const QString lineText = displayTextForLine(lineNum);
        QString indent;
        for (const QChar ch : lineText) {
            if (ch == QLatin1Char(' ') || ch == QLatin1Char('\t'))
                indent.append(ch);
            else
                break;
        }
        text = EditorTypingBehaviors::smartPasteText(text, indent);
    }

    insertText(text);
}

void CustomCodeEditor::selectAll()
{
    if (!m_buffer)
        return;

    m_selectionStart = firstTextByte();
    m_selectionLength = qMax<qint64>(0, m_buffer->size() - m_selectionStart);
    m_selectionAnchor = m_selectionStart;
    m_cursorBytePos = m_selectionStart + m_selectionLength;
    syncSelectionToBuffer();
    ensureCursorVisible();
    viewport()->update();
}

void CustomCodeEditor::undo()
{
    if (!m_buffer || !m_buffer->canUndo())
        return;

    m_applyingBufferEdit = true;
    m_buffer->undo();
    m_applyingBufferEdit = false;
    clearSelection();
    clampCursorToBuffer();
}

void CustomCodeEditor::redo()
{
    if (!m_buffer || !m_buffer->canRedo())
        return;

    m_applyingBufferEdit = true;
    m_buffer->redo();
    m_applyingBufferEdit = false;
    clearSelection();
    clampCursorToBuffer();
}

void CustomCodeEditor::deleteBackward()
{
    if (!m_buffer)
        return;

    if (hasSelection()) {
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][deleteBackward] removing current selection";
        replaceRange(m_selectionStart, m_selectionLength, QByteArray());
        return;
    }

    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineStart = lineVisibleStart(lineNum);
    if (m_cursorBytePos <= lineStart) {
        if (lineNum <= 0)
            return;

        const qint64 rawLineStart = bytePosFromLine(lineNum);
        qint64 removeStart = rawLineStart - 1;
        qint64 removeLength = 1;
        if (removeStart > 0 && m_buffer->getByte(removeStart - 1) == '\r' && m_buffer->getByte(removeStart) == '\n') {
            --removeStart;
            ++removeLength;
        }
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][deleteBackward] join-line removeStart=" << removeStart << " removeLength=" << removeLength;
        replaceRange(removeStart, removeLength, QByteArray());
        return;
    }

    const QString text = displayTextForLine(lineNum);
    const QString prefix = displayPrefixForPosition(lineNum, m_cursorBytePos);
    const int currentCharPos = prefix.length();

    if (currentCharPos < text.length()) {
        const QChar previousChar = currentCharPos > 0 ? text.at(currentCharPos - 1) : QChar();
        const QChar nextChar = text.at(currentCharPos);
        if (EditorTypingBehaviors::isMatchingPair(previousChar, nextChar)) {
            const qint64 prevPos = lineStart + m_utf8Decoder->charPosToByte(text, currentCharPos - 1);
            const qint64 nextPos = lineStart + m_utf8Decoder->charPosToByte(text, currentCharPos + 1);
            replaceRange(prevPos,
                         nextPos - prevPos,
                         QByteArray());
            return;
        }
    }

    const int smartIndentDeleteCount = EditorTypingBehaviors::smartBackspaceColumnDeleteCount(prefix, m_tabReplaceSize, m_tabReplace);
    if (smartIndentDeleteCount > 0) {
        const int smartPrevCharPos = qMax(0, currentCharPos - smartIndentDeleteCount);
        const qint64 smartPrevPos = lineStart + m_utf8Decoder->charPosToByte(text, smartPrevCharPos);
        replaceRange(smartPrevPos, m_cursorBytePos - smartPrevPos, QByteArray());
        return;
    }

    const int prevCharPos = qMax(0, currentCharPos - 1);
    const qint64 prevPos = lineStart + m_utf8Decoder->charPosToByte(text, prevCharPos);
    if (kLogInputEvents)
        qDebug() << "[CustomCodeEditor][deleteBackward] cursor=" << m_cursorBytePos << " line=" << lineNum << " prevCharPos=" << prevCharPos << " prevPos=" << prevPos;
    replaceRange(prevPos, m_cursorBytePos - prevPos, QByteArray());
}

void CustomCodeEditor::deleteForward()
{
    if (!m_buffer)
        return;

    if (hasSelection()) {
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][deleteForward] removing current selection";
        replaceRange(m_selectionStart, m_selectionLength, QByteArray());
        return;
    }

    if (m_cursorBytePos >= m_buffer->size())
        return;

    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineEnd = lineVisibleEnd(lineNum);
    if (m_cursorBytePos >= lineEnd) {
        const qint64 rawNext = lineEnd;
        qint64 removeLength = 1;
        if (rawNext + 1 <= m_buffer->size() && m_buffer->getByte(rawNext) == '\r' && m_buffer->getByte(rawNext + 1) == '\n')
            removeLength = 2;
        if (kLogInputEvents)
            qDebug() << "[CustomCodeEditor][deleteForward] join-line rawNext=" << rawNext << " removeLength=" << removeLength;
        replaceRange(rawNext, removeLength, QByteArray());
        return;
    }

    const QString text = displayTextForLine(lineNum);
    const QString prefix = displayPrefixForPosition(lineNum, m_cursorBytePos);
    const int currentCharPos = prefix.length();

    if (currentCharPos + 1 < text.length()) {
        const QChar currentChar = text.at(currentCharPos);
        const QChar nextChar = text.at(currentCharPos + 1);
        if (EditorTypingBehaviors::isMatchingPair(currentChar, nextChar)) {
            const qint64 nextPairPos = lineVisibleStart(lineNum) + m_utf8Decoder->charPosToByte(text, currentCharPos + 2);
            replaceRange(m_cursorBytePos, nextPairPos - m_cursorBytePos, QByteArray());
            return;
        }
    }

    const int nextCharPos = qMin(text.length(), currentCharPos + 1);
    const qint64 nextPos = lineVisibleStart(lineNum) + m_utf8Decoder->charPosToByte(text, nextCharPos);
    if (kLogInputEvents)
        qDebug() << "[CustomCodeEditor][deleteForward] cursor=" << m_cursorBytePos << " line=" << lineNum << " nextCharPos=" << nextCharPos << " nextPos=" << nextPos;
    replaceRange(m_cursorBytePos, nextPos - m_cursorBytePos, QByteArray());
}

void CustomCodeEditor::deleteWordBackward()
{
    if (!m_buffer)
        return;

    if (hasSelection()) {
        replaceRange(m_selectionStart, m_selectionLength, QByteArray());
        return;
    }

    const qint64 target = previousWordBoundary(m_cursorBytePos);
    if (kLogInputEvents)
        qDebug() << "[CustomCodeEditor][deleteWordBackward] cursor=" << m_cursorBytePos << " target=" << target;
    if (target < m_cursorBytePos)
        replaceRange(target, m_cursorBytePos - target, QByteArray());
}

void CustomCodeEditor::deleteWordForward()
{
    if (!m_buffer)
        return;

    if (hasSelection()) {
        replaceRange(m_selectionStart, m_selectionLength, QByteArray());
        return;
    }

    const qint64 target = nextWordBoundary(m_cursorBytePos);
    if (kLogInputEvents)
        qDebug() << "[CustomCodeEditor][deleteWordForward] cursor=" << m_cursorBytePos << " target=" << target;
    if (target > m_cursorBytePos)
        replaceRange(m_cursorBytePos, target - m_cursorBytePos, QByteArray());
}

void CustomCodeEditor::toggleLineComment()
{
    if (!m_buffer)
        return;

    const QString comment = lineCommentPrefix();
    if (comment.isEmpty())
        return;

    const qint64 selectionStart = hasSelection() ? m_selectionStart : m_cursorBytePos;
    const qint64 selectionEnd = hasSelection() ? (m_selectionStart + m_selectionLength) : m_cursorBytePos;
    const qint64 startLine = lineFromBytePos(selectionStart);
    const qint64 endAnchor = hasSelection() ? qMax(selectionStart, selectionEnd - 1) : selectionEnd;
    const qint64 endLine = lineFromBytePos(endAnchor);

    QStringList lines;
    for (qint64 lineNum = startLine; lineNum <= endLine; ++lineNum) {
        lines.append(displayTextForLine(lineNum));
    }

    qint64 replaceStart = lineVisibleStart(startLine);
    qint64 replaceEnd = lineVisibleEnd(endLine);

    const QStringList updatedLines = EditorTypingBehaviors::toggleLineComments(lines, comment);
    QByteArray replacement;
    for (int i = 0; i < updatedLines.size(); ++i) {
        replacement.append(updatedLines.at(i).toUtf8());
        if (i + 1 < updatedLines.size())
            replacement.append('\n');
    }

    replaceRange(replaceStart, replaceEnd - replaceStart, replacement);
    m_selectionStart = replaceStart;
    m_selectionLength = replacement.size();
    m_selectionAnchor = m_selectionStart;
    m_cursorBytePos = m_selectionStart + m_selectionLength;
    syncSelectionToBuffer();
    viewport()->update();
}

void CustomCodeEditor::insertNewline()
{
    if (!m_buffer)
        return;

    const qint64 insertPos = hasSelection() ? m_selectionStart : m_cursorBytePos;
    const qint64 removeLength = hasSelection() ? m_selectionLength : 0;
    const qint64 lineNum = lineFromBytePos(insertPos);
    const QString lineText = displayTextForLine(lineNum);
    const QString linePrefix = displayPrefixForPosition(lineNum, insertPos);
    const QString lineSuffix = lineText.mid(linePrefix.length());

    QString indent;
    for (const QChar ch : lineText) {
        if (ch == QLatin1Char(' ') || ch == QLatin1Char('\t')) {
            indent.append(ch);
            continue;
        }
        break;
    }

    const QString indentUnit = EditorTypingBehaviors::indentUnitText(m_tabReplace, m_tabReplaceSize);

    if (!hasSelection()) {
        const auto emptyLineReplacement = EditorTypingBehaviors::emptyLineEnterReplacement(
            lineText,
            indent,
            indentUnit,
            m_tabReplace);
        if (emptyLineReplacement.has_value()) {
            const qint64 replaceStart = lineVisibleStart(lineNum);
            const qint64 replaceEnd = lineVisibleEnd(lineNum);
            replaceRange(replaceStart, replaceEnd - replaceStart, *emptyLineReplacement);
            m_cursorBytePos = replaceStart + emptyLineReplacement->size();
            m_selectionStart = m_cursorBytePos;
            m_selectionLength = 0;
            m_selectionAnchor = -1;
            syncSelectionToBuffer();
            ensureCursorVisible();
            emit cursorPositionChanged();
            viewport()->update();
            return;
        }
    }

    const QString trimmedPrefix = linePrefix.trimmed();
    const QString trimmedSuffix = lineSuffix.trimmed();
    const auto indentDecision = EditorTypingBehaviors::indentationAfterPrefix(trimmedPrefix);
    const bool shouldIncreaseIndent = indentDecision.shouldIncrease;
    const QChar expectedCloser = indentDecision.splitCloser;
    const bool splitClosingBrace = expectedCloser.isNull()
        ? false
        : trimmedSuffix.startsWith(expectedCloser);

    QByteArray replacement("\n");
    replacement.append(indent.toUtf8());
    if (shouldIncreaseIndent)
        replacement.append(indentUnit.toUtf8());
    if (splitClosingBrace) {
        replacement.append('\n');
        replacement.append(indent.toUtf8());
    }

    replaceRange(insertPos, removeLength, replacement);

    if (splitClosingBrace) {
        m_cursorBytePos = insertPos + 1 + indent.toUtf8().size() + indentUnit.toUtf8().size();
        m_selectionStart = m_cursorBytePos;
        m_selectionLength = 0;
        m_selectionAnchor = -1;
        syncSelectionToBuffer();
        emit cursorPositionChanged();
        viewport()->update();
    }
}

void CustomCodeEditor::insertLineBelow()
{
    if (!m_buffer)
        return;

    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineEnd = lineVisibleEnd(lineNum);
    qint64 insertPos = lineEnd;
    if (insertPos < m_buffer->size()) {
        if (m_buffer->getByte(insertPos) == '\r' && insertPos + 1 < m_buffer->size() && m_buffer->getByte(insertPos + 1) == '\n')
            insertPos += 2;
        else if (m_buffer->getByte(insertPos) == '\n')
            insertPos += 1;
    }

    replaceRange(insertPos, 0, QByteArray("\n"));
    m_cursorBytePos = insertPos + 1;
    m_selectionStart = m_cursorBytePos;
    m_selectionLength = 0;
    m_selectionAnchor = -1;
    syncSelectionToBuffer();
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::insertLineAbove()
{
    if (!m_buffer)
        return;

    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineStart = lineVisibleStart(lineNum);

    replaceRange(lineStart, 0, QByteArray("\n"));
    m_cursorBytePos = lineStart;
    m_selectionStart = m_cursorBytePos;
    m_selectionLength = 0;
    m_selectionAnchor = -1;
    syncSelectionToBuffer();
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::deleteCurrentLine()
{
    if (!m_buffer)
        return;

    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineStart = lineVisibleStart(lineNum);
    const qint64 lineEnd = lineVisibleEnd(lineNum);
    qint64 deleteEnd = lineEnd;
    if (deleteEnd < m_buffer->size()) {
        if (m_buffer->getByte(deleteEnd) == '\r' && deleteEnd + 1 < m_buffer->size() && m_buffer->getByte(deleteEnd + 1) == '\n')
            deleteEnd += 2;
        else if (m_buffer->getByte(deleteEnd) == '\n')
            deleteEnd += 1;
    }

    replaceRange(lineStart, deleteEnd - lineStart, QByteArray());
    m_cursorBytePos = lineStart;
    m_selectionStart = m_cursorBytePos;
    m_selectionLength = 0;
    m_selectionAnchor = -1;
    syncSelectionToBuffer();
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::insertTab()
{
    if (hasSelection()) {
        const qint64 selStart = m_selectionStart;
        const qint64 selEnd = m_selectionStart + m_selectionLength;
        const qint64 startLine = lineFromBytePos(selStart);
        const qint64 endLine = lineFromBytePos(qMax(selStart, selEnd - 1));
        const QByteArray indent = m_tabReplace ? QByteArray(m_tabReplaceSize, ' ') : QByteArray("\t");
        QByteArray replacement;
        const qint64 replaceStart = lineVisibleStart(startLine);
        const qint64 replaceEnd = lineVisibleEnd(endLine);

        for (qint64 lineNum = startLine; lineNum <= endLine; ++lineNum) {
            replacement.append(indent);
            replacement.append(displayTextForLine(lineNum).toUtf8());
            if (lineNum < endLine)
                replacement.append('\n');
        }

        replaceRange(replaceStart, replaceEnd - replaceStart, replacement);
        const qint64 newSelectionStart = replaceStart + indent.size();
        const qint64 newSelectionEnd = replaceStart + replacement.size();
        m_selectionStart = newSelectionStart;
        m_selectionLength = qMax<qint64>(0, newSelectionEnd - newSelectionStart);
        m_selectionAnchor = m_selectionStart;
        m_cursorBytePos = newSelectionStart + m_selectionLength;
        syncSelectionToBuffer();
        viewport()->update();
        return;
    }

    const QByteArray indent = m_tabReplace ? QByteArray(m_tabReplaceSize, ' ') : QByteArray("\t");
    const qint64 insertPos = m_cursorBytePos;
    replaceRange(insertPos, 0, indent);

    // Keep the caret on the newly inserted indentation even if buffer signals
    // briefly bounce selection/cursor state through sibling tool views.
    m_cursorBytePos = insertPos + indent.size();
    m_selectionStart = m_cursorBytePos;
    m_selectionLength = 0;
    m_selectionAnchor = -1;
    syncSelectionToBuffer();
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::outdentSelection()
{
    if (!hasSelection())
        return;

    const qint64 selStart = m_selectionStart;
    const qint64 selEnd = m_selectionStart + m_selectionLength;
    const qint64 startLine = lineFromBytePos(selStart);
    const qint64 endLine = lineFromBytePos(qMax(selStart, selEnd - 1));
    const qint64 replaceStart = lineVisibleStart(startLine);
    const qint64 replaceEnd = lineVisibleEnd(endLine);
    QByteArray replacement;
    qint64 firstLineRemoved = 0;

    for (qint64 lineNum = startLine; lineNum <= endLine; ++lineNum) {
        QString text = displayTextForLine(lineNum);
        int removedCount = 0;
        if (text.startsWith(QLatin1Char('\t'))) {
            text.remove(0, 1);
            removedCount = 1;
        } else {
            int removeCount = 0;
            while (removeCount < text.length() && removeCount < m_tabReplaceSize && text.at(removeCount) == QLatin1Char(' '))
                ++removeCount;
            text.remove(0, removeCount);
            removedCount = removeCount;
        }
        if (lineNum == startLine)
            firstLineRemoved = removedCount;
        replacement.append(text.toUtf8());
        if (lineNum < endLine)
            replacement.append('\n');
    }

    replaceRange(replaceStart, replaceEnd - replaceStart, replacement);
    const qint64 newSelectionStart = replaceStart + qMin<qint64>(firstLineRemoved, selStart - replaceStart);
    m_selectionStart = newSelectionStart;
    m_selectionLength = qMax<qint64>(0, (replaceStart + replacement.size()) - newSelectionStart);
    m_selectionAnchor = m_selectionStart;
    m_cursorBytePos = m_selectionStart + m_selectionLength;
    syncSelectionToBuffer();
    viewport()->update();
}

void CustomCodeEditor::duplicateSelectionOrLine(bool duplicateAbove)
{
    if (!m_buffer)
        return;

    if (hasSelection()) {
        const auto operation = EditorEditOperations::duplicateSelection(
            m_selectionStart,
            m_selectionLength,
            m_buffer->read(m_selectionStart, m_selectionLength),
            duplicateAbove);
        replaceRange(operation.replaceStart, operation.replaceLength, operation.replacement);
        m_selectionStart = operation.selectionStart;
        m_selectionLength = operation.selectionLength;
        m_selectionAnchor = m_selectionStart;
        m_cursorBytePos = m_selectionStart + m_selectionLength;
        syncSelectionToBuffer();
        viewport()->update();
        return;
    }

    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineStart = lineVisibleStart(lineNum);
    const qint64 lineEnd = lineVisibleEnd(lineNum);
    QByteArray lineBytes = m_buffer->read(lineStart, lineEnd - lineStart);

    qint64 newlineStart = lineEnd;
    QByteArray newlineBytes;
    if (newlineStart < m_buffer->size()) {
        newlineBytes.append(m_buffer->getByte(newlineStart));
        if (m_buffer->getByte(newlineStart) == '\r' && newlineStart + 1 < m_buffer->size() && m_buffer->getByte(newlineStart + 1) == '\n')
            newlineBytes.append(m_buffer->getByte(newlineStart + 1));
    } else {
        newlineBytes = QByteArray("\n");
    }

    const auto operation = EditorEditOperations::duplicateLine(
        lineStart,
        lineEnd - lineStart,
        m_buffer->size(),
        lineBytes,
        newlineBytes,
        duplicateAbove);
    replaceRange(operation.replaceStart, operation.replaceLength, operation.replacement);
    m_selectionStart = operation.selectionStart;
    m_selectionLength = operation.selectionLength;
    m_selectionAnchor = m_selectionStart;
    m_cursorBytePos = m_selectionStart + m_selectionLength;
    syncSelectionToBuffer();
    viewport()->update();
}

void CustomCodeEditor::moveSelectedLines(int direction)
{
    if (!m_buffer || direction == 0)
        return;

    const qint64 rangeStart = hasSelection() ? m_selectionStart : m_cursorBytePos;
    const qint64 rangeEnd = hasSelection() ? (m_selectionStart + m_selectionLength) : m_cursorBytePos;
    const qint64 startLine = lineFromBytePos(rangeStart);
    const qint64 endLine = lineFromBytePos(hasSelection() ? qMax(rangeStart, rangeEnd - 1) : rangeEnd);

    if (direction < 0 && startLine <= 0)
        return;
    if (direction > 0 && endLine >= m_lineIndex->lineCount() - 1)
        return;

    const qint64 blockStart = lineVisibleStart(startLine);
    const qint64 blockEnd = lineVisibleEnd(endLine);
    qint64 blockRawEnd = blockEnd;
    if (blockRawEnd < m_buffer->size()) {
        if (m_buffer->getByte(blockRawEnd) == '\r' && blockRawEnd + 1 < m_buffer->size() && m_buffer->getByte(blockRawEnd + 1) == '\n')
            blockRawEnd += 2;
        else if (m_buffer->getByte(blockRawEnd) == '\n')
            blockRawEnd += 1;
    }
    const QByteArray blockBytes = m_buffer->read(blockStart, blockRawEnd - blockStart);

    if (direction < 0) {
        const qint64 prevLine = startLine - 1;
        const qint64 prevStart = lineVisibleStart(prevLine);
        const qint64 prevEnd = lineVisibleEnd(prevLine);
        qint64 prevRawEnd = prevEnd;
        if (prevRawEnd < m_buffer->size()) {
            if (m_buffer->getByte(prevRawEnd) == '\r' && prevRawEnd + 1 < m_buffer->size() && m_buffer->getByte(prevRawEnd + 1) == '\n')
                prevRawEnd += 2;
            else if (m_buffer->getByte(prevRawEnd) == '\n')
                prevRawEnd += 1;
        }
        const auto operation = EditorEditOperations::moveBlockUp(
            prevStart,
            m_buffer->read(prevStart, prevRawEnd - prevStart),
            blockBytes);
        if (!operation.has_value())
            return;
        replaceRange(operation->replaceStart, operation->replaceLength, operation->replacement);
        m_selectionStart = operation->selectionStart;
        m_selectionLength = operation->selectionLength;
    } else {
        const qint64 nextLine = endLine + 1;
        const qint64 nextStart = lineVisibleStart(nextLine);
        const qint64 nextEnd = lineVisibleEnd(nextLine);
        qint64 nextRawEnd = nextEnd;
        if (nextRawEnd < m_buffer->size()) {
            if (m_buffer->getByte(nextRawEnd) == '\r' && nextRawEnd + 1 < m_buffer->size() && m_buffer->getByte(nextRawEnd + 1) == '\n')
                nextRawEnd += 2;
            else if (m_buffer->getByte(nextRawEnd) == '\n')
                nextRawEnd += 1;
        }
        const auto operation = EditorEditOperations::moveBlockDown(
            blockStart,
            blockBytes,
            m_buffer->read(nextStart, nextRawEnd - nextStart));
        if (!operation.has_value())
            return;
        replaceRange(operation->replaceStart, operation->replaceLength, operation->replacement);
        m_selectionStart = operation->selectionStart;
        m_selectionLength = operation->selectionLength;
    }

    m_selectionAnchor = m_selectionStart;
    m_cursorBytePos = m_selectionStart + m_selectionLength;
    syncSelectionToBuffer();
    ensureCursorVisible();
    viewport()->update();
}

void CustomCodeEditor::insertText(const QString& text)
{
    if (text.isEmpty())
        return;

    if (!hasSelection()) {
        const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
        const qint64 lineStart = lineVisibleStart(lineNum);
        const QString linePrefix = displayPrefixForPosition(lineNum, m_cursorBytePos);
        const QString lineSuffix = displayTextForLine(lineNum).mid(linePrefix.length());
        const auto dedentedPrefix = EditorTypingBehaviors::autoDedentedPrefix(
            text,
            linePrefix,
            lineSuffix,
            EditorTypingBehaviors::indentUnitText(m_tabReplace, m_tabReplaceSize),
            m_tabReplace);

        if (dedentedPrefix.has_value()) {
            replaceRange(lineStart, m_cursorBytePos - lineStart, dedentedPrefix->toUtf8() + text.toUtf8());
            return;
        }
    }

    const auto wrapSelection = [this](const QByteArray& pair, qint64 cursorAdvance) {
        const qint64 start = hasSelection() ? m_selectionStart : m_cursorBytePos;
        const qint64 length = hasSelection() ? m_selectionLength : 0;
        const QByteArray selectedBytes = length > 0 ? m_buffer->read(start, length) : QByteArray();
        QByteArray replacement = pair.left(1);
        replacement.append(selectedBytes);
        replacement.append(pair.right(1));
        replaceRange(start, length, replacement);
        m_cursorBytePos = start + cursorAdvance + selectedBytes.size();
        m_selectionStart = m_cursorBytePos;
        m_selectionLength = 0;
        m_selectionAnchor = -1;
        syncSelectionToBuffer();
        emit cursorPositionChanged();
        viewport()->update();
    };

    const auto skipClosing = [this, &text]() {
        if (hasSelection() || m_cursorBytePos >= m_buffer->size())
            return false;
        if (!EditorTypingBehaviors::shouldSkipClosing(text, m_buffer->getByte(m_cursorBytePos)))
            return false;

        ++m_cursorBytePos;
        clearSelection();
        ensureCursorVisible();
        emit cursorPositionChanged();
        viewport()->update();
        return true;
    };

    const QByteArray pair = EditorTypingBehaviors::matchingPair(text);
    if (!pair.isEmpty()) {
        wrapSelection(pair, 1);
        return;
    }

    if (skipClosing()) {
        return;
    }

    replaceRange(hasSelection() ? m_selectionStart : m_cursorBytePos,
                 hasSelection() ? m_selectionLength : 0,
                 text.toUtf8());
}

void CustomCodeEditor::replaceRange(qint64 start, qint64 length, const QByteArray& replacement)
{
    if (!m_buffer)
        return;

    const qint64 bufferSize = m_buffer->size();
    start = qBound<qint64>(firstTextByte(), start, bufferSize);
    length = qBound<qint64>(0, length, bufferSize - start);
    const QByteArray removedBytes = length > 0 ? m_buffer->read(start, length) : QByteArray();

    logEditRange("replaceRange", start, length, removedBytes, replacement);

    m_cursorBytePos = start + replacement.size();
    m_selectionStart = m_cursorBytePos;
    m_selectionLength = 0;
    m_selectionAnchor = -1;

    m_applyingBufferEdit = true;
    if (length == 0) {
        m_buffer->insertBytes(start, replacement);
    } else if (replacement.isEmpty()) {
        m_buffer->removeBytes(start, length);
    } else if (replacement.size() == length) {
        m_buffer->setBytes(start, replacement);
    } else {
        QByteArray data = m_buffer->data();
        data.replace(start, length, replacement);
        m_buffer->replaceData(data);
    }
    m_applyingBufferEdit = false;

    syncSelectionToBuffer();
    emit cursorPositionChanged();
    emit contentsChanged();
}

void CustomCodeEditor::syncSelectionToBuffer()
{
    if (!m_buffer)
        return;

    m_updatingSelection = true;
    m_buffer->setSelection(hasSelection() ? m_selectionStart : m_cursorBytePos,
                           hasSelection() ? m_selectionLength : 0);
    m_updatingSelection = false;
}

void CustomCodeEditor::updateModificationState()
{
    emit modificationChanged(isModified());
    emit contentsChanged();
}

qint64 CustomCodeEditor::firstTextByte() const
{
    return m_hasUtf8Bom ? 3 : 0;
}

qint64 CustomCodeEditor::lineVisibleStart(qint64 lineNum) const
{
    const qint64 start = bytePosFromLine(lineNum);
    return lineNum == 0 ? qMax(start, firstTextByte()) : start;
}

qint64 CustomCodeEditor::lineVisibleEnd(qint64 lineNum) const
{
    const qint64 start = bytePosFromLine(lineNum);
    qint64 end = start + m_lineIndex->lineLength(lineNum);
    if (end > start && m_buffer) {
        if (m_buffer->getByte(end - 1) == '\n')
            --end;
        if (end > start && m_buffer->getByte(end - 1) == '\r')
            --end;
    }
    return qMax(lineVisibleStart(lineNum), end);
}

QString CustomCodeEditor::decodeBytesForDisplay(qint64 startByte, const QByteArray& bytes) const
{
    bool hasBom = false;
    QString text = startByte == 0 ? m_utf8Decoder->decodeWithBOM(bytes, hasBom) : m_utf8Decoder->decode(bytes);
    if (text.endsWith('\n'))
        text.chop(1);
    if (text.endsWith('\r'))
        text.chop(1);
    return text;
}

QString CustomCodeEditor::displayTextForLine(qint64 lineNum)
{
    if (QString* cached = m_lineCache->get(lineNum))
        return *cached;

    if (!m_buffer)
        return {};

    const qint64 start = bytePosFromLine(lineNum);
    const qint64 length = m_lineIndex->lineLength(lineNum);
    const QString text = decodeBytesForDisplay(start, m_buffer->read(start, length));
    m_lineCache->put(lineNum, text);
    return text;
}

const CustomCodeEditor::CachedLineLayout& CustomCodeEditor::cachedLineLayout(qint64 lineNum) const
{
    CachedLineLayout& cached = m_displayLayoutCache[lineNum];
    const QString rawText = const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum);
    const int wrapWidth = availableTextWidth();

    if (cached.rawText == rawText && cached.wrapWidth == wrapWidth && cached.wordWrapEnabled == m_wordWrapEnabled)
        return cached;

    cached.rawText = rawText;
    const DisplayTextLayout displayLayout = buildDisplayTextLayout(rawText, m_tabDisplaySize);
    cached.displayText = displayLayout.text;
    cached.rawToVisual = displayLayout.rawToVisual;
    cached.visualToRaw = displayLayout.visualToRaw;
    cached.wrapWidth = wrapWidth;
    cached.wordWrapEnabled = m_wordWrapEnabled;
    cached.segments.clear();

    const auto segments = buildWrappedSegments(cached.displayText, m_font, wrapWidth, m_wordWrapEnabled);
    cached.segments.reserve(segments.size());
    for (const auto& segment : segments)
        cached.segments.append({segment.startColumn, segment.length, segment.x, segment.width});

    return cached;
}

void CustomCodeEditor::invalidateDisplayLayoutCache(qint64 startLine, qint64 endLine)
{
    if (startLine < 0 || endLine < 0 || startLine > endLine) {
        m_displayLayoutCache.clear();
        return;
    }

    for (qint64 lineNum = startLine; lineNum <= endLine; ++lineNum)
        m_displayLayoutCache.remove(lineNum);
}

QString CustomCodeEditor::displayPrefixForPosition(qint64 lineNum, qint64 bytePos) const
{
    if (!m_buffer)
        return {};

    const qint64 start = lineVisibleStart(lineNum);
    const qint64 end = qBound(start, bytePos, lineVisibleEnd(lineNum));
    if (end <= start)
        return {};
    return decodeBytesForDisplay(start == firstTextByte() && lineNum == 0 ? 0 : start,
                                 m_buffer->read(start == firstTextByte() && lineNum == 0 ? 0 : start,
                                                end - (start == firstTextByte() && lineNum == 0 ? 0 : start)));
}

qint64 CustomCodeEditor::bytePosForColumn(qint64 lineNum, qint64 column) const
{
    const auto& layout = cachedLineLayout(lineNum);
    const int visualColumn = static_cast<int>(qMax<qint64>(0, column));
    const int clampedVisual = qBound(0, visualColumn, layout.displayText.length());
    const int rawColumn = layout.visualToRaw.value(clampedVisual, layout.rawText.length());
    const qint64 lineStart = lineVisibleStart(lineNum);
    const qint64 lineEnd = lineVisibleEnd(lineNum);
    const QByteArray lineBytes = m_buffer->read(lineStart, lineEnd - lineStart);
    return lineStart + m_utf8Decoder->charPosToByte(lineBytes, rawColumn);
}

qint64 CustomCodeEditor::columnForBytePos(qint64 lineNum, qint64 bytePos) const
{
    const auto& layout = cachedLineLayout(lineNum);
    const qint64 lineStart = lineVisibleStart(lineNum);
    const qint64 clampedPos = qBound(lineStart, bytePos, lineVisibleEnd(lineNum));
    const qint64 relativeBytePos = clampedPos - lineStart;
    const QByteArray lineBytes = m_buffer->read(lineStart, lineVisibleEnd(lineNum) - lineStart);
    const int rawColumn = static_cast<int>(m_utf8Decoder->byteToCharPos(lineBytes, relativeBytePos));
    return layout.rawToVisual.value(qBound(0, rawColumn, layout.rawText.length()), layout.displayText.length());
}

int CustomCodeEditor::visualColumnForRawColumn(const QString& text, int rawColumn) const
{
    const DisplayTextLayout layout = buildDisplayTextLayout(text, m_tabDisplaySize);
    return layout.rawToVisual.value(qBound(0, rawColumn, text.length()), layout.text.length());
}

int CustomCodeEditor::rawColumnForVisualColumn(const QString& text, int visualColumn) const
{
    const DisplayTextLayout layout = buildDisplayTextLayout(text, m_tabDisplaySize);
    const int clampedVisual = qBound(0, visualColumn, layout.text.length());
    return layout.visualToRaw.value(clampedVisual, text.length());
}

QString CustomCodeEditor::expandedDisplayText(const QString& text) const
{
    return buildDisplayTextLayout(text, m_tabDisplaySize).text;
}

QPoint CustomCodeEditor::contentPointForBytePos(qint64 bytePos) const
{
    if (!m_buffer || m_lineIndex->lineCount() == 0)
        return QPoint(lineNumberAreaWidth() + kTextLeftPadding, 0);

    const qint64 lineNum = lineFromBytePos(bytePos);
    const auto& layout = cachedLineLayout(lineNum);
    const QString& displayText = layout.displayText;
    const auto& segments = layout.segments;
    const qint64 column = columnForBytePos(lineNum, bytePos);
    int segmentIndex = 0;
    for (int i = 0; i < segments.size(); ++i) {
        if (column >= segments[i].startColumn && column <= segments[i].startColumn + segments[i].length) {
            segmentIndex = i;
            break;
        }
    }

    const int lineHeight = qRound(m_fontMetrics.height());
    const int xPrefixColumn = static_cast<int>(column - segments[segmentIndex].startColumn);
    const int x = lineNumberAreaWidth() + kTextLeftPadding + qRound(displayAdvanceForRange(lineNum, segments[segmentIndex].startColumn, xPrefixColumn));
    const int y = static_cast<int>(visualLineIndexForLogicalLine(lineNum) + segmentIndex) * lineHeight;
    return QPoint(x, y);
}

void CustomCodeEditor::centerViewOnBytePos(qint64 bytePos)
{
    const QPoint point = contentPointForBytePos(bytePos);
    verticalScrollBar()->setValue(qBound(verticalScrollBar()->minimum(), point.y() - viewport()->height() / 2, verticalScrollBar()->maximum()));
    if (!m_wordWrapEnabled) {
        horizontalScrollBar()->setValue(qBound(horizontalScrollBar()->minimum(), point.x() - viewport()->width() / 2, horizontalScrollBar()->maximum()));
    }
}

void CustomCodeEditor::clampCursorToBuffer()
{
    if (!m_buffer) {
        m_cursorBytePos = 0;
        return;
    }

    const qint64 minPos = firstTextByte();
    m_cursorBytePos = qBound(minPos, clampToUtf8Boundary(m_cursorBytePos), m_buffer->size());
}

void CustomCodeEditor::moveCursorLeft()
{
    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineStart = lineVisibleStart(lineNum);
    if (m_cursorBytePos <= lineStart) {
        if (lineNum <= 0)
            return;
        m_cursorBytePos = lineVisibleEnd(lineNum - 1);
    } else {
        const qint64 column = columnForBytePos(lineNum, m_cursorBytePos);
        m_cursorBytePos = bytePosForColumn(lineNum, qMax<qint64>(0, column - 1));
    }

    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorRight()
{
    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineEnd = lineVisibleEnd(lineNum);
    if (m_cursorBytePos >= lineEnd) {
        if (lineNum >= m_lineIndex->lineCount() - 1)
            return;
        m_cursorBytePos = lineVisibleStart(lineNum + 1);
    } else {
        const qint64 column = columnForBytePos(lineNum, m_cursorBytePos);
        m_cursorBytePos = bytePosForColumn(lineNum, column + 1);
    }

    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorUp()
{
    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const QString text = expandedDisplayText(displayTextForLine(lineNum));
    const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
    const qint64 column = columnForBytePos(lineNum, m_cursorBytePos);
    int segmentIndex = 0;
    for (int i = 0; i < segments.size(); ++i) {
        if (column >= segments[i].startColumn && column <= segments[i].startColumn + segments[i].length) {
            segmentIndex = i;
            break;
        }
    }

    if (m_wordWrapEnabled && segmentIndex > 0) {
        m_cursorBytePos = bytePosForColumn(lineNum, qMin<qint64>(column, segments[segmentIndex - 1].startColumn + segments[segmentIndex - 1].length));
    } else if (lineNum > 0) {
        const qint64 previousLine = lineNum - 1;
        const auto previousSegments = buildWrappedSegments(expandedDisplayText(displayTextForLine(previousLine)), m_font, availableTextWidth(), m_wordWrapEnabled);
        const int previousSegment = m_wordWrapEnabled ? previousSegments.size() - 1 : 0;
        m_cursorBytePos = bytePosForColumn(previousLine, qMin<qint64>(column, previousSegments[previousSegment].startColumn + previousSegments[previousSegment].length));
    } else {
        return;
    }

    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorDown()
{
    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const QString text = expandedDisplayText(displayTextForLine(lineNum));
    const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
    const qint64 column = columnForBytePos(lineNum, m_cursorBytePos);
    int segmentIndex = 0;
    for (int i = 0; i < segments.size(); ++i) {
        if (column >= segments[i].startColumn && column <= segments[i].startColumn + segments[i].length) {
            segmentIndex = i;
            break;
        }
    }

    if (m_wordWrapEnabled && segmentIndex + 1 < segments.size()) {
        m_cursorBytePos = bytePosForColumn(lineNum, qMin<qint64>(column, segments[segmentIndex + 1].startColumn + segments[segmentIndex + 1].length));
    } else if (lineNum < m_lineIndex->lineCount() - 1) {
        const qint64 nextLine = lineNum + 1;
        const auto nextSegments = buildWrappedSegments(expandedDisplayText(displayTextForLine(nextLine)), m_font, availableTextWidth(), m_wordWrapEnabled);
        m_cursorBytePos = bytePosForColumn(nextLine, qMin<qint64>(column, nextSegments[0].startColumn + nextSegments[0].length));
    } else {
        return;
    }

    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorHome()
{
    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 lineStart = lineVisibleStart(lineNum);
    const QString text = displayTextForLine(lineNum);

    int firstNonSpaceColumn = 0;
    while (firstNonSpaceColumn < text.length() && text.at(firstNonSpaceColumn).isSpace())
        ++firstNonSpaceColumn;

    const qint64 firstNonSpaceBytePos = lineStart + m_utf8Decoder->charPosToByte(text, firstNonSpaceColumn);
    m_cursorBytePos = (m_cursorBytePos == firstNonSpaceBytePos) ? lineStart : firstNonSpaceBytePos;
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorEnd()
{
    m_cursorBytePos = lineVisibleEnd(lineFromBytePos(m_cursorBytePos));
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorPageUp()
{
    const int lineHeight = qRound(m_fontMetrics.height());
    const int lines = qMax(1, viewport()->height() / lineHeight);
    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 targetVisualLine = qMax<qint64>(0, visualLineIndexForLogicalLine(lineNum) - lines);
    const qint64 targetLine = logicalLineFromVisualLine(targetVisualLine);
    const qint64 column = columnForBytePos(lineNum, m_cursorBytePos);
    m_cursorBytePos = bytePosForColumn(targetLine, column);
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorPageDown()
{
    const int lineHeight = qRound(m_fontMetrics.height());
    const int lines = qMax(1, viewport()->height() / lineHeight);
    const qint64 lineNum = lineFromBytePos(m_cursorBytePos);
    const qint64 targetVisualLine = qMin(visualLineIndexForLogicalLine(lineNum) + lines, visualLineCount() - 1);
    const qint64 targetLine = logicalLineFromVisualLine(targetVisualLine);
    const qint64 column = columnForBytePos(lineNum, m_cursorBytePos);
    m_cursorBytePos = bytePosForColumn(targetLine, column);
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorWordLeft()
{
    if (!m_buffer || m_cursorBytePos <= firstTextByte())
        return;

    m_cursorBytePos = previousWordBoundary(m_cursorBytePos);
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorWordRight()
{
    if (!m_buffer)
        return;

    m_cursorBytePos = nextWordBoundary(m_cursorBytePos);
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorDocumentStart()
{
    m_cursorBytePos = firstTextByte();
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

void CustomCodeEditor::moveCursorDocumentEnd()
{
    m_cursorBytePos = m_buffer ? m_buffer->size() : 0;
    clampCursorToBuffer();
    ensureCursorVisible();
    emit cursorPositionChanged();
    viewport()->update();
}

qint64 CustomCodeEditor::previousWordBoundary(qint64 fromBytePos) const
{
    if (!m_buffer)
        return 0;

    const qint64 textStart = firstTextByte();
    const QByteArray allBytes = m_buffer->read(textStart, m_buffer->size() - textStart);
    const QString allText = m_utf8Decoder->decode(allBytes);
    int charIndex = static_cast<int>(m_utf8Decoder->byteToCharPos(allBytes, fromBytePos - textStart));
    charIndex = qBound(0, charIndex, allText.length());

    while (charIndex > 0 && allText.at(charIndex - 1).isSpace())
        --charIndex;
    if (charIndex > 0) {
        const bool wordChar = isWordCharacter(allText.at(charIndex - 1));
        while (charIndex > 0 && !allText.at(charIndex - 1).isSpace()
               && isWordCharacter(allText.at(charIndex - 1)) == wordChar)
            --charIndex;
    }

    return textStart + m_utf8Decoder->charPosToByte(allText, charIndex);
}

qint64 CustomCodeEditor::nextWordBoundary(qint64 fromBytePos) const
{
    if (!m_buffer)
        return 0;

    const qint64 textStart = firstTextByte();
    const QByteArray allBytes = m_buffer->read(textStart, m_buffer->size() - textStart);
    const QString allText = m_utf8Decoder->decode(allBytes);
    int charIndex = static_cast<int>(m_utf8Decoder->byteToCharPos(allBytes, fromBytePos - textStart));
    charIndex = qBound(0, charIndex, allText.length());

    while (charIndex < allText.length() && allText.at(charIndex).isSpace())
        ++charIndex;
    if (charIndex < allText.length()) {
        const bool wordChar = isWordCharacter(allText.at(charIndex));
        while (charIndex < allText.length() && !allText.at(charIndex).isSpace()
               && isWordCharacter(allText.at(charIndex)) == wordChar)
            ++charIndex;
    }

    return textStart + m_utf8Decoder->charPosToByte(allText, charIndex);
}

void CustomCodeEditor::beginEditGrouping(int groupType)
{
    if (!m_buffer)
        return;

    const EditGroupType nextType = static_cast<EditGroupType>(groupType);
    if (m_currentEditGroupType != nextType) {
        endEditGrouping();
        m_buffer->beginHistoryGroup();
        m_currentEditGroupType = nextType;
    }

    m_editGroupTimer->start(800);
}

void CustomCodeEditor::endEditGrouping()
{
    if (!m_buffer || m_currentEditGroupType == EditGroupNone)
        return;

    m_editGroupTimer->stop();
    m_buffer->endHistoryGroup();
    m_currentEditGroupType = EditGroupNone;
}

void CustomCodeEditor::renderCursor(QPainter* painter)
{
    const qint64 cursorLine = lineFromBytePos(m_cursorBytePos);
    const auto& layout = cachedLineLayout(cursorLine);
    const QString& text = layout.displayText;
    const auto& segments = layout.segments;
    const qint64 column = columnForBytePos(cursorLine, m_cursorBytePos);
    int segmentIndex = 0;
    for (int i = 0; i < segments.size(); ++i) {
        if (column >= segments[i].startColumn && column <= segments[i].startColumn + segments[i].length) {
            segmentIndex = i;
            break;
        }
    }

    const int lineHeight = qRound(m_fontMetrics.height());
    const int scrollY = verticalScrollBar()->value();
    const int scrollX = horizontalScrollBar()->value();
    const int x = lineNumberAreaWidth() + kTextLeftPadding + qRound(displayAdvanceForRange(cursorLine, segments[segmentIndex].startColumn, column - segments[segmentIndex].startColumn)) - (m_wordWrapEnabled ? 0 : scrollX);
    const int y = static_cast<int>(visualLineIndexForLogicalLine(cursorLine) + segmentIndex) * lineHeight - scrollY;
    painter->setPen(QPen(palette().text().color(), 2));
    painter->drawLine(x, y, x, y + lineHeight);
}

void CustomCodeEditor::renderSelection(QPainter* painter)
{
    if (!m_buffer || !hasSelection())
        return;

    const qint64 selStart = m_selectionStart;
    const qint64 selEnd = m_selectionStart + m_selectionLength;
    const qint64 startLine = lineFromBytePos(selStart);
    const qint64 endLine = lineFromBytePos(qMax(selStart, selEnd - 1));
    const int lineHeight = qRound(m_fontMetrics.height());
    const int scrollY = verticalScrollBar()->value();
    const int scrollX = horizontalScrollBar()->value();

    QColor selectionColor = palette().highlight().color();
    selectionColor.setAlpha(110);

    for (qint64 lineNum = startLine; lineNum <= endLine; ++lineNum) {
        const auto& layout = cachedLineLayout(lineNum);
        const QString& text = layout.displayText;
        const auto& segments = layout.segments;
        const qint64 lineStart = lineVisibleStart(lineNum);
        const qint64 lineEnd = lineVisibleEnd(lineNum);
        const qint64 rangeStart = qMax(selStart, lineStart);
        const qint64 rangeEnd = qMin(selEnd, lineEnd);
        if (rangeStart >= rangeEnd)
            continue;

        const qint64 baseVisualLine = visualLineIndexForLogicalLine(lineNum);
        for (int segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
            const qint64 segmentStart = lineSegmentStartByte(lineNum, segmentIndex);
            const qint64 segmentEnd = lineSegmentEndByte(lineNum, segmentIndex);
            const qint64 segmentRangeStart = qMax(rangeStart, segmentStart);
            const qint64 segmentRangeEnd = qMin(rangeEnd, segmentEnd);
            if (segmentRangeStart >= segmentRangeEnd)
                continue;

            const int startColumn = columnForBytePos(lineNum, segmentRangeStart) - segments[segmentIndex].startColumn;
            const int endColumn = columnForBytePos(lineNum, segmentRangeEnd) - segments[segmentIndex].startColumn;
            const int xStart = lineNumberAreaWidth() + kTextLeftPadding + qRound(displayAdvanceForRange(lineNum, segments[segmentIndex].startColumn, startColumn)) - (m_wordWrapEnabled ? 0 : scrollX);
            const int xEnd = lineNumberAreaWidth() + kTextLeftPadding + qRound(displayAdvanceForRange(lineNum, segments[segmentIndex].startColumn, endColumn)) - (m_wordWrapEnabled ? 0 : scrollX);
            const int y = static_cast<int>(baseVisualLine + segmentIndex) * lineHeight - scrollY;
            painter->fillRect(QRect(xStart, y, qMax(1, xEnd - xStart), lineHeight), selectionColor);
        }
    }
}

void CustomCodeEditor::renderSelectionMatches(QPainter* painter)
{
    if (!m_buffer || !hasSelection())
        return;

    const QString needle = highlightedSelectionText();
    if (needle.isEmpty())
        return;

    QColor matchColor = palette().highlight().color().lighter(145);
    matchColor.setAlpha(120);

    const int lineHeight = qRound(m_fontMetrics.height());
    const int scrollY = verticalScrollBar()->value();
    const int scrollX = horizontalScrollBar()->value();

    for (qint64 lineNum = m_firstVisibleLine; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const auto& layout = cachedLineLayout(lineNum);
        const QString& text = layout.displayText;
        const auto& segments = layout.segments;
        const qint64 baseVisualLine = visualLineIndexForLogicalLine(lineNum);
        if (baseVisualLine * lineHeight - scrollY > viewport()->height())
            break;

        int index = text.indexOf(needle);
        while (index >= 0) {
            const qint64 matchStart = bytePosForColumn(lineNum, index);
            const qint64 matchEnd = bytePosForColumn(lineNum, index + needle.length());
            if (!(matchStart == m_selectionStart && matchEnd - matchStart == m_selectionLength)) {
                for (int segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
                    const int segmentStartColumn = segments[segmentIndex].startColumn;
                    const int segmentEndColumn = segmentStartColumn + segments[segmentIndex].length;
                    const int rangeStartColumn = qMax(index, segmentStartColumn);
                    const int rangeEndColumn = qMin(index + needle.length(), segmentEndColumn);
                    if (rangeStartColumn >= rangeEndColumn)
                        continue;

                    const int startColumn = rangeStartColumn - segmentStartColumn;
                    const int endColumn = rangeEndColumn - segmentStartColumn;
                    const int xStart = lineNumberAreaWidth() + kTextLeftPadding + qRound(displayAdvanceForRange(lineNum, segmentStartColumn, startColumn)) - (m_wordWrapEnabled ? 0 : scrollX);
                    const int xEnd = lineNumberAreaWidth() + kTextLeftPadding + qRound(displayAdvanceForRange(lineNum, segmentStartColumn, endColumn)) - (m_wordWrapEnabled ? 0 : scrollX);
                    const int y = static_cast<int>(baseVisualLine + segmentIndex) * lineHeight - scrollY;
                    painter->fillRect(QRect(xStart, y, qMax(1, xEnd - xStart), lineHeight), matchColor);
                }
            }
            index = text.indexOf(needle, index + needle.length());
        }
    }
}
