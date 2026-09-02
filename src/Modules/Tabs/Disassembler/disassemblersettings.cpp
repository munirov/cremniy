#include "disassemblersettings.h"

#include "core/settings/appsettings.h"

namespace DisassemblerSettings {

QString keyBackend() { return QStringLiteral("modules/disassembler/backend"); }
QString keyObjdumpPath() { return QStringLiteral("modules/disassembler/objdumpPath"); }
QString keyRadare2Path() { return QStringLiteral("modules/disassembler/radare2Path"); }
QString keyInsnLimitPerSection() { return QStringLiteral("modules/disassembler/insnLimitPerSection"); }
QString keyRadare2AnalysisLevel() { return QStringLiteral("modules/disassembler/radare2/analysisLevel"); }
QString keyAsmSyntax() { return QStringLiteral("modules/disassembler/asmSyntax"); }
QString keyRadare2PreCommands() { return QStringLiteral("modules/disassembler/radare2/preCommands"); }

static void notifyChanged(const QString &key)
{
    emit SettingsNotifier::instance()->settingsChanged(key);
}

Backend backend()
{
    const int v = AppSettings::value(keyBackend(), static_cast<int>(Backend::Objdump)).toInt();
    if (v == static_cast<int>(Backend::Capstone)) return Backend::Capstone;
    if (v == static_cast<int>(Backend::Radare2)) return Backend::Radare2;
    return Backend::Objdump;
}

void setBackend(Backend b)
{
    AppSettings::setValue(keyBackend(), static_cast<int>(b));
    notifyChanged(keyBackend());
}

QString objdumpPath()
{
    return AppSettings::value(keyObjdumpPath(), QString()).toString().trimmed();
}

void setObjdumpPath(const QString &path)
{
    AppSettings::setValue(keyObjdumpPath(), path.trimmed());
    notifyChanged(keyObjdumpPath());
}

QString radare2Path()
{
    return AppSettings::value(keyRadare2Path(), QString()).toString().trimmed();
}

void setRadare2Path(const QString &path)
{
    AppSettings::setValue(keyRadare2Path(), path.trimmed());
    notifyChanged(keyRadare2Path());
}

int insnLimitPerSection()
{
    const int v = AppSettings::value(keyInsnLimitPerSection(), 4000).toInt();
    if (v < 50) return 50;
    if (v > 200000) return 200000;
    return v;
}

void setInsnLimitPerSection(int limit)
{
    AppSettings::setValue(keyInsnLimitPerSection(), limit);
    notifyChanged(keyInsnLimitPerSection());
}

AnalysisLevel radare2AnalysisLevel()
{
    const int v = AppSettings::value(keyRadare2AnalysisLevel(),
                                     static_cast<int>(AnalysisLevel::None)).toInt();
    if (v == static_cast<int>(AnalysisLevel::Aaa)) return AnalysisLevel::Aaa;
    if (v == static_cast<int>(AnalysisLevel::Aa)) return AnalysisLevel::Aa;
    return AnalysisLevel::None;
}

void setRadare2AnalysisLevel(AnalysisLevel level)
{
    AppSettings::setValue(keyRadare2AnalysisLevel(), static_cast<int>(level));
    notifyChanged(keyRadare2AnalysisLevel());
}

Syntax asmSyntax()
{
    const int v = AppSettings::value(keyAsmSyntax(), static_cast<int>(Syntax::Intel)).toInt();
    if (v == static_cast<int>(Syntax::Att)) return Syntax::Att;
    return Syntax::Intel;
}

void setAsmSyntax(Syntax syntax)
{
    AppSettings::setValue(keyAsmSyntax(), static_cast<int>(syntax));
    notifyChanged(keyAsmSyntax());
}

QString radare2PreCommands()
{
    return AppSettings::value(keyRadare2PreCommands(), QString()).toString().trimmed();
}

void setRadare2PreCommands(const QString &cmds)
{
    AppSettings::setValue(keyRadare2PreCommands(), cmds.trimmed());
    notifyChanged(keyRadare2PreCommands());
}

} // namespace DisassemblerSettings