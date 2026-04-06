// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "KodoTerm/KodoTermConfig.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QXmlStreamReader>

TerminalTheme TerminalTheme::defaultTheme() {
    return {"Default",
            QColor(170, 170, 170),
            QColor(0, 0, 0),
            {QColor(0, 0, 0), QColor(170, 0, 0), QColor(0, 170, 0), QColor(170, 85, 0),
             QColor(0, 0, 170), QColor(170, 0, 170), QColor(0, 170, 170), QColor(170, 170, 170),
             QColor(85, 85, 85), QColor(255, 85, 85), QColor(85, 255, 85), QColor(255, 255, 85),
             QColor(85, 85, 255), QColor(255, 85, 255), QColor(85, 255, 255),
             QColor(255, 255, 255)}};
}

TerminalTheme TerminalTheme::loadTheme(const QString &path) {
    if (path.endsWith(".colorscheme")) {
        return loadKonsoleTheme(path);
    } else if (path.endsWith(".itermcolors")) {
        return loadITermTheme(path);
    } else if (path.endsWith(".json")) {
        return loadWindowsTerminalTheme(path);
    }

    // Fallback: try to guess content or return default
    return defaultTheme();
}

TerminalTheme TerminalTheme::loadKonsoleTheme(const QString &path) {
    TerminalTheme theme = defaultTheme();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return theme;
    }

    theme.name = QFileInfo(path).baseName();
    auto parseColor = [](const QString &s) -> QColor {
        QStringList parts = s.split(',');
        if (parts.size() >= 3) {
            return QColor(parts[0].toInt(), parts[1].toInt(), parts[2].toInt());
        }
        return QColor();
    };

    QMap<QString, QMap<QString, QString>> sections;
    QString currentSection;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(';')) {
            continue;
        }
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2);
        } else if (!currentSection.isEmpty()) {
            int eq = line.indexOf('=');
            if (eq != -1) {
                QString key = line.left(eq).trimmed();
                QString value = line.mid(eq + 1).trimmed();
                sections[currentSection][key] = value;
            }
        }
    }

    if (sections.contains("General") && sections["General"].contains("Description")) {
        theme.name = sections["General"]["Description"];
    }

    QColor fg = parseColor(sections["Foreground"]["Color"]);
    if (fg.isValid()) {
        theme.foreground = fg;
    }

    QColor bg = parseColor(sections["Background"]["Color"]);
    if (bg.isValid()) {
        theme.background = bg;
    }

    for (int i = 0; i < 16; ++i) {
        QString section = QString("Color%1%2").arg(i % 8).arg(i >= 8 ? "Intense" : "");
        QColor c = parseColor(sections[section]["Color"]);
        if (c.isValid()) {
            theme.palette[i] = c;
        }
    }

    return theme;
}

TerminalTheme TerminalTheme::loadWindowsTerminalTheme(const QString &path) {
    TerminalTheme theme = defaultTheme();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return theme;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object();
    if (obj.contains("name")) {
        theme.name = obj.value("name").toString();
    }
    if (obj.contains("foreground")) {
        theme.foreground = QColor(obj.value("foreground").toString());
    }
    if (obj.contains("background")) {
        theme.background = QColor(obj.value("background").toString());
    }
    QStringList keys = {"black",       "red",          "green",       "yellow",
                        "blue",        "purple",       "cyan",        "white",
                        "brightBlack", "brightRed",    "brightGreen", "brightYellow",
                        "brightBlue",  "brightPurple", "brightCyan",  "brightWhite"};
    for (int i = 0; i < 16; ++i) {
        if (obj.contains(keys[i])) {
            QColor c(obj.value(keys[i]).toString());
            if (c.isValid()) {
                theme.palette[i] = c;
            }
        }
    }
    return theme;
}

