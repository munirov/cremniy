// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "TabbedTerminal.h"
#include "AppConfig.h"
#include "ConfigDialog.h"
#include <KodoTerm/KodoTerm.hpp>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#ifdef HAS_DBUS
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusReply>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef HAS_X11
#include <QtGui/qguiapplication.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <xcb/xcb.h>
#ifdef HAS_X11_NATIVE_INTERFACE
#include <QtGui/QNativeInterface>
#endif
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#endif

TabbedTerminal::TabbedTerminal(QWidget *parent) : QMainWindow(parent) {
    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::South);
    m_tabs->setDocumentMode(true);
    m_tabs->setMovable(true);
    setCentralWidget(m_tabs);
    setupTrayIcon();
    setupWaylandShortcut();

    // New Tab button (Left corner)
    QToolButton *newTabBtn = new QToolButton(m_tabs);
    newTabBtn->setText(QString(QChar(0x2795))); // ➕
    newTabBtn->setToolTip(tr("New Tab"));
    newTabBtn->setAutoRaise(true);
    newTabBtn->setPopupMode(QToolButton::MenuButtonPopup);
    m_tabs->setCornerWidget(newTabBtn, Qt::TopLeftCorner);

    QMenu *shellsMenu = new QMenu(newTabBtn);

    auto updateMenu = [this, shellsMenu, newTabBtn]() {
        shellsMenu->clear();
        QList<AppConfig::ShellInfo> shells = AppConfig::loadShells();
        for (const auto &shell : shells) {
            shellsMenu->addAction(shell.name, this, [this, shell]() { addNewTab(shell.path); });
        }
        shellsMenu->addSeparator();
        shellsMenu->addAction(tr("Configure..."), this, &TabbedTerminal::showConfigDialog);
        shellsMenu->addAction(tr("About..."), this, &TabbedTerminal::showAboutDialog);

        QSettings s;
        if (s.value("Window/EnableTray", false).toBool()) {
            shellsMenu->addSeparator();
            shellsMenu->addAction(tr("Quit"), qApp, &QApplication::quit);
        }
    };
    updateMenu();
    connect(shellsMenu, &QMenu::aboutToShow, this, updateMenu);

    newTabBtn->setMenu(shellsMenu);
    connect(newTabBtn, &QToolButton::clicked, this, [this]() { addNewTab(); });

    // Close Tab button (Right corner)
    QToolButton *closeTabBtn = new QToolButton(m_tabs);
    closeTabBtn->setText(QString(QChar(0x2715)));
    closeTabBtn->setAutoRaise(true);
    closeTabBtn->setToolTip(tr("Close Current Tab"));
    m_tabs->setCornerWidget(closeTabBtn, Qt::TopRightCorner);
    connect(closeTabBtn, &QToolButton::clicked, this, &TabbedTerminal::closeCurrentTab);

    QAction *newTabAction = new QAction(tr("New Tab"), this);
    newTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    newTabAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(newTabAction, &QAction::triggered, this, [this]() { addNewTab(); });
    addAction(newTabAction);

    QAction *closeTabAction = new QAction(tr("Close Tab"), this);
    closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    closeTabAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(closeTabAction, &QAction::triggered, this, &TabbedTerminal::closeCurrentTab);
    addAction(closeTabAction);

    QAction *prevTabAction = new QAction(tr("Previous Tab"), this);
    prevTabAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Left));
    prevTabAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(prevTabAction, &QAction::triggered, this, &TabbedTerminal::previousTab);
    addAction(prevTabAction);

    QAction *nextTabAction = new QAction(tr("Next Tab"), this);
    nextTabAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Right));
    nextTabAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(nextTabAction, &QAction::triggered, this, &TabbedTerminal::nextTab);
    addAction(nextTabAction);

    QAction *moveTabLeftAction = new QAction(tr("Move Tab Left"), this);
    moveTabLeftAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Left));
    moveTabLeftAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(moveTabLeftAction, &QAction::triggered, this, &TabbedTerminal::moveTabLeft);
    addAction(moveTabLeftAction);

    QAction *moveTabRightAction = new QAction(tr("Move Tab Right"), this);
    moveTabRightAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right));
    moveTabRightAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(moveTabRightAction, &QAction::triggered, this, &TabbedTerminal::moveTabRight);
    addAction(moveTabRightAction);

    QAction *configAction = new QAction(tr("Configure..."), this);
    configAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    configAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(configAction, &QAction::triggered, this, &TabbedTerminal::showConfigDialog);
    addAction(configAction);

    QAction *fullScreenAction = new QAction(tr("Toggle Full Screen"), this);
    fullScreenAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
    fullScreenAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(fullScreenAction, &QAction::triggered, this, &TabbedTerminal::toggleExpanded);
    addAction(fullScreenAction);

    for (int i = 1; i <= 9; ++i) {
        QAction *selectTabAction = new QAction(this);
        selectTabAction->setShortcut(QKeySequence(Qt::ALT | (Qt::Key_0 + i)));
        selectTabAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(selectTabAction, &QAction::triggered, this, [this, i]() {
            if (i == 9) {
                m_tabs->setCurrentIndex(m_tabs->count() - 1);
            } else if (i <= m_tabs->count()) {
                m_tabs->setCurrentIndex(i - 1);
            }
        });
        addAction(selectTabAction);
    }

    QTimer *colorTimer = new QTimer(this);
    colorTimer->setInterval(1000);
    connect(colorTimer, &QTimer::timeout, this, &TabbedTerminal::updateTabColors);
    colorTimer->start();

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(60 * 1000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &TabbedTerminal::saveSession);
    m_autoSaveTimer->start();

    AppConfig::cleanupOldLogs();

    resize(1024, 768);
    QTimer::singleShot(0, this, [this]() {
        QSettings s;
        m_config.load(s);
        restoreGeometry(s.value("Window/Geometry").toByteArray());

        int tabCount = s.beginReadArray("Session/Tabs");
        if (tabCount > 0) {
            for (int i = 0; i < tabCount; ++i) {
                s.setArrayIndex(i);
                QString program = s.value("program").toString();
                QString cwd = s.value("cwd").toString();
                QString logPath = s.value("logPath").toString();
                addNewTab(program, cwd, logPath);
            }
            s.endArray();

            int activeTab = s.value("Session/ActiveTab", 0).toInt();
            if (activeTab >= 0 && activeTab < m_tabs->count()) {
                m_tabs->setCurrentIndex(activeTab);
            }
        } else {
            addNewTab();
        }
    });
}

