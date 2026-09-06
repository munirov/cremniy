#include "disassemblersettingspage.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "disassemblersettings.h"
#include "core/settings/settingsregistry.h"

namespace {
QString categoryTitle()
{
    return QObject::tr("Modules");
}

QString pageTitle()
{
    return QCoreApplication::translate("DisassemblerTab", "Disassembler");
}

const bool registered = SettingsRegistry::instance().registerModulePage("disassembler", {
    "modules.disassembler",
    "modules",
    &categoryTitle,
    300,
    &pageTitle,
    200,
    {},
    [](QWidget* parent) { return new DisassemblerSettingsPage(parent); },
    // Схема настроек этого модуля: фабрика собирает её, чтобы экспорт/
    // импорт INI знал о модуле, не имея о нём никакой информации в ядре.
    {
        { DisassemblerSettings::keyBackend(), static_cast<int>(DisassemblerSettings::Backend::Objdump) },
        { DisassemblerSettings::keyObjdumpPath(), QString() },
        { DisassemblerSettings::keyRadare2Path(), QString() },
        { DisassemblerSettings::keyInsnLimitPerSection(), 4000 },
        { DisassemblerSettings::keyRadare2AnalysisLevel(), static_cast<int>(DisassemblerSettings::AnalysisLevel::None) },
        { DisassemblerSettings::keyAsmSyntax(), static_cast<int>(DisassemblerSettings::Syntax::Intel) },
        { DisassemblerSettings::keyRadare2PreCommands(), QString() },
    }
});

QString resolvedExecutable(const QString& userPath, const QString& executableName)
{
    if (!userPath.trimmed().isEmpty())
        return userPath.trimmed();
    return QStandardPaths::findExecutable(executableName);
}

bool isRunnableExecutable(const QString& path)
{
    const QFileInfo info(path.trimmed());
    return !path.trimmed().isEmpty() && info.exists() && info.isFile() && info.isExecutable();
}

void setStatusLabel(QLabel* label, bool found, const QString& text)
{
    label->setText(found ? QStringLiteral("✓ ") + text : QStringLiteral("✗ ") + text);
    label->setProperty("statusState", found ? "ok" : "missing");
    label->style()->unpolish(label);
    label->style()->polish(label);
}

bool runVersionCheck(const QString& executable, const QStringList& arguments, QString* output, QString* error)
{
    QProcess process;
    process.start(executable, arguments);
    if (!process.waitForStarted(2000) || !process.waitForFinished(4000))
        return false;
    if (output)
        *output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (error)
        *error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QWidget* createPathRow(
    QWidget* parent,
    QLineEdit*& path,
    QLabel*& status,
    const QString& placeholder,
    const QString& browseTitle,
    const std::function<void()>& browse)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    path = new QLineEdit(row);
    path->setPlaceholderText(placeholder);
    status = new QLabel(row);
    status->setObjectName("settingsStatusLabel");
    status->setMinimumWidth(100);
    auto* browseButton = new QPushButton(browseTitle, row);
    layout->addWidget(path, 1);
    layout->addWidget(status);
    layout->addWidget(browseButton);
    QObject::connect(browseButton, &QPushButton::clicked, row, browse);
    return row;
}
}

