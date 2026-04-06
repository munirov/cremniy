// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "ConfigDialog.h"

#include <KodoTerm/KodoTerm.hpp>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

ConfigDialog::ConfigDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Configuration"));
    resize(600, 500);

    QTabWidget *tabs = new QTabWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabs);

    // --- General Tab ---
    QWidget *generalTab = new QWidget();
    QVBoxLayout *generalLayout = new QVBoxLayout(generalTab);

    QLabel *shellsLabel = new QLabel(tr("Available Shells:"), generalTab);
    m_shellList = new QListWidget(generalTab);

    QHBoxLayout *shellBtnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(tr("Add..."), generalTab);
    QPushButton *removeBtn = new QPushButton(tr("Remove"), generalTab);
    shellBtnLayout->addStretch();
    shellBtnLayout->addWidget(addBtn);
    shellBtnLayout->addWidget(removeBtn);

    QHBoxLayout *defaultShellLayout = new QHBoxLayout();
    QLabel *defaultLabel = new QLabel(tr("Default Shell:"), generalTab);
    m_defaultShellCombo = new QComboBox(generalTab);
    defaultShellLayout->addWidget(defaultLabel);
    defaultShellLayout->addWidget(m_defaultShellCombo);
    defaultShellLayout->addStretch();

    m_enableTray = new QCheckBox(tr("Enable system tray support"), generalTab);

    generalLayout->addWidget(shellsLabel);
    generalLayout->addWidget(m_shellList);
    generalLayout->addLayout(shellBtnLayout);
    generalLayout->addLayout(defaultShellLayout);
    generalLayout->addWidget(m_enableTray);

    tabs->addTab(generalTab, tr("General"));

    // --- Terminal Tab ---
    QWidget *terminalTab = new QWidget();
    QVBoxLayout *termLayout = new QVBoxLayout(terminalTab);

    QHBoxLayout *fontLayout = new QHBoxLayout();
    m_fontCombo = new QFontComboBox(terminalTab);
    m_fontCombo->setEditable(false); // Only allow selecting existing fonts
    // Filter for monospaced fonts? KodoTerm usually works best with them.
    m_fontCombo->setFontFilters(QFontComboBox::MonospacedFonts);

    m_fontSizeSpin = new QSpinBox(terminalTab);
    m_fontSizeSpin->setRange(6, 72);
    fontLayout->addWidget(new QLabel(tr("Font:")));
    fontLayout->addWidget(m_fontCombo, 1);
    fontLayout->addWidget(m_fontSizeSpin);

    QHBoxLayout *themeLayout = new QHBoxLayout();
    m_themeBtn = new QPushButton(tr("Select Theme..."), terminalTab);
    QMenu *themeMenu = new QMenu(m_themeBtn);

    auto themeCallback = [this](const TerminalTheme::ThemeInfo &info) {
        m_selectedThemePath = info.path;
        m_themeBtn->setText(info.name);
        m_currentTheme = TerminalTheme::loadTheme(info.path);
        updatePreview();
    };

    KodoTerm::populateThemeMenu(themeMenu, tr("Konsole"), TerminalTheme::ThemeFormat::Konsole,
                                themeCallback);

    KodoTerm::populateThemeMenu(themeMenu, tr("Windows Terminal"),
                                TerminalTheme::ThemeFormat::WindowsTerminal, themeCallback);

    KodoTerm::populateThemeMenu(themeMenu, tr("iTerm"), TerminalTheme::ThemeFormat::ITerm,
                                themeCallback);

    m_themeBtn->setMenu(themeMenu);

    themeLayout->addWidget(new QLabel(tr("Theme:")));
    themeLayout->addWidget(m_themeBtn, 1);

#ifdef Q_OS_WIN
    m_fontPreview =
        new QLabel(tr("C:\\> ver\nMicrosoft Windows [Version 10.0.19045.4170]"), terminalTab);
#else
    m_fontPreview = new QLabel(tr("user@localhost:~$ uptime\n 12:34:56 up 10 days,  1:23,  2 "
                                  "users,  load average: 0.05, 0.01, 0.00"),
                               terminalTab);