TabbedTerminal::~TabbedTerminal() {
#ifdef Q_OS_WIN
    UnregisterHotKey((HWND)winId(), 100);
#endif
}

void TabbedTerminal::saveSession() {
    QSettings s;
    s.setValue("Window/Geometry", saveGeometry());
    s.remove("Session/Tabs");
    s.beginWriteArray("Session/Tabs");
    for (int i = 0; i < m_tabs->count(); ++i) {
        KodoTerm *console = qobject_cast<KodoTerm *>(m_tabs->widget(i));
        if (console) {
            s.setArrayIndex(i);
            s.setValue("program", console->program());
            s.setValue("cwd", console->cwd());
            s.setValue("logPath", console->logPath());
        }
    }
    s.endArray();
    s.setValue("Session/ActiveTab", m_tabs->currentIndex());
}

void TabbedTerminal::closeEvent(QCloseEvent *event) {
    QSettings s;
    bool enableTray = s.value("Window/EnableTray", false).toBool();
    if (enableTray && m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        saveSession();
        QMainWindow::closeEvent(event);
    }
}

void TabbedTerminal::addNewTab(const QString &program, const QString &workingDirectory,
                               const QString &logPath) {
    KodoTerm *console = new KodoTerm(m_tabs);
    if (!program.isEmpty()) {
        console->setProgram(program);
    } else {
        QString defName = AppConfig::defaultShell();
        AppConfig::ShellInfo info = AppConfig::getShellInfo(defName);
        console->setProgram(info.path);
    }

    // Attempt to inject shell integration for CWD tracking (Bash mostly)
    // Problem: on git/bash this just does not work and spams the logs.
    QProcessEnvironment env = console->processEnvironment();
#ifndef Q_OS_WIN
    QString progName = QFileInfo(console->program()).baseName();
    if (progName == "bash") {
        env.insert("PROMPT_COMMAND", "printf \"\\033]7;file://localhost%s\\033\\\\\" \"$PWD\"");
    }
#endif
    console->setProcessEnvironment(env);

    if (!workingDirectory.isEmpty()) {
        console->setWorkingDirectory(workingDirectory);
    }
    connect(console, &KodoTerm::windowTitleChanged, [this, console](const QString &title) {
        int index = m_tabs->indexOf(console);
        if (index != -1) {
            m_tabs->setTabText(index, title);
            updateTabColors();
        }
    });
    connect(console, &KodoTerm::cwdChanged, [this, console](const QString &) {
        console->setProperty("cwdReceived", true);
        updateTabColors();
    });
    connect(console, &KodoTerm::finished, this,
            [this, console](int exitCode, int exitStatus) { closeTab(console); });

    int index = m_tabs->addTab(console, tr("Terminal"));
    m_tabs->setCurrentIndex(index);
    console->setConfig(m_config);
    console->setFocus();
    if (!logPath.isEmpty()) {
        console->setRestoreLog(logPath);
        console->start(false);
    } else {
        console->start(true);
    }
}

