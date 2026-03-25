#include "CustomCodeEditor.h"

#include "LineNumberArea.h"
#include "ToolTabs/CodeEditor/QCodeEditor/include/QCXXHighlighter.hpp"
#include "ToolTabs/CodeEditor/QCodeEditor/include/QJSONHighlighter.hpp"
#include "ToolTabs/CodeEditor/QCodeEditor/include/QLanguage.hpp"
#include "ToolTabs/CodeEditor/QCodeEditor/include/QSyntaxStyle.hpp"
#include "ToolTabs/CodeEditor/QCodeEditor/include/QStyleSyntaxHighlighter.hpp"
#include "core/FileDataBuffer.h"
#include "core/LineCache.h"
#include "core/LineIndex.h"
#include "core/UTF8Decoder.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
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
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

namespace {
const QByteArray kUtf8Bom("\xEF\xBB\xBF", 3);

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
    , m_firstVisibleLine(0)
    , m_visibleLineCount(0)
    , m_scaleFactor(1.0)
    , m_font("Courier New", 10)
    , m_fontMetrics(m_font)
    , m_tabReplace(true)
    , m_tabReplaceSize(4)
    , m_hasUtf8Bom(false)
    , m_selectionAnchor(-1)
    , m_mouseSelecting(false)
    , m_clickCount(0)
    , m_lastClickTimestamp(0)
    , m_highlightDocument(new QTextDocument(this))
    , m_syntaxStyle(QSyntaxStyle::defaultStyle())
    , m_savedVerticalScrollValue(0)
    , m_savedHorizontalScrollValue(0)
    , m_savedCursorBytePos(0)
    , m_restoreViewStatePending(false)
    , m_wordWrapEnabled(true)
    , m_wrapCacheWidth(-1)
{
    initSyntaxSupport();
    setFont(m_font);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    viewport()->setAutoFillBackground(true);

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
    QString value = ext.trimmed().toLower();
    if (value == QStringLiteral("makefile"))
        return QStringLiteral("make");
    if (value == QStringLiteral("cmakelists.txt"))
        return QStringLiteral("cmake");
    if (value == QStringLiteral("yml"))
        return QStringLiteral("yaml");
    if (value == QStringLiteral("bash") || value == QStringLiteral("zsh") || value == QStringLiteral("fish"))
        return QStringLiteral("sh");
    if (value == QStringLiteral("mjs") || value == QStringLiteral("cjs") || value == QStringLiteral("jsx"))
        return QStringLiteral("js");
    if (value == QStringLiteral("tsx"))
        return QStringLiteral("ts");
    if (value == QStringLiteral("cfg") || value == QStringLiteral("conf") || value == QStringLiteral("properties") || value == QStringLiteral("env"))
        return QStringLiteral("ini");
    return value;
}

void CustomCodeEditor::rebuildHighlighterForCurrentExtension()
{
    const QString ext = normalizedFileExt(m_fileExt);
    const QString resource = m_languageResourceByExt.value(ext, QStringLiteral(":/languages/cpp.xml"));
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
        setSyntaxHighlighter(new XmlLanguageHighlighter(resource,
                                                        QRegularExpression(QStringLiteral("#[^\\n]*")),
                                                        QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")),
                                                        QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+)\\b")),
                                                        m_highlightDocument));
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
    } else if (resource == QStringLiteral(":/languages/yaml") ||
               resource == QStringLiteral(":/languages/toml") ||
               resource == QStringLiteral(":/languages/ini") ||
               resource == QStringLiteral(":/languages/sh") ||
               resource == QStringLiteral(":/languages/js") ||
               resource == QStringLiteral(":/languages/ts") ||
               resource == QStringLiteral(":/languages/java") ||
               resource == QStringLiteral(":/languages/cs") ||
               resource == QStringLiteral(":/languages/go") ||
               resource == QStringLiteral(":/languages/php") ||
               resource == QStringLiteral(":/languages/sql") ||
               resource == QStringLiteral(":/languages/xml") ||
               resource == QStringLiteral(":/languages/sln")) {
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

    // QSyntaxHighlighter computes state line-by-line, so the custom editor
    // rebuilds a small rolling window around the visible line instead of
    // materializing the whole file in a QTextDocument.
    const int contextBefore = 16;
    const qint64 startLine = qMax<qint64>(0, lineNum - contextBefore);
    const qint64 endLine = qMin(lineNum + 1, m_lineIndex->lineCount());

    QStringList lines;
    lines.reserve(static_cast<int>(endLine - startLine));
    for (qint64 current = startLine; current < endLine; ++current)
        lines.append(const_cast<CustomCodeEditor*>(this)->displayTextForLine(current));

    try {
        m_highlightDocument->setPlainText(lines.join(QLatin1Char('\n')));
        m_highlighter->rehighlight();
    } catch (...) {
        const_cast<CustomCodeEditor*>(this)->setSyntaxHighlighter(nullptr);
        return {};
    }

    QTextBlock block = m_highlightDocument->firstBlock();
    for (qint64 current = startLine; current < lineNum && block.isValid(); ++current)
        block = block.next();

    if (!block.isValid())
        return {};

    const auto ranges = block.layout()->formats();
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
    const QByteArray prefix = m_buffer->read(lineStart, clamped - lineStart);
    return lineStart + m_utf8Decoder->findCharBoundary(prefix, prefix.size());
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

void CustomCodeEditor::setWordWrapEnabled(bool enabled)
{
    if (m_wordWrapEnabled == enabled)
        return;

    m_wordWrapEnabled = enabled;
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

QString CustomCodeEditor::syntaxKey() const
{
    return normalizedFileExt(m_fileExt);
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

void CustomCodeEditor::setScaleFactor(double factor)
{
    m_scaleFactor = qBound(0.5, factor, 4.0);
    m_font.setPointSizeF(10.0 * m_scaleFactor);
    setFont(m_font);
    m_fontMetrics = QFontMetricsF(m_font);
    invalidateWrapCache();
    updateLineNumberAreaWidth();
    updateScrollbars();
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

    painter.setFont(m_font);
    qint64 visualIndex = 0;
    for (qint64 lineNum = 0; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const int lineWrapCount = wrappedLineCount(lineNum);
        if (visualIndex + lineWrapCount <= firstVisibleVisualLine) {
            visualIndex += lineWrapCount;
            continue;
        }
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

    const bool shiftPressed = event->modifiers().testFlag(Qt::ShiftModifier);
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
        handleMove([this]() { moveCursorLeft(); });
        return;
    case Qt::Key_Right:
        handleMove([this]() { moveCursorRight(); });
        return;
    case Qt::Key_Up:
        handleMove([this]() { moveCursorUp(); });
        return;
    case Qt::Key_Down:
        handleMove([this]() { moveCursorDown(); });
        return;
    case Qt::Key_Home:
        handleMove([this]() { moveCursorHome(); });
        return;
    case Qt::Key_End:
        handleMove([this]() { moveCursorEnd(); });
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
        copySelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Cut)) {
        cutSelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        pasteFromClipboard();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        undo();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Redo)) {
        redo();
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Backspace:
        deleteBackward();
        event->accept();
        return;
    case Qt::Key_Delete:
        deleteForward();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        insertNewline();
        event->accept();
        return;
    case Qt::Key_Tab:
        insertTab();
        event->accept();
        return;
    default:
        break;
    }

    const QString text = event->text();
    if (!text.isEmpty() && !event->modifiers().testFlag(Qt::ControlModifier) && text.front().isPrint()) {
        insertText(text);
        event->accept();
        return;
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void CustomCodeEditor::mousePressEvent(QMouseEvent* event)
{
    if (!m_buffer || event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }

    setFocus();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastClickTimestamp < QApplication::doubleClickInterval())
        ++m_clickCount;
    else
        m_clickCount = 1;
    m_lastClickTimestamp = now;

    m_mouseSelecting = true;
    m_cursorBytePos = bytePosFromPoint(event->position().toPoint());
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

    m_cursorBytePos = bytePosFromPoint(event->position().toPoint());
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
    const qint64 lineNum = lineFromBytePos(bytePos);
    const QString text = displayTextForLine(lineNum);
    const qint64 lineStart = lineVisibleStart(lineNum);
    const qint64 column = columnForBytePos(lineNum, bytePos);

    if (m_clickCount >= 3) {
        m_selectionStart = lineStart;
        m_selectionLength = lineVisibleEnd(lineNum) - lineStart;
        m_cursorBytePos = lineVisibleEnd(lineNum);
    } else {
        int start = static_cast<int>(qBound<qint64>(0, column, text.length()));
        int end = start;
        while (start > 0 && (text[start - 1].isLetterOrNumber() || text[start - 1] == '_'))
            --start;
        while (end < text.length() && (text[end].isLetterOrNumber() || text[end] == '_'))
            ++end;
        m_selectionStart = bytePosForColumn(lineNum, start);
        m_selectionLength = bytePosForColumn(lineNum, end) - m_selectionStart;
        m_cursorBytePos = bytePosForColumn(lineNum, end);
    }

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
    buildLineIndex();
    m_lineCache->clear();
    invalidateWrapCache();
    m_cursorBytePos = clampToUtf8Boundary(m_cursorBytePos);
    clampCursorToBuffer();
    if (m_selectionLength > 0) {
        const qint64 bufferSize = m_buffer ? m_buffer->size() : 0;
        m_selectionStart = qBound<qint64>(0, m_selectionStart, bufferSize);
        m_selectionLength = qBound<qint64>(0, m_selectionLength, bufferSize - m_selectionStart);
    }
    updateScrollbars();
    ensureCursorVisible();
    updateModificationState();
    viewport()->update();
    m_lineNumberArea->update();
}


void CustomCodeEditor::onBufferSelectionChanged(qint64 pos, qint64 length)
{
    if (m_updatingSelection)
        return;

    updateSelection(pos, length);
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

    const int lineHeight = qRound(m_fontMetrics.height());
    const int totalHeight = static_cast<int>(visualLineCount()) * lineHeight;
    const int viewportHeight = viewport()->height();
    verticalScrollBar()->setRange(0, qMax(0, totalHeight - viewportHeight));
    verticalScrollBar()->setPageStep(viewportHeight);
    verticalScrollBar()->setSingleStep(lineHeight);

    if (m_wordWrapEnabled) {
        horizontalScrollBar()->setRange(0, 0);
        horizontalScrollBar()->setPageStep(viewport()->width());
    } else {
        qint64 maxLineLength = 0;
        for (qint64 lineNum = 0; lineNum < m_lineIndex->lineCount(); ++lineNum)
            maxLineLength = qMax(maxLineLength, lineVisibleEnd(lineNum) - lineVisibleStart(lineNum));

        const int approxWidth = qRound(m_fontMetrics.horizontalAdvance(QLatin1Char('M')) * maxLineLength);
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
        updateLineNumberAreaWidth();
        return;
    }

    m_lineIndex->build(m_buffer);
    m_hasUtf8Bom = m_buffer->read(0, 3) == kUtf8Bom;
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
    return qMax(1, viewport()->width() - 4);
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
        count = buildWrappedSegments(const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum), m_font, currentWidth, m_wordWrapEnabled).size();
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

    qint64 total = 0;
    for (qint64 lineNum = 0; lineNum < m_lineIndex->lineCount(); ++lineNum)
        total += wrappedLineCount(lineNum);
    return qMax<qint64>(1, total);
}

qint64 CustomCodeEditor::visualLineIndexForLogicalLine(qint64 lineNum) const
{
    qint64 visualIndex = 0;
    for (qint64 current = 0; current < lineNum && current < m_lineIndex->lineCount(); ++current)
        visualIndex += wrappedLineCount(current);
    return visualIndex;
}

qint64 CustomCodeEditor::logicalLineFromVisualLine(qint64 visualLine) const
{
    if (!m_buffer || m_lineIndex->lineCount() == 0)
        return 0;

    qint64 currentVisual = 0;
    for (qint64 lineNum = 0; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const qint64 lineVisualCount = wrappedLineCount(lineNum);
        if (visualLine < currentVisual + lineVisualCount)
            return lineNum;
        currentVisual += lineVisualCount;
    }

    return m_lineIndex->lineCount() - 1;
}

qint64 CustomCodeEditor::lineSegmentStartByte(qint64 lineNum, int segmentIndex) const
{
    const QString text = const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum);
    const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
    if (segmentIndex < 0 || segmentIndex >= segments.size())
        return lineVisibleStart(lineNum);

    return bytePosForColumn(lineNum, segments[segmentIndex].startColumn);
}

