// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "terminalview.h"
#include "terminalsurface.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QLoggingCategory>
#include <QMenu>
#include <QMimeData>
#include <QPaintEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextLayout>
#include <QToolTip>

static Q_LOGGING_CATEGORY(terminalLog, "qtc.terminal", QtWarningMsg)
static Q_LOGGING_CATEGORY(selectionLog, "qtc.terminal.selection", QtWarningMsg)
static Q_LOGGING_CATEGORY(paintLog, "qtc.terminal.paint", QtWarningMsg)

namespace TerminalSolution {

using namespace std::chrono;
using namespace std::chrono_literals;

static constexpr milliseconds minFlushInterval = 8ms;
static constexpr milliseconds minPaintInterval = 16ms;

class TerminalViewPrivate
{
public:
    TerminalViewPrivate()
    {
        m_cursorBlinkTimer.setInterval(750ms);
        m_cursorBlinkTimer.setSingleShot(false);

        m_flushDelayTimer.setSingleShot(true);
        m_flushDelayTimer.setInterval(minFlushInterval);

        m_updateTimer.setSingleShot(true);
        m_updateTimer.setInterval(minPaintInterval);

        m_scrollTimer.setSingleShot(false);
        m_scrollTimer.setInterval(500ms);
    }

    std::optional<TerminalView::Selection> m_selection;
    std::unique_ptr<TerminalSurface> m_surface;

    QSizeF m_cellSize;

    bool m_ignoreScroll{false};

    QString m_preEditString;

    std::optional<TerminalView::LinkSelection> m_linkSelection;

    struct
    {
        QPoint start;
        QPoint end;
    } m_activeMouseSelect;

    QTimer m_flushDelayTimer;

    QTimer m_updateTimer;
    std::optional<QRegion> m_updateRegion;
    QDeadlineTimer m_sinceLastPaint;

    QTimer m_scrollTimer;
    int m_scrollDirection{0};

    std::array<QColor, 19> m_currentColors;

    system_clock::time_point m_lastFlush{system_clock::now()};
    system_clock::time_point m_lastDoubleClick{system_clock::now()};
    bool m_selectLineMode{false};
    Cursor m_cursor;
    QTimer m_cursorBlinkTimer;
    bool m_cursorBlinkState{true};
    bool m_allowBlinkingCursor{true};
    bool m_allowMouseTracking{true};
    bool m_passwordModeActive{false};

    SurfaceIntegration *m_surfaceIntegration{nullptr};