void TabbedTerminal::showConfigDialog() {
    ConfigDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        applySettings();
    }
}

void TabbedTerminal::toggleExpanded() {
    if (m_useFullScreenMode) {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    } else {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    }
}

void TabbedTerminal::applySettings() {
    QSettings s;
    m_useFullScreenMode = s.value("Window/UseFullScreenMode", false).toBool();
    setupTrayIcon();

    m_config.load(s);
    for (int i = 0; i < m_tabs->count(); ++i) {
        KodoTerm *console = qobject_cast<KodoTerm *>(m_tabs->widget(i));
        if (console) {
            console->setConfig(m_config);
        }
    }
}

void TabbedTerminal::closeCurrentTab() {
    int index = m_tabs->currentIndex();
    if (index != -1) {
        closeTab(m_tabs->widget(index));
    }
}

void TabbedTerminal::closeTab(QWidget *w) {
    if (m_tabs->count() == 1) {
        close();
        return;
    }
    int index = m_tabs->indexOf(w);
    if (index != -1) {
        m_tabs->removeTab(index);
        w->deleteLater();
        if (m_tabs->currentWidget()) {
            m_tabs->currentWidget()->setFocus();
        }
    }
}

void TabbedTerminal::nextTab() {
    int count = m_tabs->count();
    if (count <= 1) {
        return;
    }
    int index = m_tabs->currentIndex();
    m_tabs->setCurrentIndex((index + 1) % count);
}

void TabbedTerminal::previousTab() {
    int count = m_tabs->count();
    if (count <= 1) {
        return;
    }
    int index = m_tabs->currentIndex();
    m_tabs->setCurrentIndex((index - 1 + count) % count);
}

void TabbedTerminal::moveTabLeft() {
    int index = m_tabs->currentIndex();
    if (index > 0) {
        m_tabs->tabBar()->moveTab(index, index - 1);
    }
}

void TabbedTerminal::moveTabRight() {
    int index = m_tabs->currentIndex();
    if (index != -1 && index < m_tabs->count() - 1) {
        m_tabs->tabBar()->moveTab(index, index + 1);
    }
}

void TabbedTerminal::updateTabColors() {
    QTabBar *bar = m_tabs->tabBar();
    for (int i = 0; i < m_tabs->count(); ++i) {
        KodoTerm *console = qobject_cast<KodoTerm *>(m_tabs->widget(i));
        if (!console) {
            continue;
        }

        QString title = console->foregroundProcessName();
        if (title.isEmpty()) {
            title = tr("Terminal");
        }

        if (console->property("cwdReceived").toBool()) {
            QString cwd = console->cwd();
            QFileInfo cwdInfo(cwd);
            QString dirName = cwdInfo.fileName();
            if (dirName.isEmpty() && !cwd.isEmpty()) {
                dirName = cwd; // Handle root or other cases
            }
            if (!dirName.isEmpty()) {
                title += QString(" [%1]").arg(dirName);
            }
            m_tabs->setTabToolTip(i, cwd);
        } else {
            m_tabs->setTabToolTip(i, QString());
        }

        if (console->isRoot()) {
            bar->setTabTextColor(i, Qt::red);
            if (!title.startsWith("root@")) {
                title = "root@" + title;
            }
        } else {
            bar->setTabTextColor(i, QPalette().color(QPalette::WindowText));
        }
        m_tabs->setTabText(i, title);
    }
}

