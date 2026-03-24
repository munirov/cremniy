#include "themecolors.h"
#include <QJsonObject>
#include <QJsonArray>

ThemeColors::ThemeColors()
    : backgroundColor("#ffffff"),
      textColor("#000000"),
      buttonBackground("#e8e8e8"),
      buttonHoverBackground("#d0d0d0"),
      buttonPressedBackground("#c0c0c0"),
      buttonBorder("#cccccc"),
      buttonTextColor("#000000"),
      inputBackground("#ffffff"),
      inputBorder("#cccccc"),
      inputFocusBorder("#4a90e2"),
      inputTextColor("#000000"),
      inputSelectionBackground("#b3d9ff"),
      listBackground("#ffffff"),
      listAlternateBackground("#f9f9f9"),
      listSelectedBackground("#e8f4ff"),
      listHoverBackground("#f0f0f0"),
      listTextColor("#000000"),
      menuBackground("#f5f5f5"),
      menuHoverBackground("#c0d9ff"),
      menuSelectedBackground("#c0d9ff"),
      menuTextColor("#000000"),
      menuBorder("#cccccc"),
      accentColor("#4a90e2"),
      borderColor("#cccccc"),
      disabledTextColor("#999999"),
      syntaxAddrColor("#4aa3ff"),
      syntaxBytesColor("#21c55d"),
      syntaxMnemonicColor("#ef4444"),
      syntaxRegColor("#22c55e"),
      syntaxImmColor("#fb7185"),
      syntaxSymColor("#3b82f6"),
      syntaxCommentColor("#34d399")
{
}

ThemeColors ThemeColors::light()
{
    return ThemeColors();
}

ThemeColors ThemeColors::dark()
{
    ThemeColors colors;
    colors.backgroundColor = "#262626";
    colors.textColor = "#ffffff";
    colors.buttonBackground = "#262626";
    colors.buttonHoverBackground = "#162033";
    colors.buttonPressedBackground = "#162033";
    colors.buttonBorder = "#1f1f1f";
    colors.buttonTextColor = "#ffffff";
    colors.inputBackground = "#262626";
    colors.inputBorder = "#1f1f1f";
    colors.inputFocusBorder = "#2c4c7c";
    colors.inputTextColor = "#ffffff";
    colors.inputSelectionBackground = "#333333";
    colors.listBackground = "#1f1f1f";
    colors.listAlternateBackground = "#0a0f18";
    colors.listSelectedBackground = "#262626";
    colors.listHoverBackground = "#333333";
    colors.listTextColor = "#ffffff";
    colors.menuBackground = "#262626";
    colors.menuHoverBackground = "#333333";
    colors.menuSelectedBackground = "#333333";
    colors.menuTextColor = "#ffffff";
    colors.menuBorder = "#111111";
    colors.accentColor = "#2626d5";
    colors.borderColor = "#1f1f1f";
    colors.disabledTextColor = "#666666";
    // Синтаксис-подсветка для темной темы
    colors.syntaxAddrColor = "#2f7bff";
    colors.syntaxBytesColor = "#21c55d";
    colors.syntaxMnemonicColor = "#ef4444";
    colors.syntaxRegColor = "#22c55e";
    colors.syntaxImmColor = "#fb7185";
    colors.syntaxSymColor = "#3b82f6";
    colors.syntaxCommentColor = "#34d399";
    return colors;
}