    std::function<void()> m_surfaceUpdater;
};

QFont defaultTerminalFont()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

TerminalView::TerminalView(QWidget *parent)
    : QAbstractScrollArea(parent)
    , d(std::make_unique<TerminalViewPrivate>())
{
    setupSurface();
    setFont(defaultTerminalFont());

    connect(&d->m_cursorBlinkTimer, &QTimer::timeout, this, [this] {
        if (hasFocus())
            d->m_cursorBlinkState = !d->m_cursorBlinkState;
        else
            d->m_cursorBlinkState = true;
        updateViewportRect(gridToViewport(QRect{d->m_cursor.position, d->m_cursor.position}));
    });

    setAttribute(Qt::WA_InputMethodEnabled);
    setAttribute(Qt::WA_MouseTracking);
    setAcceptDrops(true);

    setCursor(Qt::IBeamCursor);

    setViewportMargins(1, 1, 1, 1);

    setFocus();
    setFocusPolicy(Qt::StrongFocus);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    connect(&d->m_flushDelayTimer, &QTimer::timeout, this, [this] { flushVTerm(true); });
    connect(&d->m_updateTimer, &QTimer::timeout, this, &TerminalView::scheduleViewportUpdate);

    connect(&d->m_scrollTimer, &QTimer::timeout, this, [this] {
        if (d->m_scrollDirection < 0)
            verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
        else if (d->m_scrollDirection > 0)
            verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    });
}

TerminalView::~TerminalView() = default;

void TerminalView::setSurfaceIntegration(SurfaceIntegration *surfaceIntegration)
{
    d->m_surfaceIntegration = surfaceIntegration;
    if (d->m_surface)
        d->m_surface->setSurfaceIntegration(d->m_surfaceIntegration);
}

TerminalSurface *TerminalView::surface() const
{
    return d->m_surface.get();
}

/*!
    Sets an \a updater function to call when the surface changes.
    If \a updater is valid, it also directly calls it once.
*/
void TerminalView::setSurfaceUpdater(const std::function<void()> &updater)
{
    d->m_surfaceUpdater = updater;
    if (updater)
        updater();
}

std::function<void()> TerminalView::surfaceUpdater() const
{
    return d->m_surfaceUpdater;
}

void TerminalView::setupSurface()
{
    d->m_surface = std::make_unique<TerminalSurface>(QSize{80, 60});
    connect(d->m_surface.get(), &TerminalSurface::cleared, this, &TerminalView::cleared);

    if (d->m_surfaceIntegration)
        d->m_surface->setSurfaceIntegration(d->m_surfaceIntegration);

    d->m_surface->setWriteToPty([this](const QByteArray &data) { return writeToPty(data); });

    connect(d->m_surface.get(), &TerminalSurface::fullSizeChanged, this, [this] {
        updateScrollBars();
    });
    connect(d->m_surface.get(), &TerminalSurface::invalidated, this, [this](const QRect &rect) {
        setSelection(std::nullopt);
        const QRect dirtyRows{QPoint{0, rect.top()},
                              QPoint{d->m_surface->liveSize().width() - 1, rect.bottom()}};
        updateViewportRect(gridToViewport(dirtyRows));
        if (verticalScrollBar()->value() == verticalScrollBar()->maximum())
            verticalScrollBar()->setValue(d->m_surface->fullSize().height());
    });
    connect(
        d->m_surface.get(),
        &TerminalSurface::cursorChanged,
        this,
        [this](const Cursor &oldCursor, const Cursor &newCursor) {
            int startY = oldCursor.position.y();
            int endY = newCursor.position.y();
            if (startY > endY)
                std::swap(startY, endY);

            d->m_cursor = newCursor;

            const QRect dirtyRows{QPoint{0, startY},
                                  QPoint{d->m_surface->liveSize().width() - 1, endY}};
            updateViewportRect(gridToViewport(dirtyRows));
            configBlinkTimer();
        });
    connect(d->m_surface.get(), &TerminalSurface::altscreenChanged, this, [this] {
        updateScrollBars();
        if (!setSelection(std::nullopt))
            updateViewport();
    });
    connect(d->m_surface.get(), &TerminalSurface::unscroll, this, [this] {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    });

    if (d->m_surfaceUpdater)
        d->m_surfaceUpdater();
    updateScrollBars();
}

void TerminalView::setAllowBlinkingCursor(bool allow)
{
    d->m_allowBlinkingCursor = allow;
}
bool TerminalView::allowBlinkingCursor() const
{
    return d->m_allowBlinkingCursor;
}

void TerminalView::configBlinkTimer()
{
    bool shouldRun = d->m_cursor.visible && d->m_cursor.blink && hasFocus()
                     && d->m_allowBlinkingCursor;
    if (shouldRun != d->m_cursorBlinkTimer.isActive()) {
        if (shouldRun)
            d->m_cursorBlinkTimer.start();
        else
            d->m_cursorBlinkTimer.stop();
    }
}

QColor TerminalView::toQColor(std::variant<int, QColor> color) const
{
    if (std::holds_alternative<int>(color)) {
        int idx = std::get<int>(color);
        if (idx >= 0 && idx < 18)
            return d->m_currentColors[idx];

        return d->m_currentColors[(int) WidgetColorIdx::Background];
    }
    return std::get<QColor>(color);
}

void TerminalView::setColors(const std::array<QColor, 19> &newColors)
{
    if (d->m_currentColors == newColors)
        return;

    d->m_currentColors = newColors;

    updateViewport();
    update();
}

void TerminalView::setPasswordMode(bool passwordMode)
{
    if (passwordMode != d->m_passwordModeActive) {
        d->m_passwordModeActive = passwordMode;
        updateViewport();
    }
}

void TerminalView::enableMouseTracking(bool enable)
{
    d->m_allowMouseTracking = enable;
}

void TerminalView::setFont(const QFont &font)
{
    QFont terminalFont = font;
    terminalFont.setHintingPreference(QFont::PreferVerticalHinting);
    terminalFont.setKerning(false);
    terminalFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        terminalFont.styleStrategy() | QFont::PreferAntialias | QFont::PreferNoShaping));

    QAbstractScrollArea::setFont(terminalFont);

    QFontMetricsF qfm{terminalFont, viewport()};
    const qreal cellWidth = qfm.horizontalAdvance(QLatin1Char('M'));
    qCInfo(terminalLog) << terminalFont.family() << terminalFont.pointSize() << cellWidth
                        << qfm.maxWidth() << viewport()->size();

    d->m_cellSize = {cellWidth, (double) qCeil(qfm.height())};

    applySizeChange();
}

void TerminalView::copyToClipboard()
{
    if (!d->m_selection.has_value())
        return;

    QString text = textFromSelection();

    qCDebug(selectionLog) << "Copied to clipboard: " << text;

    setClipboard(text);

    clearSelection();
}

void TerminalView::pasteFromClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    const QString clipboardText = clipboard->text(QClipboard::Clipboard);

    if (clipboardText.isEmpty())
        return;

    d->m_surface->pasteFromClipboard(clipboardText);
}

void TerminalView::paste(const QString &text)
{
    if (text.isEmpty())
        return;

    d->m_surface->pasteFromClipboard(text);
}

void TerminalView::copyLinkToClipboard()
{
    if (d->m_linkSelection)
        setClipboard(d->m_linkSelection->link.text);
}

std::optional<TerminalView::Selection> TerminalView::selection() const
{
    return d->m_selection;
}

void TerminalView::clearSelection()
{
    setSelection(std::nullopt);
    //d->m_surface->sendKey(Qt::Key_Escape);
}

void TerminalView::selectAll()
{
    setSelection(Selection{0, d->m_surface->fullSize().width() * d->m_surface->fullSize().height()});
}

void TerminalView::zoomIn()
{
    QFont f = font();
    f.setPointSize(f.pointSize() + 1);
    setFont(f);
}

void TerminalView::zoomOut()
{
    QFont f = font();
    f.setPointSize(qMax(f.pointSize() - 1, 1));
    setFont(f);
}

void TerminalView::moveCursorWordLeft()
{
    writeToPty("\x1b\x62");
}

void TerminalView::moveCursorWordRight()
{
    writeToPty("\x1b\x66");
}