qint64 CustomCodeEditor::lineSegmentEndByte(qint64 lineNum, int segmentIndex) const
{
    const QString text = const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum);
    const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
    if (segmentIndex < 0 || segmentIndex >= segments.size())
        return lineVisibleEnd(lineNum);

    const WrappedSegment& segment = segments[segmentIndex];
    return bytePosForColumn(lineNum, segment.startColumn + segment.length);
}

void CustomCodeEditor::invalidateLineCache(qint64 startLine, qint64 endLine)
{
    m_lineCache->invalidate(startLine, endLine);
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
        return;
    }

    for (qint64 lineNum = startLine; lineNum <= endLine; ++lineNum)
        m_wrapCountCache.remove(lineNum);
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

    qint64 visualIndex = 0;
    for (qint64 lineNum = 0; lineNum < m_lineIndex->lineCount(); ++lineNum) {
        const QString text = displayTextForLine(lineNum);
        const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
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
            const int x = lineNumberWidth - (m_wordWrapEnabled ? 0 : scrollX);
            const QRectF lineRect(x, y, viewport()->width() + (m_wordWrapEnabled ? 0 : scrollX), lineHeight);
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

    // Paint the line in segments so syntax formats can be applied without
    // handing ownership of the document text to QTextEdit/QPlainTextEdit.
    const auto formats = highlightFormatsForVisibleLine(lineNum, text);
    qreal x = rect.left();
    int cursor = 0;
    const QString segmentText = text.mid(segmentStartColumn, segmentLength);

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

    for (const auto& range : formats) {
        const int rangeStart = qMax(range.start, segmentStartColumn);
        const int rangeEnd = qMin(range.start + range.length, segmentStartColumn + segmentLength);
        if (rangeEnd <= rangeStart)
            continue;

        if (rangeStart > segmentStartColumn + cursor)
            drawSegment(text.mid(segmentStartColumn + cursor, rangeStart - (segmentStartColumn + cursor)), QTextCharFormat());
        drawSegment(text.mid(rangeStart, rangeEnd - rangeStart), range.format);
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
    const QString text = const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum);
    const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
    const qint64 segmentIndex = qBound<qint64>(0, visualLine - visualLineIndexForLogicalLine(lineNum), segments.size() - 1);
    const WrappedSegment& segment = segments[segmentIndex];
    const int localX = qMax(0, point.x() + (m_wordWrapEnabled ? 0 : scrollX) - lineNumberAreaWidth());

    int bestColumn = segment.startColumn + segment.length;
    for (int column = 0; column <= segment.length; ++column) {
        const int width = qRound(m_fontMetrics.horizontalAdvance(text.mid(segment.startColumn, column)));
        if (width >= localX) {
            bestColumn = segment.startColumn + column;
            break;
        }
    }

    return bytePosForColumn(lineNum, bestColumn);
}