DisassemblerSettingsPage::DisassemblerSettingsPage(QWidget* parent)
    : SettingsPage(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* backendGroup = new QGroupBox(tr("Backend"), this);
    auto* backendForm = new QFormLayout(backendGroup);
    m_backendCombo = new QComboBox(backendGroup);
    m_backendCombo->addItem(tr("objdump"), static_cast<int>(DisassemblerSettings::Backend::Objdump));
    m_backendCombo->addItem(tr("radare2"), static_cast<int>(DisassemblerSettings::Backend::Radare2));
    m_backendCombo->addItem(tr("Capstone"), static_cast<int>(DisassemblerSettings::Backend::Capstone));
    backendForm->addRow(tr("Disassembler backend"), m_backendCombo);
    m_syntaxCombo = new QComboBox(backendGroup);
    m_syntaxCombo->addItem(tr("Intel"), static_cast<int>(DisassemblerSettings::Syntax::Intel));
    m_syntaxCombo->addItem(tr("AT&T"), static_cast<int>(DisassemblerSettings::Syntax::Att));
    backendForm->addRow(tr("Assembly syntax"), m_syntaxCombo);
    layout->addWidget(backendGroup);

    auto* performanceGroup = new QGroupBox(tr("Performance"), this);
    auto* performanceForm = new QFormLayout(performanceGroup);
    m_insnLimit = new QSpinBox(performanceGroup);
    m_insnLimit->setRange(50, 200000);
    m_insnLimit->setSingleStep(250);
    m_insnLimit->setToolTip(tr("Maximum number of instructions per section (keeps UI responsive)"));
    performanceForm->addRow(tr("Instruction limit/section"), m_insnLimit);
    layout->addWidget(performanceGroup);

    auto* toolsGroup = new QGroupBox(tr("External Tools"), this);
    auto* toolsForm = new QFormLayout(toolsGroup);
    toolsForm->addRow(
        tr("objdump path"),
        createPathRow(
            toolsGroup,
            m_objdumpPath,
            m_objdumpStatus,
            tr("Leave empty to use PATH lookup"),
            tr("Browse…"),
            [this]() { browseObjdump(); }
        )
    );
    toolsForm->addRow(
        tr("radare2 path"),
        createPathRow(
            toolsGroup,
            m_radare2Path,
            m_radare2Status,
            tr("Path to r2 (radare2) executable"),
            tr("Browse…"),
            [this]() { browseRadare2(); }
        )
    );
    m_fileStatus = new QLabel(toolsGroup);
    m_fileStatus->setObjectName("settingsStatusLabel");
    toolsForm->addRow(tr("Dependency: file(1)"), m_fileStatus);
    auto* testButton = new QPushButton(tr("Test Tools"), toolsGroup);
    toolsForm->addRow(QString(), testButton);
    layout->addWidget(toolsGroup);

    m_r2Options = new QGroupBox(tr("radare2"), this);
    auto* r2Form = new QFormLayout(m_r2Options);
    m_r2AnalysisCombo = new QComboBox(m_r2Options);
    m_r2AnalysisCombo->addItem(tr("None (fast)"), static_cast<int>(DisassemblerSettings::AnalysisLevel::None));
    m_r2AnalysisCombo->addItem(tr("aa (basic)"), static_cast<int>(DisassemblerSettings::AnalysisLevel::Aa));
    m_r2AnalysisCombo->addItem(tr("aaa (full)"), static_cast<int>(DisassemblerSettings::AnalysisLevel::Aaa));
    r2Form->addRow(tr("Analysis"), m_r2AnalysisCombo);
    m_r2PreCommands = new QPlainTextEdit(m_r2Options);
    m_r2PreCommands->setPlaceholderText(tr("Optional r2 commands before JSON queries (one per line). Example:\ne asm.syntax=intel\ne asm.bits=64"));
    m_r2PreCommands->setFixedHeight(90);
    r2Form->addRow(tr("Pre-commands"), m_r2PreCommands);
    layout->addWidget(m_r2Options);
    layout->addStretch(1);

    connect(m_backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        updateUiState();
        updateDependencyStatus();
    });
    connect(m_objdumpPath, &QLineEdit::textChanged, this, &DisassemblerSettingsPage::updateDependencyStatus);
    connect(m_radare2Path, &QLineEdit::textChanged, this, &DisassemblerSettingsPage::updateDependencyStatus);
    connect(testButton, &QPushButton::clicked, this, &DisassemblerSettingsPage::testTools);
}

void DisassemblerSettingsPage::load()
{
    m_backendCombo->setCurrentIndex(m_backendCombo->findData(static_cast<int>(DisassemblerSettings::backend())));
    m_insnLimit->setValue(DisassemblerSettings::insnLimitPerSection());
    m_syntaxCombo->setCurrentIndex(m_syntaxCombo->findData(static_cast<int>(DisassemblerSettings::asmSyntax())));
    m_objdumpPath->setText(DisassemblerSettings::objdumpPath());
    m_radare2Path->setText(DisassemblerSettings::radare2Path());
    m_r2AnalysisCombo->setCurrentIndex(m_r2AnalysisCombo->findData(static_cast<int>(DisassemblerSettings::radare2AnalysisLevel())));
    m_r2PreCommands->setPlainText(DisassemblerSettings::radare2PreCommands().replace(';', '\n'));
    updateUiState();
    updateDependencyStatus();
}