void TerminalView::clearContents()
{
    d->m_surface->clearAll();
}

void TerminalView::writeToTerminal(const QByteArray &data, bool forceFlush)
{
    d->m_surface->dataFromPty(data);
    flushVTerm(forceFlush);
}

void TerminalView::flushVTerm(bool force)
{
    const system_clock::time_point now = system_clock::now();
    const milliseconds timeSinceLastFlush = duration_cast<milliseconds>(now - d->m_lastFlush);

    const bool shouldFlushImmediately = timeSinceLastFlush > minFlushInterval;
    if (force || shouldFlushImmediately) {
        if (d->m_flushDelayTimer.isActive())
            d->m_flushDelayTimer.stop();

        d->m_lastFlush = now;
        d->m_surface->flush();
        return;
    }

    if (!d->m_flushDelayTimer.isActive()) {
        const milliseconds timeToNextFlush = (minFlushInterval - timeSinceLastFlush);
        d->m_flushDelayTimer.start(timeToNextFlush.count());
    }
}

QString TerminalView::textFromSelection() const
{
    if (!d->m_selection)
        return {};

    if (d->m_selection->start == d->m_selection->end)
        return {};

    CellIterator it = d->m_surface->iteratorAt(d->m_selection->start);
    CellIterator end = d->m_surface->iteratorAt(d->m_selection->end);

    if (it.position() > end.position()) {
        qCWarning(selectionLog) << "Invalid selection: start >= end";
        return {};
    }

    std::u32string s;
    bool previousWasZero = false;
    for (; it != end; ++it) {
        if (it.gridPos().x() == 0 && !s.empty() && previousWasZero)
            s += U'\n';

        if (*it != 0) {
            previousWasZero = false;
            s += *it;
        } else {
            previousWasZero = true;
        }
    }

    return QString::fromUcs4(s.data(), static_cast<int>(s.size()));
}

bool TerminalView::setSelection(const std::optional<Selection> &selection, bool scroll)
{
    qCDebug(selectionLog) << "setSelection" << selection.has_value();
    if (selection.has_value())
        qCDebug(selectionLog) << "start:" << selection->start << "end:" << selection->end
                              << "final:" << selection->final;

    if (selectionLog().isDebugEnabled())
        updateViewport();

    if (selection == d->m_selection)
        return false;

    d->m_selection = selection;
    selectionChanged(d->m_selection);

    if (d->m_selection && d->m_selection->final && scroll) {
        QPoint start = d->m_surface->posToGrid(d->m_selection->start);
        QPoint end = d->m_surface->posToGrid(d->m_selection->end);
        QRect viewRect = gridToViewport(QRect{start, end});
        if (viewRect.y() >= viewport()->height() || viewRect.y() < 0) {
            // Selection is outside of the viewport, scroll to it.
            verticalScrollBar()->setValue(start.y());
        }
    }

    if (!selectionLog().isDebugEnabled())
        updateViewport();

    return true;
}

void TerminalView::restart()
{
    setupSurface();
    applySizeChange();
}

QPoint TerminalView::viewportToGlobal(QPoint p) const
{
    int y = p.y() - topMargin();
    const double offset = verticalScrollBar()->value() * d->m_cellSize.height();
    y += offset;

    return {p.x(), y};
}

QPoint TerminalView::globalToViewport(QPoint p) const
{
    int y = p.y() + topMargin();
    const double offset = verticalScrollBar()->value() * d->m_cellSize.height();
    y -= offset;

    return {p.x(), y};
}

QPoint TerminalView::globalToGrid(QPointF p) const
{
    return QPoint(p.x() / d->m_cellSize.width(), p.y() / d->m_cellSize.height());
}

/*
	Зря вы сюда полезли! Ребята, не стоит копаться в терминале.
	Вы молодые, шутливые, вам всё легко. Это не то.
	Это не Чикатило и даже не архивы спецслужб. Сюда лучше не лезть. Серьезно, любой из вас будет жалеть.
	Лучше закройте вкладку и забудьте, что тут писалось.
	Я вполне понимаю, что данным сообщением вызову дополнительный интерес, но хочу сразу предостеречь пытливых - стоп.
	Остальных просто не найдут...
*/

