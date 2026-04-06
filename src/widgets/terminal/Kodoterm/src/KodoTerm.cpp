// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "KodoTerm/KodoTerm.hpp"
#include "PtyProcess.h"

#include <vterm.h>

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDataStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>
#include <QUrl>
#include <algorithm>
#include <cstring>

static void vterm_output_callback(const char *s, size_t len, void *user) {
    auto *pty = static_cast<PtyProcess *>(user);
    if (pty) {
        pty->write(QByteArray(s, (int)len));
    }
}

static VTermColor toVTermColor(const QColor &c) {
    VTermColor vc;
    vc.type = VTERM_COLOR_RGB;
    vc.rgb.red = c.red();
    vc.rgb.green = c.green();
    vc.rgb.blue = c.blue();
    return vc;
}

void KodoTerm::setConfig(const KodoTermConfig &config) {
    m_config = config;
    setFont(m_config.font);
    setTheme(m_config.theme);

    // Force a full redraw by resetting cell size and calling updateTerminalSize
    m_cellSize = QSize(0, 0);
    updateTerminalSize();
}

void KodoTerm::setTheme(const TerminalTheme &theme) {
    m_config.theme = theme;
    VTermState *state = vterm_obtain_state(m_vterm);
    VTermColor fg = toVTermColor(theme.foreground), bg = toVTermColor(theme.background);
    vterm_state_set_default_colors(state, &fg, &bg);
    for (int i = 0; i < 16; ++i) {
        VTermColor c = toVTermColor(theme.palette[i]);
        vterm_state_set_palette_color(state, i, &c);
    }
    for (int i = 0; i < 256; ++i) {
        m_paletteCacheValid[i] = false;
    }
    for (auto &cell : m_cellCache) {
        cell.chars[0] = (uint32_t)-1;
    }
    damageAll();
}

KodoTerm::KodoTerm(QWidget *parent) : QWidget(parent) {
    m_restoring = false;
    for (int i = 0; i < 256; ++i) {
        m_paletteCacheValid[i] = false;
    }
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    memset(&m_lastVTermFg, 0, sizeof(VTermColor));
    memset(&m_lastVTermBg, 0, sizeof(VTermColor));
    m_config.font.setStyleHint(QFont::Monospace);
    setFocusPolicy(Qt::StrongFocus);
    m_scrollBar = new QScrollBar(Qt::Vertical, this);
    m_scrollBar->setRange(0, 0);
    connect(m_scrollBar, &QScrollBar::valueChanged, this, &KodoTerm::onScrollValueChanged);
    setMouseTracking(true);
    updateTerminalSize();
    m_cursorBlinkTimer = new QTimer(this);
    m_cursorBlinkTimer->setInterval(500);
    m_restorationBannerTimer = new QTimer(this);
    m_restorationBannerTimer->setSingleShot(true);
    m_restorationBannerTimer->setInterval(3000);
    connect(m_restorationBannerTimer, &QTimer::timeout, this, [this]() {
        m_restorationBannerActive = false;
        update();
    });
    connect(m_cursorBlinkTimer, &QTimer::timeout, this, [this]() {
        if (m_cursorBlink) {
            m_cursorBlinkState = !m_cursorBlinkState;
            QRect r(m_cursorCol * m_cellSize.width(), m_cursorRow * m_cellSize.height(),
                    m_cellSize.width(), m_cellSize.height());
            update(r);
        }
    });
    m_vterm = vterm_new(25, 80);
    if (!m_vterm) {
        return;
    }
    vterm_set_utf8(m_vterm, 1);
    m_vtermScreen = vterm_obtain_screen(m_vterm);
    if (!m_vtermScreen) {
        return;
    }
    vterm_screen_enable_altscreen(m_vtermScreen, 1);
    static VTermScreenCallbacks callbacks = {.damage = &KodoTerm::onDamage,
                                             .moverect = &KodoTerm::onMoveRect,
                                             .movecursor = &KodoTerm::onMoveCursor,
                                             .settermprop = &KodoTerm::onSetTermProp,
                                             .bell = &KodoTerm::onBell,
                                             .resize = nullptr,
                                             .sb_pushline = &KodoTerm::onSbPushLine,
                                             .sb_popline = &KodoTerm::onSbPopLine,
                                             .sb_clear = nullptr,
                                             .sb_pushline4 = nullptr};
    vterm_screen_set_callbacks(m_vtermScreen, &callbacks, this);
    vterm_screen_reset(m_vtermScreen, 1);
    if (!m_environment.contains("TERM")) {
        m_environment.insert("TERM", "xterm-256color");
    }
    if (!m_environment.contains("COLORTERM")) {
        m_environment.insert("COLORTERM", "truecolor");
    }
    setTheme(m_config.theme);
    resetDirtyRect();
    VTermState *state = vterm_obtain_state(m_vterm);
    static VTermStateFallbacks fallbacks = {.control = nullptr,
                                            .csi = nullptr,
                                            .osc = &KodoTerm::onOsc,
                                            .dcs = nullptr,
                                            .apc = nullptr,
                                            .pm = nullptr,
                                            .sos = nullptr};
    vterm_state_set_unrecognised_fallbacks(state, &fallbacks, this);
}

KodoTerm::~KodoTerm() {
    if (m_replayFile) {
        m_replayFile->close();
        delete m_replayFile;
    }
    if (m_pty) {
        m_pty->kill();
    }
    if (m_vterm) {
        vterm_free(m_vterm);
    }
}

bool KodoTerm::start(bool reset) {
    if (m_pty) {
        m_pty->kill();
        delete m_pty;
        m_pty = nullptr;
    }
    if (reset) {
        resetTerminal();
    }
    setupPty();
    if (m_program.isEmpty()) {
        return false;
    }
    m_pty->setProgram(m_program);
    m_pty->setArguments(m_arguments);
    m_pty->setWorkingDirectory(m_workingDirectory);
    m_pty->setProcessEnvironment(m_environment);
    if (m_config.enableLogging) {
        QDir logDir(m_config.logDirectory);
        if (!logDir.exists()) {
            logDir.mkpath(".");
        }
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
        m_logFile.setFileName(logDir.filePath(QString("kodoterm_%1.log").arg(timestamp)));
        if (m_logFile.open(QIODevice::WriteOnly)) {
            QString h = QString("-- KodoTerm Session Log ---\nProgram: %1\nArguments: %2\nCWD: "
                                "%3\nLOG_START_MARKER\n")
                            .arg(m_program)
                            .arg(m_arguments.join(" "))
                            .arg(m_workingDirectory);
            m_logFile.write(h.toUtf8());
            m_logFile.flush();
        }
    }
    int rows, cols;
    vterm_get_size(m_vterm, &rows, &cols);
    if (cols <= 0) cols = 80; // Дефолт
    if (rows <= 0) rows = 24;
    updateTerminalSize();
    return m_pty->start(m_program, m_arguments, QSize(cols, rows));
}

void KodoTerm::setupPty() {
    if (m_pty) {
        return;
    }
    m_pty = PtyProcess::create(this);
    if (!m_pty) {
        return;
    }
    connect(m_pty, &PtyProcess::readyRead, this, &KodoTerm::onPtyReadyRead);
    connect(m_pty, &PtyProcess::finished, this, &KodoTerm::finished);
    vterm_output_set_callback(m_vterm, vterm_output_callback, m_pty);
}