bool DisassemblerSettingsPage::validate(QString* errorMessage) const
{
    Q_UNUSED(errorMessage);
    return true;
}

void DisassemblerSettingsPage::apply()
{
    DisassemblerSettings::setBackend(static_cast<DisassemblerSettings::Backend>(m_backendCombo->currentData().toInt()));
    DisassemblerSettings::setInsnLimitPerSection(m_insnLimit->value());
    DisassemblerSettings::setAsmSyntax(static_cast<DisassemblerSettings::Syntax>(m_syntaxCombo->currentData().toInt()));
    DisassemblerSettings::setObjdumpPath(m_objdumpPath->text());
    DisassemblerSettings::setRadare2Path(m_radare2Path->text());
    DisassemblerSettings::setRadare2AnalysisLevel(static_cast<DisassemblerSettings::AnalysisLevel>(m_r2AnalysisCombo->currentData().toInt()));
    DisassemblerSettings::setRadare2PreCommands(m_r2PreCommands->toPlainText().split('\n', Qt::SkipEmptyParts).join(';'));
    // Каждый сеттер выше сам слает SettingsNotifier::settingsChanged(key),
    // так что отдельный агрегатный сигнал модуля не нужен.
}

void DisassemblerSettingsPage::updateUiState()
{
    const bool radare2 = m_backendCombo->currentData().toInt() == static_cast<int>(DisassemblerSettings::Backend::Radare2);
    m_r2Options->setEnabled(radare2);
}

void DisassemblerSettingsPage::updateDependencyStatus()
{
    const QString objdump = resolvedExecutable(m_objdumpPath->text(), "objdump");
    const bool objdumpFound = isRunnableExecutable(objdump);
    setStatusLabel(m_objdumpStatus, objdumpFound, objdumpFound ? tr("found") : tr("missing"));
    m_objdumpStatus->setToolTip(objdumpFound ? objdump : tr("Not found in PATH and no valid path set"));

    const QString radare2 = resolvedExecutable(m_radare2Path->text(), "r2");
    const bool radare2Found = isRunnableExecutable(radare2);
    setStatusLabel(m_radare2Status, radare2Found, radare2Found ? tr("found") : tr("missing"));
    m_radare2Status->setToolTip(radare2Found ? radare2 : tr("Not found in PATH and no valid path set"));

    const QString file = QStandardPaths::findExecutable("file");
    setStatusLabel(m_fileStatus, isRunnableExecutable(file), isRunnableExecutable(file) ? tr("found") : tr("missing"));
}

void DisassemblerSettingsPage::browseObjdump()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Select objdump executable"), m_objdumpPath->text());
    if (!file.isEmpty())
        m_objdumpPath->setText(file);
}

void DisassemblerSettingsPage::browseRadare2()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Select radare2 (r2) executable"), m_radare2Path->text());
    if (!file.isEmpty())
        m_radare2Path->setText(file);
}

void DisassemblerSettingsPage::testTools()
{
    QStringList results;
    for (const auto& item : {std::pair{tr("objdump"), resolvedExecutable(m_objdumpPath->text(), "objdump")},
                             std::pair{tr("radare2"), resolvedExecutable(m_radare2Path->text(), "r2")}}) {
        QString output;
        QString error;
        const QStringList arguments = item.first == tr("objdump") ? QStringList{"--version"} : QStringList{"-v"};
        const bool ok = !item.second.isEmpty() && runVersionCheck(item.second, arguments, &output, &error);
        results.append(ok ? tr("%1: OK (%2)").arg(item.first, item.second)
                          : tr("%1: FAIL (%2)").arg(item.first, item.second.isEmpty() ? tr("not found") : item.second));
    }
    QMessageBox::information(this, tr("Tool check"), results.join('\n'));
    updateDependencyStatus();
}
