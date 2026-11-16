#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

// Центральное хранилище настроек.
//
// Ядро НЕ знает о настройках конкретных модулей. Здесь живут только
// настройки самого приложения (язык, исключённые паттерны) и обобщённый
// доступ «ключ -> значение». Какие ключи существуют, какие у них типы и
// умолчания — объявляет каждый модуль сам через SettingsRegistry
// (см. SettingsPageDescriptor::moduleOptions). Экспорт/импорт INI обходит
// все модульные ключи, зарегистрированные в фабрике настроек, поэтому при
// добавлении нового модуля core/settings менять не нужно.
class AppSettings
{
public:
    // ── Настройки самого приложения ───────────────────────────────────────
    static QString language();
    static void setLanguage(const QString& locale);

    // File-tree exclusion patterns ("node_modules", "*.log", ".git").
    static QStringList excludedPatterns();
    static void setExcludedPatterns(const QStringList &patterns);

    // ── Обобщённое хранилище ──────────────────────────────────────────────
    // Модули читают и пишут СВОИ ключи через эти методы. Ключи владеются
    // модулем и объявляются фабрике настроек (SettingsRegistry::moduleOptions),
    // поэтому ядро никогда не должно хардкодить "modules/...".
    static QVariant value(const QString &key, const QVariant &defaultValue = QVariant());
    static void setValue(const QString &key, const QVariant &value);
    static bool contains(const QString &key);

    // Import/export settings to share with others (INI file).
    static bool exportToIni(const QString &filePath, QString *error = nullptr);
    static bool importFromIni(const QString &filePath, QString *error = nullptr);

private:
    static QString keyLanguage();
    static QString keyExcludedPatterns();
};

class SettingsNotifier : public QObject
{
    Q_OBJECT
public:
    static SettingsNotifier *instance();
signals:
    void excludedPatternsChanged();
    // Универсальный сигнал: изменился ключ настройки. Ядро рассылает его при
    // импорте INI, а модули — при записи своих ключей. Подписчики сами
    // фильтруют ключи, которые им принадлежат (сравнивая с ключами, которые
    // модуль объявил в своей схеме настроек).
    void settingsChanged(const QString &key);
private:
    explicit SettingsNotifier(QObject *parent = nullptr) : QObject(parent) {}
};

#endif // APPSETTINGS_H