void KodoTerm::flushTerminal() {
    if (m_vtermScreen) {
        vterm_screen_flush_damage(m_vtermScreen);
    }
}
void KodoTerm::onPtyReadyRead(const QByteArray &data) {
    if (!data.isEmpty()) {
        if (m_logFile.isOpen()) {
            m_logFile.write(data);
            m_logFile.flush();
        }
        vterm_input_write(m_vterm, data.constData(), data.size());
        flushTerminal();
    }
}
void KodoTerm::onScrollValueChanged(int value) {
    if (m_vterm && !m_backBuffer.isNull()) {
        VTermState *state = vterm_obtain_state(m_vterm);
        VTermColor dfg, dbg;
        vterm_state_get_default_colors(state, &dfg, &dbg);
        m_backBuffer.fill(mapColor(dbg, state));
        for (auto &cell : m_cellCache) {
            cell.chars[0] = (uint32_t)-1;
        }
    }
    damageAll();
}
void KodoTerm::scrollUp(int lines) { m_scrollBar->setValue(m_scrollBar->value() - lines); }
void KodoTerm::scrollDown(int lines) { m_scrollBar->setValue(m_scrollBar->value() + lines); }
void KodoTerm::pageUp() { scrollUp(m_scrollBar->pageStep()); }
void KodoTerm::pageDown() { scrollDown(m_scrollBar->pageStep()); }
int KodoTerm::onSbPushLine(int cols, const VTermScreenCell *cells, void *user) {
    return static_cast<KodoTerm *>(user)->pushScrollback(cols, cells);
}
int KodoTerm::onSbPopLine(int cols, VTermScreenCell *cells, void *user) {
    return static_cast<KodoTerm *>(user)->popScrollback(cols, cells);
}

int KodoTerm::onOsc(int command, VTermStringFragment frag, void *user) {
    auto *w = static_cast<KodoTerm *>(user);
    if (frag.initial) {
        w->m_oscBuffer.clear();
    }
    w->m_oscBuffer.append(frag.str, frag.len);
    if (frag.final && command == 7) {
        QString s = QString::fromUtf8(w->m_oscBuffer);
        while (!s.isEmpty() &&
               (s.endsWith(';') || s.endsWith('') || s.endsWith('\n') || s.endsWith(' '))) {
            s.chop(1);
        }
        if (s.startsWith("file://")) {
            QUrl u(s);
            QString p = u.toLocalFile();
            if (p.isEmpty() || (p.startsWith("//") && !u.host().isEmpty())) {
                p = u.path();
            }
            if (!p.isEmpty() && w->m_cwd != p) {
                w->m_cwd = p;
                emit w->cwdChanged(p);
            }
        } else if (!s.isEmpty() && w->m_cwd != s) {
            w->m_cwd = s;
            emit w->cwdChanged(s);
        }
    }
    return 1;
}

int KodoTerm::pushScrollback(int cols, const VTermScreenCell *cells) {
    if (m_altScreen) {
        return 0;
    }
    SavedLine line;
    line.reserve(cols);
    for (int i = 0; i < cols; ++i) {
        SavedCell sc;
        memcpy(sc.chars, cells[i].chars, sizeof(sc.chars));
        sc.attrs = cells[i].attrs;
        sc.fg = cells[i].fg;
        sc.bg = cells[i].bg;
        sc.width = cells[i].width;
        line.push_back(sc);
    }
    m_scrollback.push_back(std::move(line));
    if ((int)m_scrollback.size() > m_config.maxScrollback) {
        m_scrollback.pop_front();
    }
    bool bottom = m_scrollBar->value() == m_scrollBar->maximum();
    m_scrollBar->setRange(0, (int)m_scrollback.size());
    if (bottom) {
        m_scrollBar->setValue(m_scrollBar->maximum());
    }
    return 1;
}

int KodoTerm::popScrollback(int cols, VTermScreenCell *cells) {
    if (m_scrollback.empty()) {
        return 0;
    }
    const SavedLine &line = m_scrollback.back();
    int n = std::min(cols, (int)line.size());
    for (int i = 0; i < n; ++i) {
        memcpy(cells[i].chars, line[i].chars, sizeof(cells[i].chars));
        cells[i].attrs = line[i].attrs;
        cells[i].fg = line[i].fg;
        cells[i].bg = line[i].bg;
        cells[i].width = line[i].width;
    }
    for (int i = n; i < cols; ++i) {
        memset(&cells[i], 0, sizeof(VTermScreenCell));
        cells[i].width = 1;
    }
    m_scrollback.pop_back();
    m_scrollBar->setRange(0, (int)m_scrollback.size());
    return 1;
}

void KodoTerm::updateTerminalSize() {
    QFontMetrics fm(m_config.font);
    QSize oldCellSize = m_cellSize;
    m_cellSize = QSize(fm.horizontalAdvance(QChar(' ')), fm.height());
    if (m_cellSize.width() <= 0 || m_cellSize.height() <= 0) {
        m_cellSize = QSize(10, 20);
    }
    int rows = height() / m_cellSize.height(),
        sb = m_scrollBar->isVisible() ? m_scrollBar->sizeHint().width() : 0,
        cols = (width() - sb) / m_cellSize.width();
    if (cols < 80 && !m_pty) { 
        cols = 80; 
    }
    if (rows < 24 && !m_pty) {
        rows = 24;
    }
    int orows, ocols;
    if (m_vterm) {
        vterm_get_size(m_vterm, &orows, &ocols);
        if (rows == orows && cols == ocols && m_cellSize == oldCellSize &&
            m_pendingLogReplay.isEmpty()) {
            return;
        }
        vterm_set_size(m_vterm, rows, cols);
        vterm_screen_flush_damage(m_vtermScreen);
        qreal dpr = devicePixelRatioF();
        QSize pixelSize = QSize(std::ceil(cols * m_cellSize.width() * dpr), 
                        std::ceil(rows * m_cellSize.height() * dpr));
        m_backBuffer = QImage(pixelSize, QImage::Format_RGB32);
        m_backBuffer.setDevicePixelRatio(dpr);
        VTermState *state = vterm_obtain_state(m_vterm);
        VTermColor dfg, dbg;
        vterm_state_get_default_colors(state, &dfg, &dbg);
        m_backBuffer.fill(mapColor(dbg, state));
        m_cellCache.clear();
        m_cellCache.assign(rows * cols, VTermScreenCell{});
        for (auto &cell : m_cellCache) {
            cell.chars[0] = (uint32_t)-1; // Флаг пустой клетки
        }
        
        m_selectedCache.clear();
        m_selectedCache.assign(rows * cols, false);
        m_scrollBar->setPageStep(rows);
        if (m_scrollBar->value() == m_scrollBar->maximum()) {
            m_scrollBar->setValue(m_scrollBar->maximum());
        }
        if (!m_pendingLogReplay.isEmpty() && cols > 40) {
            QTimer::singleShot(100, this, &KodoTerm::processLogReplay);
        }
    }
    if (m_pty) {
        m_pty->resize(QSize(cols, rows));
    }
    m_dirty = true; 
    damageAll(); 
    update();
}

void KodoTerm::processLogReplay() {
    if (m_pendingLogReplay.isEmpty() && !m_replayFile) {
        return;
    }

    if (!m_replayFile) {
        m_restoring = true;
        m_restorationBannerActive = true;
        m_restorationBannerText = tr("Restoring session...");

        // Thorough reset
        vterm_screen_reset(m_vtermScreen, 1);
        m_scrollback.clear();
        m_scrollBar->setRange(0, 0);
        m_scrollBar->setValue(0);

        VTermState *state = vterm_obtain_state(m_vterm);
        VTermColor dfg, dbg;
        vterm_state_get_default_colors(state, &dfg, &dbg);
        if (!m_backBuffer.isNull()) {
            m_backBuffer.fill(mapColor(dbg, state));
        }

        for (auto &c : m_cellCache) {
            c.chars[0] = (uint32_t)-1;
        }
        m_selectedCache.assign(m_selectedCache.size(), false);

        update();
        m_replayFile = new QFile(m_pendingLogReplay);
        m_pendingLogReplay.clear();
        if (!m_replayFile->open(QIODevice::ReadOnly)) {
            delete m_replayFile;
            m_replayFile = nullptr;
            m_restoring = false;
            m_restorationBannerActive = false;
            return;
        }
        // Read header
        QByteArray header;
        while (!m_replayFile->atEnd()) {
            char c;
            if (m_replayFile->getChar(&c)) {
                header.append(c);
                if (c == '\n' && header.endsWith("LOG_START_MARKER\n")) {
                    break;
                }
                if (header.size() > 1024) { // Header too big or invalid
                    break;
                }
            } else {
                break;
            }
        }
    }

    if (m_replayFile) {
        QByteArray chunk = m_replayFile->read(65536); // 64KB
        if (!chunk.isEmpty()) {
            onPtyReadyRead(chunk);
            QTimer::singleShot(0, this, &KodoTerm::processLogReplay);
        } else {
            m_replayFile->close();
            delete m_replayFile;
            m_replayFile = nullptr;
            onPtyReadyRead("\r\n");
            scrollToBottom();
            m_restoring = false;
            m_restorationBannerTimer->start();
            damageAll();
            update();
        }
    }
}