QPoint TerminalView::globalToRenderedGrid(QPointF p) const
{
    const int rowCount = d->m_surface->fullSize().height();
    const int columnCount = d->m_surface->liveSize().width();
    if (rowCount <= 0 || columnCount <= 0)
        return {};

    const int row = qBound(0, qFloor(p.y() / d->m_cellSize.height()), rowCount - 1);
    QString text;
    QList<QTextLayout::FormatRange> formats;
    QVector<int> columnsForTextOffset;
    columnsForTextOffset.append(0);

    for (int cellX = 0; cellX < columnCount;) {
        const auto cell = d->m_surface->fetchCell(cellX, row);
        const int occupiedCells = qMax(cell.width, 1);
        const QString cellText = cell.text.isEmpty()
                                     ? QString(occupiedCells, QLatin1Char(' '))
                                     : cell.text;

        QTextCharFormat format;
        format.setFontWeight(cell.bold ? QFont::Bold : QFont::Normal);
        format.setFontItalic(cell.italic);

        const int textStart = static_cast<int>(text.size());
        text += cellText;
        const int textEnd = static_cast<int>(text.size());

        for (int offset = textStart; offset < textEnd; ++offset) {
            const int relativeOffset = offset - textStart;
            columnsForTextOffset.append(
                cellX
                + ((relativeOffset + 1) * occupiedCells) / qMax(textEnd - textStart, 1));
        }
        columnsForTextOffset.last() = qMin(cellX + occupiedCells, columnCount);

        if (!formats.isEmpty() && formats.constLast().start + formats.constLast().length
                                      == textStart
            && formats.constLast().format == format) {
            formats.last().length += cellText.size();
        } else {
            QTextLayout::FormatRange range;
            range.start = textStart;
            range.length = cellText.size();
            range.format = format;
            formats.append(range);
        }

        cellX += occupiedCells;
    }

    QTextLayout layout(text, font(), viewport());
    layout.setFormats(formats);
    layout.beginLayout();
    const QTextLine line = layout.createLine();
    layout.endLayout();

    if (!line.isValid())
        return {qBound(0, qFloor(p.x() / d->m_cellSize.width()), columnCount), row};

    const int textOffset = qBound(0,
                                  line.xToCursor(p.x(), QTextLine::CursorBetweenCharacters),
                                  static_cast<int>(text.size()));
    return {columnsForTextOffset.value(textOffset, columnCount), row};
}

QPointF TerminalView::gridToGlobal(QPoint p, bool bottom, bool right) const
{
    QPointF result = QPointF(p.x() * d->m_cellSize.width(), p.y() * d->m_cellSize.height());
    if (bottom || right)
        result += {right ? d->m_cellSize.width() : 0, bottom ? d->m_cellSize.height() : 0};
    return result;
}

qreal TerminalView::renderedColumnX(int column, int row) const
{
    if (column <= 0)
        return 0.0;

    QString text;
    QList<QTextLayout::FormatRange> formats;

    for (int cellX = 0; cellX < column;) {
        const auto cell = d->m_surface->fetchCell(cellX, row);
        const int occupiedCells = qMax(cell.width, 1);
        const QString cellText = cell.text.isEmpty()
                                     ? QString(occupiedCells, QLatin1Char(' '))
                                     : cell.text;

        QTextCharFormat format;
        format.setFontWeight(cell.bold ? QFont::Bold : QFont::Normal);
        format.setFontItalic(cell.italic);

        const int textStart = text.size();
        text += cellText;

        if (!formats.isEmpty() && formats.constLast().start + formats.constLast().length
                                      == textStart
            && formats.constLast().format == format) {
            formats.last().length += cellText.size();
        } else {
            QTextLayout::FormatRange range;
            range.start = textStart;
            range.length = cellText.size();
            range.format = format;
            formats.append(range);
        }

        cellX += occupiedCells;
    }

    QTextLayout layout(text, font(), viewport());
    layout.setFormats(formats);
    layout.beginLayout();
    const QTextLine line = layout.createLine();
    layout.endLayout();

    return line.isValid() ? line.cursorToX(text.size()) : column * d->m_cellSize.width();
}

qreal TerminalView::topMargin() const
{
    return viewport()->size().height()
           - (d->m_surface->liveSize().height() * d->m_cellSize.height());
}

bool TerminalView::paintSelection(QPainter &p, const QRectF &cellRect, const QPoint gridPos) const
{
    bool isInSelection = false;
    const int pos = d->m_surface->gridToPos(gridPos);

    if (d->m_selection)
        isInSelection = pos >= d->m_selection->start && pos < d->m_selection->end;

    if (isInSelection)
        p.fillRect(cellRect, d->m_currentColors[(size_t) WidgetColorIdx::Selection]);

    return isInSelection;
}

void TerminalView::paintCursor(QPainter &p) const
{
    auto cursor = d->m_surface->cursor();

    const int cursorCellWidth = qMax(
        d->m_surface->cellWidthAt(cursor.position.x(), cursor.position.y()), 1);
    const QPointF cursorTopLeft{renderedColumnX(cursor.position.x(), cursor.position.y()),
                                gridToGlobal(cursor.position).y()};
    const QSizeF cursorSize{d->m_cellSize.width() * cursorCellWidth, d->m_cellSize.height()};

    if (!d->m_preEditString.isEmpty()) {
        cursor.shape = Cursor::Shape::Underline;
    } else if (d->m_passwordModeActive) {
        QRectF cursorRect = QRectF(cursorTopLeft, cursorSize).toAlignedRect();

        const qreal unit = qMax<qreal>(1.0, cursorRect.height() / 8.0);
        const QRectF body = cursorRect.adjusted(unit, cursorRect.height() * 0.42,
                                                -unit, -unit);
        const QRectF shackle(body.left() + body.width() * 0.2,
                             cursorRect.top() + unit,
                             body.width() * 0.6,
                             cursorRect.height() * 0.55);
        p.save();
        p.setPen(QPen(d->m_currentColors[static_cast<size_t>(WidgetColorIdx::Foreground)],
                      qMax<qreal>(1.0, unit * 0.65)));
        p.setBrush(Qt::NoBrush);
        p.drawArc(shackle, 0, 180 * 16);
        p.setBrush(d->m_currentColors[static_cast<size_t>(WidgetColorIdx::Foreground)]);
        p.drawRoundedRect(body, unit, unit);
        p.restore();

        return;
    }
    const bool blinkState = !cursor.blink || d->m_cursorBlinkState || !d->m_allowBlinkingCursor
                            || !d->m_cursorBlinkTimer.isActive();

    if (cursor.visible && blinkState) {
        QRectF cursorRect = QRectF(cursorTopLeft, cursorSize).toAlignedRect();

        cursorRect.adjust(1, 1, -1, -1);

        QPen pen(Qt::white, 0, Qt::SolidLine);
        p.setPen(pen);

        if (hasFocus()) {
            QPainter::CompositionMode oldMode = p.compositionMode();
            p.setCompositionMode(QPainter::RasterOp_NotDestination);
            switch (cursor.shape) {
            case Cursor::Shape::Block:
                p.fillRect(cursorRect, p.pen().brush());
                break;
            case Cursor::Shape::Underline:
                p.drawLine(cursorRect.bottomLeft(), cursorRect.bottomRight());
                break;
            case Cursor::Shape::LeftBar:
                p.drawLine(cursorRect.topLeft(), cursorRect.bottomLeft());
                break;
            }
            p.setCompositionMode(oldMode);
        } else {
            p.drawRect(cursorRect);
        }
    }
}

