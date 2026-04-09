#ifndef CUSTOMCODEEDITOR_H
#define CUSTOMCODEEDITOR_H

#include <QAbstractScrollArea>
#include <QFont>
#include <QFontMetricsF>
#include <QElapsedTimer>
#include <QHash>
#include <QPoint>
#include <QTextLayout>
#include <QVector>
#include <QCache>
#include "toolwidget.hpp"

class FileDataBuffer;
class LineIndex;
class LineCache;
class UTF8Decoder;
class QStyleSyntaxHighlighter;
class QSyntaxStyle;
class QTextDocument;
class LineNumberArea;
class QTimer;

/**
 * @brief Custom code editor with direct buffer access
 * 
 * A code editor that reads directly from FileDataBuffer without
 * data duplication. Renders only visible lines for efficiency.
 */
class CustomCodeEditor : public QAbstractScrollArea, public ToolWidget {
    Q_OBJECT

public:
    explicit CustomCodeEditor(QWidget* parent = nullptr);
    ~CustomCodeEditor() override;
    static QString syntaxKeyForPath(const QString& filePath);
    
    // ToolWidget interface
    void setBData(const QByteArray& data) override;
    QByteArray getBData() override;
    
    // Buffer management
    void setBuffer(FileDataBuffer* buffer);
    FileDataBuffer* getBuffer() const;
    
    // Configuration
    void setFileExt(const QString& ext);
    void setSyntaxHighlighter(QStyleSyntaxHighlighter* highlighter);
    void setTabReplaceSize(int spaces);
    void setTabReplace(bool enabled);
    bool tabReplaceEnabled() const;
    void setTabDisplaySize(int spaces);
    int tabDisplaySize() const;
    void setWordWrapEnabled(bool enabled);
    bool wordWrapEnabled() const;
    
    // State queries
    bool isModified() const;
    qint64 cursorPosition() const;
    qint64 cursorLineNumber() const;
    qint64 cursorColumnNumber() const;
    qint64 lineCount() const;
    bool hasSelection() const;
    QString selectedText() const;
    QString syntaxKey() const;
    bool findText(const QString& text, bool forward = true, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive);
    bool goToLine(qint64 oneBasedLineNumber);
    int countMatches(const QString& text, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive) const;
    int currentMatchIndex(const QString& text, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive) const;
    bool replaceCurrentSelection(const QString& text, const QString& replacement, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive);
    int replaceAllMatches(const QString& text, const QString& replacement, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive);
    
    // Zoom support
    void setScaleFactor(double factor);
    double scaleFactor() const;
    
    // Line number area support
    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);

signals:
    void contentsChanged();
    void modificationChanged(bool modified);
    void cursorPositionChanged();

protected:
    bool event(QEvent* event) override;
    // QAbstractScrollArea overrides
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private slots:
    void onBufferByteChanged(qint64 pos);
    void onBufferBytesChanged(qint64 pos, qint64 length);
    void onBufferDataChanged();
    void onBufferSelectionChanged(qint64 pos, qint64 length);
    void updateScrollbars();

