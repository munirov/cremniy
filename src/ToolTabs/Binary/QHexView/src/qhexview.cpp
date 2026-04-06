#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QMenu>
#include <QHexView/model/buffer/qmemorybuffer.h>
#include <QHexView/model/qhexcursor.h>
#include <QHexView/model/qhexutils.h>
#include <QHexView/qhexview.h>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QToolTip>
#include <QWheelEvent>
#include <QtGlobal>
#include <QtMath>
#include <limits>

#if defined(QHEXVIEW_ENABLE_DIALOGS)
#include <QHexView/dialogs/hexfinddialog.h>
#endif

#if defined(QHEXVIEW_DEBUG)
#include <QDebug>
#define qhexview_fmtprint(fmt, ...) qDebug("%s " fmt, __func__, __VA_ARGS__)
#else
#define qhexview_fmtprint(fmt, ...)
#endif

namespace {

void merge_formats(QHexCharFormat& dst, const QHexCharFormat& src) {
    if(dst.background == Qt::NoBrush)
        dst.background = src.background;

    if(!dst.foreground.isValid())
        dst.foreground = src.foreground;

    if(!dst.underline.isValid())
        dst.underline = src.underline;
}

QString qstring_rtrim(const QString s) {
    QString res = s;
    while(res.size() > 0 && res.at(res.size() - 1).isSpace())
        res.chop(1);
    return res;
}

} // namespace

QHexView::PaintContext::PaintContext(const QHexView* hv, QPainter* p,
                                     const QFontMetricsF* fm)
    : hexview{hv}, painter{p}, fontmetrics{fm}, x{0}, y{0} {}

void QHexView::PaintContext::drawText(const QString& s,
                                      const QHexCharFormat& cf, bool pad) {
    this->format = cf;

    // Always apply a valid foreground color
    if(!this->format.foreground.isValid()) {
        this->format.foreground =
            this->hexview->palette().color(QPalette::WindowText);
    }

    this->drawText(s, pad);
}

void QHexView::PaintContext::drawText(const QString& s, bool pad) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    qreal w = this->fontmetrics->horizontalAdvance(s);
#else
    qreal w = this->fontmetrics->width(s);
#endif

    QRectF r = {this->x, this->y, w, this->hexview->lineHeight()};

    if(this->format.background != Qt::NoBrush) {
        QRectF br = r;

        if(this->hexview->m_options.hasFlag(QHexFlags::PaddedHighlight) &&
           this->hexview->m_options.group_length == 1 && pad) {
            br.adjust(-this->hexview->cellWidth() / 2, 0,
                      this->hexview->cellWidth() / 2, 0);
        }

        this->painter->fillRect(br, this->format.background);
    }

    this->painter->setPen(this->format.foreground);
    this->painter->drawText(r, 0, s);

    if(this->format.underline.isValid()) {
        qreal yt = this->y + (this->fontmetrics->height() -
                              (this->fontmetrics->descent() / 2.0));
        this->painter->fillRect(QRectF{this->x, yt, w, 2.0},
                                this->format.underline);
    }

    x += this->hexview->cellWidth() * s.length();
}

void QHexView::PaintContext::fillLine(QColor c) const {
    this->painter->fillRect(
        QRectF{
            this->x,
            this->y,
            this->hexview->lineWidth(),
            this->hexview->lineHeight(),
        },
        c);
}

void QHexView::PaintContext::clearFormat() { this->format = {}; }

void QHexView::PaintContext::nextLine() {
    x = 0;
    y += this->hexview->lineHeight();
}

void QHexView::PaintContext::prevLine() {
    x = 0;
    y -= this->hexview->lineHeight();
}

void QHexView::PaintContext::advanceX() { x += this->hexview->cellWidth(); }

QHexView::QHexView(QWidget* parent)
    : QAbstractScrollArea(parent), m_fontmetrics(this->font()) {
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    if(f.styleHint() != QFont::TypeWriter) {
        f.setFamily("Monospace"); // Force Monospaced font
        f.setStyleHint(QFont::TypeWriter);
    }

    this->setFont(f);
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::StrongFocus);
    this->viewport()->setCursor(Qt::IBeamCursor);

    connect(this->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { this->viewport()->update(); });

    m_hexmetadata = new QHexMetadata(&m_options, this);
    connect(m_hexmetadata, &QHexMetadata::changed, this,
            [this]() { this->viewport()->update(); });

    m_hexcursor = new QHexCursor(&m_options, this);
    this->setDocument(
        QHexDocument::fromMemory<QMemoryBuffer>(QByteArray(), this));
    this->checkState();

    connect(m_hexcursor, &QHexCursor::positionChanged, this, [this]() {
        m_writing = false;
        this->ensureVisible();
        qDebug() << "positionChanged";
        Q_EMIT positionChanged();
    });

    connect(m_hexcursor, &QHexCursor::modeChanged, this, [this]() {
        m_writing = false;
        this->viewport()->update();
        Q_EMIT modeChanged();
    });
}

QRectF QHexView::headerRect() const {
    if(m_options.hasFlag(QHexFlags::NoHeader))
        return QRectF{0, 0, 0, 0};
    return QRectF(0, 0, this->lineWidth(), this->lineHeight());
}

QRectF QHexView::documentRect() const {
    QRectF hdr = this->headerRect();

    return QRectF{0, hdr.height(), this->lineWidth(),
                  this->viewport()->height() - hdr.height()};
}

QRectF QHexView::addressRect() const {
    qreal y = m_options.hasFlag(QHexFlags::NoHeader) ? 0 : this->lineHeight();
    return QRectF(0, y, this->endColumnX(), this->height() - y);
}

QRectF QHexView::hexRect() const {
    qreal y = m_options.hasFlag(QHexFlags::NoHeader) ? 0 : this->lineHeight();

    return QRectF(this->hexColumnX(), y,
                  this->asciiColumnX() - this->hexColumnX(),
                  this->height() - y);
}

QRectF QHexView::asciiRect() const {
    qreal y = m_options.hasFlag(QHexFlags::NoHeader) ? 0 : this->lineHeight();

    return QRectF(this->asciiColumnX(), y,
                  this->endColumnX() - this->asciiColumnX(),
                  this->height() - y);
}

QHexDocument* QHexView::hexDocument() const { return m_hexdocument; }

QHexCursor* QHexView::hexCursor() const {
    return m_hexdocument ? m_hexcursor : nullptr;
}

const QHexMetadata* QHexView::hexMetadata() const { return m_hexmetadata; }
QHexOptions QHexView::options() const { return m_options; }

void QHexView::setOptions(const QHexOptions& options) {
    auto oldlinelength = m_options.line_length;
    m_options = options;

    if(oldlinelength != m_options.line_length)
        m_hexmetadata->invalidate();

    this->checkAndUpdate();
}

void QHexView::setBaseAddress(quint64 baseaddress) {
    if(m_options.base_address == baseaddress)
        return;

    m_options.base_address = baseaddress;
    this->checkAndUpdate();
}

void QHexView::setDelegate(QHexDelegate* rd) {
    if(m_hexdelegate == rd)
        return;
    m_hexdelegate = rd;
    this->checkAndUpdate();
}