void CustomCodeEditor::ensureCursorVisible()
{
    if (!m_buffer)
        return;

    const qint64 cursorLine = lineFromBytePos(m_cursorBytePos);
    const int lineHeight = qRound(m_fontMetrics.height());
    const QString text = displayTextForLine(cursorLine);
    const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
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
    const int cursorX = lineNumberAreaWidth() + qRound(m_fontMetrics.horizontalAdvance(text.mid(segments[segmentIndex].startColumn, xPrefixColumn)));
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
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    if (m_lineNumberArea)
        m_lineNumberArea->updateGeometry();
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
    insertText(QApplication::clipboard()->text());
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

    m_buffer->undo();
    clearSelection();
    clampCursorToBuffer();
}

void CustomCodeEditor::redo()
{
    if (!m_buffer || !m_buffer->canRedo())
        return;

    m_buffer->redo();
    clearSelection();
    clampCursorToBuffer();
}

void CustomCodeEditor::deleteBackward()
{
    if (!m_buffer)
        return;

    if (hasSelection()) {
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
        replaceRange(removeStart, removeLength, QByteArray());
        return;
    }

    const QString prefix = displayPrefixForPosition(lineNum, m_cursorBytePos);
    const qint64 prevColumn = qMax<qint64>(0, prefix.length() - 1);
    const qint64 prevPos = bytePosForColumn(lineNum, prevColumn);
    replaceRange(prevPos, m_cursorBytePos - prevPos, QByteArray());
}

