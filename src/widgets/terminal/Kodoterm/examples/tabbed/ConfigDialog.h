// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "AppConfig.h"
#include <KodoTerm/KodoTermConfig.hpp>
#include <QDialog>

class QListWidget;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;

class ConfigDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ConfigDialog(QWidget *parent = nullptr);

    KodoTermConfig getTerminalConfig() const;
    void setTerminalConfig(const KodoTermConfig &config);

  private slots:
    void addShell();
    void removeShell();
    void save();
    void updatePreview();

  private:
    void loadSettings();

    // General Tab
    QListWidget *m_shellList;
    QComboBox *m_defaultShellCombo;
    QList<AppConfig::ShellInfo> m_currentShells;

    // Terminal Tab
    QFontComboBox *m_fontCombo;
    QSpinBox *m_fontSizeSpin;
    QPushButton *m_themeBtn;
    QLabel *m_fontPreview;
    QLabel *m_colorBoxes[16];
    QString m_selectedThemePath;
    TerminalTheme m_currentTheme;

    QCheckBox *m_copyOnSelect;
    QCheckBox *m_pasteOnMiddleClick;
    QCheckBox *m_textAntialiasing;
    QCheckBox *m_customBoxDrawing;
    QCheckBox *m_mouseWheelZoom;
    QCheckBox *m_visualBell;
    QCheckBox *m_audibleBell;
    QCheckBox *m_tripleClick;
    QCheckBox *m_fullScreen;
    QCheckBox *m_enableTray;
    QCheckBox *m_enableLogging;
    QLineEdit *m_logDirectory;
    QLineEdit *m_wordSelectionRegex;
    QSpinBox *m_maxScrollback;
};
