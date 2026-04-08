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
    { "Bytes",     "Byte",   1.0 },
    { "Kilobytes", "KB",     1024.0 },
    { "Megabytes", "MB",     1024.0 * 1024.0 },
    { "Gigabytes", "GB",     1024.0 * 1024.0 * 1024.0 },
    { "Terabytes", "TB",     1024.0 * 1024.0 * 1024.0 * 1024.0 },
    { "Petabytes", "PB",     1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 },
    { "Exabytes",  "EB",     1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 },
    { "Zettabytes","ZB",     1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 },
    { "Yottabytes","YB",     1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 },
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
    if (value == 0) return "0";

    if (std::abs(value) < 0.000001) {
        return QString::number(value, 'g', 6);
    }

    QString s = QString::number(value, 'f', 10);
    if (s.contains('.')) {
        while (s.endsWith('0')) s.chop(1);
        if (s.endsWith('.')) s.chop(1);
    }
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
    auto* copyAllButton = new QPushButton(tr("Copy All"), this);
    copyAllButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    root->addWidget(copyAllButton);
    connect(copyAllButton, &QPushButton::clicked, this, &DataConverterDialog::copyAll);

    connect(m_input, &QLineEdit::textChanged, this, &DataConverterDialog::onInputChanged);
    connect(m_sourceUnit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataConverterDialog::onSourceUnitChanged);

    onInputChanged();
}

