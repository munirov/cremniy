#include "appsettings.h"

#include <QSettings>
#include <QFileInfo>

static QSettings &settings()
{
    static QSettings s;
    return s;
}

QString AppSettings::keyDisasmBackend() { return "disasm/backend"; }
QString AppSettings::keyObjdumpPath()   { return "tools/objdumpPath"; }
QString AppSettings::keyRadare2Path()   { return "tools/radare2Path"; }
QString AppSettings::keyInsnLimitPerSection() { return "disasm/insnLimitPerSection"; }
QString AppSettings::keyRadare2AnalysisLevel() { return "radare2/analysisLevel"; }
QString AppSettings::keyAsmSyntax() { return "disasm/asmSyntax"; }
QString AppSettings::keyRadare2PreCommands() { return "radare2/preCommands"; }

AppSettings::DisasmBackend AppSettings::disasmBackend()
{
    const int v = settings().value(keyDisasmBackend(), static_cast<int>(DisasmBackend::Objdump)).toInt();
    if (v == static_cast<int>(DisasmBackend::Radare2)) return DisasmBackend::Radare2;
    return DisasmBackend::Objdump;
}

void AppSettings::setDisasmBackend(DisasmBackend backend)
{
    settings().setValue(keyDisasmBackend(), static_cast<int>(backend));
}

QString AppSettings::objdumpPath()
{
    return settings().value(keyObjdumpPath()).toString().trimmed();
}

void AppSettings::setObjdumpPath(const QString &path)
{
    settings().setValue(keyObjdumpPath(), path.trimmed());
}

QString AppSettings::radare2Path()
{
    return settings().value(keyRadare2Path()).toString().trimmed();
}

void AppSettings::setRadare2Path(const QString &path)
{
    settings().setValue(keyRadare2Path(), path.trimmed());
}

int AppSettings::disasmInsnLimitPerSection()
{
    const int v = settings().value(keyInsnLimitPerSection(), 4000).toInt();
    if (v < 50) return 50;
    if (v > 200000) return 200000;
    return v;
}

void AppSettings::setDisasmInsnLimitPerSection(int limit)
{
    settings().setValue(keyInsnLimitPerSection(), limit);
}

AppSettings::Radare2AnalysisLevel AppSettings::radare2AnalysisLevel()
{
    const int v = settings().value(keyRadare2AnalysisLevel(), static_cast<int>(Radare2AnalysisLevel::None)).toInt();
    if (v == static_cast<int>(Radare2AnalysisLevel::Aaa)) return Radare2AnalysisLevel::Aaa;
    if (v == static_cast<int>(Radare2AnalysisLevel::Aa)) return Radare2AnalysisLevel::Aa;
    return Radare2AnalysisLevel::None;
}

void AppSettings::setRadare2AnalysisLevel(Radare2AnalysisLevel lvl)
{
    settings().setValue(keyRadare2AnalysisLevel(), static_cast<int>(lvl));
}

AppSettings::AsmSyntax AppSettings::asmSyntax()
{
    const int v = settings().value(keyAsmSyntax(), static_cast<int>(AsmSyntax::Intel)).toInt();
    if (v == static_cast<int>(AsmSyntax::Att)) return AsmSyntax::Att;
    return AsmSyntax::Intel;
}

void AppSettings::setAsmSyntax(AsmSyntax syntax)
{
    settings().setValue(keyAsmSyntax(), static_cast<int>(syntax));
}

QString AppSettings::radare2PreCommands()
{
    return settings().value(keyRadare2PreCommands()).toString().trimmed();
}

void AppSettings::setRadare2PreCommands(const QString &cmds)
{
    settings().setValue(keyRadare2PreCommands(), cmds.trimmed());
}

static void copyKeys(QSettings &dst, QSettings &src)
{
    const QStringList keys = src.allKeys();
    for (const QString &k : keys)
        dst.setValue(k, src.value(k));
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
    copyKeys(out, settings());
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

    /* Only import known keys (so random settings won't pollute). */
    const QStringList allowed = {
        keyDisasmBackend(),
        keyObjdumpPath(),
        keyRadare2Path(),
        keyInsnLimitPerSection(),
        keyRadare2AnalysisLevel(),
        keyAsmSyntax(),
        keyRadare2PreCommands(),
    };

    for (const QString &k : allowed) {
        if (in.contains(k))
            settings().setValue(k, in.value(k));
    }
    settings().sync();
    if (settings().status() != QSettings::NoError) {
        if (error) *error = QObject::tr("Failed to apply settings");
        return false;
    }
    return true;
}