QJsonObject ThemeColors::toJson() const
{
    QJsonObject obj;
    obj["backgroundColor"] = backgroundColor.name();
    obj["textColor"] = textColor.name();
    obj["buttonBackground"] = buttonBackground.name();
    obj["buttonHoverBackground"] = buttonHoverBackground.name();
    obj["buttonPressedBackground"] = buttonPressedBackground.name();
    obj["buttonBorder"] = buttonBorder.name();
    obj["buttonTextColor"] = buttonTextColor.name();
    obj["inputBackground"] = inputBackground.name();
    obj["inputBorder"] = inputBorder.name();
    obj["inputFocusBorder"] = inputFocusBorder.name();
    obj["inputTextColor"] = inputTextColor.name();
    obj["inputSelectionBackground"] = inputSelectionBackground.name();
    obj["listBackground"] = listBackground.name();
    obj["listAlternateBackground"] = listAlternateBackground.name();
    obj["listSelectedBackground"] = listSelectedBackground.name();
    obj["listHoverBackground"] = listHoverBackground.name();
    obj["listTextColor"] = listTextColor.name();
    obj["menuBackground"] = menuBackground.name();
    obj["menuHoverBackground"] = menuHoverBackground.name();
    obj["menuSelectedBackground"] = menuSelectedBackground.name();
    obj["menuTextColor"] = menuTextColor.name();
    obj["menuBorder"] = menuBorder.name();
    obj["accentColor"] = accentColor.name();
    obj["borderColor"] = borderColor.name();
    obj["disabledTextColor"] = disabledTextColor.name();
    obj["syntaxAddrColor"] = syntaxAddrColor.name();
    obj["syntaxBytesColor"] = syntaxBytesColor.name();
    obj["syntaxMnemonicColor"] = syntaxMnemonicColor.name();
    obj["syntaxRegColor"] = syntaxRegColor.name();
    obj["syntaxImmColor"] = syntaxImmColor.name();
    obj["syntaxSymColor"] = syntaxSymColor.name();
    obj["syntaxCommentColor"] = syntaxCommentColor.name();
    return obj;
}

ThemeColors ThemeColors::fromJson(const QJsonObject &obj)
{
    ThemeColors colors;
    colors.backgroundColor = QColor(obj["backgroundColor"].toString("#ffffff"));
    colors.textColor = QColor(obj["textColor"].toString("#000000"));
    colors.buttonBackground = QColor(obj["buttonBackground"].toString("#e8e8e8"));
    colors.buttonHoverBackground = QColor(obj["buttonHoverBackground"].toString("#d0d0d0"));
    colors.buttonPressedBackground = QColor(obj["buttonPressedBackground"].toString("#c0c0c0"));
    colors.buttonBorder = QColor(obj["buttonBorder"].toString("#cccccc"));
    colors.buttonTextColor = QColor(obj["buttonTextColor"].toString("#000000"));
    colors.inputBackground = QColor(obj["inputBackground"].toString("#ffffff"));
    colors.inputBorder = QColor(obj["inputBorder"].toString("#cccccc"));
    colors.inputFocusBorder = QColor(obj["inputFocusBorder"].toString("#4a90e2"));
    colors.inputTextColor = QColor(obj["inputTextColor"].toString("#000000"));
    colors.inputSelectionBackground = QColor(obj["inputSelectionBackground"].toString("#b3d9ff"));
    colors.listBackground = QColor(obj["listBackground"].toString("#ffffff"));
    colors.listAlternateBackground = QColor(obj["listAlternateBackground"].toString("#f9f9f9"));
    colors.listSelectedBackground = QColor(obj["listSelectedBackground"].toString("#e8f4ff"));
    colors.listHoverBackground = QColor(obj["listHoverBackground"].toString("#f0f0f0"));
    colors.listTextColor = QColor(obj["listTextColor"].toString("#000000"));
    colors.menuBackground = QColor(obj["menuBackground"].toString("#f5f5f5"));
    colors.menuHoverBackground = QColor(obj["menuHoverBackground"].toString("#c0d9ff"));
    colors.menuSelectedBackground = QColor(obj["menuSelectedBackground"].toString("#c0d9ff"));
    colors.menuTextColor = QColor(obj["menuTextColor"].toString("#000000"));
    colors.menuBorder = QColor(obj["menuBorder"].toString("#cccccc"));
    colors.accentColor = QColor(obj["accentColor"].toString("#4a90e2"));
    colors.borderColor = QColor(obj["borderColor"].toString("#cccccc"));
    colors.disabledTextColor = QColor(obj["disabledTextColor"].toString("#999999"));
    colors.syntaxAddrColor = QColor(obj["syntaxAddrColor"].toString("#4aa3ff"));
    colors.syntaxBytesColor = QColor(obj["syntaxBytesColor"].toString("#21c55d"));
    colors.syntaxMnemonicColor = QColor(obj["syntaxMnemonicColor"].toString("#ef4444"));
    colors.syntaxRegColor = QColor(obj["syntaxRegColor"].toString("#22c55e"));
    colors.syntaxImmColor = QColor(obj["syntaxImmColor"].toString("#fb7185"));
    colors.syntaxSymColor = QColor(obj["syntaxSymColor"].toString("#3b82f6"));
    colors.syntaxCommentColor = QColor(obj["syntaxCommentColor"].toString("#34d399"));
    return colors;
}