void QHexView::setDocument(QHexDocument* doc) {
    if(!doc)
        doc = QHexDocument::fromMemory<QMemoryBuffer>(QByteArray(), this);
    if(!doc->parent())
        doc->setParent(this);

    m_writing = false;
    m_hexmetadata->clear();
    m_hexcursor->move(0);

    if(m_hexdocument) {
        disconnect(m_hexdocument, &QHexDocument::changed, this, nullptr);
        disconnect(m_hexdocument, &QHexDocument::dataChanged, this, nullptr);
        disconnect(m_hexdocument, &QHexDocument::reset, this, nullptr);
        disconnect(m_hexdocument, &QHexDocument::trackChangesChanged, this,
                   nullptr);
        disconnect(m_hexdocument, &QHexDocument::modifiedChanged, this,
                   nullptr);
    }

    m_hexdocument = doc;

    connect(m_hexdocument, &QHexDocument::reset, this, [this]() {
        m_writing = false;
        m_hexcursor->move(0);
        this->checkAndUpdate(true);
    });

    connect(m_hexdocument, &QHexDocument::dataChanged, this,
            &QHexView::dataChanged);

    connect(m_hexdocument, &QHexDocument::trackChangesChanged, this,
            &QHexView::trackChangesChanged);

    connect(m_hexdocument, &QHexDocument::modifiedChanged, this,
            &QHexView::modifiedChanged);

    connect(m_hexdocument, &QHexDocument::changed, this,
            [this]() { this->checkAndUpdate(true); });

    this->checkAndUpdate(true);
}

void QHexView::setData(const QByteArray& ba) { m_hexdocument->setData(ba); }
void QHexView::setData(QHexBuffer* buffer) { m_hexdocument->setData(buffer); }
void QHexView::setTrackChanges(bool b) { m_hexdocument->setTrackChanges(b); }

void QHexView::setCursorMode(QHexCursor::Mode mode) {
    m_hexcursor->setMode(mode);
}

void QHexView::setByteColor(quint8 b, const QHexCharFormat& c) {
    m_options.byte_colors[b] = c;
    this->checkAndUpdate();
}

void QHexView::setByteForeground(quint8 b, const QColor& c) {
    m_options.byte_colors[b].foreground = c;
    this->checkAndUpdate();
}

void QHexView::setByteBackground(quint8 b, const QBrush& c) {
    m_options.byte_colors[b].background = c;
    this->checkAndUpdate();
}

void QHexView::setMetadata(qint64 begin, qint64 end, const QColor& fg,
                           const QBrush& bg, const QString& comment) {
    m_hexmetadata->setMetadata(begin, end, fg, bg, comment);
}
void QHexView::setForeground(qint64 begin, qint64 end, const QColor& fg) {
    m_hexmetadata->setForeground(begin, end, fg);
}
void QHexView::setBackground(qint64 begin, qint64 end, const QBrush& bg) {
    m_hexmetadata->setBackground(begin, end, bg);
}
void QHexView::setComment(qint64 begin, qint64 end, const QString& comment) {
    m_hexmetadata->setComment(begin, end, comment);
}
void QHexView::setMetadataSize(qint64 begin, qint64 length, const QColor& fg,
                               const QBrush& bg, const QString& comment) {
    m_hexmetadata->setMetadataSize(begin, length, fg, bg, comment);
}
void QHexView::setForegroundSize(qint64 begin, qint64 length,
                                 const QColor& fg) {
    m_hexmetadata->setForegroundSize(begin, length, fg);
}
void QHexView::setBackgroundSize(qint64 begin, qint64 length,
                                 const QBrush& bg) {
    m_hexmetadata->setBackgroundSize(begin, length, bg);
}
void QHexView::setCommentSize(qint64 begin, qint64 length,
                              const QString& comment) {
    m_hexmetadata->setCommentSize(begin, length, comment);
}
void QHexView::removeMetadata(qint64 line) {
    m_hexmetadata->removeMetadata(line);
}
void QHexView::removeBackground(qint64 line) {
    m_hexmetadata->removeBackground(line);
}
void QHexView::removeForeground(qint64 line) {
    m_hexmetadata->removeForeground(line);
}
void QHexView::removeComments(qint64 line) {
    m_hexmetadata->removeComments(line);
}
void QHexView::unhighlight(qint64 line) { m_hexmetadata->unhighlight(line); }
void QHexView::clearMetadata() { m_hexmetadata->clear(); }

#if defined(QHEXVIEW_ENABLE_DIALOGS)
void QHexView::showFind() {
    if(!m_hexdlgfind)
        m_hexdlgfind = new HexFindDialog(HexFindDialog::Type::Find, this);
    m_hexdlgfind->show();
}

void QHexView::showReplace() {
    if(!m_hexdlgreplace)
        m_hexdlgreplace = new HexFindDialog(HexFindDialog::Type::Replace, this);
    m_hexdlgreplace->show();
}
#endif

void QHexView::invertByteOrder() {
    if(m_options.hasFlag(QHexFlags::InvertedByteOrder))
        m_options.flags &= ~QHexFlags::InvertedByteOrder;
    else
        m_options.flags |= QHexFlags::InvertedByteOrder;

    this->checkAndUpdate();
}

void QHexView::undo() {
    if(m_hexdocument)
        m_hexdocument->undo();
}
void QHexView::redo() {
    if(m_hexdocument)
        m_hexdocument->redo();
}

void QHexView::cut(bool hex) {
    this->copy(hex);
    if(m_readonly)
        return;

    if(m_hexcursor->hasSelection())
        this->removeSelection();
    else
        m_hexdocument->remove(m_hexcursor->offset(), 1);
}

void QHexView::copyVisual() const {
    auto line = static_cast<quint64>(this->verticalScrollBar()->value());
    QByteArray bytes = m_hexcursor->hasSelection() ? this->selectedBytes()
                                                   : this->visibleBytes();

    auto nbytes = static_cast<qint64>(bytes.size());
    QString s;

    for(qint64 i = 0, l = 0; line < this->lines() && l < this->visibleLines();
        l++, line++) {
        quint64 address = line * m_options.line_length + this->baseAddress();
        QString addrstr = QString::number(address, 16)
                              .rightJustified(this->addressWidth(), '0')
                              .toUpper();

        s += addrstr;

        for(unsigned int col = 0u; col < m_options.line_length;) {
            s += " ";

            for(unsigned int byteidx = 0u; byteidx < m_options.group_length;
                byteidx++, col++) {
                qint64 adjcol,
                    pos = this->positionFromLineCol(line, col, adjcol);

                if(m_hexdocument->accept(pos)) {
                    s += (i + adjcol) >= nbytes
                             ? "  "
                             : QString{QHexUtils::toHex(
                                           bytes[static_cast<unsigned int>(
                                               i + adjcol)])
                                           .toUpper()};
                }
                else
                    s += QString(m_options.invalid_char).repeated(2);
            }
        }

        s += " ";

        for(unsigned int col = 0u; col < m_options.line_length; col++) {
            qint64 adjcol, pos = this->positionFromLineCol(line, col, adjcol);

            if(m_hexdocument->accept(pos)) {
                s += (i + adjcol) >= nbytes
                         ? QChar{' '}
                         : (QChar::isPrint(bytes[static_cast<int>(i + adjcol)])
                                ? QChar{bytes[static_cast<int>(i + adjcol)]}
                                : m_options.unprintable_char);
            }
            else
                s += m_options.invalid_char;
        }

        i += m_options.line_length;
        s += "\n";

        if(i >= bytes.size())
            break;
    }

    qApp->clipboard()->setText(s);
}

void QHexView::copyFormat(const QHexCopyFormat& cf) const {
    QByteArray bytes = m_hexcursor->hasSelection()
                           ? this->selectedBytes()
                           : m_hexdocument->read(m_hexcursor->offset(), 1);

    const bool IS_LONG =
        cf.line_break && (bytes.length() > m_options.line_length);
    const QString INDENT_CHAR = cf.use_tabs ? "\t" : " ";

    QString indentstr;

    if(cf.indent == -1 && !cf.prefix.isEmpty())
        indentstr = INDENT_CHAR.repeated(cf.prefix.size() + 1);
    else if(cf.indent > 0)
        indentstr = INDENT_CHAR.repeated(cf.indent);

    QString s;
    s += cf.prefix;

    if(IS_LONG)
        s += "\n" + indentstr;

    for(int i = 0; i < bytes.size(); i++) {
        if(i) {
            if(cf.line_break && !(i % m_options.line_length)) {
                s += cf.trim_last_separator ? qstring_rtrim(cf.separator)
                                            : cf.separator;
                s += "\n";
                if(IS_LONG)
                    s += indentstr;
            }
            else
                s += cf.separator;
        }

        s += cf.byte_prefix;
        s += QHexUtils::toHex(bytes[i]).toUpper();
        s += cf.byte_suffix;
    }

    if(IS_LONG)
        s += "\n";

    s += cf.suffix;
    qApp->clipboard()->setText(s);
}