void CustomCodeEditor::deleteForward()
{
    if (!m_buffer)
        return;

    if (hasSelection()) {
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
        replaceRange(rawNext, removeLength, QByteArray());
        return;
    }

    const qint64 currentColumn = columnForBytePos(lineNum, m_cursorBytePos);
    const qint64 nextPos = bytePosForColumn(lineNum, currentColumn + 1);
    replaceRange(m_cursorBytePos, nextPos - m_cursorBytePos, QByteArray());
}

void CustomCodeEditor::insertNewline()
{
    replaceRange(hasSelection() ? m_selectionStart : m_cursorBytePos,
                 hasSelection() ? m_selectionLength : 0,
                 QByteArray("\n"));
}

void CustomCodeEditor::insertTab()
{
    if (m_tabReplace)
        replaceRange(hasSelection() ? m_selectionStart : m_cursorBytePos,
                     hasSelection() ? m_selectionLength : 0,
                     QByteArray(m_tabReplaceSize, ' '));
    else
        replaceRange(hasSelection() ? m_selectionStart : m_cursorBytePos,
                     hasSelection() ? m_selectionLength : 0,
                     QByteArray("\t"));
}

void CustomCodeEditor::insertText(const QString& text)
{
    if (text.isEmpty())
        return;
    replaceRange(hasSelection() ? m_selectionStart : m_cursorBytePos,
                 hasSelection() ? m_selectionLength : 0,
                 text.toUtf8());
}

