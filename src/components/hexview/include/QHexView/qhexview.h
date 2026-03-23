#pragma once

#define QHEXVIEW_VERSION 5.0

#include <QAbstractScrollArea>
#include <QFontMetricsF>
#include <QHexView/model/qhexcursor.h>
#include <QHexView/model/qhexdelegate.h>
#include <QHexView/model/qhexdocument.h>
#include <QList>
#include <QRectF>
#include <toolwidget.hpp>

#if defined(QHEXVIEW_ENABLE_DIALOGS)
class HexFindDialog;
#endif

struct QHexCopyFormat {
    QString prefix;
    QString suffix;
    QString byte_prefix;
    QString byte_suffix;
    QString separator;
    int indent{-1};
    bool trim_last_separator{false};
    bool line_break{false};
    bool use_tabs{false};
};

class QHexView: public QAbstractScrollArea, public ToolWidget {
    Q_OBJECT

    struct PaintContext {
        const QHexView* hexview;
        QPainter* painter;
        const QFontMetricsF* fontmetrics;
        QHexCharFormat format;
        qreal x, y;

        explicit PaintContext(const QHexView* hv, QPainter* p,
                              const QFontMetricsF* fm);
        void drawText(const QString& s, const QHexCharFormat& cf,
                      bool pad = false);
        void drawText(const QString& s, bool pad = false);
        void fillLine(QColor c) const;
        void clearFormat();
        void nextLine();
        void prevLine();
        void advanceX();
    };

public:
    explicit QHexView(QWidget* parent = nullptr);

    bool m_ignoreModification = false;

    QRectF headerRect() const;
    QRectF documentRect() const;
    QRectF addressRect() const;
    QRectF hexRect() const;
    QRectF asciiRect() const;
    QHexDocument* hexDocument() const;
    QHexCursor* hexCursor() const;
    const QHexMetadata* hexMetadata() const;
    QHexOptions options() const;
    QColor getReadableColor(QColor c) const;
    QByteArray selectedBytes() const;
    QByteArray visibleBytes() const;
    QByteArray getLine(qint64 line) const;
    void setFixedCellWidth(qreal width);
    unsigned int addressWidth() const;
    unsigned int lineLength() const;
    bool isModified() const;
    bool canUndo() const;
    bool canRedo() const;
    bool trackChanges() const;
    quint64 offset() const;
    quint64 address() const;
    QHexPosition positionFromOffset(quint64 offset) const;
    QHexPosition positionFromAddress(quint64 address) const;
    QHexPosition position() const;
    QHexPosition selectionStart() const;
    QHexPosition selectionEnd() const;
    quint64 selectionStartOffset() const;
    quint64 selectionEndOffset() const;
    quint64 baseAddress() const;
    quint64 lines() const;
    qint64 replace(const QVariant& oldvalue, const QVariant& newvalue,
                   qint64 offset, QHexFindMode mode = QHexFindMode::Text,
                   unsigned int options = QHexFindOptions::None,
                   QHexFindDirection fd = QHexFindDirection::Forward) const;
    qint64 find(const QVariant& value, qint64 offset,
                QHexFindMode mode = QHexFindMode::Text,
                unsigned int options = QHexFindOptions::None,
                QHexFindDirection fd = QHexFindDirection::Forward) const;
    void setOptions(const QHexOptions& options);
    void setBaseAddress(quint64 baseaddress);
    void setDelegate(QHexDelegate* rd);
    void setDocument(QHexDocument* doc);
    void setData(const QByteArray& ba);
    void setData(QHexBuffer* buffer);
    void setTrackChanges(bool b);
    void setCursorMode(QHexCursor::Mode mode);
    void setByteColor(quint8 b, const QHexCharFormat& cf);
    void setByteForeground(quint8 b, const QColor& c);
    void setByteBackground(quint8 b, const QBrush& c);
    void setMetadata(qint64 begin, qint64 end, const QColor& fg,
                     const QBrush& bg, const QString& comment);
    void setForeground(qint64 begin, qint64 end, const QColor& fg);
    void setBackground(qint64 begin, qint64 end, const QBrush& bg);
    void setComment(qint64 begin, qint64 end, const QString& comment);
    void setMetadataSize(qint64 begin, qint64 length, const QColor& fg,
                         const QBrush& bg, const QString& comment);
    void setForegroundSize(qint64 begin, qint64 length, const QColor& fg);
    void setBackgroundSize(qint64 begin, qint64 length, const QBrush& bg);
    void setCommentSize(qint64 begin, qint64 length, const QString& comment);
    void removeMetadata(qint64 line);
    void removeBackground(qint64 line);
    void removeForeground(qint64 line);
    void removeComments(qint64 line);
    void unhighlight(qint64 line);
    void clearMetadata();

