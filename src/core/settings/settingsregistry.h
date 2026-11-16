#ifndef SETTINGSREGISTRY_H
#define SETTINGSREGISTRY_H

#include <functional>

#include <QList>
#include <QString>
#include <QVariant>
#include <QVector>

class QWidget;
class SettingsPage;

/**
 * @brief Описание одной настройки, которой владеет модуль.
 *
 * Ключ и значение по умолчанию объявляет САМ модуль (например, в своём
 * settings-файле). Ядро настроек никогда не знает конкретных модульных
 * ключей — оно лишь собирает их из фабрики и использует при
 * экспорте/импорте INI и при рассылке уведомлений об изменении.
 */
struct ModuleOptionDescriptor
{
    QString key;            // e.g. "modules/disassembler/objdumpPath"
    QVariant defaultValue;  // умолчание, которое модуль использует при чтении
    bool notify = true;     // слать SettingsNotifier::settingsChanged(key) при импорте
};

struct SettingsPageDescriptor
{
    QString pageId;
    QString categoryId;
    std::function<QString()> categoryTitle;
    int categoryOrder = 0;
    std::function<QString()> pageTitle;
    int pageOrder = 0;
    QString ownerId;
    std::function<SettingsPage*(QWidget*)> createPage;
    // Схема настроек модуля: реестр понимает структуру "Настройки -> Модули",
    // и каждый модуль через это поле отвечает на вопрос фабрики
    // "какие настройки ты поддерживаешь?".
    QList<ModuleOptionDescriptor> moduleOptions;
};

class SettingsRegistry
{
public:
    static SettingsRegistry& instance();

    bool registerPage(SettingsPageDescriptor descriptor);
    bool registerModulePage(const QString& moduleId, SettingsPageDescriptor descriptor);
    QVector<SettingsPageDescriptor> pages() const;

    /// Все ключи, объявленные модулями (для экспорта/импорта и уведомлений).
    QStringList moduleOptionKeys() const;

private:
    QVector<SettingsPageDescriptor> m_pages;
};

#endif // SETTINGSREGISTRY_H