void QHexView::copy(bool hex) const {
    QClipboard* c = qApp->clipboard();

    QByteArray bytes = m_hexcursor->hasSelection()
                           ? this->selectedBytes()
                           : m_hexdocument->read(m_hexcursor->offset(), 1);

    if(hex)
        bytes = QHexUtils::toHex(bytes, ' ').toUpper();
    c->setText(bytes);
}

void QHexView::paste(bool hex) {
    if(m_readonly)
        return;

    QClipboard* c = qApp->clipboard();
    QByteArray pastedata = c->text().toUtf8();
    if(pastedata.isEmpty())
        return;

    this->removeSelection();
    if(hex)
        pastedata = QByteArray::fromHex(pastedata);

    if(m_hexcursor->mode() == QHexCursor::Mode::Insert)
        m_hexdocument->insert(m_hexcursor->offset(), pastedata);
    else
        m_hexdocument->replace(m_hexcursor->offset(), pastedata);
}

void QHexView::clearModified() {
    if(m_hexdocument)
        m_hexdocument->clearModified();
}

void QHexView::clearChanges() {
    if(m_hexdocument)
        m_hexdocument->clearChanges();
}

void QHexView::selectAll() {
    m_hexcursor->move(0);
    m_hexcursor->select(m_hexdocument->length());
}

void QHexView::removeSelection() {
    if(!m_hexcursor->hasSelection())
        return;
    if(!m_readonly)
        m_hexdocument->remove(m_hexcursor->selectionStartOffset(),
                              m_hexcursor->selectionLength() - 1);
    m_hexcursor->clearSelection();
}

void QHexView::switchMode() { m_hexcursor->switchMode(); }

void QHexView::setAddressWidth(unsigned int w) {
    if(w == m_options.address_width)
        return;
    m_options.address_width = w;
    this->checkState();
}

void QHexView::setScrollSteps(int scrollsteps) {
    m_options.scroll_steps = scrollsteps;
}

void QHexView::setReadOnly(bool r) { m_readonly = r; }

void QHexView::setAutoWidth(bool r) {
    if(m_autowidth == r)
        return;
    m_autowidth = r;
    this->checkState();
}

void QHexView::paint(QPainter* p) const {
    PaintContext ctx{this, p, &m_fontmetrics};

    if(this->atBottom()) {
        this->drawDocument(&ctx);
        this->drawHeader(&ctx);
    }
    else {
        this->drawHeader(&ctx);
        this->drawDocument(&ctx);
    }

    p->setClipRect(QRectF{}, Qt::NoClip);
    this->drawSeparators(p);
}

void QHexView::checkOptions() {
    if(m_options.group_length > m_options.line_length)
        m_options.group_length = m_options.line_length;

    // Only auto-calculate if not manually set
    if(m_options.address_width == 0)
        m_options.address_width = this->calcAddressWidth();

    // Round to nearest multiple of 2
    m_options.group_length =
        1u << (static_cast<unsigned int>(qFloor(m_options.group_length / 2.0)));

    if(m_options.group_length <= 1)
        m_options.group_length = 1;

    if(!m_options.header_format.foreground.isValid()) {
        m_options.header_format.foreground =
            this->palette().color(QPalette::Normal, QPalette::Highlight);
    }
}

void QHexView::setLineLength(unsigned int l) {
    if(l == m_options.line_length)
        return;
    m_options.line_length = l;
    m_hexmetadata->invalidate();
    this->checkAndUpdate(true);
}

void QHexView::setGroupLength(unsigned int l) {
    if(l == m_options.group_length)
        return;
    m_options.group_length = l;
    this->checkAndUpdate(true);
}

void QHexView::checkState() {
    if(!m_hexdocument)
        return;
    this->checkOptions();

    int doclines = static_cast<int>(this->lines()),
        vislines = this->visibleLines(true);
    qint64 vscrollmax = doclines - vislines;
    if(doclines >= vislines)
        vscrollmax++;

    this->verticalScrollBar()->setRange(0, qMax<qint64>(0, vscrollmax));
    this->verticalScrollBar()->setPageStep(vislines - 1);
    this->verticalScrollBar()->setSingleStep(m_options.scroll_steps);

    qreal vw = this->verticalScrollBar()->isVisible()
                   ? this->verticalScrollBar()->width()
                   : 0;

    qreal pad = vw + this->cellWidth();

    this->setMaximumWidth(m_autowidth ? qCeil(this->endColumnX()) + qCeil(pad)
                                      : this->maximumWidth());

    this->horizontalScrollBar()->setRange(
        0,
        qMax<int>(0, qCeil(this->endColumnX()) - this->width() + qCeil(pad)));
    this->horizontalScrollBar()->setPageStep(this->width());
}

void QHexView::checkAndUpdate(bool calccolumns) {
    this->checkState();
    if(calccolumns)
        this->calcColumns();
    this->viewport()->update();
}

void QHexView::calcColumns() {
    if(!m_hexdocument)
        return;

    m_hexcolumns.clear();
    m_hexcolumns.reserve(m_options.line_length);

    qreal x = this->hexColumnX() + this->cellWidth(); // Pad to align
    qreal cw = this->cellWidth() * 2;

    for(unsigned int i = 0u; i < m_options.line_length;) {
        for(unsigned int j = 0u; j < m_options.group_length; i++, j++, x += cw)
            m_hexcolumns.push_back(QRectF{x, 0, cw, 0});

        x += this->cellWidth();
    }
}

void QHexView::ensureVisible() {
    if(!m_hexdocument)
        return;

    QHexPosition pos = m_hexcursor->position();
    int vlines = this->visibleLines();
    int vscroll = this->verticalScrollBar()->value();

    // Calculate target scroll position to center the cursor
    qint64 tgtscroll = pos.line - (vlines / 2);

    // Ensure we don't scroll past the beginning or end
    if(tgtscroll < 0)
        tgtscroll = 0;
    else if(tgtscroll > this->lines() - vlines)
        tgtscroll = this->lines() - vlines;

    // Line is outside of visible range
    if((pos.line < this->verticalScrollBar()->value()) ||
       (pos.line >= (this->verticalScrollBar()->value() + vlines)))
        this->verticalScrollBar()->setValue(tgtscroll);
    else
        this->viewport()->update();
}

void QHexView::drawSeparators(QPainter* p) const {
    if(!m_options.hasFlag(QHexFlags::Separators))
        return;

    const QPen& oldpen = p->pen();
    p->setPen(m_options.separator_color.isValid()
                  ? m_options.separator_color
                  : this->palette().color(QPalette::Dark));

    if(!m_options.hasFlag(QHexFlags::NoHeader) &&
       m_options.hasFlag(QHexFlags::HSeparator)) {
        QLineF l(0, this->lineHeight(), this->lineWidth() - 1,
         this->lineHeight());
        if(!m_hexdelegate || !m_hexdelegate->paintSeparator(p, l, this))
            p->drawLine(l);
    }

    if(m_options.hasFlag(QHexFlags::VSeparator)) {
        QLineF l1(this->hexColumnX(), 0, this->hexColumnX(), this->height());
        QLineF l2(this->asciiColumnX(), 0, this->asciiColumnX(),
                  this->height());

        if(!m_hexdelegate ||
           (m_hexdelegate && !m_hexdelegate->paintSeparator(p, l1, this)))
            p->drawLine(l1);

        if(!m_hexdelegate ||
           (m_hexdelegate && !m_hexdelegate->paintSeparator(p, l2, this)))
            p->drawLine(l2);
    }

    p->setPen(oldpen);
}

