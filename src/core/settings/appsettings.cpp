#include "appsettings.h"

#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <utility>

#include "core/settings/settingsregistry.h"

static QSettings &settings()
{
    static QSettings s;
    return s;
}

QString AppSettings::keyLanguage() { return "application/language"; }
QString AppSettings::keyExcludedPatterns() { return "workspace/files/excludedPatterns"; }

QString AppSettings::language()
{
    const QString stored = settings().value(keyLanguage()).toString().trimmed();
    return stored.isEmpty() ? QStringLiteral("en") : stored;
}

void AppSettings::setLanguage(const QString& locale)
{
    settings().setValue(keyLanguage(), locale.trimmed());
}

QVariant AppSettings::value(const QString &key, const QVariant &defaultValue)
{
    return settings().value(key, defaultValue);
}

void AppSettings::setValue(const QString &key, const QVariant &value)
{
    settings().setValue(key, value);
}

bool AppSettings::contains(const QString &key)
{
    return settings().contains(key);
}

QStringList AppSettings::excludedPatterns()
{
    const QString raw = settings().value(keyExcludedPatterns()).toString();
    if (raw.trimmed().isEmpty())
        return {};
    QStringList list = raw.split(';', Qt::SkipEmptyParts);
    for (QString &p : list)
        p = p.trimmed();
    list.removeAll(QString());
    return list;
}

void AppSettings::setExcludedPatterns(const QStringList &patterns)
{
    settings().setValue(keyExcludedPatterns(), patterns.join(';'));
    emit SettingsNotifier::instance()->excludedPatternsChanged();
}

SettingsNotifier *SettingsNotifier::instance()
{
    static SettingsNotifier s;
    return &s;
}

bool AppSettings::exportToIni(const QString &filePath, QString *error)
{
    const QFileInfo fi(filePath);
    if (fi.filePath().trimmed().isEmpty()) {
        if (error) *error = QObject::tr("Empty file path");
        return false;
    }

    QSettings out(fi.filePath(), QSettings::IniFormat);
    out.clear();
    // Экспортируем всё, что знает ядро: настройки приложения и все ключи,
    // которые модули объявили в фабрике настроек. Хардкод-список исчез:
    // новый модуль добавляется в экспорт автоматически через свою схему.
    const QStringList moduleKeys = SettingsRegistry::instance().moduleOptionKeys();
    for (const QString& key : {keyLanguage(), keyExcludedPatterns()}) {
        if (settings().contains(key))
            out.setValue(key, settings().value(key));
    }
    for (const QString& key : moduleKeys) {
        if (settings().contains(key))
            out.setValue(key, settings().value(key));
    }
    out.sync();
    if (out.status() != QSettings::NoError) {
        if (error) *error = QObject::tr("Failed to write INI file");
        return false;
    }
    return true;
}

bool AppSettings::importFromIni(const QString &filePath, QString *error)
{
    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        if (error) *error = QObject::tr("File does not exist");
        return false;
    }

    QSettings in(fi.filePath(), QSettings::IniFormat);
    if (in.status() != QSettings::NoError) {
        if (error) *error = QObject::tr("Failed to read INI file");
        return false;
    }

    // Импортируем только известные ключи: приложение + схема модулей.
    const QStringList moduleKeys = SettingsRegistry::instance().moduleOptionKeys();

    bool excludedPatternsChanged = false;
    QSet<QString> changedModuleKeys;

    for (const QString& key : {keyLanguage(), keyExcludedPatterns()}) {
        if (in.contains(key))
            settings().setValue(key, in.value(key));
        if (in.contains(key) && key == keyExcludedPatterns())
            excludedPatternsChanged = true;
    }
    for (const QString& key : moduleKeys) {
        if (!in.contains(key))
            continue;
        settings().setValue(key, in.value(key));
        changedModuleKeys.insert(key);
    }

    settings().sync();
    if (settings().status() != QSettings::NoError) {
        if (error) *error = QObject::tr("Failed to apply settings");
        return false;
    }

    if (excludedPatternsChanged)
        emit SettingsNotifier::instance()->excludedPatternsChanged();
    // Ядро не знает, что означают эти ключи; оно лишь сообщает «такой-то
    // ключ изменился», а модуль сам решает, что с этим делать.
    for (const QString& key : std::as_const(changedModuleKeys))
        emit SettingsNotifier::instance()->settingsChanged(key);
    return true;
}