    void setBData(const QByteArray& data) override  {
        qDebug() << "HexView: setBData()";
        m_ignoreModification = true;
        m_hexdocument->setData(data);
        m_ignoreModification = false;
    }
    QByteArray getBData() override {
        return m_hexdocument->read(0, m_hexdocument->length());
    }

public Q_SLOTS:
#if defined(QHEXVIEW_ENABLE_DIALOGS)
    void showFind();
    void showReplace();
#endif
    void invertByteOrder();
    void undo();
    void redo();
    void cut(bool hex = false);
    void copyVisual() const;
    void copyFormat(const QHexCopyFormat& cf) const;
    void copy(bool hex = false) const;
    void paste(bool hex = false);
    void clearModified();
    void clearChanges();
    void selectAll();
    void removeSelection();
    void switchMode();
    void setAddressWidth(unsigned int w);
    void setLineLength(unsigned int l);
    void setGroupLength(unsigned int l);
    void setScrollSteps(int scrollsteps);
    void setReadOnly(bool r);
    void setAutoWidth(bool r);

private:
    void paint(QPainter* p) const;
    void checkOptions();
    void checkState();
    void checkAndUpdate(bool calccolumns = false);
    void calcColumns();
    void ensureVisible();
    void drawSeparators(QPainter* p) const;
    void drawHeader(PaintContext* ctx) const;
    void drawDocument(PaintContext* ctx) const;
    void drawAddressPart(PaintContext* ctx, quint64 line) const;
    void drawHexPart(PaintContext* ctx, const QByteArray& linebytes,
                     quint64 line) const;
    void drawAsciiPart(PaintContext* ctx, const QByteArray& linebytes,
                       quint64 line) const;
    QHexCharFormat drawFormat(PaintContext* ctx, quint8 b, const QString& s,
                              QHexArea area, qint64 line, qint64 column,
                              bool applyformat) const;
    unsigned int calcAddressWidth() const;
    int visibleLines(bool absolute = false) const;
    qint64 getLastColumn(qint64 line) const;
    qint64 lastLine() const;
    qreal getNCellsWidth(int n) const;
    qreal hexColumnWidth() const;
    qreal hexColumnX() const;
    qreal asciiColumnX() const;
    qreal endColumnX() const;
    qreal cellWidth() const;
    qreal lineWidth() const;
    qreal lineHeight() const;
    qint64 positionFromLineCol(qint64 line, qint64 col, qint64& adjcol) const;
    QHexPosition positionFromPoint(QPoint pt) const;
    QPoint absolutePoint(QPoint pt) const;
    QHexArea areaFromPoint(QPoint pt) const;
    void moveNext(bool select = false);
    void movePrevious(bool select = false);
    bool atBottom() const;
    bool keyPressMove(QKeyEvent* e);
    bool keyPressTextInput(QKeyEvent* e);
    bool keyPressAction(QKeyEvent* e);

protected:
    bool event(QEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    static QString reduced(const QString& s, int maxlen);
    static bool isColorLight(QColor c);

Q_SIGNALS:
    void dataChanged(const QByteArray& data, quint64 offset,
                     QHexChangeReason reason);
    void trackChangesChanged(bool trackchanges);
    void modifiedChanged(bool modified);
    void positionChanged();
    void modeChanged();

private:
    bool m_readonly{false}, m_writing{false}, m_autowidth{false};
    QHexArea m_currentarea{QHexArea::Ascii};
    QList<QRectF> m_hexcolumns;
    QFontMetricsF m_fontmetrics;
    QHexOptions m_options;
    QHexCursor* m_hexcursor{nullptr};
    QHexDocument* m_hexdocument{nullptr};
    QHexMetadata* m_hexmetadata{nullptr};
    QHexDelegate* m_hexdelegate{nullptr};
    qreal m_fixedCellWidth{0.0};
#if defined(QHEXVIEW_ENABLE_DIALOGS)
    HexFindDialog *m_hexdlgfind{nullptr}, *m_hexdlgreplace{nullptr};
#endif

    friend class QHexDelegate;
    friend class QHexCursor;
};