void QHexView::drawHeader(PaintContext* ctx) const {
    if(m_options.hasFlag(QHexFlags::NoHeader))
        return;

    ctx->painter->setClipRect(this->headerRect());
    ctx->y = 0; // Move to top

    QHexCharFormat hcf = m_options.header_format;

    // NOTE: QBrush::gradient() const-casting is also done
    // inside Qt codebase (see qplaintextedit.cpp)
    auto* g = const_cast<QGradient*>(hcf.background.gradient());

    if(m_options.hasFlag(QHexFlags::StyledHeader))
        hcf.background = this->palette().brush(QPalette::Window);
    else if(g && g->type() == QGradient::LinearGradient) {
        auto* lg = static_cast<QLinearGradient*>(g);
        lg->setStart(0, 0);
        lg->setFinalStop(0, this->headerRect().height());
    }

    if(hcf.background != Qt::NoBrush) // Draw background directly
        ctx->painter->fillRect(this->headerRect(), hcf.background);

    QString addresslabel;
    if(m_hexdelegate)
        addresslabel = m_hexdelegate->addressHeader(this);
    if(addresslabel.isEmpty() && !m_options.address_label.isEmpty())
        addresslabel = m_options.address_label;

    QHexCharFormat cf = m_options.addressheader_format;
    merge_formats(cf, hcf);

    ctx->drawText(" ", cf);
    ctx->drawText(QHexView::reduced(addresslabel, this->addressWidth()), cf);
    ctx->drawText(" ", cf);
    ctx->clearFormat();

    cf = m_options.hexheader_format;
    merge_formats(cf, hcf);

    QString hexlabel;
    if(m_hexdelegate)
        hexlabel = m_hexdelegate->hexHeader(this);
    if(hexlabel.isEmpty())
        hexlabel = m_options.hex_label;

    if(hexlabel.isNull()) {
        ctx->drawText(" ", cf);

        for(auto i = 0u; i < m_options.line_length;
            i += m_options.group_length) {
            QString h = QString::number(i, 16)
                            .rightJustified(m_options.group_length * 2, '0')
                            .toUpper();

            QHexCharFormat icf = cf;
            if(m_hexcursor->column() == static_cast<qint64>(i) &&
               m_options.hasFlag(QHexFlags::HighlightColumn)) {
                icf.background = this->palette().brush(QPalette::Highlight);
                icf.foreground =
                    this->palette().color(QPalette::HighlightedText);
            }

            ctx->drawText(h, icf);
            ctx->drawText(" ", cf);
        }
    }
    else {
        ctx->drawText(" ", cf);
        ctx->drawText(
            QHexView::reduced(hexlabel,
                              (this->hexColumnWidth() / this->cellWidth()) - 1),
            cf);
        ctx->drawText(" ", cf);
    }

    QString asciilabel;
    if(m_hexdelegate)
        asciilabel = m_hexdelegate->asciiHeader(this);
    if(asciilabel.isEmpty())
        asciilabel = m_options.ascii_label;

    cf = m_options.asciiheader_format;
    merge_formats(cf, hcf);

    if(asciilabel.isNull()) {
        ctx->drawText(" ", cf);

        for(unsigned int i = 0; i < m_options.line_length; i++) {
            QString a = QString::number(i, 16).toUpper();
            QHexCharFormat icf = cf;

            if(m_hexcursor->column() == static_cast<qint64>(i) &&
               m_options.hasFlag(QHexFlags::HighlightColumn)) {
                icf.background = this->palette().brush(QPalette::Highlight);
                icf.foreground =
                    this->palette().color(QPalette::HighlightedText);
            }

            ctx->drawText(a, icf);
        }

        ctx->drawText(" ", cf);
    }
    else {
        ctx->drawText(" ", cf);
        ctx->drawText(QHexView::reduced(asciilabel, ((this->endColumnX() -
                                                      this->asciiColumnX() -
                                                      this->cellWidth()) /
                                                     this->cellWidth()) -
                                                        1),
                      cf);
        ctx->drawText(" ", cf);
    }

    ctx->nextLine();
}

void QHexView::drawDocument(PaintContext* ctx) const {
    if(!m_hexdocument)
        return;

    ctx->painter->setClipRect(this->documentRect());

    auto do_draw_document = [&](qint64 line) {
        // Draw background
        if(m_options.linealt_background.isValid() && line % 2)
            ctx->fillLine(m_options.linealt_background);
        else if(m_options.line_background.isValid() && !(line % 2))
            ctx->fillLine(m_options.line_background);

        // Draw contents
        this->drawAddressPart(ctx, line);
        QByteArray linebytes = this->getLine(line);
        this->drawHexPart(ctx, linebytes, line);
        this->drawAsciiPart(ctx, linebytes, line);
    };

    if(this->atBottom()) {
        // Seek to end of viewport
        ctx->y = this->viewport()->height() - this->lineHeight();
        quint64 line = this->lines();

        for(qint64 l = 0; line-- > 0 && l < this->visibleLines(); l++) {
            do_draw_document(line);
            ctx->prevLine();
        }
    }
    else {
        quint64 line = static_cast<quint64>(this->verticalScrollBar()->value());
        for(qint64 l = 0; line < this->lines() && l < this->visibleLines();
            l++, line++) {
            do_draw_document(line);
            ctx->nextLine();
        }
    }
}

void QHexView::drawAddressPart(PaintContext* ctx, quint64 line) const {
    quint64 address = line * m_options.line_length + this->baseAddress();
    QString addrstr = QString::number(address, 16)
                          .rightJustified(this->addressWidth(), '0')
                          .toUpper();

    // Address Part
    QHexCharFormat acf = m_options.address_format;

    if(m_options.hasFlag(QHexFlags::StyledAddress))
        acf.background = this->palette().brush(QPalette::Window);

    if(m_hexdelegate)
        m_hexdelegate->renderAddress(address, acf, this);

    if(m_hexcursor->line() == static_cast<qint64>(line) &&
       m_options.hasFlag(QHexFlags::HighlightAddress)) {
        acf.background = this->palette().brush(QPalette::Highlight);
        acf.foreground = this->palette().color(QPalette::HighlightedText);
    }

    if(m_options.hasFlag(QHexFlags::PaddedAddress)) {
        ctx->drawText(" " + addrstr + " ", acf);
    }
    else {
        ctx->advanceX();
        ctx->drawText(addrstr, acf);
        ctx->advanceX();
    }

    ctx->advanceX();
    ctx->clearFormat();
}

void QHexView::drawHexPart(PaintContext* ctx, const QByteArray& linebytes,
                           quint64 line) const {
    for(unsigned int col = 0u; col < m_options.line_length;) {
        QHexCharFormat cf{};

        for(unsigned int byteidx = 0u; byteidx < m_options.group_length;
            byteidx++, col++) {
            QString s;
            quint8 b{};
            qint64 adjcol, pos = this->positionFromLineCol(line, col, adjcol);

            if(m_hexdocument->accept(pos)) {
                s = linebytes.isEmpty() ||
                            adjcol >= static_cast<qint64>(linebytes.size())
                        ? "  "
                        : QString(QHexUtils::toHex(linebytes.mid(adjcol, 1))
                                      .toUpper());
                b = static_cast<int>(adjcol) < linebytes.size()
                        ? linebytes.at(adjcol)
                        : 0x00;
            }
            else
                s = QString(m_options.invalid_char).repeated(2);

            cf = this->drawFormat(ctx, b, s, QHexArea::Hex, line, col,
                                  static_cast<int>(col) < linebytes.size());
        }

        ctx->drawText(" ", cf);
    }

    ctx->drawText(" ", {});
}