void TerminalView::paintPreedit(QPainter &p) const
{
    auto cursor = d->m_surface->cursor();
    if (!d->m_preEditString.isEmpty()) {
        const QPointF cursorTopLeft{renderedColumnX(cursor.position.x(), cursor.position.y()),
                                    gridToGlobal(cursor.position).y()};
        QRectF rect(cursorTopLeft, d->m_cellSize);

        rect.setWidth(viewport()->width() - rect.x());

        p.setPen(toQColor((int) WidgetColorIdx::Foreground));
        QFont f = font();
        f.setUnderline(true);
        p.setFont(f);
        p.drawText(rect, Qt::TextDontClip | Qt::TextWrapAnywhere, d->m_preEditString);
    }
}

void TerminalView::paintCells(QPainter &p, QPaintEvent *event) const
{
    const int scrollOffset = verticalScrollBar()->value();

    const int maxRow = d->m_surface->fullSize().height();
    const int startRow = qFloor((qreal) event->rect().y() / d->m_cellSize.height()) + scrollOffset;
    const int endRow = qMin(maxRow,
                            qCeil((event->rect().y() + event->rect().height())
                                  / d->m_cellSize.height())
                                + scrollOffset);

    for (int cellY = startRow; cellY < endRow; ++cellY) {
        struct RowCell
        {
            int column;
            int textStart;
            int textEnd;
            TerminalCell cell;
        };

        QString lineText;
        QList<QTextLayout::FormatRange> formats;
        QList<RowCell> rowCells;

        for (int cellX = 0; cellX < d->m_surface->liveSize().width();) {
            const auto cell = d->m_surface->fetchCell(cellX, cellY);
            const int occupiedCells = qMax(cell.width, 1);

            const QString cellText = cell.text.isEmpty()
                                         ? QString(occupiedCells, QLatin1Char(' '))
                                         : cell.text;

            QTextCharFormat format;
            format.setForeground(toQColor(cell.foregroundColor));
            format.setFontWeight(cell.bold ? QFont::Bold : QFont::Normal);
            format.setFontItalic(cell.italic);
            format.setFontStrikeOut(cell.strikeOut);

            QTextCharFormat::UnderlineStyle underlineStyle = cell.underlineStyle;
            if (d->m_linkSelection) {
                const int position = d->m_surface->gridToPos({cellX, cellY});
                if (position >= d->m_linkSelection->start
                    && position < d->m_linkSelection->end) {
                    underlineStyle = QTextCharFormat::DashUnderline;
                }
            }
            format.setUnderlineStyle(underlineStyle);

            const int textStart = lineText.size();
            lineText += cellText;
            rowCells.append({cellX, textStart, static_cast<int>(lineText.size()), cell});

            if (!formats.isEmpty() && formats.constLast().start + formats.constLast().length
                                          == textStart
                && formats.constLast().format == format) {
                formats.last().length += cellText.size();
            } else {
                QTextLayout::FormatRange range;
                range.start = textStart;
                range.length = cellText.size();
                range.format = format;
                formats.append(range);
            }

            cellX += occupiedCells;
        }

        QTextLayout layout(lineText, font(), viewport());
        layout.setFormats(formats);
        layout.beginLayout();
        QTextLine line = layout.createLine();
        if (line.isValid())
            line.setNumColumns(lineText.size());
        layout.endLayout();

        if (line.isValid()) {
            const qreal rowTop = gridToGlobal({0, cellY}).y();
            for (const RowCell &rowCell : rowCells) {
                const qreal left = line.cursorToX(rowCell.textStart);
                const qreal right = line.cursorToX(rowCell.textEnd);
                const QRectF cellRect{QPointF{qMin(left, right), rowTop},
                                      QSizeF{qAbs(right - left), d->m_cellSize.height()}};

                const bool paintBackground = !paintSelection(
                    p, cellRect, {rowCell.column, cellY});
                const bool isDefaultBackground = std::holds_alternative<int>(
                                                       rowCell.cell.backgroundColor)
                                                   && std::get<int>(rowCell.cell.backgroundColor)
                                                          == ColorIndex::Background;
                if (paintBackground && !isDefaultBackground)
                    p.fillRect(cellRect, toQColor(rowCell.cell.backgroundColor));
            }
        }

        layout.draw(&p, gridToGlobal({0, cellY}));
    }
}