TerminalTheme TerminalTheme::loadITermTheme(const QString &path) {
    TerminalTheme theme = defaultTheme();
    theme.name = QFileInfo(path).baseName();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return theme;
    }

    QXmlStreamReader xml(&file);
    if (xml.readNextStartElement() && xml.name() == QLatin1String("plist")) {
        if (xml.readNextStartElement() && xml.name() == QLatin1String("dict")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("key")) {
                    QString keyName = xml.readElementText();
                    if (xml.readNextStartElement()) {
                        if (xml.name() == QLatin1String("dict")) {
                            double red = 0.0, green = 0.0, blue = 0.0;
                            while (xml.readNextStartElement()) {
                                if (xml.name() == QLatin1String("key")) {
                                    QString componentKey = xml.readElementText();
                                    if (xml.readNextStartElement()) {
                                        if (xml.name() == QLatin1String("real")) {
                                            double val = xml.readElementText().toDouble();
                                            if (componentKey == "Red Component") {
                                                red = val;
                                            } else if (componentKey == "Green Component") {
                                                green = val;
                                            } else if (componentKey == "Blue Component") {
                                                blue = val;
                                            }
                                        } else {
                                            xml.skipCurrentElement();
                                        }
                                    }
                                } else {
                                    xml.skipCurrentElement();
                                }
                            }
                            QColor color;
                            color.setRgbF(red, green, blue);

                            if (keyName == "Background Color") {
                                theme.background = color;
                            } else if (keyName == "Foreground Color") {
                                theme.foreground = color;
                            } else if (keyName.startsWith("Ansi ") && keyName.endsWith(" Color")) {
                                int index = keyName.mid(5, keyName.length() - 11).toInt();
                                if (index >= 0 && index < 16) {
                                    theme.palette[index] = color;
                                }
                            }
                        } else {
                            xml.skipCurrentElement();
                        }
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        }
    }
    return theme;
}

QList<TerminalTheme::ThemeInfo> TerminalTheme::builtInThemes() {
    Q_INIT_RESOURCE(KodoTermThemes);
    QList<ThemeInfo> themes;
    QDirIterator it(":/KodoTermThemes",
                    QStringList() << "*.colorscheme" << "*.json" << "*.itermcolors", QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ThemeInfo info;
        info.path = it.filePath();
        if (info.path.endsWith(".colorscheme")) {
            info.format = ThemeFormat::Konsole;
            QSettings settings(info.path, QSettings::IniFormat);
            info.name = settings.value("General/Description", it.fileName()).toString();
        } else if (info.path.endsWith(".itermcolors")) {
            info.format = ThemeFormat::ITerm;
            info.name = QFileInfo(info.path).baseName();
            // Optional: Parse the file to find "Name" comment if available, but filename is usually
            // good enough.
        } else {
            info.format = ThemeFormat::WindowsTerminal;
            QFile file(info.path);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                info.name = doc.object().value("name").toString();
            }
            if (info.name.isEmpty()) {
                info.name = it.fileName();
            }
        }
        themes.append(info);
    }
    std::sort(themes.begin(), themes.end(), [](const ThemeInfo &a, const ThemeInfo &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return themes;
}

QJsonObject TerminalTheme::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["foreground"] = foreground.name();
    obj["background"] = background.name();
    QJsonArray paletteArray;
    for (const auto &c : palette) {
        paletteArray.append(c.name());
    }
    obj["palette"] = paletteArray;
    return obj;
}

TerminalTheme TerminalTheme::fromJson(const QJsonObject &json) {
    TerminalTheme theme = defaultTheme();
    if (json.contains("name")) {
        theme.name = json["name"].toString();
    }
    if (json.contains("foreground")) {
        theme.foreground = QColor(json["foreground"].toString());
    }
    if (json.contains("background")) {
        theme.background = QColor(json["background"].toString());
    }
    if (json.contains("palette") && json["palette"].isArray()) {
        QJsonArray arr = json["palette"].toArray();
        for (int i = 0; i < std::min(16, (int)arr.size()); ++i) {
            theme.palette[i] = QColor(arr[i].toString());
        }
    }
    return theme;
}