void QHexView::drawAsciiPart(PaintContext* ctx, const QByteArray& linebytes,
                             quint64 line) const {
    for(unsigned int col = 0u; col < m_options.line_length; col++) {
        QString s;
        quint8 b{};
        qint64 adjcol;

        if(m_hexdocument->accept(
               this->positionFromLineCol(line, col, adjcol))) {
            s = linebytes.isEmpty() ||
                        adjcol >= static_cast<qint64>(linebytes.size())
                    ? QChar{' '}
                    : (QChar::isPrint(linebytes.at(adjcol))
                           ? QChar{linebytes.at(adjcol)}
                           : m_options.unprintable_char);

            b = static_cast<int>(adjcol) < linebytes.size()
                    ? linebytes.at(adjcol)
                    : 0x00;
        }
        else
            s = m_options.invalid_char;

        this->drawFormat(ctx, b, s, QHexArea::Ascii, line, col,
                         static_cast<int>(col) < linebytes.size());
    }
}

unsigned int QHexView::calcAddressWidth() const {
    if(!m_hexdocument)
        return 0;

    auto maxaddr =
        static_cast<quint64>(m_options.base_address + m_hexdocument->length());
    if(maxaddr <= std::numeric_limits<quint32>::max())
        return 8;
    return QString::number(maxaddr, 16).size();
}

int QHexView::visibleLines(bool absolute) const {
    int vl = static_cast<int>(
        qCeil(this->viewport()->height() / this->lineHeight()));
    if(!m_options.hasFlag(QHexFlags::NoHeader))
        vl--;
    return absolute ? vl : qMin<int>(this->lines(), vl);
}

qint64 QHexView::getLastColumn(qint64 line) const {
    return this->getLine(line).size() - 1;
}
qint64 QHexView::lastLine() const { return qMax<qint64>(0, this->lines() - 1); }

qreal QHexView::hexColumnWidth() const {
    int l = 0;

    for(auto i = 0u; i < m_options.line_length; i += m_options.group_length)
        l += (2 * m_options.group_length) + 1;

    return this->getNCellsWidth(l);
}

unsigned int QHexView::addressWidth() const {
    if(!m_hexdocument || m_options.address_width)
        return m_options.address_width;
    return this->calcAddressWidth();
}

unsigned int QHexView::lineLength() const { return m_options.line_length; }

bool QHexView::isModified() const {
    return m_hexdocument && m_hexdocument->isModified();
}

bool QHexView::canUndo() const {
    return m_hexdocument && m_hexdocument->canUndo();
}

bool QHexView::canRedo() const {
    return m_hexdocument && m_hexdocument->canRedo();
}

bool QHexView::trackChanges() const {
    return m_hexdocument && m_hexdocument->trackChanges();
}

quint64 QHexView::offset() const { return m_hexcursor->offset(); }
quint64 QHexView::address() const { return m_hexcursor->address(); }

QHexPosition QHexView::positionFromOffset(quint64 offset) const {
    QHexPosition opt = QHexPosition::invalid();

    if(offset < static_cast<quint64>(m_hexdocument->length())) {
        opt.line = offset / m_options.line_length;
        opt.column = offset % m_options.line_length;
    }

    return opt;
}

QHexPosition QHexView::positionFromAddress(quint64 address) const {
    return this->positionFromOffset(address - m_options.base_address);
}

QHexPosition QHexView::position() const { return m_hexcursor->position(); }

QHexPosition QHexView::selectionStart() const {
    return m_hexcursor->selectionStart();
}

QHexPosition QHexView::selectionEnd() const {
    return m_hexcursor->selectionEnd();
}

quint64 QHexView::selectionStartOffset() const {
    return m_hexcursor->selectionStartOffset();
}

quint64 QHexView::selectionEndOffset() const {
    return m_hexcursor->selectionEndOffset();
}

quint64 QHexView::baseAddress() const { return m_options.base_address; }

quint64 QHexView::lines() const {
    if(!m_hexdocument || !m_options.line_length)
        return 0;

    lldiv_t division = std::div(m_hexdocument->length(), m_options.line_length);
    quint64 lines = division.rem ? division.quot + 1 : division.quot;
    return !m_hexdocument->isEmpty() && !lines ? 1 : lines;
}

qint64 QHexView::replace(const QVariant& oldvalue, const QVariant& newvalue,
                         qint64 offset, QHexFindMode mode, unsigned int options,
                         QHexFindDirection fd) const {
    auto res =
        QHexUtils::replace(this, oldvalue, newvalue, offset, mode, options, fd);

    if(res.first > -1) {
        m_hexcursor->move(res.first);
        m_hexcursor->selectSize(res.second);
    }

    return res.first;
}

qint64 QHexView::find(const QVariant& value, qint64 offset, QHexFindMode mode,
                      unsigned int options, QHexFindDirection fd) const {
    auto res = QHexUtils::find(this, value, offset, mode, options, fd);

    if(res.first > -1) {
        m_hexcursor->move(res.first);
        m_hexcursor->selectSize(res.second);
    }

    return res.first;
}

qreal QHexView::hexColumnX() const {
    return this->getNCellsWidth(this->addressWidth() + 2);
}

qreal QHexView::asciiColumnX() const {
    return this->hexColumnX() + this->hexColumnWidth() + this->cellWidth();
}

qreal QHexView::endColumnX() const {
    return this->asciiColumnX() + this->getNCellsWidth(m_options.line_length) +
           this->cellWidth();
}

qreal QHexView::getNCellsWidth(int n) const { return n * this->cellWidth(); }

qreal QHexView::cellWidth() const {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return m_fontmetrics.horizontalAdvance(" ");
#else
    return m_fontmetrics.width(" ");
#endif
}

qreal QHexView::lineWidth() const {
    return this->endColumnX() + this->cellWidth();
}

qreal QHexView::lineHeight() const { return m_fontmetrics.height(); }

qint64 QHexView::positionFromLineCol(qint64 line, qint64 col,
                                     qint64& adjcol) const {
    if(m_hexdocument) {
        adjcol = QHexUtils::adjustColumn(&m_options, col);
        return qMin(line * m_options.line_length + adjcol,
                    m_hexdocument->length());
    }

    return 0;
}

QHexPosition QHexView::positionFromPoint(QPoint pt) const {
    QHexPosition pos = QHexPosition::invalid();
    auto abspt = this->absolutePoint(pt);

    switch(this->areaFromPoint(pt)) {
        case QHexArea::Hex: {
            pos.column = -1;

            for(qint64 i = 0; i < m_hexcolumns.size(); i++) {
                if(m_hexcolumns.at(i).left() > abspt.x())
                    break;
                pos.column = i;
            }

            break;
        }

        case QHexArea::Ascii:
            pos.column = qMax<qint64>(
                qFloor((abspt.x() - this->asciiColumnX()) / this->cellWidth()) -
                    1,
                0);
            break;
        case QHexArea::Address: pos.column = 0; break;
        case QHexArea::Header: return QHexPosition::invalid();
        default: break;
    }

    qreal hdrheight = this->headerRect().height();
    qreal contenty = abspt.y() - hdrheight;
    if(contenty < 0) // Click in header area
        return {};

    if(this->atBottom()) {
        qreal contentheight = this->viewport()->height() - hdrheight;
        qint64 l =
            this->lines() - ((contentheight - contenty) / this->lineHeight());
        pos.line = qMax<qint64>(l, 0);
    }
    else {
        pos.line = qMin<qint64>(this->verticalScrollBar()->value() +
                                    (contenty / this->lineHeight()),
                                this->lines());
    }

    auto docline = this->getLine(pos.line);
    pos.column =
        qMin<qint64>(pos.column, docline.isEmpty() ? 0 : docline.size());

    qhexview_fmtprint("line: %lld, col: %lld", pos.line, pos.column);
    return pos;
}

