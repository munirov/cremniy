#include "codeeditorsettings.h"

#include "core/settings/appsettings.h"

namespace CodeEditorSettings {

static const QString kDefaultBlameColor = QStringLiteral("#6D6552");
static const int kDefaultBlamePadding = 6;

QString keyGitBlameEnabled() { return QStringLiteral("modules/codeEditor/gitBlameEnabled"); }
QString keyGitBlameColor() { return QStringLiteral("modules/codeEditor/gitBlameColor"); }
QString keyGitBlamePadding() { return QStringLiteral("modules/codeEditor/gitBlamePadding"); }

bool gitBlameEnabled()
{
    return AppSettings::value(keyGitBlameEnabled(), false).toBool();
}

void setGitBlameEnabled(bool enabled)
{
    if (gitBlameEnabled() == enabled)
        return;

    AppSettings::setValue(keyGitBlameEnabled(), enabled);
    emit SettingsNotifier::instance()->settingsChanged(keyGitBlameEnabled());
}

QString gitBlameColor()
{
    return AppSettings::value(keyGitBlameColor(), kDefaultBlameColor).toString();
}

void setGitBlameColor(const QString &color)
{
    AppSettings::setValue(keyGitBlameColor(), color);
    emit SettingsNotifier::instance()->settingsChanged(keyGitBlameColor());
}

int gitBlamePadding()
{
    return AppSettings::value(keyGitBlamePadding(), kDefaultBlamePadding).toInt();
}

void setGitBlamePadding(int padding)
{
    AppSettings::setValue(keyGitBlamePadding(), padding);
    emit SettingsNotifier::instance()->settingsChanged(keyGitBlamePadding());
}

} // namespace CodeEditorSettings
