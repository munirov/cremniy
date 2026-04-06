// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <KodoTerm/KodoTermConfig.hpp>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTabWidget>

#ifdef HAS_DBUS
#include <QtDBus/QDBusObjectPath>
#endif

class TabbedTerminal : public QMainWindow {
    Q_OBJECT
  public:
    TabbedTerminal(QWidget *parent = nullptr);
    ~TabbedTerminal();

  public slots:
    void addNewTab(const QString &program = QString(), const QString &workingDirectory = QString(),
                   const QString &logPath = QString());
    void closeCurrentTab();
    void closeTab(QWidget *w);
    void nextTab();
    void previousTab();
    void moveTabLeft();
    void moveTabRight();
    void updateTabColors();
    void showConfigDialog();
    void applySettings();
    void saveSession();
    void toggleExpanded();
    void toggleWindowVisibility();
    void showAboutDialog();

  private slots:
#ifdef HAS_DBUS
    void onPortalShortcutActivated(const QString &sessionHandle, const QString &shortcutId,
                                   const QVariantMap &options);
    void onPortalSessionCreated(const QDBusObjectPath &handle);
    void onPortalShortcutsBound(const QDBusObjectPath &handle);
#endif

  protected:
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

  private:
    void setupTrayIcon();
    void setupWaylandShortcut();

    QTabWidget *m_tabs;
    QTimer *m_autoSaveTimer;
    KodoTermConfig m_config;
    bool m_useFullScreenMode = false;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QAction *m_toggleWindowAction = nullptr;
#ifdef HAS_DBUS
    QString m_portalSessionHandle;
#endif
};