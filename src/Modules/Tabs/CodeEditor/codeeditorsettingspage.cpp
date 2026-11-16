#include "codeeditorsettingspage.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "codeeditorsettings.h"
#include "core/settings/settingsregistry.h"

namespace {
QString categoryTitle()
{
    return QObject::tr("Modules");
}

QString pageTitle()
{
    return QCoreApplication::translate("CodeEditorTab", "Code");
}

const bool registered = SettingsRegistry::instance().registerModulePage("codeEditor", {
    "modules.codeEditor",
    "modules",
    &categoryTitle,
    300,
    &pageTitle,
    100,
    {},
    [](QWidget* parent) { return new CodeEditorSettingsPage(parent); },
    // Схема настроек модуля: фабрика собирает её, чтобы экспорт/импорт INI
    // знал о модуле, не имея о нём никакой информации в ядре.
    {
        { CodeEditorSettings::keyGitBlameEnabled(), false },
        { CodeEditorSettings::keyGitBlameColor(), QStringLiteral("#6D6552") },
        { CodeEditorSettings::keyGitBlamePadding(), 6 },
    }
});
}

CodeEditorSettingsPage::CodeEditorSettingsPage(QWidget* parent)
    : SettingsPage(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_gitBlameEnabled = new QCheckBox(tr("Enable Git Blame"), this);
    layout->addWidget(m_gitBlameEnabled);

    m_options = new QWidget(this);
    auto* form = new QFormLayout(m_options);
    form->setContentsMargins(20, 0, 0, 0);

    m_gitBlameColor = new QComboBox(m_options);
    m_gitBlameColor->addItem(tr("Default (Gray)"), "#6D6552");
    m_gitBlameColor->addItem(tr("Red"), "#FF0000");
    m_gitBlameColor->addItem(tr("Green"), "#00FF00");
    m_gitBlameColor->addItem(tr("Blue"), "#0000FF");
    m_gitBlameColor->addItem(tr("Custom..."), "custom");
    auto* resetColor = new QPushButton(tr("Reset"), m_options);
    auto* colorRow = new QHBoxLayout();
    colorRow->addWidget(m_gitBlameColor, 1);
    colorRow->addWidget(resetColor);
    form->addRow(tr("Blame Color"), colorRow);

    m_gitBlamePadding = new QSpinBox(m_options);
    m_gitBlamePadding->setRange(0, 50);
    m_gitBlamePadding->setSuffix(tr(" chars"));
    auto* resetPadding = new QPushButton(tr("Reset"), m_options);
    auto* paddingRow = new QHBoxLayout();
    paddingRow->addWidget(m_gitBlamePadding, 1);
    paddingRow->addWidget(resetPadding);
    form->addRow(tr("Blame Padding"), paddingRow);
    layout->addWidget(m_options);

    auto* hint = new QLabel(tr("Show the author and relative date for the current line at the end of the code."), this);
    hint->setObjectName("settingsHintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);

    connect(m_gitBlameEnabled, &QCheckBox::toggled, m_options, &QWidget::setEnabled);
    connect(m_gitBlameColor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CodeEditorSettingsPage::chooseCustomColor);
    connect(resetColor, &QPushButton::clicked, this, [this]() {
        m_gitBlameColor->setCurrentIndex(m_gitBlameColor->findData("#6D6552"));
    });
    connect(resetPadding, &QPushButton::clicked, this, [this]() {
        m_gitBlamePadding->setValue(6);
    });
}

void CodeEditorSettingsPage::load()
{
    m_gitBlameEnabled->setChecked(CodeEditorSettings::gitBlameEnabled());
    m_options->setEnabled(m_gitBlameEnabled->isChecked());

    const QString color = CodeEditorSettings::gitBlameColor();
    int colorIndex = m_gitBlameColor->findData(color);
    if (colorIndex < 0) {
        colorIndex = m_gitBlameColor->findData("custom");
        m_gitBlameColor->setItemData(colorIndex, color);
        m_gitBlameColor->setItemText(colorIndex, tr("Custom (%1)").arg(color));
    }
    m_gitBlameColor->setCurrentIndex(colorIndex);
    m_gitBlamePadding->setValue(CodeEditorSettings::gitBlamePadding());
}

bool CodeEditorSettingsPage::validate(QString* errorMessage) const
{
    Q_UNUSED(errorMessage);
    return true;
}

void CodeEditorSettingsPage::apply()
{
    CodeEditorSettings::setGitBlameEnabled(m_gitBlameEnabled->isChecked());
    CodeEditorSettings::setGitBlameColor(m_gitBlameColor->currentData().toString());
    CodeEditorSettings::setGitBlamePadding(m_gitBlamePadding->value());
}

void CodeEditorSettingsPage::chooseCustomColor(int index)
{
    if (m_gitBlameColor->itemData(index).toString() != "custom")
        return;

    const QColor color = QColorDialog::getColor(QColor(CodeEditorSettings::gitBlameColor()), this, tr("Select Blame Color"));
    if (!color.isValid()) {
        load();
        return;
    }

    const QString hex = color.name().toUpper();
    m_gitBlameColor->setItemData(index, hex);
    m_gitBlameColor->setItemText(index, tr("Custom (%1)").arg(hex));
}