void TabbedTerminal::setupTrayIcon() {
    QSettings s;
    if (!s.value("Window/EnableTray", false).toBool()) {
        if (m_trayIcon) {
            m_trayIcon->hide();
            m_trayIcon->deleteLater();
            m_trayIcon = nullptr;
        }
        return;
    }

    if (m_trayIcon) {
        return;
    }

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(windowIcon());
    m_trayIcon->setToolTip("KodoShell");

#ifdef Q_OS_WIN
    // Register Ctrl+Alt+T globally (ID 100)
    RegisterHotKey((HWND)winId(), 100, MOD_CONTROL | MOD_ALT, 'T');
#endif

#if defined(HAS_X11_NATIVE_INTERFACE)
    if (auto dpy = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->display()) {
        Window root = DefaultRootWindow(dpy);
        int keycode = XKeysymToKeycode(dpy, XK_T);
        XGrabKey(dpy, keycode, ControlMask | Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    }
#endif

    QMenu *trayMenu = new QMenu(this);

    m_toggleWindowAction =
        trayMenu->addAction(tr("Show/Hide Window"), this, &TabbedTerminal::toggleWindowVisibility);
    m_toggleWindowAction->setShortcut(QKeySequence("Ctrl+Alt+T"));
    m_toggleWindowAction->setShortcutContext(Qt::ApplicationShortcut);
    addAction(m_toggleWindowAction);

    trayMenu->addAction(tr("Configure..."), this, &TabbedTerminal::showConfigDialog);
    trayMenu->addAction(tr("About..."), this, &TabbedTerminal::showAboutDialog);
    trayMenu->addSeparator();
    trayMenu->addAction(tr("Quit"), qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    toggleWindowVisibility();
                }
            });

    m_trayIcon->show();
}

void TabbedTerminal::setupWaylandShortcut() {
#ifdef HAS_DBUS
    if (!QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) {
        return;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }

    // 1. Create a session
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.GlobalShortcuts", "CreateSession");

    QVariantMap options;
    options["session_handle_token"] = "kodoshell_session";
    msg << options;

    bus.callWithCallback(msg, this, SLOT(onPortalSessionCreated(QDBusObjectPath)), nullptr);

    bus.connect("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                "org.freedesktop.portal.GlobalShortcuts", "Activated", this,
                SLOT(onPortalShortcutActivated(QString, QString, QVariantMap)));
#endif
}

#ifdef HAS_DBUS
void TabbedTerminal::onPortalSessionCreated(const QDBusObjectPath &handle) {
    m_portalSessionHandle = handle.path();
    QDBusConnection bus = QDBusConnection::sessionBus();

    // 2. Bind shortcuts
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.GlobalShortcuts", "BindShortcuts");

    struct Shortcut {
        QString id;
        QVariantMap description;
    };

    QVariantMap desc;
    desc["description"] = tr("Toggle KodoShell Visibility");
    // Ctrl+Alt+T represented as a string or keysym is portal dependent,
    // but usually handled in a dialog. We provide a hint.
    desc["preferred_trigger"] = "Ctrl+Alt+T";

    QList<QVariant> shortcuts;
    QVariantList shortcutList;
    QVariantList s;
    s << "toggle_window" << desc;
    shortcutList << QVariant(s);

    msg << QDBusObjectPath(m_portalSessionHandle) << shortcutList << "" << QVariantMap();

    bus.callWithCallback(msg, this, SLOT(onPortalShortcutsBound(QDBusObjectPath)));
}

void TabbedTerminal::onPortalShortcutsBound(const QDBusObjectPath &handle) {
    qDebug() << "Wayland shortcuts bound to portal session:" << handle.path();
}

void TabbedTerminal::onPortalShortcutActivated(const QString &sessionHandle,
                                               const QString &shortcutId,
                                               const QVariantMap &options) {
    if (shortcutId == "toggle_window") {
        toggleWindowVisibility();
    }
}
#endif

void TabbedTerminal::toggleWindowVisibility() {
    if (isVisible() && !isMinimized()) {
        hide();
    } else {
        show();
        showNormal();
        activateWindow();
        raise();
    }
}

void TabbedTerminal::showAboutDialog() {
    QMessageBox::about(this, tr("About KodoShell"),
                       tr("KodoShell - A terminal emulator example based KodoTerm.\n\n"
                          "Copyright (C) 2026 Diego Iastrubni, MIT licensed."));
}

bool TabbedTerminal::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == 100) {
            toggleWindowVisibility();
            return true;
        }
    }
#endif

#ifdef HAS_X11
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t *event = static_cast<xcb_generic_event_t *>(message);
        if ((event->response_type & ~0x80) == XCB_KEY_PRESS) {
            xcb_key_press_event_t *keyEvent = reinterpret_cast<xcb_key_press_event_t *>(event);
#ifdef HAS_X11_NATIVE_INTERFACE
            if (auto dpy =
                    qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->display()) {
                int keycode = XKeysymToKeycode(dpy, XK_T);
                if (keyEvent->detail == keycode &&
                    (keyEvent->state & (XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1))) {
                    toggleWindowVisibility();
                    return true;
                }
            }
#endif
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}