void KodoTerm::resetDirtyRect() {
    m_dirtyRect.start_row = 10000;
    m_dirtyRect.start_col = 10000;
    m_dirtyRect.end_row = -1;
    m_dirtyRect.end_col = -1;
}
void KodoTerm::damageAll() {
    int r, c;
    if (!m_vterm) {
        return;
    }
    vterm_get_size(m_vterm, &r, &c);
    m_dirtyRect.start_row = 0;
    m_dirtyRect.start_col = 0;
    m_dirtyRect.end_row = r;
    m_dirtyRect.end_col = c;
    m_dirty = true;
    if (!m_restoring) {
        update();
    }
}

void KodoTerm::drawRestorationBanner(QPainter &painter) {
    if (m_restorationBannerText.isEmpty()) {
        return;
    }

    QFont f = font();
    f.setBold(true);
    f.setPointSize(f.pointSize() * 1.5);
    painter.setFont(f);

    QFontMetrics fm(f);
    int padding = 20;
    QRect textRect = fm.boundingRect(m_restorationBannerText);
    QRect bannerRect(0, 0, textRect.width() + padding * 2, textRect.height() + padding);
    bannerRect.moveCenter(rect().center());

    painter.setRenderHint(QPainter::Antialiasing, m_config.textAntialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing, m_config.textAntialiasing);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(bannerRect, 10, 10);

    painter.setPen(Qt::white);
    painter.drawText(bannerRect, Qt::AlignCenter, m_restorationBannerText);
}

static bool colorsEqual(const VTermColor &a, const VTermColor &b) {
    if (a.type != b.type) {
        return false;
    }
    if (a.type == VTERM_COLOR_RGB) {
        return a.rgb.red == b.rgb.red && a.rgb.green == b.rgb.green && a.rgb.blue == b.rgb.blue;
    }
    if (a.type == VTERM_COLOR_INDEXED) {
        return a.indexed.idx == b.indexed.idx;
    }
    return true;
}

static bool cellsEqual(const VTermScreenCell &a, const VTermScreenCell &b) {
    if (a.width != b.width) {
        return false;
    }
    if (memcmp(&a.attrs, &b.attrs, sizeof(VTermScreenCellAttrs)) != 0) {
        return false;
    }
    if (!colorsEqual(a.fg, b.fg) || !colorsEqual(a.bg, b.bg)) {
        return false;
    }
    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL; ++i) {
        if (a.chars[i] != b.chars[i]) {
            return false;
        }
        if (a.chars[i] == 0) {
            break;
        }
    }
    return true;
}

static bool isBoxChar(uint32_t c) { return (c >= 0x2500 && c <= 0x257F); }

