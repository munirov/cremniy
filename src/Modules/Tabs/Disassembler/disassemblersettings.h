#ifndef DISASSEMBLERSETTINGS_H
#define DISASSEMBLERSETTINGS_H

#include <QString>

// Настройки модуля Disassembler.
//
// ВСЁ, что касается настроек этого модуля (ключи, умолчания, типы и
// уведомления об изменении), живёт в САМОМ модуле. Ядро (AppSettings)
// хранит только пары "ключ -> значение" и не знает ни одного из этих
// ключей. Поэтому разработчик может добавить/изменить настройку модуля,
// не трогая core/settings.
//
// Ключи модуля также объявляются фабрике настроек
// (SettingsRegistry::moduleOptions) в disassemblersettingspage.cpp — они
// автоматически попадают в экспорт/импорт INI.
namespace DisassemblerSettings {

enum class Backend {
    Objdump  = 0,
    Radare2  = 1,
    Capstone = 2,
    };

enum class AnalysisLevel {
    None = 0,   // r2 без анализа (быстро)
    Aa   = 1,   // aa (базовый)
    Aaa  = 2,   // aaa (полный)
};

enum class Syntax {
    Intel = 0,
    Att   = 1,
};

QString keyBackend();
QString keyObjdumpPath();
QString keyRadare2Path();
QString keyInsnLimitPerSection();
QString keyRadare2AnalysisLevel();
QString keyAsmSyntax();
QString keyRadare2PreCommands();

Backend backend();
void setBackend(Backend backend);

QString objdumpPath();
void setObjdumpPath(const QString &path);

QString radare2Path();
void setRadare2Path(const QString &path);

int insnLimitPerSection();
void setInsnLimitPerSection(int limit);

AnalysisLevel radare2AnalysisLevel();
void setRadare2AnalysisLevel(AnalysisLevel level);

Syntax asmSyntax();
void setAsmSyntax(Syntax syntax);

QString radare2PreCommands();
void setRadare2PreCommands(const QString &cmds);

} // namespace DisassemblerSettings

#endif // DISASSEMBLERSETTINGS_H