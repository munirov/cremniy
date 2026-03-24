#ifndef THEMECOLORS_H
#define THEMECOLORS_H

#include <QString>
#include <QColor>
#include <QJsonObject>

struct ThemeColors {
    // Основные цвета
    QColor backgroundColor;
    QColor textColor;
    
    // Кнопки
    QColor buttonBackground;
    QColor buttonHoverBackground;
    QColor buttonPressedBackground;
    QColor buttonBorder;
    QColor buttonTextColor;
    
    // Поля ввода
    QColor inputBackground;
    QColor inputBorder;
    QColor inputFocusBorder;
    QColor inputTextColor;
    QColor inputSelectionBackground;
    
    // Элементы списков
    QColor listBackground;
    QColor listAlternateBackground;
    QColor listSelectedBackground;
    QColor listHoverBackground;
    QColor listTextColor;
    
    // Меню
    QColor menuBackground;
    QColor menuHoverBackground;
    QColor menuSelectedBackground;
    QColor menuTextColor;
    QColor menuBorder;
    
    // Другое
    QColor accentColor;
    QColor borderColor;
    QColor disabledTextColor;
    
    // Синтаксис-подсветка (для дизассемблера и редакторов)
    QColor syntaxAddrColor;
    QColor syntaxBytesColor;
    QColor syntaxMnemonicColor;
    QColor syntaxRegColor;
    QColor syntaxImmColor;
    QColor syntaxSymColor;
    QColor syntaxCommentColor;
    
    // Методы
    ThemeColors();
    static ThemeColors light();
    static ThemeColors dark();
    
    QJsonObject toJson() const;
    static ThemeColors fromJson(const QJsonObject &obj);
};

#endif // THEMECOLORS_H