void DataConverterDialog::onInputChanged() {
    const QString text = m_input->text().trimmed();

    if (text.isEmpty()) {
        m_status->clear();
        for (int i = 0; i < kUnitCount; ++i)
            m_labels[i]->setText("-");
        return;
    }

    qulonglong v = 0;
    QString error;
    bool ok = false;

    if (looksLikeExpression(text)) {
        ok = parseExpression(text, &v, &error);
    }
    else {
        ok = parseValue(text, &v);
        error = tr("Invalid input");
    }

    if (!ok) {
        m_status->setText(error);
        for (int i = 0; i < kUnitCount; ++i)
            m_labels[i]->setText("-");
        return;
    }

    m_status->clear();
    const double value = static_cast<double>(v);

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

void DataConverterDialog::copyAll()
{
    QStringList result;
    for (int i = 0; i < kUnitCount; ++i) {
        const QString val = m_labels[i]->text();
        if (val != "-") {
            result << QString("%1: %2").arg(tr(kUnits[i].label), val);
        }
    }

    if (!result.isEmpty())
        QGuiApplication::clipboard()->setText(result.join("\n"));
}

bool DataConverterDialog::parseValue(const QString& text, qulonglong* outValue)
{
    if (!outValue) return false;
    const QString t = text.trimmed();
    if (t.isEmpty()) return false;

    // strip spaces from binary literals with spaces
    {
        // explicit 0b prefix with embedded spaces
        static const QRegularExpression binPrefixed(R"(^\s*([+-]?)\s*(0[bB][01 ]+)\s*$)");
        auto bm = binPrefixed.match(t);
        if (bm.hasMatch()) {
            QString digits = bm.captured(2).mid(2);
            digits.remove(' ');
            if (digits.isEmpty()) return false;
            bool ok = false;
            qulonglong v = digits.toULongLong(&ok, 2);
            if (!ok) return false;
            const QString sign = bm.captured(1);
            *outValue = (sign == "-") ? static_cast<qulonglong>(-static_cast<qlonglong>(v)) : v;
            return true;
        }
    }

    static const QRegularExpression re(R"(^\s*([+-]?)\s*(0x[0-9a-fA-F]+|0b[01]+|\d+)\s*$)");
    auto m = re.match(t);
    if (!m.hasMatch()) return false;

    const QString sign = m.captured(1);
    const QString num = m.captured(2);

    bool ok = false;
    qulonglong v = 0;
    if (num.startsWith("0x", Qt::CaseInsensitive)) {
        v = num.mid(2).toULongLong(&ok, 16);
    }
    else if (num.startsWith("0b", Qt::CaseInsensitive)) {
        v = num.mid(2).toULongLong(&ok, 2);
    }
    else {
        v = num.toULongLong(&ok, 10);
    }
    if (!ok) return false;

    *outValue = (sign == "-") ? static_cast<qulonglong>(-static_cast<qlonglong>(v)) : v;
    return true;
}

bool DataConverterDialog::parseExpression(const QString& text, qulonglong* outValue, QString* errorOut)
{
    if (!outValue || !errorOut) return false;

    static const QRegularExpression tokenRe(R"(\s*(0[xX][0-9a-fA-F]+|0[bB][01 ]+|\d+|[+\-*/%&|^]|<<|>>)\s*)");

    QStringList tokens;
    QRegularExpressionMatchIterator i = tokenRe.globalMatch(text);
    int lastEnd = 0;

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        if (match.capturedStart() > lastEnd && text.mid(lastEnd, match.capturedStart() - lastEnd).trimmed().length() > 0) {
            *errorOut = tr("Invalid syntax");
            return false;
        }
        tokens << match.captured(1).trimmed();
        lastEnd = match.capturedEnd();
    }

    if (tokens.isEmpty()) {
        *errorOut = tr("Empty expression");
        return false;
    }

    QStringList mergedTokens;
    for (int j = 0; j < tokens.size(); ++j) {
        QString t = tokens[j];
        if ((t == "+" || t == "-") &&
            (mergedTokens.isEmpty() ||
                QString("+ - * / % & | ^ << >>").split(" ").contains(mergedTokens.last())))
        {
            if (j + 1 < tokens.size()) {
                mergedTokens << (t + tokens[j + 1]);
                j++;
            }
            else {
                *errorOut = tr("Invalid syntax at end");
                return false;
            }
        }
        else {
            mergedTokens << t;
        }
    }

    if (mergedTokens.size() % 2 == 0) {
        *errorOut = tr("Incomplete expression");
        return false;
    }

    QList<qulonglong> values;
    QList<QString> ops;

    qulonglong firstVal = 0;
    if (!parseValue(mergedTokens[0], &firstVal)) {
        *errorOut = tr("Invalid operand: ") + mergedTokens[0];
        return false;
    }
    values.append(firstVal);

    for (int j = 1; j < mergedTokens.size(); j += 2) {
        ops.append(mergedTokens[j]);
        qulonglong val = 0;
        if (!parseValue(mergedTokens[j + 1], &val)) {
            *errorOut = tr("Invalid operand: ") + mergedTokens[j + 1];
            return false;
        }
        values.append(val);
    }

    // priority-based calculation
    QList<QStringList> precedenceLevels = {
        {"*", "/", "%"},
        {"+", "-"},
        {"<<", ">>"},
        {"&"},
        {"^"},
        {"|"}
    };

    for (const QStringList& level : precedenceLevels) {
        for (int j = 0; j < ops.size();) {
            if (level.contains(ops[j])) {
                QString op = ops[j];
                qulonglong lhs = values[j];
                qulonglong rhs = values[j + 1];
                qulonglong res = 0;

                if (op == "*")       res = lhs * rhs;
                else if (op == "/") {
                    if (rhs == 0) { *errorOut = tr("Division by zero"); return false; }
                    res = lhs / rhs;
                }
                else if (op == "%") {
                    if (rhs == 0) { *errorOut = tr("Modulo by zero"); return false; }
                    res = lhs % rhs;
                }
                else if (op == "+")  res = lhs + rhs;
                else if (op == "-")  res = lhs - rhs;
                else if (op == "<<") res = lhs << rhs;
                else if (op == ">>") res = lhs >> rhs;
                else if (op == "&")  res = lhs & rhs;
                else if (op == "^")  res = lhs ^ rhs;
                else if (op == "|")  res = lhs | rhs;

                values[j] = res;

                values.removeAt(j + 1);
                ops.removeAt(j);
            }
            else {
                j++;
            }
        }
    }

    if (values.isEmpty()) return false;
    *outValue = values[0];
    return true;
}

bool DataConverterDialog::looksLikeExpression(const QString& text)
{
    static const QRegularExpression exprRe(R"((?:^|\s|\d)(?:\+|-|\*|/|%|&|\||\^|<<|>>)(?:\s|\d|$))");
    return exprRe.match(text).hasMatch();
}