static void drawBoxChar(QPainter &p, const QRect &r, uint32_t c, const QColor &fg) {
    enum { L = 1, R = 2, U = 4, D = 8, H = 16, Db = 32 };
    int f = 0;
    switch (c) {
    case 0x2500:
        f = L | R;
        break;
    case 0x2501:
        f = L | R | H;
        break;
    case 0x2502:
        f = U | D;
        break;
    case 0x2503:
        f = U | D | H;
        break;
    case 0x250C:
        f = D | R;
        break;
    case 0x250F:
        f = D | R | H;
        break;
    case 0x2510:
        f = D | L;
        break;
    case 0x2513:
        f = D | L | H;
        break;
    case 0x2514:
        f = U | R;
        break;
    case 0x2517:
        f = U | R | H;
        break;
    case 0x2518:
        f = U | L;
        break;
    case 0x251B:
        f = U | L | H;
        break;
    case 0x251C:
        f = U | D | R;
        break;
    case 0x2523:
        f = U | D | R | H;
        break;
    case 0x2524:
        f = U | D | L;
        break;
    case 0x252B:
        f = U | D | L | H;
        break;
    case 0x252C:
        f = D | L | R;
        break;
    case 0x2533:
        f = D | L | R | H;
        break;
    case 0x2534:
        f = U | L | R;
        break;
    case 0x253B:
        f = U | L | R | H;
        break;
    case 0x253C:
        f = U | D | L | R;
        break;
    case 0x254B:
        f = U | D | L | R | H;
        break;
    case 0x2550:
        f = L | R | Db;
        break;
    case 0x2551:
        f = U | D | Db;
        break;
    case 0x2554:
        f = D | R | Db;
        break;
    case 0x2557:
        f = D | L | Db;
        break;
    case 0x255A:
        f = U | R | Db;
        break;
    case 0x255D:
        f = U | L | Db;
        break;
    case 0x2560:
        f = U | D | R | Db;
        break;
    case 0x2563:
        f = U | D | L | Db;
        break;
    case 0x2566:
        f = D | L | R | Db;
        break;
    case 0x2569:
        f = U | L | R | Db;
        break;
    case 0x256C:
        f = U | D | L | R | Db;
        break;
    }

    if (f == 0) {
        p.setPen(fg);
        p.drawText(r, Qt::AlignCenter, QString::fromUcs4((const char32_t *)&c, 1));
        return;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(fg);
    int cx = r.center().x();
    int cy = r.center().y();
    int t = (f & H) ? 2 : 1;

    if (f & Db) {
        int g = 1; // gap
        if (f & U) {
            p.drawRect(cx - 1, r.top(), 1, cy - r.top() + g);
            p.drawRect(cx + 1, r.top(), 1, cy - r.top() + g);
        }
        if (f & D) {
            p.drawRect(cx - 1, cy - g, 1, r.bottom() - cy + 1 + g);
            p.drawRect(cx + 1, cy - g, 1, r.bottom() - cy + 1 + g);
        }
        if (f & L) {
            p.drawRect(r.left(), cy - 1, cx - r.left() + g, 1);
            p.drawRect(r.left(), cy + 1, cx - r.left() + g, 1);
        }
        if (f & R) {
            p.drawRect(cx - g, cy - 1, r.right() - cx + 1 + g, 1);
            p.drawRect(cx - g, cy + 1, r.right() - cx + 1 + g, 1);
        }
    } else {
        if (f & U) {
            p.drawRect(cx, r.top(), t, cy - r.top() + t);
        }
        if (f & D) {
            p.drawRect(cx, cy, t, r.bottom() - cy + 1);
        }
        if (f & L) {
            p.drawRect(r.left(), cy, cx - r.left() + t, t);
        }
        if (f & R) {
            p.drawRect(cx, cy, r.right() - cx + 1, t);
        }
    }
}

void KodoTerm::renderToBackbuffer() {
    if (m_backBuffer.isNull()) {
        return;
    }
    if (!m_dirty && m_scrollBar->value() == (int)m_scrollback.size()) {
        return;
    }
    int rows, cols;
    vterm_get_size(m_vterm, &rows, &cols);
    int cur = m_scrollBar->value(), sb = (int)m_scrollback.size();
    if (cur > sb) {
        cur = sb;
        m_scrollBar->setValue(sb);
    }   
    bool useCache = (cur == sb);
    if (useCache && (m_dirtyRect.start_row > m_dirtyRect.end_row)) {
        m_dirty = false;
        return;
    }

    QPainter painter(&m_backBuffer);
    QFont f = font();
    f.setKerning(false);
    f.setStyleStrategy(m_config.textAntialiasing ? QFont::PreferAntialias : QFont::NoAntialias);
    painter.setFont(f);
    painter.setRenderHint(QPainter::TextAntialiasing, m_config.textAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing,
                          false); // Always false for cell backgrounds to prevent gaps

    VTermState *state = vterm_obtain_state(m_vterm);
    VTermColor dfg, dbg;
    vterm_state_get_default_colors(state, &dfg, &dbg);
    QColor defBg = mapColor(dbg, state), defFg = mapColor(dfg, state);

    VTermPos sS = m_selectionStart, sE = m_selectionEnd;
    bool hasS = (sS.row != -1);
    if (hasS && (sS.row > sE.row || (sS.row == sE.row && sS.col > sE.col))) {
        std::swap(sS, sE);
    }
    int sR = 0, eR = rows, sC = 0, eC = cols;
    if (useCache) {
        sR = std::max(0, m_dirtyRect.start_row);
        eR = std::min(rows, m_dirtyRect.end_row);
        sC = std::max(0, m_dirtyRect.start_col);
        eC = std::min(cols, m_dirtyRect.end_col);
    }
    for (int r = sR; r < eR; ++r) {
    int absR = cur + r; // Это строка, которую мы хотим видеть на экране виджета
    for (int c = sC; c < eC; ++c) {
        VTermScreenCell cell;
        
        if (absR < sb) {
            // Рисуем из истории (scrollback)
            const SavedLine &l = m_scrollback[absR];
            if (c < (int)l.size()) {
                const SavedCell &sc = l[c];
                memcpy(cell.chars, sc.chars, sizeof(cell.chars));
                cell.attrs = sc.attrs;
                cell.fg = sc.fg;
                cell.bg = sc.bg;
                cell.width = sc.width;
            } else {
                memset(&cell, 0, sizeof(cell));
                cell.bg = dbg; // Используй default background из vterm
                cell.width = 1;
            }
        } else {
            // ФИКС ТУТ: вычисляем правильный индекс строки внутри текущего экрана vterm
            // Если история (sb) = 100 строк, а мы на 105-й строке виджета, 
            // значит нам нужна 5-я строка экрана vterm.
            int vtermRow = absR - sb; 
            
            // Защита от выхода за границы, если vterm еще не успел обновиться
            if (vtermRow >= 0 && vtermRow < rows) {
                vterm_screen_get_cell(m_vtermScreen, {vtermRow, c}, &cell);
            } else {
                memset(&cell, 0, sizeof(cell));
                cell.bg = dbg; // <--- Вот это спасет от черных кусков в neofetch
                cell.width = 1;
            }
        }
        
        if (cell.width == 0) continue;
            bool sel = false;
            if (hasS) {
                if (absR > sS.row && absR < sE.row) {
                    sel = true;
                } else if (absR == sS.row && absR == sE.row) {
                    sel = (c >= sS.col && c <= sE.col);
                } else if (absR == sS.row) {
                    sel = (c >= sS.col);
                } else if (absR == sE.row) {
                    sel = (c <= sE.col);
                }
            }
            if (useCache && sel == m_selectedCache[r * cols + c] &&
                cellsEqual(cell, m_cellCache[r * cols + c])) {
                if (cell.width > 1) {
                    c += (cell.width - 1);
                }
                continue;
            }
            if (useCache) {
                m_cellCache[r * cols + c] = cell;
                m_selectedCache[r * cols + c] = sel;
            }
            QColor fg = defFg, bg = defBg;
            if (!VTERM_COLOR_IS_DEFAULT_FG(&cell.fg)) {
                fg = mapColor(cell.fg, state);
            }
            if (!VTERM_COLOR_IS_DEFAULT_BG(&cell.bg)) {
                bg = mapColor(cell.bg, state);
            }
            if (cell.attrs.reverse ^ sel) {
                std::swap(fg, bg);
            }

            QRectF rect(c * m_cellSize.width(), r * m_cellSize.height(),
                        cell.width * m_cellSize.width(), m_cellSize.height());
            painter.fillRect(rect, bg);

            if (m_config.customBoxDrawing && isBoxChar(cell.chars[0])) {
                drawBoxChar(painter, rect.toRect(), cell.chars[0], fg);
            } else if (cell.chars[0] != 0) {
                int n_chars = 0;
                while (n_chars < VTERM_MAX_CHARS_PER_CELL && cell.chars[n_chars]) {
                    n_chars++;
                }
                painter.setPen(fg);
                painter.drawText(rect, Qt::AlignCenter,
                                 QString::fromUcs4((const char32_t *)cell.chars, n_chars));
            }

            if (cell.width > 1) {
                c += (cell.width - 1);
            }
        }
    }
    resetDirtyRect();
    m_dirty = false;
}

int KodoTerm::onDamage(VTermRect r, void *u) {
    auto *w = static_cast<KodoTerm *>(u);
    if (!w->m_pendingLogReplay.isEmpty()) {
        return 1;
    }
    w->m_dirtyRect.start_row = std::min(w->m_dirtyRect.start_row, r.start_row);
    w->m_dirtyRect.start_col = std::min(w->m_dirtyRect.start_col, r.start_col);
    w->m_dirtyRect.end_row = std::max(w->m_dirtyRect.end_row, r.end_row);
    w->m_dirtyRect.end_col = std::max(w->m_dirtyRect.end_col, r.end_col);
    w->m_dirty = true;
    if (!w->m_restoring) {
        w->update();
    }
    return 1;
}

int KodoTerm::onMoveRect(VTermRect d, VTermRect s, void *u) {
    auto *w = static_cast<KodoTerm *>(u);
    if (!w->m_pendingLogReplay.isEmpty()) {
        return 1;
    }

    int cols, rows;
    vterm_get_size(w->m_vterm, &rows, &cols);
    int h = s.end_row - s.start_row;
    if (d.start_row < s.start_row) {
        for (int r = 0; r < h; ++r) {
            int sr = s.start_row + r, dr = d.start_row + r;
            if (sr >= 0 && sr < rows && dr >= 0 && dr < rows) {
                std::copy(w->m_cellCache.begin() + sr * cols,
                          w->m_cellCache.begin() + sr * cols + cols,
                          w->m_cellCache.begin() + dr * cols);
                std::copy(w->m_selectedCache.begin() + sr * cols,
                          w->m_selectedCache.begin() + sr * cols + cols,
                          w->m_selectedCache.begin() + dr * cols);
            }
        }
    } else {
        for (int r = h - 1; r >= 0; --r) {
            int sr = s.start_row + r, dr = d.start_row + r;
            if (sr >= 0 && sr < rows && dr >= 0 && dr < rows) {
                std::copy(w->m_cellCache.begin() + sr * cols,
                          w->m_cellCache.begin() + sr * cols + cols,
                          w->m_cellCache.begin() + dr * cols);
                std::copy(w->m_selectedCache.begin() + sr * cols,
                          w->m_selectedCache.begin() + sr * cols + cols,
                          w->m_selectedCache.begin() + dr * cols);
            }
        }
    }

    w->m_dirtyRect.start_row = std::min({w->m_dirtyRect.start_row, d.start_row, s.start_row});
    w->m_dirtyRect.end_row = std::max({w->m_dirtyRect.end_row, d.end_row, s.end_row});
    w->m_dirtyRect.start_col = 0;
    w->m_dirtyRect.end_col = cols;

    w->m_dirty = true;
    if (!w->m_restoring) {
        w->update();
    }
    return 1;
}

int KodoTerm::onMoveCursor(VTermPos p, VTermPos op, int v, void *u) {
    auto *w = static_cast<KodoTerm *>(u);
    if (!w->m_pendingLogReplay.isEmpty()) {
        return 1;
    }
    w->m_cursorRow = p.row;
    w->m_cursorCol = p.col;
    w->m_cursorVisible = v;
    if (!w->m_restoring) {
        w->update();
    }
    return 1;
}

int KodoTerm::onSetTermProp(VTermProp p, VTermValue *v, void *u) {
    auto *w = static_cast<KodoTerm *>(u);
    if (!w->m_pendingLogReplay.isEmpty()) {
        return 1;
    }
    switch (p) {
    case VTERM_PROP_CURSORVISIBLE:
        w->m_cursorVisible = v->boolean;
        break;
    case VTERM_PROP_CURSORBLINK:
        w->m_cursorBlink = v->boolean;
        if (w->m_cursorBlink) {
            w->m_cursorBlinkTimer->start();
        } else {
            w->m_cursorBlinkTimer->stop();
            w->m_cursorBlinkState = true;
        }
        break;
    case VTERM_PROP_CURSORSHAPE:
        w->m_cursorShape = v->number;
        break;
    case VTERM_PROP_ALTSCREEN:
        w->m_altScreen = v->boolean;
        if (w->m_altScreen) {
            w->m_scrollBar->hide();
        } else {
            w->m_scrollBar->show();
        }
        w->updateTerminalSize();
        break;
    case VTERM_PROP_TITLE:
        w->setWindowTitle(QString::fromUtf8(v->string.str, (int)v->string.len));
        break;
    case VTERM_PROP_MOUSE:
        w->m_mouseMode = v->number;
        break;
    default:
        break;
    }
    if (!w->m_restoring) {
        w->damageAll();
    }
    return 1;
}

int KodoTerm::onBell(void *u) {
    auto *w = static_cast<KodoTerm *>(u);
    if (w->m_restoring || !w->m_pendingLogReplay.isEmpty()) {
        return 1;
    }
    if (w->m_config.audibleBell) {
        QApplication::beep();
    }
    if (w->m_config.visualBell) {
        w->m_visualBellActive = true;
        w->update();
        QTimer::singleShot(100, w, [w]() {
            w->m_visualBellActive = false;
            w->update();
        });
    }
    return 1;
}

void KodoTerm::resizeEvent(QResizeEvent *e) {
    int sb = m_scrollBar->sizeHint().width();
    m_scrollBar->setGeometry(width() - sb, 0, sb, height());
    updateTerminalSize();
    QWidget::resizeEvent(e);
}

void KodoTerm::wheelEvent(QWheelEvent *e) {
    if (m_config.mouseWheelZoom && (e->modifiers() & Qt::ControlModifier)) {
        if (e->angleDelta().y() > 0) {
            zoomIn();
        } else if (e->angleDelta().y() < 0) {
            zoomOut();
        }
        return;
    }
    if (m_mouseMode > 0 && !(e->modifiers() & Qt::ShiftModifier)) {
        VTermModifier m = VTERM_MOD_NONE;
        if (e->modifiers() & Qt::ShiftModifier) {
            m = (VTermModifier)(m | VTERM_MOD_SHIFT);
        }
        if (e->modifiers() & Qt::ControlModifier) {
            m = (VTermModifier)(m | VTERM_MOD_CTRL);
        }
        if (e->modifiers() & Qt::AltModifier) {
            m = (VTermModifier)(m | VTERM_MOD_ALT);
        }
        int r = e->position().toPoint().y() / m_cellSize.height(),
            c = e->position().toPoint().x() / m_cellSize.width(),
            b = e->angleDelta().y() > 0 ? 4 : 5;
        vterm_mouse_move(m_vterm, r, c, m);
        vterm_mouse_button(m_vterm, b, true, m);
        vterm_screen_flush_damage(m_vtermScreen);
        return;
    }
    m_scrollBar->event(e);
}

void KodoTerm::mousePressEvent(QMouseEvent *e) {
    VTermModifier m = VTERM_MOD_NONE;
    if (e->modifiers() & Qt::ShiftModifier) {
        m = (VTermModifier)(m | VTERM_MOD_SHIFT);
    }
    if (e->modifiers() & Qt::ControlModifier) {
        m = (VTermModifier)(m | VTERM_MOD_CTRL);
    }
    if (e->modifiers() & Qt::AltModifier) {
        m = (VTermModifier)(m | VTERM_MOD_ALT);
    }
    int r = e->pos().y() / m_cellSize.height(), c = e->pos().x() / m_cellSize.width();
    if (m_mouseMode > 0 && !(e->modifiers() & Qt::ShiftModifier)) {
        int b = 0;
        if (e->button() == Qt::LeftButton) {
            b = 1;
        } else if (e->button() == Qt::MiddleButton) {
            b = 2;
        } else if (e->button() == Qt::RightButton) {
            b = 3;
        }
        if (b > 0) {
            vterm_mouse_move(m_vterm, r, c, m);
            vterm_mouse_button(m_vterm, b, true, m);
            vterm_screen_flush_damage(m_vtermScreen);
            e->accept();
            return;
        }
    }
    if (e->button() == Qt::LeftButton) {
        if (!m_clickTimer.isValid() ||
            m_clickTimer.elapsed() > QApplication::doubleClickInterval() ||
            (e->pos() - m_lastClickPos).manhattanLength() > 5) {
            m_clickCount = 1;
        } else {
            m_clickCount++;
        }
        m_clickTimer.restart();
        m_lastClickPos = e->pos();
        VTermPos vp = mouseToPos(e->pos());
        if (m_clickCount == 3 && m_config.tripleClickSelectsLine) {
            int rows, cols;
            vterm_get_size(m_vterm, &rows, &cols);
            m_selectionStart = {vp.row, 0};
            m_selectionEnd = {vp.row, cols - 1};
            m_selecting = false;
            if (m_config.copyOnSelect) {
                copyToClipboard();
            }
            damageAll();
        } else if (m_clickCount == 1) {
            m_selecting = true;
            m_selectionStart = vp;
            m_selectionEnd = m_selectionStart;
            damageAll();
        }
        update();
    } else if (e->button() == Qt::MiddleButton && m_config.pasteOnMiddleClick) {
        pasteFromClipboard();
    }
    e->accept();
}

void KodoTerm::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton ||
        (m_mouseMode > 0 && !(e->modifiers() & Qt::ShiftModifier))) {
        return;
    }
    m_clickCount = 2;
    m_clickTimer.restart();
    m_lastClickPos = e->pos();
    m_selecting = false;
    VTermPos vp = mouseToPos(e->pos());
    int sb = (int)m_scrollback.size(), rows, cols;
    vterm_get_size(m_vterm, &rows, &cols);
    QString line;
    line.fill(' ', cols);
    int cc = vp.col;
    if (vp.row < sb) {
        const SavedLine &l = m_scrollback[vp.row];
        int n = std::min((int)l.size(), cols);
        for (int c = 0; c < n; ++c) {
            if (l[c].chars[0] != 0) {
                line[c] = QChar(static_cast<ushort>(l[c].chars[0] & 0xFFFF));
            }
        }
    } else {
        int vr = vp.row - sb;
        if (vr < rows) {
            for (int c = 0; c < cols; ++c) {
                VTermScreenCell cell;
                vterm_screen_get_cell(m_vtermScreen, {vr, c}, &cell);
                if (cell.chars[0] != 0) {
                    line[c] = QChar(static_cast<ushort>(cell.chars[0] & 0xFFFF));
                }
            }
        }
    }
    QRegularExpression re(m_config.wordSelectionRegex);
    if (re.isValid()) {
        QRegularExpressionMatchIterator it = re.globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (cc >= m.capturedStart() && cc < m.capturedEnd()) {
                m_selectionStart = {vp.row, static_cast<int>(m.capturedStart())};
                m_selectionEnd = {vp.row, static_cast<int>(m.capturedEnd() - 1)};
                if (m_config.copyOnSelect) {
                    copyToClipboard();
                }
                break;
            }
        }
    }
    damageAll();
    e->accept();
}

