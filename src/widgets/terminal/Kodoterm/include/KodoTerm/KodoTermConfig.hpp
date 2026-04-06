// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QList>
#include <QSettings>
#include <QString>

struct TerminalTheme {
    QString name;
    QColor foreground;
    QColor background;
    QColor palette[16];

    enum class ThemeFormat { Konsole, WindowsTerminal, ITerm };

    struct ThemeInfo {
        QString name;
        QString path;
        ThemeFormat format;
    };

    static TerminalTheme defaultTheme();
    static TerminalTheme loadTheme(const QString &path);
    static TerminalTheme loadKonsoleTheme(const QString &path);
    static TerminalTheme loadWindowsTerminalTheme(const QString &path);
    static TerminalTheme loadITermTheme(const QString &path);
    static QList<ThemeInfo> builtInThemes();

    QJsonObject toJson() const;
    static TerminalTheme fromJson(const QJsonObject &json);
    void save(QSettings &settings, const QString &group = "Theme") const;
    void load(QSettings &settings, const QString &group = "Theme");
};

class KodoTermConfig {
  public:
    KodoTermConfig();
    KodoTermConfig(QSettings &settings);

    QFont font;
    bool textAntialiasing;
    bool customBoxDrawing;
    bool copyOnSelect;
    bool pasteOnMiddleClick;
    bool mouseWheelZoom;
    bool visualBell;
    bool audibleBell;
    bool tripleClickSelectsLine;
    bool enableLogging;
    QString logDirectory;
    QString wordSelectionRegex;
    int maxScrollback;
    TerminalTheme theme;

    void setDefaults();
    void load(const QJsonObject &json);
    QJsonObject saveToJson() const;
    void load(QSettings &settings);
    void save(QSettings &settings) const;
};