#endif
    m_fontPreview->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_fontPreview->setMinimumHeight(80);
    m_fontPreview->setContentsMargins(5, 5, 5, 5);
    m_fontPreview->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_fontPreview->setAutoFillBackground(true);

    QHBoxLayout *colorLayout = new QHBoxLayout();
    colorLayout->setSpacing(2);
    for (int i = 0; i < 16; ++i) {
        m_colorBoxes[i] = new QLabel(terminalTab);
        m_colorBoxes[i]->setFixedSize(20, 20);
        m_colorBoxes[i]->setFrameStyle(QFrame::Box | QFrame::Plain);
        m_colorBoxes[i]->setAutoFillBackground(true);
        colorLayout->addWidget(m_colorBoxes[i]);
    }
    colorLayout->addStretch();

    m_copyOnSelect = new QCheckBox(tr("Copy on select"), terminalTab);
    m_pasteOnMiddleClick = new QCheckBox(tr("Paste on middle click"), terminalTab);
    m_textAntialiasing = new QCheckBox(tr("Text Antialiasing"), terminalTab);
    m_customBoxDrawing = new QCheckBox(tr("Custom Box Drawing"), terminalTab);
    m_mouseWheelZoom = new QCheckBox(tr("Mouse wheel zoom"), terminalTab);
    m_visualBell = new QCheckBox(tr("Visual Bell"), terminalTab);
    m_audibleBell = new QCheckBox(tr("Audible Bell"), terminalTab);
    m_tripleClick = new QCheckBox(tr("Triple click selects whole line"), terminalTab);
    m_fullScreen = new QCheckBox(tr("Use Borderless Full Screen mode"), terminalTab);

    m_enableLogging = new QCheckBox(tr("Enable Session Logging"), terminalTab);
    QHBoxLayout *logDirLayout = new QHBoxLayout();
    m_logDirectory = new QLineEdit(terminalTab);
    QPushButton *browseLogBtn = new QPushButton(tr("Browse..."), terminalTab);
    logDirLayout->addWidget(new QLabel(tr("Log Directory:")));
    logDirLayout->addWidget(m_logDirectory, 1);
    logDirLayout->addWidget(browseLogBtn);

    QHBoxLayout *regexLayout = new QHBoxLayout();
    m_wordSelectionRegex = new QLineEdit(terminalTab);
    regexLayout->addWidget(new QLabel(tr("Word Selection Regex:")));
    regexLayout->addWidget(m_wordSelectionRegex, 1);

    QHBoxLayout *sbLayout = new QHBoxLayout();
    m_maxScrollback = new QSpinBox(terminalTab);
    m_maxScrollback->setRange(0, 100000);
    m_maxScrollback->setSingleStep(100);
    sbLayout->addWidget(new QLabel(tr("Max Scrollback Lines:")));
    sbLayout->addWidget(m_maxScrollback);
    sbLayout->addStretch();

    termLayout->addLayout(fontLayout);
    termLayout->addLayout(themeLayout);
    termLayout->addWidget(m_fontPreview);
    termLayout->addLayout(colorLayout);
    termLayout->addWidget(m_copyOnSelect);
    termLayout->addWidget(m_pasteOnMiddleClick);
    termLayout->addWidget(m_textAntialiasing);
    termLayout->addWidget(m_customBoxDrawing);
    termLayout->addWidget(m_mouseWheelZoom);
    termLayout->addWidget(m_visualBell);
    termLayout->addWidget(m_audibleBell);
    termLayout->addWidget(m_tripleClick);
    termLayout->addWidget(m_fullScreen);
    termLayout->addWidget(m_enableLogging);
    termLayout->addLayout(logDirLayout);
    termLayout->addLayout(regexLayout);
    termLayout->addLayout(sbLayout);
    termLayout->addStretch();

    tabs->addTab(terminalTab, tr("Terminal"));

    // --- Dialog Buttons ---
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // Connections
    connect(addBtn, &QPushButton::clicked, this, &ConfigDialog::addShell);
    connect(removeBtn, &QPushButton::clicked, this, &ConfigDialog::removeShell);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &ConfigDialog::updatePreview);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &ConfigDialog::updatePreview);
    connect(m_textAntialiasing, &QCheckBox::toggled, this, &ConfigDialog::updatePreview);

    connect(browseLogBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Log Directory"),
                                                        m_logDirectory->text());
        if (!dir.isEmpty()) {
            m_logDirectory->setText(dir);
        }
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ConfigDialog::save);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ConfigDialog::reject);

    loadSettings();
}

void ConfigDialog::updatePreview() {
    QFont f = m_fontCombo->currentFont();
    f.setPointSizeF(m_fontSizeSpin->value());
    f.setKerning(false);
    f.setStyleStrategy(m_textAntialiasing->isChecked() ? QFont::PreferAntialias
                                                       : QFont::NoAntialias);
    m_fontPreview->setFont(f);

    QPalette pal = m_fontPreview->palette();
    pal.setColor(QPalette::Window, m_currentTheme.background);
    pal.setColor(QPalette::WindowText, m_currentTheme.foreground);
    m_fontPreview->setPalette(pal);

    for (int i = 0; i < 16; ++i) {
        QPalette cp = m_colorBoxes[i]->palette();
        cp.setColor(QPalette::Window, m_currentTheme.palette[i]);
        m_colorBoxes[i]->setPalette(cp);
    }
}