void KodoTerm::mouseMoveEvent(QMouseEvent *e) {
    VTermModifier m = VTERM_MOD_NONE;
    if (e->modifiers() & Qt::ShiftModifier) {
        m = (VTermModifier)(m | VTERM_MOD_SHIFT);
    }
    if (e->modifiers() & Qt::ControlModifier) {
        m = (VTermModifier)(m | VTERM_MOD_CTRL);
    }
    if (e->modifiers() & Qt::AltModifier) {
        m = (VTermModifier)(m | VTERM_MOD_ALT);
    }
    VTermPos vp = mouseToPos(e->pos());
    int r = e->pos().y() / m_cellSize.height(), c = e->pos().x() / m_cellSize.width();
    if (m_mouseMode > 0 && !(e->modifiers() & Qt::ShiftModifier)) {
        vterm_mouse_move(m_vterm, r, c, m);
        vterm_screen_flush_damage(m_vtermScreen);
        return;
    }
    if (m_selecting) {
        m_selectionEnd = vp;
        damageAll();
    }
    QWidget::mouseMoveEvent(e);
}

void KodoTerm::mouseReleaseEvent(QMouseEvent *e) {
    VTermModifier m = VTERM_MOD_NONE;
    if (e->modifiers() & Qt::ShiftModifier) {
        m = (VTermModifier)(m | VTERM_MOD_SHIFT);
    }
    if (e->modifiers() & Qt::ControlModifier) {
        m = (VTermModifier)(m | VTERM_MOD_CTRL);
    }
    if (e->modifiers() & Qt::AltModifier) {
        m = (VTermModifier)(m | VTERM_MOD_ALT);
    }
    int r = e->pos().y() / m_cellSize.height(), c = e->pos().x() / m_cellSize.width();
    if (m_mouseMode > 0 && !(e->modifiers() & Qt::ShiftModifier)) {
        int b = 0;
        if (e->button() == Qt::LeftButton) {
            b = 1;
        } else if (e->button() == Qt::MiddleButton) {
            b = 2;
        } else if (e->button() == Qt::RightButton) {
            b = 3;
        }
        if (b > 0) {
            vterm_mouse_move(m_vterm, r, c, m);
            vterm_mouse_button(m_vterm, b, false, m);
            vterm_screen_flush_damage(m_vtermScreen);
            e->accept();
            return;
        }
    }
    if (e->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        m_selectionEnd = mouseToPos(e->pos());
        if (m_selectionStart.row == m_selectionEnd.row &&
            m_selectionStart.col == m_selectionEnd.col) {
            m_selectionStart = {-1, -1};
            m_selectionEnd = {-1, -1};
        } else if (m_config.copyOnSelect) {
            copyToClipboard();
        }
        damageAll();
    }
    e->accept();
}