QPoint QHexView::absolutePoint(QPoint pt) const {
    return pt + QPoint(this->horizontalScrollBar()->value(), 0);
}

QHexArea QHexView::areaFromPoint(QPoint pt) const {
    pt = this->absolutePoint(pt);
    qreal line =
        this->verticalScrollBar()->value() + pt.y() / this->lineHeight();

    if(!m_options.hasFlag(QHexFlags::NoHeader) && !qFloor(line))
        return QHexArea::Header;
    if(pt.x() < this->hexColumnX())
        return QHexArea::Address;
    if(pt.x() < this->asciiColumnX())
        return QHexArea::Hex;
    if(pt.x() < this->endColumnX())
        return QHexArea::Ascii;
    return QHexArea::Extra;
}

QHexCharFormat QHexView::drawFormat(PaintContext* ctx, quint8 b,
                                    const QString& s, QHexArea area,
                                    qint64 line, qint64 column,
                                    bool applyformat) const {
    QHexCharFormat cf{}, selcf{};
    QHexPosition pos{line, column};

    if(applyformat) {
        auto offset = m_hexcursor->positionToOffset(pos);
        bool hasdelegate =
            m_hexdelegate && m_hexdelegate->renderByte(offset, b, cf, this);

        if(!hasdelegate) {
            auto it = m_options.byte_colors.find(b);
            if(it != m_options.byte_colors.end())
                cf = *it;
        }

        const QHexMetadataLine* metadataline = m_hexmetadata->find(line);

        if(metadataline) {
            for(const QHexMetadataItem& metadata : *metadataline) {
                if(offset < metadata.begin || offset >= metadata.end)
                    continue;

                if(!hasdelegate) {
                    if(metadata.format.foreground.isValid())
                        cf.foreground = metadata.format.foreground;

                    if(metadata.format.background != Qt::NoBrush) {
                        cf.background = metadata.format.background;

                        if(!metadata.format.foreground.isValid()) {
                            cf.foreground = this->getReadableColor(
                                metadata.format.background.color());
                        }
                    }

                    if(metadata.format.underline.isValid())
                        cf.underline = metadata.format.underline;
                }

                if(!metadata.comment.isEmpty()) {
                    cf.underline =
                        m_options.comment_color.isValid()
                            ? m_options.comment_color
                            : this->palette().color(QPalette::WindowText);
                }

                // Remove previous metadata's style, if needed
                if(offset == metadata.begin) {
                    if(metadata.comment.isEmpty())
                        selcf.underline = QColor{};
                    if(!metadata.format.foreground.isValid())
                        selcf.foreground = Qt::color1;
                    if(metadata.format.background == Qt::NoBrush)
                        selcf.background = Qt::transparent;
                }

                if(offset < metadata.end - 1 &&
                   column < this->getLastColumn(line))
                    selcf = cf;
            }
        }

        if(hasdelegate && column < this->getLastColumn(line))
            selcf = cf;
    }

    if(m_hexdocument->trackChanges()) {
        qint64 offset = this->hexCursor()->positionToOffset(pos);

        switch(m_hexdocument->getChangeReason(offset)) {
            case QHexChangeReason::Replace:
                cf = m_options.trackchange_format_overwrite;
                break;

            case QHexChangeReason::Insert:
                cf = m_options.trackchange_format_insert;
                break;

            default: break;
        }
    }

    // Check: user select bytes?
    if(this->hexCursor()->isSelected(line, column)) {
        qint64 offset = this->hexCursor()->positionToOffset(pos);
        qint64 selend = this->hexCursor()->selectionEndOffset();

        if (this->hexCursor()->getSelectFromFormatPage()) {
            cf.background = QBrush(QColor(255, 175, 50));
        }
        else {
            cf.background = this->palette().brush(QPalette::Normal, QPalette::Highlight);
        }

        cf.foreground = this->palette().color(QPalette::Normal, QPalette::HighlightedText);

        if(offset < selend && column < this->getLastColumn(line))
            selcf = cf;
    }

    if(this->hexCursor()->position() == pos) {

        QBrush cursorbg;
        QColor cursorfg;
        QBrush discursorbg;
        QColor discursorfg;

        if (this->hexCursor()->getSelectFromFormatPage()) {
            cursorbg = QBrush(QColor(255, 175, 50));
            cursorfg = this->palette().color(QPalette::Normal, QPalette::HighlightedText);
            discursorbg = QBrush(QColor(255, 175, 50));
            discursorfg = this->palette().color(QPalette::Normal, QPalette::HighlightedText);
        }
        else{
            cursorbg = this->palette().brush(QPalette::Normal, QPalette::Highlight);
            cursorfg = this->palette().color(QPalette::Normal, QPalette::HighlightedText);
            discursorbg = this->palette().brush(QPalette::Normal, QPalette::Highlight);
            discursorfg = this->palette().color(QPalette::Normal, QPalette::HighlightedText);
        }

        switch(m_hexcursor->mode()) {
            case QHexCursor::Mode::Insert:
                cf.underline =
                    (m_currentarea == area ? cursorbg : discursorbg).color();
                break;

            case QHexCursor::Mode::Overwrite:
                cf.background = m_currentarea == area ? cursorbg : discursorbg;
                cf.foreground = m_currentarea == area ? cursorfg : discursorfg;
                break;
        }
    }

    ctx->drawText(s, cf, area == QHexArea::Hex);
    return selcf;
}

void QHexView::moveNext(bool select) {
    auto line = this->hexCursor()->line(), column = this->hexCursor()->column();

    if(column >= m_options.line_length - 1) {
        line++;
        column = 0;
    }
    else
        column++;

    qint64 offset =
        this->hexCursor()->mode() == QHexCursor::Mode::Insert ? 1 : 0;
    if(select)
        this->hexCursor()->select(
            qMin<qint64>(line, this->lines()),
            qMin<qint64>(column, this->getLastColumn(line) + offset));
    else
        this->hexCursor()->move(
            line,
            column);
}

void QHexView::movePrevious(bool select) {
    auto line = this->hexCursor()->line(), column = this->hexCursor()->column();

    if(column <= 0) {
        if(!line)
            return;
        column = this->getLine(--line).size() - 1;
    }
    else
        column--;

    if(select)
        this->hexCursor()->select(
            qMin<qint64>(line, this->lines()),
            qMin<qint64>(column, this->getLastColumn(line)));
    else
        this->hexCursor()->move(
            qMin<qint64>(line, this->lines()),
            qMin<qint64>(column, this->getLastColumn(line)));
}

bool QHexView::atBottom() const {
    const QScrollBar* vscroll = this->verticalScrollBar();
    return vscroll && vscroll->value() &&
           vscroll->value() == vscroll->maximum();
}