void TerminalView::paintDebugSelection(QPainter &p, const Selection &selection) const
{
    auto s = globalToViewport(gridToGlobal(d->m_surface->posToGrid(selection.start)).toPoint());
    const auto e = globalToViewport(
        gridToGlobal(d->m_surface->posToGrid(selection.end), true).toPoint());

    p.setPen(QPen(Qt::green, 1, Qt::DashLine));
    p.drawLine(s.x(), 0, s.x(), height());
    p.drawLine(0, s.y(), width(), s.y());

    p.setPen(QPen(Qt::red, 1, Qt::DashLine));

    p.drawLine(e.x(), 0, e.x(), height());
    p.drawLine(0, e.y(), width(), e.y());
}

void TerminalView::paintEvent(QPaintEvent *event)
{
    QElapsedTimer t;
    t.start();
    event->accept();
    QPainter p(viewport());
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.save();

    if (paintLog().isDebugEnabled())
        p.fillRect(event->rect(), QColor::fromRgb(rand() % 60, rand() % 60, rand() % 60));
    else
        p.fillRect(event->rect(), d->m_currentColors[(size_t) WidgetColorIdx::Background]);

    int scrollOffset = verticalScrollBar()->value();
    int offset = -(scrollOffset * d->m_cellSize.height());

    qreal margin = topMargin();

    p.translate(QPointF{0.0, offset + margin});

    paintCells(p, event);
    paintCursor(p);
    paintPreedit(p);

    p.restore();

    p.fillRect(QRectF{{0, 0}, QSizeF{(qreal) width(), topMargin()}},
               d->m_currentColors[(size_t) WidgetColorIdx::Background]);

    if (selectionLog().isDebugEnabled()) {
        if (d->m_selection)
            paintDebugSelection(p, *d->m_selection);
        if (d->m_linkSelection)
            paintDebugSelection(p, *d->m_linkSelection);
    }

    if (paintLog().isDebugEnabled()) {
        QToolTip::showText(this->mapToGlobal(QPoint(width() - 200, 0)),
                           QString("Paint: %1ms").arg(t.elapsed()));
    }

    d->m_sinceLastPaint = QDeadlineTimer(minPaintInterval);
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    // Don't blink during typing
    if (d->m_cursorBlinkTimer.isActive()) {
        d->m_cursorBlinkTimer.start();
        d->m_cursorBlinkState = true;
    }

    if (event->key() == Qt::Key_Control) {
        if (!d->m_linkSelection.has_value() && checkLinkAt(mapFromGlobal(QCursor::pos()))) {
            setCursor(Qt::PointingHandCursor);
        }
    }

    event->accept();

    if (d->m_surface->isInAltScreen()) {
        d->m_surface->sendKey(event);
    } else {
        switch (event->key()) {
        case Qt::Key_PageDown:
            verticalScrollBar()->setValue(qBound(
                0,
                verticalScrollBar()->value() + d->m_surface->liveSize().height(),
                verticalScrollBar()->maximum()));
            break;
        case Qt::Key_PageUp:
            verticalScrollBar()->setValue(qBound(
                0,
                verticalScrollBar()->value() - d->m_surface->liveSize().height(),
                verticalScrollBar()->maximum()));
            break;
        default:
            if (event->key() < Qt::Key_Shift || event->key() > Qt::Key_ScrollLock)
                verticalScrollBar()->setValue(verticalScrollBar()->maximum());
            d->m_surface->sendKey(event);
            break;
        }
    }
}

void TerminalView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control && d->m_linkSelection.has_value()) {
        d->m_linkSelection.reset();
        setCursor(Qt::IBeamCursor);
        updateViewport();
    }
}

void TerminalView::applySizeChange()
{
    QSize newLiveSize = {
        qFloor((qreal) (viewport()->size().width()) / (qreal) d->m_cellSize.width()),
        qFloor((qreal) (viewport()->size().height()) / d->m_cellSize.height()),
    };

    if (newLiveSize.height() <= 0)
        return;

    if (newLiveSize.width() <= 0)
        newLiveSize.setWidth(1);

    if (d->m_surface->liveSize() == newLiveSize)
        return;

    if (resizePty(newLiveSize)) {
        d->m_surface->resize(newLiveSize);
        flushVTerm(true);
    }
}

void TerminalView::updateScrollBars()
{
    int scrollSize = d->m_surface->fullSize().height() - d->m_surface->liveSize().height();
    const bool shouldScroll = verticalScrollBar()->value() == verticalScrollBar()->maximum();
    verticalScrollBar()->setRange(0, scrollSize);
    if (shouldScroll)
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    updateViewport();
}

void TerminalView::resizeEvent(QResizeEvent *event)
{
    event->accept();

    // If increasing in size, we'll trigger libvterm to call sb_popline in
    // order to pull lines out of the history.  This will cause the scrollback
    // to decrease in size which reduces the size of the verticalScrollBar.
    // That will trigger a scroll offset increase which we want to ignore.
    d->m_ignoreScroll = true;

    applySizeChange();

    setSelection(std::nullopt);
    d->m_ignoreScroll = false;
}

QRect TerminalView::gridToViewport(QRect rect) const
{
    int offset = verticalScrollBar()->value();

    int startRow = rect.y() - offset;
    int numRows = rect.height();
    int numCols = rect.width();

    QRectF r{rect.x() * d->m_cellSize.width(),
             startRow * d->m_cellSize.height(),
             numCols * d->m_cellSize.width(),
             numRows * d->m_cellSize.height()};

    r.translate(0, topMargin());

    return r.toAlignedRect();
}

