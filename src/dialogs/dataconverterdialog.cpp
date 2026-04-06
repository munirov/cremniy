#include "dataconverterdialog.h"

#include <QClipboard>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

struct UnitInfo {
	const char* label;      // displayed in the form label
	const char* shortName;  // displayed in the combo box
	double      byteFactor; // how many bytes in one unit
};

static const UnitInfo kUnits[] = {
    { "Bits",      "Bit",    1.0 / 8.0 },
    { "Bytes",     "Byte",    1.0 },
    { "Kibibytes", "KiB",  1024.0 },
    { "Mebibytes", "MiB",  1024.0 * 1024.0 },
    { "Gibibytes", "GiB",  1024.0 * 1024.0 * 1024.0 },
    { "Tebibytes", "TiB",  1024.0 * 1024.0 * 1024.0 * 1024.0 },
    { "Pebibytes", "PiB",  1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 },
    { "Exbibytes", "EiB",  1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 },
    { "Zebibytes", "ZiB",  1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 },
    { "Yobibytes", "YiB",  1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 },
};

static constexpr int kUnitCount = 10;
static_assert(std::size(kUnits) == kUnitCount, "kUnits size must match kUnitCount");

double DataConverterDialog::toBytes(double value, int unitIndex)
{
    return value * kUnits[unitIndex].byteFactor;
}

double DataConverterDialog::fromBytes(double bytes, int unitIndex)
{
    return bytes / kUnits[unitIndex].byteFactor;
}

QString DataConverterDialog::formatValue(double value)
{
    QString s = QString::number(value, 'g', 10);
    return s;
}

DataConverterDialog::DataConverterDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Data Converter"));
    setModal(false);
    setMinimumWidth(620);

    auto* root = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout();

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Enter value…"));
    topRow->addWidget(m_input, 1);

    topRow->addWidget(new QLabel(tr("Unit:"), this));

    m_sourceUnit = new QComboBox(this);
    for (const auto& u : kUnits)
        m_sourceUnit->addItem(u.shortName);
    m_sourceUnit->setCurrentIndex(1); // default: Bytes
    m_sourceUnit->setMinimumWidth(64);
    m_sourceUnit->setStyleSheet(
        "QComboBox { background:#1a1a1a; color:#60a5fa; border:1px solid #262626; padding:2px 6px; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background:#1a1a1a; color:#21c55d;"
        "  selection-background-color:#2d2d50; selection-color:#ffffff; }");
    if (m_sourceUnit->view()) {
        m_sourceUnit->view()->setStyleSheet(
            "QListView { background:#1a1a1a; color:#21c55d; }"
            "QListView::item:selected { background:#2d2d50; color:#ffffff; }");
    }
    topRow->addWidget(m_sourceUnit);
    root->addLayout(topRow);

    // ---- status label ----
    m_status = new QLabel(this);
    m_status->setStyleSheet("color: #ef4444;");
    root->addWidget(m_status);

    // ---- output form ----
    m_form = new QFormLayout();
    m_form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    static const char* kColors[] = {
        "#a78bfa", // bits:     violet
        "#21c55d", // bytes:    green
        "#60a5fa", // KB:       blue
        "#34d399", // MB:       emerald
        "#f472b6", // GB:       pink
        "#facc15", // TB:       yellow
        "#fb923c", // PB:       orange
        "#38bdf8", // EB:       sky
        "#a3e635", // ZB:       lime
        "#e879f9", // YB:       fuchsia
    };

    for (int i = 0; i < kUnitCount; ++i) {
        m_labels[i] = new QLabel("-", this);
        m_labels[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_labels[i]->setStyleSheet(QString("color: %1;").arg(kColors[i]));

        m_copies[i] = new QPushButton(tr("Copy"), this);
        m_copies[i]->setFixedWidth(56);

        auto* rowWidget = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(m_labels[i], 1);
        rowLayout->addWidget(m_copies[i]);

        m_form->addRow(tr(kUnits[i].label), rowWidget);

        connect(m_copies[i], &QPushButton::clicked, this, [this, i]() {
            copyRow(i);
        });
    }

    root->addLayout(m_form);

    connect(m_input, &QLineEdit::textChanged,
        this, &DataConverterDialog::onInputChanged);
    connect(m_sourceUnit, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &DataConverterDialog::onSourceUnitChanged);

    onInputChanged();
}

void DataConverterDialog::onInputChanged()
{
    const QString text = m_input->text().trimmed();

    if (text.isEmpty()) {
        m_status->clear();
        for (int i = 0; i < kUnitCount; ++i)
            m_labels[i]->setText("-");
        return;
    }

    bool ok = false;
    const double value = text.toDouble(&ok);

    if (!ok || value < 0.0) {
        m_status->setText(tr("Invalid input"));
        for (int i = 0; i < kUnitCount; ++i)
            m_labels[i]->setText("-");
        return;
    }

    m_status->clear();
    const double bytes = toBytes(value, m_sourceUnit->currentIndex());
    updateOutputs(bytes);
}

void DataConverterDialog::onSourceUnitChanged()
{
    onInputChanged();
}

void DataConverterDialog::updateOutputs(double bytes)
{
    for (int i = 0; i < kUnitCount; ++i)
        m_labels[i]->setText(formatValue(fromBytes(bytes, i)));
}

void DataConverterDialog::copyRow(int rowIndex)
{
    const QString text = m_labels[rowIndex]->text();
    if (text != "-")
        QGuiApplication::clipboard()->setText(text);
}