void CustomCodeEditor::replaceRange(qint64 start, qint64 length, const QByteArray& replacement)
{
    if (!m_buffer)
        return;

    QByteArray data = m_buffer->data();
    start = qBound<qint64>(firstTextByte(), start, data.size());
    length = qBound<qint64>(0, length, data.size() - start);
    data.replace(start, length, replacement);

    m_cursorBytePos = start + replacement.size();
    m_selectionStart = m_cursorBytePos;
    m_selectionLength = 0;
    m_selectionAnchor = -1;

    m_buffer->replaceData(data);
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
    const QString text = const_cast<CustomCodeEditor*>(this)->displayTextForLine(lineNum);
    const qint64 clampedColumn = qBound<qint64>(0, column, text.length());
    return lineVisibleStart(lineNum) + m_utf8Decoder->charPosToByte(text, clampedColumn);
}

qint64 CustomCodeEditor::columnForBytePos(qint64 lineNum, qint64 bytePos) const
{
    return displayPrefixForPosition(lineNum, bytePos).length();
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
    const QString text = displayTextForLine(lineNum);
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
        const auto previousSegments = buildWrappedSegments(displayTextForLine(previousLine), m_font, availableTextWidth(), m_wordWrapEnabled);
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
    const QString text = displayTextForLine(lineNum);
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
        const auto nextSegments = buildWrappedSegments(displayTextForLine(nextLine), m_font, availableTextWidth(), m_wordWrapEnabled);
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
    m_cursorBytePos = lineVisibleStart(lineFromBytePos(m_cursorBytePos));
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

void CustomCodeEditor::renderCursor(QPainter* painter)
{
    const qint64 cursorLine = lineFromBytePos(m_cursorBytePos);
    const QString text = displayTextForLine(cursorLine);
    const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
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
    const int x = lineNumberAreaWidth() + qRound(m_fontMetrics.horizontalAdvance(text.mid(segments[segmentIndex].startColumn, column - segments[segmentIndex].startColumn))) - (m_wordWrapEnabled ? 0 : scrollX);
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
        const QString text = displayTextForLine(lineNum);
        const auto segments = buildWrappedSegments(text, m_font, availableTextWidth(), m_wordWrapEnabled);
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
            const int xStart = lineNumberAreaWidth() + qRound(m_fontMetrics.horizontalAdvance(text.mid(segments[segmentIndex].startColumn, startColumn))) - (m_wordWrapEnabled ? 0 : scrollX);
            const int xEnd = lineNumberAreaWidth() + qRound(m_fontMetrics.horizontalAdvance(text.mid(segments[segmentIndex].startColumn, endColumn))) - (m_wordWrapEnabled ? 0 : scrollX);
            const int y = static_cast<int>(baseVisualLine + segmentIndex) * lineHeight - scrollY;
            painter->fillRect(QRect(xStart, y, qMax(1, xEnd - xStart), lineHeight), selectionColor);
        }
    }
}
