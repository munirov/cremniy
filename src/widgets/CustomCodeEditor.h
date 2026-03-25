#ifndef CUSTOMCODEEDITOR_H
#define CUSTOMCODEEDITOR_H

#include <QAbstractScrollArea>
#include <QFont>
#include <QFontMetricsF>
#include <QElapsedTimer>
#include <QHash>
#include <QTextLayout>
#include "toolwidget.hpp"

class FileDataBuffer;
class LineIndex;
class LineCache;
class UTF8Decoder;
class QStyleSyntaxHighlighter;
class QSyntaxStyle;
class QTextDocument;
class LineNumberArea;

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
    void setWordWrapEnabled(bool enabled);
    bool wordWrapEnabled() const;
    
    // State queries
    bool isModified() const;
    qint64 cursorPosition() const;
    qint64 lineCount() const;
    bool hasSelection() const;
    QString selectedText() const;
    QString syntaxKey() const;
    bool findText(const QString& text, bool forward = true, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive);
    bool goToLine(qint64 oneBasedLineNumber);
    int countMatches(const QString& text, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive) const;
    int currentMatchIndex(const QString& text, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive) const;
    
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
    
    // Rendering state
    qint64 m_firstVisibleLine;
    qint64 m_visibleLineCount;
    double m_scaleFactor;
    QFont m_font;
    QFontMetricsF m_fontMetrics;
    
    // Configuration
    bool m_tabReplace;
    int m_tabReplaceSize;
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
    void undo();
    void redo();
    void deleteBackward();
    void deleteForward();
    void insertNewline();
    void insertTab();
    void insertText(const QString& text);
    void replaceRange(qint64 start, qint64 length, const QByteArray& replacement);
    void syncSelectionToBuffer();
    void updateModificationState();
    qint64 firstTextByte() const;
    qint64 lineVisibleStart(qint64 lineNum) const;
    qint64 lineVisibleEnd(qint64 lineNum) const;
    QString decodeBytesForDisplay(qint64 startByte, const QByteArray& bytes) const;
    QString displayTextForLine(qint64 lineNum);
    QString displayPrefixForPosition(qint64 lineNum, qint64 bytePos) const;
    qint64 bytePosForColumn(qint64 lineNum, qint64 column) const;
    qint64 columnForBytePos(qint64 lineNum, qint64 bytePos) const;
    void clampCursorToBuffer();
    void initSyntaxSupport();
    void rebuildHighlighterForCurrentExtension();
    QString normalizedFileExt(const QString& ext) const;
    QVector<QTextLayout::FormatRange> highlightFormatsForVisibleLine(qint64 lineNum, const QString& text) const;
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

    qint64 m_selectionAnchor;
    bool m_mouseSelecting;
    int m_clickCount;
    qint64 m_lastClickTimestamp;
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
};

#endif // CUSTOMCODEEDITOR_H
