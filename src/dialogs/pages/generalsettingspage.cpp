#include "generalsettingspage.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "core/locale/LanguageManager.h"
#include "core/settings/appsettings.h"
#include "core/settings/settingsregistry.h"

namespace {
QString categoryTitle()
{
    return QObject::tr("Application");
}

QString pageTitle()
{
    return QObject::tr("General");
}

const bool registered = SettingsRegistry::instance().registerPage({
    "application.general",
    "application",
    &categoryTitle,
    100,
    &pageTitle,
    100,
    "application",
    [](QWidget* parent) { return new GeneralSettingsPage(parent); }
});
}

GeneralSettingsPage::GeneralSettingsPage(QWidget* parent)
    : SettingsPage(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_languageCombo = new QComboBox(this);
    for (const auto& locale : LanguageManager::supportedLanguages())
        m_languageCombo->addItem(QLocale(locale).nativeLanguageName(), locale);
    form->addRow(tr("Language"), m_languageCombo);

    layout->addLayout(form);
    auto* hint = new QLabel(tr("Restart the IDE after changing the language."), this);
    hint->setObjectName("settingsHintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);
}

void GeneralSettingsPage::load()
{
    const QString locale = AppSettings::language();
    const int index = m_languageCombo->findData(locale);
    m_languageCombo->setCurrentIndex(index >= 0 ? index : 0);
}

bool GeneralSettingsPage::validate(QString* errorMessage) const
{
    if (!m_languageCombo->currentData().isValid()) {
        if (errorMessage)
            *errorMessage = tr("Choose an application language.");
        return false;
    }
    return true;
}

void GeneralSettingsPage::apply()
{
    const QString locale = m_languageCombo->currentData().toString();
    if (locale == AppSettings::language())
        return;

    AppSettings::setLanguage(locale);
    QMessageBox::information(this, tr("Information"), tr("Restart the IDE to apply the language everywhere."));
}