void TerminalTheme::save(QSettings &settings, const QString &group) const {
    if (!group.isEmpty()) {
        settings.beginGroup(group);
    }
    settings.setValue("name", name);
    settings.setValue("foreground", foreground.name());
    settings.setValue("background", background.name());
    QStringList paletteList;
    for (const auto &c : palette) {
        paletteList << c.name();
    }
    settings.setValue("palette", paletteList);
    if (!group.isEmpty()) {
        settings.endGroup();
    }
}

void TerminalTheme::load(QSettings &settings, const QString &group) {
    if (!group.isEmpty()) {
        settings.beginGroup(group);
    }
    name = settings.value("name", "Default").toString();
    foreground = QColor(settings.value("foreground", "#aaaaaa").toString());
    background = QColor(settings.value("background", "#000000").toString());
    QStringList paletteList = settings.value("palette").toStringList();
    if (paletteList.size() >= 16) {
        for (int i = 0; i < 16; ++i) {
            palette[i] = QColor(paletteList[i]);
        }
    } else {
        *this = defaultTheme(); // Fallback if palette is incomplete or missing
                                // Re-apply overrides if any
        if (settings.contains("foreground")) {
            foreground = QColor(settings.value("foreground").toString());
        }
        if (settings.contains("background")) {
            background = QColor(settings.value("background").toString());
        }
    }
    if (!group.isEmpty()) {
        settings.endGroup();
    }
}

KodoTermConfig::KodoTermConfig(QSettings &settings) {
    setDefaults();
    load(settings);
}

KodoTermConfig::KodoTermConfig() { setDefaults(); }

void KodoTermConfig::setDefaults() {
    font = QFont("Monospace", 10);
    font.setStyleHint(QFont::Monospace);
    font.setKerning(false);
    textAntialiasing = false;
    font.setStyleStrategy(QFont::NoAntialias);

    customBoxDrawing = false;
    copyOnSelect = true;
    pasteOnMiddleClick = true;
    mouseWheelZoom = true;
    visualBell = true;
    audibleBell = true;
    tripleClickSelectsLine = true;
    enableLogging = true;
    logDirectory =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/KodoShell";
    wordSelectionRegex = "[a-zA-Z0-9_\\.\\-\\/~\\:]+";
    maxScrollback = 1000;
    theme = TerminalTheme::defaultTheme();
}

void KodoTermConfig::load(const QJsonObject &json) {
    if (json.contains("font")) {
        QJsonObject fontObj = json["font"].toObject();
        font.setFamily(fontObj["family"].toString());
        font.setPointSizeF(fontObj["size"].toDouble());
    }
    if (json.contains("textAntialiasing")) {
        textAntialiasing = json["textAntialiasing"].toBool();
    }
    font.setKerning(false);
    font.setStyleStrategy(textAntialiasing ? QFont::PreferAntialias : QFont::NoAntialias);

    if (json.contains("customBoxDrawing")) {
        customBoxDrawing = json["customBoxDrawing"].toBool();
    }
    if (json.contains("copyOnSelect")) {
        copyOnSelect = json["copyOnSelect"].toBool();
    }
    if (json.contains("pasteOnMiddleClick")) {
        pasteOnMiddleClick = json["pasteOnMiddleClick"].toBool();
    }
    if (json.contains("mouseWheelZoom")) {
        mouseWheelZoom = json["mouseWheelZoom"].toBool();
    }
    if (json.contains("visualBell")) {
        visualBell = json["visualBell"].toBool();
    }
    if (json.contains("audibleBell")) {
        audibleBell = json["audibleBell"].toBool();
    }
    if (json.contains("tripleClickSelectsLine")) {
        tripleClickSelectsLine = json["tripleClickSelectsLine"].toBool();
    }
    if (json.contains("enableLogging")) {
        enableLogging = json["enableLogging"].toBool();
    }
    if (json.contains("logDirectory")) {
        logDirectory = json["logDirectory"].toString();
    }
    if (json.contains("wordSelectionRegex")) {
        wordSelectionRegex = json["wordSelectionRegex"].toString();
    }
    if (json.contains("maxScrollback")) {
        maxScrollback = json["maxScrollback"].toInt();
    }
    if (json.contains("theme")) {
        theme = TerminalTheme::fromJson(json["theme"].toObject());
    }
}