bool QHexView::keyPressMove(QKeyEvent* e) {
    if(e->matches(QKeySequence::MoveToNextChar) ||
       e->matches(QKeySequence::SelectNextChar))
        this->moveNext(e->matches(QKeySequence::SelectNextChar));
    else if(e->matches(QKeySequence::MoveToPreviousChar) ||
            e->matches(QKeySequence::SelectPreviousChar))
        this->movePrevious(e->matches(QKeySequence::SelectPreviousChar));
    else if(e->matches(QKeySequence::MoveToNextLine) ||
            e->matches(QKeySequence::SelectNextLine)) {
        if(this->hexCursor()->line() == this->lastLine())
            return true;
        auto nextline = this->hexCursor()->line() + 1;
        if(e->matches(QKeySequence::MoveToNextLine))
            this->hexCursor()->move(nextline, this->hexCursor()->column());
        else
            this->hexCursor()->select(nextline, this->hexCursor()->column());
    }
    else if(e->matches(QKeySequence::MoveToPreviousLine) ||
            e->matches(QKeySequence::SelectPreviousLine)) {
        if(!this->hexCursor()->line())
            return true;
        auto prevline = this->hexCursor()->line() - 1;
        if(e->matches(QKeySequence::MoveToPreviousLine))
            this->hexCursor()->move(prevline, this->hexCursor()->column());
        else
            this->hexCursor()->select(prevline, this->hexCursor()->column());
    }
    else if(e->matches(QKeySequence::MoveToNextPage) ||
            e->matches(QKeySequence::SelectNextPage)) {
        if(this->lastLine() == this->hexCursor()->line())
            return true;
        auto pageline = qMin(this->lastLine(),
                             this->hexCursor()->line() + this->visibleLines());
        if(e->matches(QKeySequence::MoveToNextPage))
            this->hexCursor()->move(pageline, this->hexCursor()->column());
        else
            this->hexCursor()->select(pageline, this->hexCursor()->column());
    }
    else if(e->matches(QKeySequence::MoveToPreviousPage) ||
            e->matches(QKeySequence::SelectPreviousPage)) {
        if(!this->hexCursor()->line())
            return true;
        auto pageline =
            qMax<qint64>(0, this->hexCursor()->line() - this->visibleLines());
        if(e->matches(QKeySequence::MoveToPreviousPage))
            this->hexCursor()->move(pageline, this->hexCursor()->column());
        else
            this->hexCursor()->select(pageline, this->hexCursor()->column());
    }
    else if(e->matches(QKeySequence::MoveToStartOfDocument) ||
            e->matches(QKeySequence::SelectStartOfDocument)) {
        if(!this->hexCursor()->line())
            return true;
        if(e->matches(QKeySequence::MoveToStartOfDocument))
            this->hexCursor()->move(0, 0);
        else
            this->hexCursor()->select(0, 0);
    }
    else if(e->matches(QKeySequence::MoveToEndOfDocument) ||
            e->matches(QKeySequence::SelectEndOfDocument)) {
        if(this->lastLine() == this->hexCursor()->line())
            return true;
        if(e->matches(QKeySequence::MoveToEndOfDocument))
            this->hexCursor()->move(
                this->lastLine(),
                this->getLastColumn(this->hexCursor()->line()));
        else
            this->hexCursor()->select(this->lastLine(),
                                      this->getLastColumn(this->lastLine()));
    }
    else if(e->matches(QKeySequence::MoveToStartOfLine) ||
            e->matches(QKeySequence::SelectStartOfLine)) {
        auto offset =
            this->hexCursor()->positionToOffset({this->hexCursor()->line(), 0});
        if(e->matches(QKeySequence::MoveToStartOfLine))
            this->hexCursor()->move(offset);
        else
            this->hexCursor()->select(offset);
    }
    else if(e->matches(QKeySequence::SelectEndOfLine) ||
            e->matches(QKeySequence::MoveToEndOfLine)) {
        auto offset = this->hexCursor()->positionToOffset(
            {this->hexCursor()->line(),
             this->getLastColumn(this->hexCursor()->line())});
        if(e->matches(QKeySequence::SelectEndOfLine))
            this->hexCursor()->select(offset);
        else
            this->hexCursor()->move(offset);
    }
    else
        return false;

    return true;
}

bool QHexView::keyPressTextInput(QKeyEvent* e) {
    if(m_readonly || e->text().isEmpty() ||
       (e->modifiers() & Qt::ControlModifier))
        return false;

    bool atend = m_hexcursor->offset() >= m_hexdocument->length();
    if(atend && m_hexcursor->mode() == QHexCursor::Mode::Overwrite){
        // return false;
    }

    char key = e->text().at(0).toLatin1();

    switch(m_currentarea) {
        case QHexArea::Hex: {
            if(!QHexUtils::isHex(key))
                return false;

            bool ok = false;
            auto val = static_cast<quint8>(QString(key).toUInt(&ok, 16));
            if(!ok)
                return false;
            m_hexcursor->removeSelection();

            quint8 ch = m_hexdocument->isEmpty() ||
                                m_hexcursor->offset() >= m_hexdocument->length()
                            ? '\x00'
                            : m_hexdocument->at(m_hexcursor->offset());
            ch = m_writing ? (ch << 4) | val : val;

            if(!m_writing && (m_hexcursor->mode() == QHexCursor::Mode::Insert))
                m_hexdocument->insert(m_hexcursor->offset(), val);
            else
                m_hexdocument->replace(m_hexcursor->offset(), ch);

            m_writing = !m_writing;
            if(!m_writing)
                this->moveNext();

            break;
        }

        case QHexArea::Ascii: {
            if(!QChar::isPrint(key))
                return false;
            m_hexcursor->removeSelection();
            if(m_hexcursor->mode() == QHexCursor::Mode::Insert)
                m_hexdocument->insert(m_hexcursor->offset(), key);
            else
                m_hexdocument->replace(m_hexcursor->offset(), key);
            this->moveNext();
            break;
        }

        default: return false;
    }

    return true;
}

bool QHexView::keyPressAction(QKeyEvent* e) {
    if(e->modifiers() != Qt::NoModifier) {
        if(e->matches(QKeySequence::SelectAll))
            this->selectAll();
        else if(!m_readonly && e->matches(QKeySequence::Undo))
            this->undo();
        else if(!m_readonly && e->matches(QKeySequence::Redo))
            this->redo();
        else if(!m_readonly && e->matches(QKeySequence::Cut))
            this->cut(m_currentarea != QHexArea::Ascii);
        else if(e->matches(QKeySequence::Copy))
            this->copy(m_currentarea != QHexArea::Ascii);
        else if(!m_readonly && e->matches(QKeySequence::Paste))
            this->paste(m_currentarea != QHexArea::Ascii);
        else
            return false;

        return true;
    }

    if(m_readonly)
        return false;

    switch(e->key()) {
        case Qt::Key_Backspace:
        case Qt::Key_Delete: {
            if(!m_hexcursor->hasSelection()) {
                auto offset = m_hexcursor->offset();
                if(offset <= 0)
                    return true;

                if(e->key() == Qt::Key_Backspace)
                    m_hexdocument->remove(offset - 1, 1);
                else
                    m_hexdocument->remove(offset, 1);
            }
            else {
                auto oldpos = m_hexcursor->selectionStart();
                m_hexcursor->removeSelection();
                m_hexcursor->move(oldpos);
            }

            if(e->key() == Qt::Key_Backspace)
                this->movePrevious();
            m_writing = false;
            break;
        }

        case Qt::Key_Insert:
            m_writing = false;
            m_hexcursor->switchMode();
            break;

        default: return false;
    }

    return true;
}

bool QHexView::event(QEvent* e) {
    switch(e->type()) {
        case QEvent::FontChange:
            m_fontmetrics = QFontMetricsF(this->font());
            this->checkAndUpdate(true);
            return true;

        case QEvent::ToolTip: {
            if(m_hexdocument && (m_currentarea == QHexArea::Hex ||
                                 m_currentarea == QHexArea::Ascii)) {
                auto* helpevent = static_cast<QHelpEvent*>(e);
                auto pos = this->positionFromPoint(helpevent->pos());
                auto comment = m_hexmetadata->getComment(pos.line, pos.column);
                if(!comment.isEmpty())
                    QToolTip::showText(helpevent->globalPos(), comment);
                return true;
            }

            break;
        }

        default: break;
    }

    return QAbstractScrollArea::event(e);
}

void QHexView::showEvent(QShowEvent* e) {
    QAbstractScrollArea::showEvent(e);
    this->checkAndUpdate(true);
}

