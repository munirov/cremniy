#include "filessettingspage.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QSet>
#include <QVBoxLayout>

#include "core/settings/appsettings.h"
#include "core/settings/settingsregistry.h"

namespace {
QString categoryTitle()
{
    return QObject::tr("Workspace");
}

QString pageTitle()
{
    return QObject::tr("Files");
}

const bool registered = SettingsRegistry::instance().registerPage({
    "workspace.files",
    "workspace",
    &categoryTitle,
    200,
    &pageTitle,
    100,
    "application",
    [](QWidget* parent) { return new FilesSettingsPage(parent); }
});
}

FilesSettingsPage::FilesSettingsPage(QWidget* parent)
    : SettingsPage(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel(tr("One pattern per line. Examples: node_modules, .git, *.log, dist/"), this);
    hint->setObjectName("settingsHintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_excludedPatterns = new QPlainTextEdit(this);
    m_excludedPatterns->setPlaceholderText(tr("node_modules\n.git\n*.log"));
    layout->addWidget(m_excludedPatterns);
}

void FilesSettingsPage::load()
{
    m_excludedPatterns->setPlainText(AppSettings::excludedPatterns().join('\n'));
}

bool FilesSettingsPage::validate(QString* errorMessage) const
{
    QSet<QString> patterns;
    for (const auto& pattern : m_excludedPatterns->toPlainText().split('\n', Qt::SkipEmptyParts)) {
        const QString normalized = pattern.trimmed();
        if (normalized.isEmpty())
            continue;
        if (patterns.contains(normalized)) {
            if (errorMessage)
                *errorMessage = tr("Excluded patterns must not be repeated.");
            return false;
        }
        patterns.insert(normalized);
    }
    return true;
}

void FilesSettingsPage::apply()
{
    QStringList patterns;
    for (const auto& pattern : m_excludedPatterns->toPlainText().split('\n', Qt::SkipEmptyParts)) {
        const QString normalized = pattern.trimmed();
        if (!normalized.isEmpty())
            patterns.append(normalized);
    }
    AppSettings::setExcludedPatterns(patterns);
}
