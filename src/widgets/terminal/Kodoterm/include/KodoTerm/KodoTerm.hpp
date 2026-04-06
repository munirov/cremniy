// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QMenu>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QSocketNotifier>
#include <QTimer>
#include <QWidget>
#include <deque>
#include <functional>
#include <vector>
#include <vterm.h>

#include "KodoTermConfig.hpp"

class PtyProcess;

class KodoTerm : public QWidget {
    Q_OBJECT

  public:
    explicit KodoTerm(QWidget *parent = nullptr);
    ~KodoTerm();

    static void
    populateThemeMenu(QMenu *parentMenu, const QString &title, TerminalTheme::ThemeFormat format,
                      const std::function<void(const TerminalTheme::ThemeInfo &)> &callback);

    void setTheme(const TerminalTheme &theme);
    void setConfig(const KodoTermConfig &config);
    KodoTermConfig getConfig() const { return m_config; }

    void setProgram(const QString &program) { m_program = program; }
    QString program() const { return m_program; }
    void setArguments(const QStringList &arguments) { m_arguments = arguments; }
    QStringList arguments() const { return m_arguments; }
    void setWorkingDirectory(const QString &workingDirectory) {
        m_workingDirectory = workingDirectory;
    }
    QString workingDirectory() const { return m_workingDirectory; }
    void setProcessEnvironment(const QProcessEnvironment &environment) {
        m_environment = environment;
    }
    QProcessEnvironment processEnvironment() const { return m_environment; }
    bool start(bool reset = true);

    void saveState(const QString &path);
    void loadState(const QString &path);

  protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

  signals:
    void contextMenuRequested(QMenu *menu, const QPoint &pos);
    void cwdChanged(const QString &cwd);
    void finished(int exitCode, int exitStatus);

  public slots:
    void onPtyReadyRead(const QByteArray &data);
    void onScrollValueChanged(int value);
    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);
    void pageUp();
    void pageDown();
    void copyToClipboard();
    void pasteFromClipboard();
    void selectAll();
    void clearScrollback();
    void resetTerminal();
    void openFileBrowser();
    void kill();

    void logData(const QByteArray &data);
    QString logPath() const { return m_logFile.fileName(); }
    void setRestoreLog(const QString &path) { m_pendingLogReplay = path; }
    void scrollToBottom();
    void processLogReplay();

    void zoomIn();
    void zoomOut();
    void resetZoom();

    QString foregroundProcessName() const;
    bool isRoot() const;
    const QString &cwd() const { return m_cwd; }

    bool copyOnSelect() const { return m_config.copyOnSelect; }
    void setCopyOnSelect(bool enable) { m_config.copyOnSelect = enable; }

    bool pasteOnMiddleClick() const { return m_config.pasteOnMiddleClick; }
    void setPasteOnMiddleClick(bool enable) { m_config.pasteOnMiddleClick = enable; }
    bool mouseWheelZoom() const { return m_config.mouseWheelZoom; }
    void setMouseWheelZoom(bool enable) { m_config.mouseWheelZoom = enable; }
    bool visualBell() const { return m_config.visualBell; }
    void setVisualBell(bool enable) { m_config.visualBell = enable; }
    bool audibleBell() const { return m_config.audibleBell; }
    void setAudibleBell(bool enable) { m_config.audibleBell = enable; }

  private:
    void setupPty();
    void updateTerminalSize();
    QColor mapColor(const VTermColor &c, const VTermState *state) const;
    QString getTextRange(VTermPos start, VTermPos end);
    bool isSelected(int row, int col) const;
    VTermPos mouseToPos(const QPoint &pos) const;

    // VTerm callbacks
    static int onDamage(VTermRect rect, void *user);
    static int onMoveRect(VTermRect dest, VTermRect src, void *user);
    static int onMoveCursor(VTermPos pos, VTermPos oldpos, int visible, void *user);
    static int onSetTermProp(VTermProp prop, VTermValue *val, void *user);
    static int onBell(void *user);
    static int onSbPushLine(int cols, const VTermScreenCell *cells, void *user);
    static int onSbPopLine(int cols, VTermScreenCell *cells, void *user);
    static int onOsc(int command, VTermStringFragment frag, void *user);

    int pushScrollback(int cols, const VTermScreenCell *cells);
    int popScrollback(int cols, VTermScreenCell *cells);

    struct SavedCell {
        uint32_t chars[VTERM_MAX_CHARS_PER_CELL];
        VTermScreenCellAttrs attrs;
        VTermColor fg, bg;
        int width;
    };
    using SavedLine = std::vector<SavedCell>;

    PtyProcess *m_pty = nullptr;
    VTerm *m_vterm = nullptr;
    VTermScreen *m_vtermScreen = nullptr;

    QSocketNotifier *m_notifier = nullptr;
    QSize m_cellSize;
    int m_cursorRow = 0;
    int m_cursorCol = 0;
    bool m_cursorVisible = true;
    bool m_cursorBlink = false;
    int m_cursorShape = 1; // VTERM_PROP_CURSORSHAPE_BLOCK
    bool m_cursorBlinkState = true;
    bool m_altScreen = false;
    bool m_flowControlStopped = false;
    bool m_restorationBannerActive = false;
    QString m_restorationBannerText;
    QTimer *m_restorationBannerTimer = nullptr;
    int m_mouseMode = 0; // VTERM_PROP_MOUSE_NONE
    QTimer *m_cursorBlinkTimer = nullptr;

    QScrollBar *m_scrollBar = nullptr;
    std::deque<SavedLine> m_scrollback;

    bool m_selecting = false;
    VTermPos m_selectionStart = {-1, -1};
    VTermPos m_selectionEnd = {-1, -1};

    QElapsedTimer m_clickTimer;
    int m_clickCount = 0;
    QPoint m_lastClickPos;

    bool m_visualBellActive = false;
    QString m_cwd;
    QByteArray m_oscBuffer;

    QString m_program;
    QStringList m_arguments;
    QString m_workingDirectory;
    QProcessEnvironment m_environment = QProcessEnvironment::systemEnvironment();
    KodoTermConfig m_config;
    QFile m_logFile;
    QString m_pendingLogReplay;
    QFile *m_replayFile = nullptr;
    bool m_restoring = false;

    mutable QColor m_paletteCache[256];
    mutable bool m_paletteCacheValid[256];

    mutable VTermColor m_lastVTermFg, m_lastVTermBg;
    mutable QColor m_lastFg, m_lastBg;
    double m_avgDrawTime = 0.0;

    VTermRect m_dirtyRect;
    void resetDirtyRect();

    bool m_dirty = false;
    QImage m_backBuffer;
    std::vector<VTermScreenCell> m_cellCache;
    std::vector<bool> m_selectedCache;
    void renderToBackbuffer();
    void flushTerminal();
    void damageAll();
    void drawRestorationBanner(QPainter &painter);
    int m_offsetX = 0;
    int m_offsetY = 0;
};