VTermPos KodoTerm::mouseToPos(const QPoint &p) const {
    if (m_cellSize.width() <= 0 || m_cellSize.height() <= 0) {
        return {0, 0};
    }
    
    // Вычитаем смещение перед расчетом колонки/строки
    int relativeX = p.x() - m_offsetX;
    int relativeY = p.y() - m_offsetY;

    int r = relativeY / m_cellSize.height();
    int c = relativeX / m_cellSize.width();
    
    int sb = (int)m_scrollback.size(), cur = m_scrollBar->value();
    
    VTermPos vp;
    vp.row = std::clamp(cur + r, 0, (int)(sb + 1000)); // защита от вылета
    vp.col = std::clamp(c, 0, 2000); 
    return vp;
}

bool KodoTerm::isSelected(int r, int c) const {
    if (m_selectionStart.row == -1) {
        return false;
    }
    VTermPos s = m_selectionStart, e = m_selectionEnd;
    if (s.row > e.row || (s.row == e.row && s.col > e.col)) {
        std::swap(s, e);
    }
    if (r < s.row || r > e.row) {
        return false;
    }
    if (r == s.row && r == e.row) {
        return c >= s.col && c <= e.col;
    }
    if (r == s.row) {
        return c >= s.col;
    }
    if (r == e.row) {
        return c <= e.col;
    }
    return true;
}

QString KodoTerm::getTextRange(VTermPos s, VTermPos e) {
    if (s.row > e.row || (s.row == e.row && s.col > e.col)) {
        std::swap(s, e);
    }
    QString t;
    int sb = (int)m_scrollback.size(), rs, cs;
    vterm_get_size(m_vterm, &rs, &cs);
    for (int r = s.row; r <= e.row; ++r) {
        int sc = (r == s.row) ? s.col : 0, ec = (r == e.row) ? e.col : 1000;
        if (r < sb) {
            const SavedLine &l = m_scrollback[r];
            for (int c = sc; c <= ec && c < (int)l.size(); ++c) {
                for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && l[c].chars[i]; ++i) {
                    t.append(QChar::fromUcs4(l[c].chars[i]));
                }
            }
        } else {
            int vr = r - sb;
            if (vr < rs) {
                for (int c = sc; c <= ec && c < cs; ++c) {
                    VTermScreenCell cell;
                    vterm_screen_get_cell(m_vtermScreen, {vr, c}, &cell);
                    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i) {
                        t.append(QChar::fromUcs4(cell.chars[i]));
                    }
                }
            }
        }
        if (r < e.row) {
            t.append('\n');
        }
    }
    return t;
}

void KodoTerm::copyToClipboard() {
    if (m_selectionStart.row != -1) {
        QApplication::clipboard()->setText(getTextRange(m_selectionStart, m_selectionEnd));
    }
}
void KodoTerm::pasteFromClipboard() {
    QString t = QApplication::clipboard()->text();
    if (!t.isEmpty()) {
        m_pty->write(t.toUtf8());
    }
}
void KodoTerm::selectAll() {
    int rs, cs;
    vterm_get_size(m_vterm, &rs, &cs);
    int sb = (int)m_scrollback.size();
    m_selectionStart = {0, 0};
    m_selectionEnd = {sb + rs - 1, cs - 1};
    damageAll();
}
void KodoTerm::clearScrollback() {
    m_scrollback.clear();
    m_scrollBar->setRange(0, 0);
    m_scrollBar->setValue(0);
    damageAll();
}
void KodoTerm::resetTerminal() {
    vterm_screen_reset(m_vtermScreen, 1);
    m_flowControlStopped = false;
    if (m_vterm && !m_backBuffer.isNull()) {
        VTermState *state = vterm_obtain_state(m_vterm);
        VTermColor dfg, dbg;
        vterm_state_get_default_colors(state, &dfg, &dbg);
        m_backBuffer.fill(mapColor(dbg, state));
    }
    clearScrollback();
    damageAll();
}
void KodoTerm::openFileBrowser() {
    if (!m_cwd.isEmpty()) {
        QDir d(m_cwd);
        if (d.exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(d.absolutePath()));
        }
    }
}
void KodoTerm::kill() {
    if (m_pty) {
        m_pty->kill();
    }
}
void KodoTerm::logData(const QByteArray &d) {
    if (m_logFile.isOpen()) {
        m_logFile.write(d);
        m_logFile.flush();
    }
}
void KodoTerm::scrollToBottom() {
    if (m_scrollBar) {
        m_scrollBar->setValue(m_scrollBar->maximum());
    }
}

void KodoTerm::contextMenuEvent(QContextMenuEvent *e) {
    if (m_mouseMode > 0 && !(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)) {
        return;
    }
    auto *m = new QMenu(this);
    auto *cA = m->addAction(tr("Copy"), this, &KodoTerm::copyToClipboard);
    cA->setEnabled(m_selectionStart.row != -1);
    cA->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    auto *pA = m->addAction(tr("Paste"), this, &KodoTerm::pasteFromClipboard);
    pA->setEnabled(!QApplication::clipboard()->text().isEmpty());
    pA->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    m->addSeparator();
    m->addAction(tr("Select All"), this, &KodoTerm::selectAll);
    m->addSeparator();
    m->addAction(tr("Clear Scrollback"), this, &KodoTerm::clearScrollback);
    m->addAction(tr("Reset"), this, &KodoTerm::resetTerminal);
    m->addSeparator();
    auto *oB = m->addAction(tr("Open current directory in file browser"), this,
                            &KodoTerm::openFileBrowser);
    oB->setEnabled(!m_cwd.isEmpty() && QDir(m_cwd).exists());
    m->addSeparator();
    m->addAction(tr("Zoom In"), this, &KodoTerm::zoomIn);
    m->addAction(tr("Zoom Out"), this, &KodoTerm::zoomOut);
    m->addAction(tr("Reset Zoom"), this, &KodoTerm::resetZoom);
    m->addSeparator();
    auto *tM = m->addMenu(tr("Themes"));
    auto tC = [this](const TerminalTheme::ThemeInfo &i) {
        setTheme(TerminalTheme::loadTheme(i.path));
    };
    populateThemeMenu(tM, tr("Konsole"), TerminalTheme::ThemeFormat::Konsole, tC);
    populateThemeMenu(tM, tr("Windows Terminal"), TerminalTheme::ThemeFormat::WindowsTerminal, tC);
    populateThemeMenu(tM, tr("iTerm"), TerminalTheme::ThemeFormat::ITerm, tC);
    emit contextMenuRequested(m, e->globalPos());
    m->exec(e->globalPos());
    delete m;
}