QPoint TerminalView::toGridPos(QMouseEvent *event) const
{
    return globalToGrid(QPointF(event->pos()) + QPointF(0, -topMargin() + 0.5));
}

void TerminalView::scheduleViewportUpdate()
{
    if (!d->m_passwordModeActive && d->m_updateRegion)
        viewport()->update(*d->m_updateRegion);
    else
        viewport()->update();

    d->m_updateRegion.reset();
}

void TerminalView::updateViewport()
{
    updateViewportRect({});
}

void TerminalView::updateViewportRect(const QRect &rect)
{
    if (rect.isEmpty())
        d->m_updateRegion = QRegion{viewport()->rect()};
    else if (!d->m_updateRegion)
        d->m_updateRegion = QRegion(rect);
    else
        d->m_updateRegion = d->m_updateRegion->united(rect);

    if (d->m_updateTimer.isActive())
        return;

    if (!d->m_sinceLastPaint.hasExpired()) {
        d->m_updateTimer.start();
        return;
    }

    scheduleViewportUpdate();
}

void TerminalView::focusInEvent(QFocusEvent *)
{
    updateViewport();
    configBlinkTimer();
    selectionChanged(d->m_selection);
    d->m_surface->sendFocus(true);
}
void TerminalView::focusOutEvent(QFocusEvent *)
{
    updateViewport();
    configBlinkTimer();
    d->m_surface->sendFocus(false);
}

void TerminalView::inputMethodEvent(QInputMethodEvent *event)
{
    // Gnome sends empty events when switching virtual desktops, so ignore those.
    if (event->commitString().isEmpty() && event->preeditString().isEmpty()
        && event->attributes().empty() && d->m_preEditString.isEmpty())
        return;

    verticalScrollBar()->setValue(verticalScrollBar()->maximum());

    d->m_preEditString = event->preeditString();

    if (event->commitString().isEmpty()) {
        updateViewport();
        return;
    }

    d->m_surface->sendKey(event->commitString());
}

void TerminalView::mousePressEvent(QMouseEvent *event)
{
    if (d->m_allowMouseTracking) {
        d->m_surface->mouseMove(toGridPos(event), event->modifiers());
        d->m_surface->mouseButton(event->button(), true, event->modifiers());
    }

    d->m_scrollDirection = 0;

    d->m_activeMouseSelect.start = viewportToGlobal(event->pos());

    if (event->button() == Qt::LeftButton && event->modifiers() & Qt::ControlModifier) {
        if (d->m_linkSelection) {
            if (event->modifiers() & Qt::ShiftModifier) {
                copyLinkToClipboard();
                return;
            }

            linkActivated(d->m_linkSelection->link);
        }
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (d->m_selection && system_clock::now() - d->m_lastDoubleClick < 500ms) {
            d->m_selectLineMode = true;
            const Selection newSelection{d->m_surface->gridToPos(
                                             {0,
                                              d->m_surface->posToGrid(d->m_selection->start).y()}),
                                         d->m_surface->gridToPos(
                                             {d->m_surface->liveSize().width(),
                                              d->m_surface->posToGrid(d->m_selection->end).y()}),
                                         false};
            setSelection(newSelection);
        } else {
            d->m_selectLineMode = false;
            int pos = d->m_surface->gridToPos(
                globalToRenderedGrid(viewportToGlobal(event->pos())));
            setSelection(Selection{pos, pos, false});
        }
        event->accept();
        updateViewport();
    } else if (event->button() == Qt::RightButton) {
        if (event->modifiers() & Qt::ShiftModifier) {
            contextMenuRequested(event->pos());
        } else if (d->m_selection) {
            copyToClipboard();
            setSelection(std::nullopt);
        } else {
            pasteFromClipboard();
        }
    } else if (event->button() == Qt::MiddleButton) {
        QClipboard *clipboard = QApplication::clipboard();
        if (clipboard->supportsSelection()) {
            const QString selectionText = clipboard->text(QClipboard::Selection);
            if (!selectionText.isEmpty())
                d->m_surface->pasteFromClipboard(selectionText);
        } else {
            d->m_surface->pasteFromClipboard(textFromSelection());
        }
    }
}
void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    if (d->m_allowMouseTracking)
        d->m_surface->mouseMove(toGridPos(event), event->modifiers());

    if (d->m_selection && event->buttons() & Qt::LeftButton) {
        Selection newSelection = *d->m_selection;
        int scrollVelocity = 0;
        if (event->pos().y() < 0) {
            scrollVelocity = (event->pos().y());
        } else if (event->pos().y() > viewport()->height()) {
            scrollVelocity = (event->pos().y() - viewport()->height());
        }

        if ((scrollVelocity != 0) != d->m_scrollTimer.isActive()) {
            if (scrollVelocity != 0)
                d->m_scrollTimer.start();
            else
                d->m_scrollTimer.stop();
        }

        d->m_scrollDirection = scrollVelocity;

        if (d->m_scrollTimer.isActive() && scrollVelocity != 0) {
            const milliseconds scrollInterval = 1000ms / qAbs(scrollVelocity);
            if (d->m_scrollTimer.intervalAsDuration() != scrollInterval)
                d->m_scrollTimer.setInterval(scrollInterval);
        }

        QPoint posBoundedToViewport = event->pos();
        posBoundedToViewport.setX(qBound(0, posBoundedToViewport.x(), viewport()->width()));

        int start = d->m_surface->gridToPos(
            globalToRenderedGrid(d->m_activeMouseSelect.start));
        int newEnd = d->m_surface->gridToPos(
            globalToRenderedGrid(viewportToGlobal(posBoundedToViewport)));

        if (start > newEnd) {
            std::swap(start, newEnd);
        }
        if (start < 0)
            start = 0;

        if (d->m_selectLineMode) {
            newSelection.start = d->m_surface->gridToPos({0, d->m_surface->posToGrid(start).y()});
            newSelection.end = d->m_surface->gridToPos(
                {d->m_surface->liveSize().width(), d->m_surface->posToGrid(newEnd).y()});
        } else {
            newSelection.start = start;
            newSelection.end = newEnd;
        }

        setSelection(newSelection);
    } else if (event->modifiers() & Qt::ControlModifier) {
        checkLinkAt(event->pos());
    } else if (d->m_linkSelection) {
        d->m_linkSelection.reset();
        updateViewport();
    }

    if (d->m_linkSelection) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::IBeamCursor);
    }
}