void QHexView::paintEvent(QPaintEvent*) {
    if(!m_hexdocument)
        return;

    QPainter painter(this->viewport());
    painter.translate(-this->horizontalScrollBar()->value(), 0);
    painter.setFont(this->font());

    if(m_hexdelegate)
        m_hexdelegate->paint(&painter, this);
    else
        this->paint(&painter);

    // DEBUG: Render hex-columns area
    // int i = 0;
    //
    // for(const auto& col : m_hexcolumns) {
    //     QRectF r = col;
    //     r.setY(0);
    //     r.setHeight(this->viewport()->height());
    //
    //     QColor c{i++ % 2 ? Qt::darkRed : Qt::darkGreen};
    //     c.setAlpha(128);
    //     painter.fillRect(r, c);
    // }
}

void QHexView::resizeEvent(QResizeEvent* e) {
    this->checkState();
    QAbstractScrollArea::resizeEvent(e);
}

void QHexView::focusInEvent(QFocusEvent* e) {
    QAbstractScrollArea::focusInEvent(e);
    if(m_hexdocument)
        this->viewport()->update();
}

void QHexView::focusOutEvent(QFocusEvent* e) {
    QAbstractScrollArea::focusOutEvent(e);
    if(m_hexdocument)
        this->viewport()->update();
}

void QHexView::mousePressEvent(QMouseEvent* e) {
    hexCursor()->setSelectFromFormatPage(false);
    QAbstractScrollArea::mousePressEvent(e);
    if(!m_hexdocument || e->button() != Qt::LeftButton)
        return;

    auto pos = this->positionFromPoint(e->pos());
    if(!pos.isValid())
        return;

    auto area = this->areaFromPoint(e->pos());
    qhexview_fmtprint("%d", static_cast<int>(area));

    switch(area) {
        case QHexArea::Address: this->hexCursor()->move(pos.line, 0); break;
        case QHexArea::Hex:
            m_currentarea = area;
            this->hexCursor()->move(pos);
            break;
        case QHexArea::Ascii:
            m_currentarea = area;
            this->hexCursor()->move(pos.line, pos.column);
            break;
        default: return;
    }

    this->viewport()->update();
}

void QHexView::mouseMoveEvent(QMouseEvent* e) {
    hexCursor()->setSelectFromFormatPage(false);
    qDebug() << "QHexView::mouseMoveEvent(QMouseEvent* e)";
    QAbstractScrollArea::mouseMoveEvent(e);
    if(!this->hexCursor())
        return;

    e->accept();
    auto area = this->areaFromPoint(e->pos());

    switch(area) {
        case QHexArea::Header:
            this->viewport()->setCursor(Qt::ArrowCursor);
            return;
        case QHexArea::Address:
            this->viewport()->setCursor(Qt::ArrowCursor);
            break;
        default: this->viewport()->setCursor(Qt::IBeamCursor); break;
    }

    if(e->buttons() == Qt::LeftButton) {
        qDebug() << "QHexView::mouseMoveEvent(QMouseEvent* e): LeftButton";
        auto pos = this->positionFromPoint(e->pos());
        if(!pos.isValid())
            return;
        if(area == QHexArea::Ascii || area == QHexArea::Hex)
            m_currentarea = area;
        this->hexCursor()->select(pos);
    }
}

void QHexView::contextMenuEvent(QContextMenuEvent* e) {
    if(!m_hexdocument) {
        e->ignore();
        return;
    }

    QPoint vpos = this->viewport()->mapFrom(this, e->pos());
    auto pos = this->positionFromPoint(vpos);
    auto area = this->areaFromPoint(vpos);

    if(pos.isValid() && (area == QHexArea::Hex || area == QHexArea::Ascii)) {
        m_currentarea = area;
        if(!m_hexcursor->isSelected(pos.line, pos.column)) {
            m_hexcursor->move(pos);
            m_hexcursor->selectSize(1);
        }
        this->viewport()->update();
    }

    QMenu menu(this);
    QAction* actionSelectAll = menu.addAction(tr("Select All"));
    QAction* actionCopy = menu.addAction(tr("Copy"));
    QAction* actionPaste = menu.addAction(tr("Paste"));

    actionSelectAll->setEnabled(m_hexdocument->length() > 0);
    actionPaste->setEnabled(!m_readonly);

    QAction* chosen = menu.exec(e->globalPos());
    if(!chosen)
        return;

    if(chosen == actionSelectAll) {
        this->selectAll();
    } else if(chosen == actionCopy) {
        this->copy(m_currentarea != QHexArea::Ascii);
    } else if(chosen == actionPaste) {
        if(!m_readonly)
            this->paste(m_currentarea != QHexArea::Ascii);
    }
}

void QHexView::wheelEvent(QWheelEvent* e) {
    e->ignore();

#if defined(Q_OS_OSX)
    // In macOS scrollbar invisibility should not prevent
    // scrolling from working
#else
    if(!m_hexdocument || !this->verticalScrollBar()->isVisible())
        return;
#endif

    // https://doc.qt.io/qt-6/qwheelevent.html
    // "Returns the relative amount that the wheel was rotated, in eighths
    // of a degree." "Most mouse types work in steps of 15 degrees, in which
    // case the delta value is a multiple of 120; i.e., 120 units * 1/8 = 15
    // degrees."
    int const ydelta = e->angleDelta().y();
    if(0 != ydelta) {
        int const ydeltaAbsolute = qAbs(ydelta);
        int const numberOfLinesToMove =
            (ydeltaAbsolute * m_options.scroll_steps + 119) /
            120; // always move at least 1 line
        int const ydeltaSign = ydelta / ydeltaAbsolute;

        int const oldValue = this->verticalScrollBar()->value();
        int const newValue = oldValue - ydeltaSign * numberOfLinesToMove;
        this->verticalScrollBar()->setValue(newValue);
    }
}

void QHexView::keyPressEvent(QKeyEvent* e) {
    bool handled = false;

    if(this->hexCursor()) {
        handled = this->keyPressMove(e);
        if(!handled)
            handled = this->keyPressAction(e);
        if(!handled)
            handled = this->keyPressTextInput(e);
    }

    if(handled)
        e->accept();
    else
        QAbstractScrollArea::keyPressEvent(e);
}

QString QHexView::reduced(const QString& s, int maxlen) {
    if(s.length() <= maxlen)
        return s.leftJustified(maxlen);
    return s.mid(0, maxlen - 1) + "\u2026";
}

bool QHexView::isColorLight(QColor c) {
    return std::sqrt(0.299 * std::pow(c.red(), 2) +
                     0.587 * std::pow(c.green(), 2) +
                     0.114 * std::pow(c.blue(), 2)) > 127.5;
}

QColor QHexView::getReadableColor(QColor c) const {
    QPalette palette = this->palette();
    return QHexView::isColorLight(c)
               ? palette.color(QPalette::Normal, QPalette::WindowText)
               : palette.color(QPalette::Normal, QPalette::HighlightedText);
}

QByteArray QHexView::selectedBytes() const {
    return m_hexcursor->hasSelection()
               ? m_hexdocument->read(m_hexcursor->selectionStartOffset(),
                                     m_hexcursor->selectionLength())
               : QByteArray{};
}

QByteArray QHexView::visibleBytes() const {
    auto line = static_cast<quint64>(this->verticalScrollBar()->value());
    int nbytes = m_options.line_length * this->visibleLines();

    return m_hexdocument
               ? m_hexdocument->read(line * m_options.line_length, nbytes)
               : QByteArray{};
}

QByteArray QHexView::getLine(qint64 line) const {
    return m_hexdocument ? m_hexdocument->read(line * m_options.line_length,
                                               m_options.line_length)
                         : QByteArray{};
}