void KodoTerm::zoomIn() {
    qreal s = m_config.font.pointSizeF();
    if (s <= 0) {
        s = m_config.font.pointSize();
    }
    m_config.font.setPointSizeF(s + 1.0);
    m_config.font.setStyleStrategy(m_config.textAntialiasing ? QFont::PreferAntialias
                                                             : QFont::NoAntialias);
    setFont(m_config.font);
    updateTerminalSize();
    update();
}
void KodoTerm::zoomOut() {
    qreal s = m_config.font.pointSizeF();
    if (s <= 0) {
        s = m_config.font.pointSize();
    }
    if (s > 1.0) {
        m_config.font.setPointSizeF(s - 1.0);
        m_config.font.setStyleStrategy(m_config.textAntialiasing ? QFont::PreferAntialias
                                                                 : QFont::NoAntialias);
        setFont(m_config.font);
        updateTerminalSize();
        update();
    }
}
void KodoTerm::resetZoom() {
    m_config.font.setPointSize(10);
    m_config.font.setStyleStrategy(m_config.textAntialiasing ? QFont::PreferAntialias
                                                             : QFont::NoAntialias);
    setFont(m_config.font);
    updateTerminalSize();
    update();
}
QString KodoTerm::foregroundProcessName() const {
    return m_pty ? m_pty->foregroundProcessName() : QString();
}
bool KodoTerm::isRoot() const { return m_pty && m_pty->isRoot(); }

QColor KodoTerm::mapColor(const VTermColor &c, const VTermState *s) const {
    if (VTERM_COLOR_IS_RGB(&c)) {
        if (c.rgb.red == m_lastVTermFg.rgb.red && c.rgb.green == m_lastVTermFg.rgb.green &&
            c.rgb.blue == m_lastVTermFg.rgb.blue) {
            return m_lastFg;
        }
        if (c.rgb.red == m_lastVTermBg.rgb.red && c.rgb.green == m_lastVTermBg.rgb.green &&
            c.rgb.blue == m_lastVTermBg.rgb.blue) {
            return m_lastBg;
        }
        QColor col(c.rgb.red, c.rgb.green, c.rgb.blue);
        m_lastVTermBg = m_lastVTermFg;
        m_lastBg = m_lastFg;
        m_lastVTermFg = c;
        m_lastFg = col;
        return col;
    } else if (VTERM_COLOR_IS_INDEXED(&c)) {
        uint8_t i = c.indexed.idx;
        if (!m_paletteCacheValid[i]) {
            VTermColor rgb = c;
            vterm_state_convert_color_to_rgb(s, &rgb);
            m_paletteCache[i] = QColor(rgb.rgb.red, rgb.rgb.green, rgb.rgb.blue);
            m_paletteCacheValid[i] = true;
        }
        return m_paletteCache[i];
    }
    return Qt::white;
}

void KodoTerm::paintEvent(QPaintEvent *e) {
    if (!m_vterm || m_backBuffer.isNull()) return;

    // СНАЧАЛА рисуем текст в буфер (если что-то изменилось)
    renderToBackbuffer();

    QPainter painter(this);
    
    int rows, cols;
    vterm_get_size(m_vterm, &rows, &cols);

    // Центрирование
    int totalTextWidth = cols * m_cellSize.width();
    int totalTextHeight = rows * m_cellSize.height();
    int offsetX = std::max(0, (width() - totalTextWidth) / 2);
    int offsetY = std::max(0, (height() - totalTextHeight) / 2);
    
    m_offsetX = offsetX; 
    m_offsetY = offsetY;

    // Заливка фона виджета (поля)
    VTermState *state = vterm_obtain_state(m_vterm);
    VTermColor dfg, dbg;
    vterm_state_get_default_colors(state, &dfg, &dbg);
    painter.fillRect(rect(), mapColor(dbg, state));

    // Рисуем буфер с текстом
    if (!m_restoring) {
        // Указываем dpr, чтобы Qt не мылил картинку
        painter.drawImage(offsetX, offsetY, m_backBuffer);
    }

    // Рисуем курсор
    int sb = (int)m_scrollback.size(), cur = m_scrollBar->value();
    if (hasFocus() && m_cursorVisible && cur == sb && (!m_cursorBlink || m_cursorBlinkState)) {
        QRect r(offsetX + m_cursorCol * m_cellSize.width(), 
                offsetY + m_cursorRow * m_cellSize.height(),
                m_cellSize.width(), m_cellSize.height());
                
        painter.setCompositionMode(QPainter::CompositionMode_Difference);
        painter.fillRect(r, Qt::white);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    if (m_restorationBannerActive) {
        drawRestorationBanner(painter);
    }
}

void KodoTerm::keyPressEvent(QKeyEvent *e) {
    VTermModifier m = VTERM_MOD_NONE;
    if (e->modifiers() & Qt::ShiftModifier) {
        m = (VTermModifier)(m | VTERM_MOD_SHIFT);
    }
    if (e->modifiers() & Qt::ControlModifier) {
        m = (VTermModifier)(m | VTERM_MOD_CTRL);
    }
    if (e->modifiers() & Qt::AltModifier) {
        m = (VTermModifier)(m | VTERM_MOD_ALT);
    }
    int k = e->key();
    if (k >= Qt::Key_F1 && k <= Qt::Key_F12) {
        vterm_keyboard_key(m_vterm, (VTermKey)(VTERM_KEY_FUNCTION(1 + k - Qt::Key_F1)), m);
    } else {
        switch (k) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            vterm_keyboard_key(m_vterm, VTERM_KEY_ENTER, m);
            break;
        case Qt::Key_Backspace:
            vterm_keyboard_key(m_vterm, VTERM_KEY_BACKSPACE, m);
            break;
        case Qt::Key_Tab:
            vterm_keyboard_key(m_vterm, VTERM_KEY_TAB, m);
            break;
        case Qt::Key_Escape:
            vterm_keyboard_key(m_vterm, VTERM_KEY_ESCAPE, m);
            break;
        case Qt::Key_Up:
            vterm_keyboard_key(m_vterm, VTERM_KEY_UP, m);
            break;
        case Qt::Key_Down:
            vterm_keyboard_key(m_vterm, VTERM_KEY_DOWN, m);
            break;
        case Qt::Key_Left:
            vterm_keyboard_key(m_vterm, VTERM_KEY_LEFT, m);
            break;
        case Qt::Key_Right:
            vterm_keyboard_key(m_vterm, VTERM_KEY_RIGHT, m);
            break;
        case Qt::Key_PageUp:
            if (e->modifiers() & Qt::ShiftModifier) {
                pageUp();
            } else {
                vterm_keyboard_key(m_vterm, VTERM_KEY_PAGEUP, m);
            }
            break;
        case Qt::Key_PageDown:
            if (e->modifiers() & Qt::ShiftModifier) {
                pageDown();
            } else {
                vterm_keyboard_key(m_vterm, VTERM_KEY_PAGEDOWN, m);
            }
            break;
        case Qt::Key_Home:
            if (e->modifiers() & Qt::ShiftModifier) {
                m_scrollBar->setValue(m_scrollBar->minimum());
            } else {
                vterm_keyboard_key(m_vterm, VTERM_KEY_HOME, m);
            }
            break;
        case Qt::Key_End:
            if (e->modifiers() & Qt::ShiftModifier) {
                m_scrollBar->setValue(m_scrollBar->maximum());
            } else {
                vterm_keyboard_key(m_vterm, VTERM_KEY_END, m);
            }
            break;
        case Qt::Key_Insert:
            vterm_keyboard_key(m_vterm, VTERM_KEY_INS, m);
            break;
        case Qt::Key_Delete:
            vterm_keyboard_key(m_vterm, VTERM_KEY_DEL, m);
            break;
        default:
            if (e->modifiers() & Qt::ControlModifier) {
                if (k == Qt::Key_Plus || k == Qt::Key_Equal) {
                    zoomIn();
                    return;
                } else if (k == Qt::Key_Minus) {
                    zoomOut();
                    return;
                } else if (k == Qt::Key_0) {
                    resetZoom();
                    return;
                }
            }
            if ((e->modifiers() & Qt::ControlModifier) && (e->modifiers() & Qt::ShiftModifier)) {
                if (k == Qt::Key_C) {
                    copyToClipboard();
                    return;
                } else if (k == Qt::Key_V) {
                    pasteFromClipboard();
                    return;
                }
            }
            if ((m & VTERM_MOD_CTRL) && k >= Qt::Key_A && k <= Qt::Key_Z) {
                if (k == Qt::Key_S) {
                    m_flowControlStopped = true;
                    update();
                } else if (k == Qt::Key_Q) {
                    m_flowControlStopped = false;
                    update();
                }
                vterm_keyboard_unichar(m_vterm, k - Qt::Key_A + 1, VTERM_MOD_NONE);
            } else if (!e->text().isEmpty()) {
                for (const QChar &qc : e->text()) {
                    vterm_keyboard_unichar(m_vterm, qc.unicode(), m);
                }
            }
            break;
        }
    }
}