void ConfigDialog::loadSettings() {
    // General
    m_currentShells = AppConfig::loadShells();
    m_shellList->clear();
    m_defaultShellCombo->clear();
    for (const auto &info : m_currentShells) {
        if (info.name == info.path) {
            m_shellList->addItem(info.name);
        } else {
            m_shellList->addItem(info.name + " (" + info.path + ")");
        }
        m_defaultShellCombo->addItem(info.name);
    }
    m_defaultShellCombo->setCurrentText(AppConfig::defaultShell());

    // Terminal
    QSettings s;
    m_fullScreen->setChecked(s.value("Window/UseFullScreenMode", false).toBool());
    m_enableTray->setChecked(s.value("Window/EnableTray", false).toBool());

    KodoTermConfig config(s);
    m_currentTheme = config.theme; // Store current theme
    setTerminalConfig(config);
}

void ConfigDialog::addShell() {
    QString name = QInputDialog::getText(this, tr("Add Shell"), tr("Shell Name:"));
    if (name.isEmpty()) {
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, tr("Select Shell Executable"));
    if (path.isEmpty()) {
        return;
    }

    m_currentShells.append({name, path});
    if (name == path) {
        m_shellList->addItem(name);
    } else {
        m_shellList->addItem(name + " (" + path + ")");
    }
    m_defaultShellCombo->addItem(name);
}

void ConfigDialog::removeShell() {
    int row = m_shellList->currentRow();
    if (row >= 0 && row < m_currentShells.size()) {
        QString nameToRemove = m_currentShells[row].name;
        m_currentShells.removeAt(row);
        delete m_shellList->takeItem(row);
        int comboIdx = m_defaultShellCombo->findText(nameToRemove);
        if (comboIdx != -1) {
            m_defaultShellCombo->removeItem(comboIdx);
        }
    }
}

void ConfigDialog::save() {
    // Save Shells
    AppConfig::saveShells(m_currentShells);
    AppConfig::setDefaultShell(m_defaultShellCombo->currentText());

    // Save Terminal Config
    QSettings s;
    s.setValue("Window/UseFullScreenMode", m_fullScreen->isChecked());
    s.setValue("Window/EnableTray", m_enableTray->isChecked());

    KodoTermConfig config = getTerminalConfig();
    config.save(s);
    s.sync();

    accept();
}

KodoTermConfig ConfigDialog::getTerminalConfig() const {
    KodoTermConfig config;
    config.font = m_fontCombo->currentFont();
    config.font.setPointSizeF(m_fontSizeSpin->value());

    if (!m_selectedThemePath.isEmpty()) {
        config.theme = TerminalTheme::loadTheme(m_selectedThemePath);
    } else {
        config.theme = m_currentTheme; // Keep current theme if no path selected
    }

    config.copyOnSelect = m_copyOnSelect->isChecked();
    config.pasteOnMiddleClick = m_pasteOnMiddleClick->isChecked();
    config.textAntialiasing = m_textAntialiasing->isChecked();
    config.customBoxDrawing = m_customBoxDrawing->isChecked();
    config.mouseWheelZoom = m_mouseWheelZoom->isChecked();
    config.visualBell = m_visualBell->isChecked();
    config.audibleBell = m_audibleBell->isChecked();
    config.tripleClickSelectsLine = m_tripleClick->isChecked();
    config.enableLogging = m_enableLogging->isChecked();
    config.logDirectory = m_logDirectory->text();
    config.wordSelectionRegex = m_wordSelectionRegex->text();
    config.maxScrollback = m_maxScrollback->value();

    return config;
}

void ConfigDialog::setTerminalConfig(const KodoTermConfig &config) {
    m_fontCombo->setCurrentFont(config.font);
    m_fontSizeSpin->setValue(config.font.pointSizeF());

    auto themes = TerminalTheme::builtInThemes();
    m_selectedThemePath.clear();
    for (const auto &info : themes) {
        if (info.name == config.theme.name) {
            m_selectedThemePath = info.path;
            m_themeBtn->setText(info.name);
            break;
        }
    }
    if (m_selectedThemePath.isEmpty()) {
        m_themeBtn->setText(config.theme.name);
    }

    m_copyOnSelect->setChecked(config.copyOnSelect);
    m_pasteOnMiddleClick->setChecked(config.pasteOnMiddleClick);
    m_textAntialiasing->setChecked(config.textAntialiasing);
    m_customBoxDrawing->setChecked(config.customBoxDrawing);
    m_mouseWheelZoom->setChecked(config.mouseWheelZoom);
    m_visualBell->setChecked(config.visualBell);
    m_audibleBell->setChecked(config.audibleBell);
    m_tripleClick->setChecked(config.tripleClickSelectsLine);
    m_enableLogging->setChecked(config.enableLogging);
    m_logDirectory->setText(config.logDirectory);
    m_wordSelectionRegex->setText(config.wordSelectionRegex);
    m_maxScrollback->setValue(config.maxScrollback);

    updatePreview();
}