void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    if (d->m_allowMouseTracking) {
        d->m_surface->mouseMove(toGridPos(event), event->modifiers());
        d->m_surface->mouseButton(event->button(), false, event->modifiers());
    }

    d->m_scrollTimer.stop();

    if (d->m_selection && event->button() == Qt::LeftButton) {
        if (d->m_selection->end - d->m_selection->start == 0)
            setSelection(std::nullopt);
        else
            setSelection(Selection{d->m_selection->start, d->m_selection->end, true});
    }
}

void TerminalView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (d->m_allowMouseTracking) {
        d->m_surface->mouseMove(toGridPos(event), event->modifiers());
        d->m_surface->mouseButton(event->button(), true, event->modifiers());
        d->m_surface->mouseButton(event->button(), false, event->modifiers());
    }

    const auto hit = textAt(event->pos());

    setSelection(Selection{hit.start, hit.end, true});

    d->m_lastDoubleClick = system_clock::now();

    event->accept();
}

void TerminalView::wheelEvent(QWheelEvent *event)
{
    verticalScrollBar()->event(event);

    if (!d->m_allowMouseTracking)
        return;

    if (event->angleDelta().ry() > 0)
        d->m_surface->mouseButton(Qt::ExtraButton1, true, event->modifiers());
    else if (event->angleDelta().ry() < 0)
        d->m_surface->mouseButton(Qt::ExtraButton2, true, event->modifiers());
}

bool TerminalView::checkLinkAt(const QPoint &pos)
{
    const TextAndOffsets hit = textAt(pos);

    if (hit.text.size() > 0) {
        QString t = QString::fromUcs4(hit.text.c_str(), hit.text.size()).trimmed();
        auto newLink = toLink(t);
        if (newLink) {
            const LinkSelection newSelection = LinkSelection{{hit.start, hit.end}, *newLink};
            if (!d->m_linkSelection || *d->m_linkSelection != newSelection) {
                d->m_linkSelection = newSelection;
                updateViewport();
            }

            return true;
        }
    }

    if (d->m_linkSelection) {
        d->m_linkSelection.reset();
        updateViewport();
    }
    return false;
}

TerminalView::TextAndOffsets TerminalView::textAt(const QPoint &pos) const
{
    const QPoint gridPos = globalToRenderedGrid(viewportToGlobal(pos));
    auto it = d->m_surface->iteratorAt(gridPos);
    auto itRev = d->m_surface->rIteratorAt(gridPos);

    std::u32string whiteSpaces = U" \t\x00a0";

    const bool inverted = whiteSpaces.find(*it) != std::u32string::npos || *it == 0;

    auto predicate = [inverted, whiteSpaces](const std::u32string::value_type &ch) {
        if (inverted)
            return ch != 0 && whiteSpaces.find(ch) == std::u32string::npos;
        else
            return ch == 0 || whiteSpaces.find(ch) != std::u32string::npos;
    };

    auto itRight = std::find_if(it, d->m_surface->end(), predicate);
    auto itLeft = std::find_if(itRev, d->m_surface->rend(), predicate);

    std::u32string text;
    std::copy(itLeft.base(), it, std::back_inserter(text));
    std::copy(it, itRight, std::back_inserter(text));
    std::transform(text.begin(), text.end(), text.begin(), [](const char32_t &ch) {
        return ch == 0 ? U' ' : ch;
    });

    return {(itLeft.base()).position(), itRight.position(), text};
}

bool TerminalView::event(QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        QPainter p(this);
        p.fillRect(QRect(QPoint(0, 0), size()),
                   d->m_currentColors[(size_t) WidgetColorIdx::Background]);
        return true;
    }

    // TODO: Is this necessary?
    if (event->type() == QEvent::KeyPress) {
        auto k = static_cast<QKeyEvent *>(event);
        keyPressEvent(k);
        return true;
    }
    if (event->type() == QEvent::KeyRelease) {
        auto k = static_cast<QKeyEvent *>(event);
        keyReleaseEvent(k);
        return true;
    }

    return QAbstractScrollArea::event(event);
}

} // namespace TerminalSolution