private:
    struct CachedLineLayout {
        struct Segment {
            int startColumn = 0;
            int length = 0;
            qreal x = 0;
            qreal width = 0;
        };

        QString rawText;
        QString displayText;
        QVector<int> rawToVisual;
        QVector<int> visualToRaw;
        QVector<Segment> segments;
        int wrapWidth = -1;
        bool wordWrapEnabled = true;
    };

    FileDataBuffer* m_buffer;
    LineIndex* m_lineIndex;
    LineCache* m_lineCache;
    UTF8Decoder* m_utf8Decoder;
    QStyleSyntaxHighlighter* m_highlighter;
    LineNumberArea* m_lineNumberArea;
    
    // Cursor and selection
    qint64 m_cursorBytePos;
    qint64 m_selectionStart;
    qint64 m_selectionLength;
    bool m_updatingSelection;
    bool m_applyingBufferEdit;
    
    // Rendering state
    qint64 m_firstVisibleLine;
    qint64 m_visibleLineCount;
    double m_scaleFactor;
    QFont m_font;
    QFontMetricsF m_fontMetrics;
    
    // Configuration
    bool m_tabReplace;
    int m_tabReplaceSize;
    int m_tabDisplaySize;
    QString m_fileExt;
    bool m_hasUtf8Bom;
    
    // Helper methods
    void buildLineIndex();
    void invalidateLineCache(qint64 startLine, qint64 endLine);
    void renderVisibleLines(QPainter* painter);
    void renderLineNumber(QPainter* painter, qint64 lineNum, const QRectF& rect);
    void renderLine(QPainter* painter, qint64 lineNum, const QString& text, const QRectF& rect, int segmentStartColumn, int segmentLength);
    void renderCursor(QPainter* painter);
    void renderSelection(QPainter* painter);
    void renderSelectionMatches(QPainter* painter);
    qint64 lineFromBytePos(qint64 bytePos) const;
    qint64 bytePosFromLine(qint64 lineNum) const;
    qint64 bytePosFromPoint(const QPoint& point) const;
    void ensureCursorVisible();
    void updateSelection(qint64 byteStart, qint64 byteLength);
    void updateLineNumberAreaWidth();
    void updateSelectionAfterMove(qint64 oldCursorPos);
    void clearSelection();
    void copySelection();
    void cutSelection();
    void pasteFromClipboard();
    void selectAll();
    void duplicateSelectionOrLine(bool duplicateAbove = false);
    void moveSelectedLines(int direction);
    void undo();
    void redo();
    void deleteBackward();
    void deleteForward();
    void deleteWordBackward();
    void deleteWordForward();
    void toggleLineComment();
    void insertNewline();
    void insertLineBelow();
    void insertLineAbove();
    void deleteCurrentLine();
    void insertTab();
    void outdentSelection();
    void insertText(const QString& text);
    void replaceRange(qint64 start, qint64 length, const QByteArray& replacement);
    void syncSelectionToBuffer();
    void updateModificationState();
    qint64 firstTextByte() const;
    qint64 lineVisibleStart(qint64 lineNum) const;
    qint64 lineVisibleEnd(qint64 lineNum) const;
    QString decodeBytesForDisplay(qint64 startByte, const QByteArray& bytes) const;
    QString displayTextForLine(qint64 lineNum);
    const CachedLineLayout& cachedLineLayout(qint64 lineNum) const;
    void invalidateDisplayLayoutCache(qint64 startLine = -1, qint64 endLine = -1);
    QString displayPrefixForPosition(qint64 lineNum, qint64 bytePos) const;
    qint64 bytePosForColumn(qint64 lineNum, qint64 column) const;
    qint64 columnForBytePos(qint64 lineNum, qint64 bytePos) const;
    int visualColumnForRawColumn(const QString& text, int rawColumn) const;
    int rawColumnForVisualColumn(const QString& text, int visualColumn) const;
    QString expandedDisplayText(const QString& text) const;
    QPoint contentPointForBytePos(qint64 bytePos) const;
    void centerViewOnBytePos(qint64 bytePos);
    void clampCursorToBuffer();
    void initSyntaxSupport();
    void rebuildHighlighterForCurrentExtension();
    QString normalizedFileExt(const QString& ext) const;
    QString lineCommentPrefix() const;
    QString highlightedSelectionText() const;
    QVector<QTextLayout::FormatRange> highlightFormatsForVisibleLine(qint64 lineNum, const QString& text) const;
    qreal displayAdvanceForRange(qint64 lineNum, int startColumn, int length) const;
    void applyEditorPalette();
    void ensureLineIndexValid();
    qint64 clampToUtf8Boundary(qint64 bytePos) const;
    void saveViewState();
    void restoreViewState();
    int availableTextWidth() const;
    qint64 visualLineCount() const;
    qint64 visualLineIndexForLogicalLine(qint64 lineNum) const;
    qint64 logicalLineFromVisualLine(qint64 visualLine) const;
    int wrappedLineCount(qint64 lineNum) const;
    void ensureLayoutCache() const;
    void ensureLayoutCacheForLine(qint64 lineNum) const;
    void ensureLayoutCacheForVisualLine(qint64 visualLine) const;
    void appendLayoutCacheUntilLine(qint64 inclusiveLine) const;
    void appendLayoutCacheUntilVisual(qint64 inclusiveVisualLine) const;
    void trimVisibleCaches(qint64 firstVisibleLine, qint64 lastVisibleLine) const;
    qint64 lineSegmentStartByte(qint64 lineNum, int segmentIndex) const;
    qint64 lineSegmentEndByte(qint64 lineNum, int segmentIndex) const;
    void invalidateWrapCache(qint64 startLine = -1, qint64 endLine = -1);
     
    // Cursor movement methods
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorUp();
    void moveCursorDown();
    void moveCursorHome();
    void moveCursorEnd();
    void moveCursorPageUp();
    void moveCursorPageDown();
    void moveCursorWordLeft();
    void moveCursorWordRight();
    void moveCursorDocumentStart();
    void moveCursorDocumentEnd();
    qint64 previousWordBoundary(qint64 fromBytePos) const;
    qint64 nextWordBoundary(qint64 fromBytePos) const;
    void beginEditGrouping(int groupType);
    void endEditGrouping();

    enum EditGroupType {
        EditGroupNone = 0,
        EditGroupTyping,
        EditGroupBackspace,
        EditGroupDelete
    };

    qint64 m_selectionAnchor;
    bool m_mouseSelecting;
    int m_clickCount;
    qint64 m_lastClickTimestamp;
    QPoint m_lastClickPosition;
    bool m_pendingTripleClick;
    QTextDocument* m_highlightDocument;
    QSyntaxStyle* m_syntaxStyle;
    QHash<QString, QString> m_languageResourceByExt;
    QString m_languageResource;
    int m_savedVerticalScrollValue;
    int m_savedHorizontalScrollValue;
    qint64 m_savedCursorBytePos;
    bool m_restoreViewStatePending;
    bool m_wordWrapEnabled;
    mutable QHash<qint64, int> m_wrapCountCache;
    mutable int m_wrapCacheWidth;
    mutable bool m_layoutCacheValid;
    mutable qint64 m_layoutCacheComputedLines;
    mutable qint64 m_layoutCacheComputedVisualLines;
    mutable QVector<qint64> m_visualLineOffsets;
    mutable qint64 m_totalVisualLineCount;
    mutable qint64 m_maxExpandedLineLength;
    mutable qint64 m_highlightCacheStartLine;
    mutable qint64 m_highlightCacheEndLine;
    mutable QHash<qint64, QVector<QTextLayout::FormatRange>> m_highlightFormatCache;
    mutable QHash<qint64, CachedLineLayout> m_displayLayoutCache;
    QTimer* m_editGroupTimer;
    EditGroupType m_currentEditGroupType;
};

#endif // CUSTOMCODEEDITOR_H