QJsonObject KodoTermConfig::saveToJson() const {
    QJsonObject obj;
    QJsonObject fontObj;
    fontObj["family"] = font.family();
    fontObj["size"] = font.pointSizeF();
    obj["font"] = fontObj;
    obj["textAntialiasing"] = textAntialiasing;
    obj["customBoxDrawing"] = customBoxDrawing;
    obj["copyOnSelect"] = copyOnSelect;
    obj["pasteOnMiddleClick"] = pasteOnMiddleClick;
    obj["mouseWheelZoom"] = mouseWheelZoom;
    obj["visualBell"] = visualBell;
    obj["audibleBell"] = audibleBell;
    obj["tripleClickSelectsLine"] = tripleClickSelectsLine;
    obj["enableLogging"] = enableLogging;
    obj["logDirectory"] = logDirectory;
    obj["wordSelectionRegex"] = wordSelectionRegex;
    obj["maxScrollback"] = maxScrollback;
    obj["theme"] = theme.toJson();
    return obj;
}

void KodoTermConfig::load(QSettings &settings) {
    if (settings.contains("font/family")) {
        font.setFamily(settings.value("font/family").toString());
        font.setPointSizeF(settings.value("font/size", 10).toDouble());
    }
    textAntialiasing = settings.value("textAntialiasing", textAntialiasing).toBool();
    font.setKerning(false);
    font.setStyleStrategy(textAntialiasing ? QFont::PreferAntialias : QFont::NoAntialias);
    customBoxDrawing = settings.value("customBoxDrawing", customBoxDrawing).toBool();
    copyOnSelect = settings.value("copyOnSelect", copyOnSelect).toBool();
    pasteOnMiddleClick = settings.value("pasteOnMiddleClick", pasteOnMiddleClick).toBool();
    mouseWheelZoom = settings.value("mouseWheelZoom", mouseWheelZoom).toBool();
    visualBell = settings.value("visualBell", visualBell).toBool();
    audibleBell = settings.value("audibleBell", audibleBell).toBool();
    tripleClickSelectsLine =
        settings.value("tripleClickSelectsLine", tripleClickSelectsLine).toBool();
    enableLogging = settings.value("enableLogging", enableLogging).toBool();
    logDirectory = settings.value("logDirectory", logDirectory).toString();
    wordSelectionRegex = settings.value("wordSelectionRegex", wordSelectionRegex).toString();
    maxScrollback = settings.value("maxScrollback", maxScrollback).toInt();
    theme.load(settings, "Theme");
}

void KodoTermConfig::save(QSettings &settings) const {
    settings.setValue("font/family", font.family());
    settings.setValue("font/size", font.pointSizeF());
    settings.setValue("textAntialiasing", textAntialiasing);
    settings.setValue("customBoxDrawing", customBoxDrawing);
    settings.setValue("copyOnSelect", copyOnSelect);
    settings.setValue("pasteOnMiddleClick", pasteOnMiddleClick);
    settings.setValue("mouseWheelZoom", mouseWheelZoom);
    settings.setValue("visualBell", visualBell);
    settings.setValue("audibleBell", audibleBell);
    settings.setValue("tripleClickSelectsLine", tripleClickSelectsLine);
    settings.setValue("enableLogging", enableLogging);
    settings.setValue("logDirectory", logDirectory);
    settings.setValue("wordSelectionRegex", wordSelectionRegex);
    settings.setValue("maxScrollback", maxScrollback);
    theme.save(settings, "Theme");
}
