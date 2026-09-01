#ifndef CODEEDITORSETTINGS_H
#define CODEEDITORSETTINGS_H

#include <QString>

// Настройки модуля CodeEditor.
//
// ВСЁ, что касается настроек этого модуля (ключи, умолчания и уведомления
// об изменении), живёт в САМОМ модуле. Ядро (AppSettings) хранит только
// пары "ключ -> значение" и не знает ни одного из этих ключей.
//
// Ключи модуля также объявляются фабрике настроек
// (SettingsRegistry::moduleOptions) в codeeditorsettingspage.cpp — они
// автоматически попадают в экспорт/импорт INI.
namespace CodeEditorSettings {

QString keyGitBlameEnabled();
QString keyGitBlameColor();
QString keyGitBlamePadding();

bool gitBlameEnabled();
void setGitBlameEnabled(bool enabled);

// Default Gray chain is documented in the settings page UI.
QString gitBlameColor();
void setGitBlameColor(const QString &color);

int gitBlamePadding();
void setGitBlamePadding(int padding);

} // namespace CodeEditorSettings

#endif // CODEEDITORSETTINGS_H