bool KodoTerm::focusNextPrevChild(bool n) { return false; }

void KodoTerm::focusInEvent(QFocusEvent *e) {
    QWidget::focusInEvent(e);
    m_cursorBlinkState = true;
    m_cursorBlinkTimer->start();
}
void KodoTerm::focusOutEvent(QFocusEvent *e) {
    QWidget::focusOutEvent(e);
    m_cursorBlinkTimer->stop();
}

void KodoTerm::populateThemeMenu(QMenu *pM, const QString &t, TerminalTheme::ThemeFormat f,
                                 const std::function<void(const TerminalTheme::ThemeInfo &)> &c) {
    QList<TerminalTheme::ThemeInfo> ths = TerminalTheme::builtInThemes();
    QList<TerminalTheme::ThemeInfo> fT;
    for (const auto &theme : ths) {
        if (theme.format == f) {
            fT.append(theme);
        }
    }
    if (fT.isEmpty()) {
        return;
    }
    QMenu *mT = pM->addMenu(t);
    auto aTA = [&](QMenu *m, const TerminalTheme::ThemeInfo &i) {
        m->addAction(i.name, [c, i]() { c(i); });
    };
    if (fT.size() < 26) {
        for (const auto &i : fT) {
            aTA(mT, i);
        }
    } else {
        QMap<QString, QMenu *> sM;
        for (const auto &i : fT) {
            QChar fLC = i.name.isEmpty() ? QChar('#') : i.name[0].toUpper();
            if (!fLC.isLetter()) {
                fLC = QChar('#');
            }
            QString fL(fLC);
            if (!sM.contains(fL)) {
                sM[fL] = mT->addMenu(fL);
            }
            aTA(sM[fL], i);
        }
    }
}

static QDataStream &operator<<(QDataStream &out, const VTermColor &c) {
    out << (quint8)c.type;
    if (c.type == VTERM_COLOR_RGB) {
        out << c.rgb.red << c.rgb.green << c.rgb.blue;
    } else if (c.type == VTERM_COLOR_INDEXED) {
        out << c.indexed.idx;
    }
    return out;
}

static QDataStream &operator>>(QDataStream &in, VTermColor &c) {
    quint8 t;
    in >> t;
    c.type = (VTermColorType)t;
    if (c.type == VTERM_COLOR_RGB) {
        in >> c.rgb.red >> c.rgb.green >> c.rgb.blue;
    } else if (c.type == VTERM_COLOR_INDEXED) {
        in >> c.indexed.idx;
    }
    return in;
}

void KodoTerm::saveState(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return;
    }
    QDataStream out(&f);
    out << (quint32)0x4B4F444F; // "KODO"
    out << (quint32)3;          // Version 3
    out << (quint32)m_cursorRow << (quint32)m_cursorCol;
    out << (quint32)m_scrollback.size();
    for (const auto &line : m_scrollback) {
        out << (quint32)line.size();
        if (!line.empty()) {
            out.writeRawData((const char *)line.data(), line.size() * sizeof(SavedCell));
        }
    }

    // Save current screen
    int rows, cols;
    vterm_get_size(m_vterm, &rows, &cols);
    out << (quint32)rows << (quint32)cols;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            VTermScreenCell cell;
            vterm_screen_get_cell(m_vtermScreen, {r, c}, &cell);
            // Save as SavedCell for consistency/speed?
            // VTermScreenCell and SavedCell are slightly different.
            // SavedCell has fixed char array. VTermScreenCell has uint32_t
            // chars[VTERM_MAX_CHARS_PER_CELL]. SavedCell definition: uint32_t
            // chars[VTERM_MAX_CHARS_PER_CELL]; ... They are structurally nearly identical. Let's
            // stick to safe member-wise save for screen (it's small, 25x80).
            out.writeRawData((const char *)cell.chars, sizeof(cell.chars));
            quint32 attrs = 0;
            memcpy(&attrs, &cell.attrs, std::min(sizeof(attrs), sizeof(cell.attrs)));
            out << attrs;
            out << cell.fg << cell.bg << (quint32)cell.width;
        }
    }
}

void KodoTerm::loadState(const QString &path) {
    m_restoring = true;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_restoring = false;
        return;
    }
    QDataStream in(&f);
    quint32 magic, ver;
    in >> magic >> ver;
    if (magic != 0x4B4F444F) {
        m_restoring = false;
        return;
    }

    quint32 cR = 0, cC = 0;
    if (ver >= 2) {
        in >> cR >> cC;
    }

    quint32 sbSize;
    in >> sbSize;
    m_scrollback.clear();
    // Fast path for Version 3+
    if (ver >= 3) {
        for (quint32 i = 0; i < sbSize; ++i) {
            quint32 lineSize;
            in >> lineSize;
            SavedLine line;
            line.resize(lineSize);
            if (lineSize > 0) {
                in.readRawData((char *)line.data(), lineSize * sizeof(SavedCell));
            }
            m_scrollback.push_back(std::move(line));
        }
    } else {
        // Slow path for older versions
        for (quint32 i = 0; i < sbSize; ++i) {
            quint32 lineSize;
            in >> lineSize;
            SavedLine line;
            line.resize(lineSize);
            for (quint32 j = 0; j < lineSize; ++j) {
                in.readRawData((char *)line[j].chars, sizeof(line[j].chars));
                quint32 attrs;
                in >> attrs;
                memcpy(&line[j].attrs, &attrs, std::min(sizeof(attrs), sizeof(line[j].attrs)));
                in >> line[j].fg >> line[j].bg;
                quint32 width;
                in >> width;
                line[j].width = width;
            }
            m_scrollback.push_back(std::move(line));
        }
    }

    // Load screen lines and replay to vterm
    quint32 rows, cols;
    in >> rows >> cols;
    QByteArray replayData;

    for (quint32 r = 0; r < rows; ++r) {
        for (quint32 c = 0; c < cols; ++c) {
            SavedCell cell;
            in.readRawData((char *)cell.chars, sizeof(cell.chars));
            quint32 attrs;
            in >> attrs;
            in >> cell.fg >> cell.bg;
            quint32 width;
            in >> width;

            // Replay Logic
            if (cell.chars[0] == 0) {
                replayData.append(' ');
            } else {
                if (cell.fg.type == VTERM_COLOR_RGB) {
                    replayData.append(QString("\033[38;2;%1;%2;%3m")
                                          .arg(cell.fg.rgb.red)
                                          .arg(cell.fg.rgb.green)
                                          .arg(cell.fg.rgb.blue)
                                          .toUtf8());
                }
                if (cell.bg.type == VTERM_COLOR_RGB) {
                    replayData.append(QString("\033[48;2;%1;%2;%3m")
                                          .arg(cell.bg.rgb.red)
                                          .arg(cell.bg.rgb.green)
                                          .arg(cell.bg.rgb.blue)
                                          .toUtf8());
                }
                int n = 0;
                while (n < VTERM_MAX_CHARS_PER_CELL && cell.chars[n]) {
                    n++;
                }
                if (n > 0) {
                    replayData.append(QString::fromUcs4((const char32_t *)cell.chars, n).toUtf8());
                }
            }
            if (width > 1) {
                c += (width - 1);
            }
        }
        replayData.append("\r\n");
    }
    replayData.append("\033[0m");

    if (ver >= 2) {
        replayData.append(QString("\033[%1;%2H").arg(cR + 1).arg(cC + 1).toUtf8());
    }

    vterm_input_write(m_vterm, replayData.constData(), replayData.size());

    m_scrollBar->setRange(0, (int)m_scrollback.size());
    m_scrollBar->setValue(m_scrollBar->maximum());
    damageAll();
    m_restoring = false;
    update();
}
