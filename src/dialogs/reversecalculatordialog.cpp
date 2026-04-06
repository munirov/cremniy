// ===== reversecalculatordialog.cpp =====
// SPDX-License-Identifier: MIT
#include "reversecalculatordialog.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

static QString fmtHex(qulonglong v, int bits)
{
    const int nyb = qMax(1, bits / 4);
    return QString("0x%1").arg(QString::number(v, 16).toUpper().rightJustified(nyb, '0'));
}

static QString fmtBin(qulonglong v, int bits)
{
    QString s;
    s.reserve(bits + bits / 4);
    for (int i = bits - 1; i >= 0; --i) {
        s += ((v >> i) & 1ULL) ? '1' : '0';
        if (i % 8 == 0 && i != 0) s += ' ';
    }
    return s;
}

// detect the "dominant" base of a token string for result formatting
static int detectBase(const QString& token)
{
    const QString t = token.trimmed();
    if (t.startsWith("0x", Qt::CaseInsensitive) || t.startsWith("+0x", Qt::CaseInsensitive) || t.startsWith("-0x", Qt::CaseInsensitive))
        return 16;
    if (t.startsWith("0b", Qt::CaseInsensitive) || t.startsWith("+0b", Qt::CaseInsensitive) || t.startsWith("-0b", Qt::CaseInsensitive))
        return 2;
    return 10;
}

static QString fmtResult(qulonglong v, int bits, int base)
{
    const qulonglong masked = v & (bits >= 64 ? ~0ULL : ((1ULL << bits) - 1ULL));
    switch (base) {
    case 16: return fmtHex(masked, bits);
    case 2:  return fmtBin(masked, bits);
    default: {
        if (bits >= 64)
            return QString::number(static_cast<qlonglong>(masked));
        const qulonglong signBit = 1ULL << (bits - 1);
        if (masked & signBit) {
            const qulonglong mask = (1ULL << bits) - 1ULL;
            return QString::number(-static_cast<qlonglong>((~masked + 1ULL) & mask));
        }
        return QString::number(masked);
    }
    }
}

ReverseCalculatorDialog::ReverseCalculatorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Reverse Calculator"));
    setModal(false);
    setMinimumWidth(620);

    auto* root = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Enter value: 1234, -1, 0xDEADBEEF, 0b1010"));
    topRow->addWidget(m_input, 1);

    m_width = new QComboBox(this);
    m_width->addItem("8", 8);
    m_width->addItem("16", 16);
    m_width->addItem("32", 32);
    m_width->addItem("64", 64);
    m_width->setCurrentIndex(2);
    m_width->setMinimumWidth(64);
    m_width->setStyleSheet(
        "QComboBox { background:#1a1a1a; color:#60a5fa; border:1px solid #262626; padding:2px 6px; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background:#1a1a1a; color:#21c55d; selection-background-color:#2d2d50; selection-color:#ffffff; }");
    if (m_width->view()) {
        m_width->view()->setStyleSheet(
            "QListView { background:#1a1a1a; color:#21c55d; }"
            "QListView::item:selected { background:#2d2d50; color:#ffffff; }");
    }
    topRow->addWidget(new QLabel(tr("Bits:"), this));
    topRow->addWidget(m_width);

    m_showSigned = new QCheckBox(tr("Show signed"), this);
    m_showSigned->setChecked(true);
    topRow->addWidget(m_showSigned);

    root->addLayout(topRow);

    m_historyList = new QListWidget(this);
    m_historyList->setMaximumHeight(80);
    m_historyList->hide();
    m_historyList->setStyleSheet(
        "QListWidget { background:#1a1a1a; color:#60a5fa; border:1px solid #262626; padding:2px; }"
        "QListWidget::item:selected { background:#2d2d50; color:#ffffff; }"
    );

    m_clearHistoryBtn = new QPushButton(tr("Clear"), this);
    m_clearHistoryBtn->hide();

    auto* historyLayout = new QHBoxLayout();
    historyLayout->addWidget(m_historyList, 1);

    auto* clearLayout = new QVBoxLayout();
    clearLayout->addWidget(m_clearHistoryBtn);
    clearLayout->addStretch(1);
    historyLayout->addLayout(clearLayout);

    root->addLayout(historyLayout);

    m_status = new QLabel(this);
    m_status->setStyleSheet("color: #ef4444;");
    root->addWidget(m_status);

    m_form = new QFormLayout();
    m_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_hex = new QLabel("-", this);
    m_decU = new QLabel("-", this);
    m_decS = new QLabel("-", this);
    m_bin = new QLabel("-", this);

    auto applyLabelStyle = [](QLabel* l, const QString& color) {
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->setStyleSheet(QString("color: %1;").arg(color));
        };

    applyLabelStyle(m_hex, "#21c55d");
    applyLabelStyle(m_decU, "#60a5fa");
    applyLabelStyle(m_decS, "#ef4444");
    applyLabelStyle(m_bin, "#21c55d");

    m_form->addRow(tr("Hex"), m_hex);
    m_form->addRow(tr("Dec (unsigned)"), m_decU);
    m_form->addRow(tr("Dec (signed)"), m_decS);
    m_form->addRow(tr("Bin"), m_bin);
    root->addLayout(m_form);

    auto* btnRow = new QHBoxLayout();
    m_swapBtn = new QPushButton(tr("Swap endian"), this);
    m_swapBtn->setToolTip(tr("Swap byte order within selected bit width"));
    btnRow->addWidget(m_swapBtn);
    btnRow->addStretch(1);

    m_copyHex = new QPushButton(tr("Copy hex"), this);
    m_copyDec = new QPushButton(tr("Copy dec"), this);
    m_copyBin = new QPushButton(tr("Copy bin"), this);
    btnRow->addWidget(m_copyHex);
    btnRow->addWidget(m_copyDec);
    btnRow->addWidget(m_copyBin);
    root->addLayout(btnRow);

    connect(m_input, &QLineEdit::textChanged, this, &ReverseCalculatorDialog::onInputChanged);
    connect(m_input, &QLineEdit::returnPressed, this, &ReverseCalculatorDialog::onReturnPressed);
    connect(m_width, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ReverseCalculatorDialog::onInputChanged);
    connect(m_showSigned, &QCheckBox::toggled, this, &ReverseCalculatorDialog::onInputChanged);
    connect(m_swapBtn, &QPushButton::clicked, this, &ReverseCalculatorDialog::onSwapEndian);
    connect(m_copyHex, &QPushButton::clicked, this, &ReverseCalculatorDialog::onCopyHex);
    connect(m_copyDec, &QPushButton::clicked, this, &ReverseCalculatorDialog::onCopyDec);
    connect(m_copyBin, &QPushButton::clicked, this, &ReverseCalculatorDialog::onCopyBin);
    connect(m_historyList, &QListWidget::itemClicked, this, &ReverseCalculatorDialog::onHistoryItemClicked);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, [this]() {
        m_historyList->clear();
        m_historyList->hide();
        m_clearHistoryBtn->hide();
        });

    onInputChanged();
}

qulonglong ReverseCalculatorDialog::maskToWidth(qulonglong v, int bits)
{
    if (bits >= 64) return v;
    const qulonglong mask = (1ULL << bits) - 1ULL;
    return v & mask;
}

qlonglong ReverseCalculatorDialog::toSigned(qulonglong v, int bits)
{
    if (bits >= 64) return static_cast<qlonglong>(v);
    const qulonglong signBit = 1ULL << (bits - 1);
    const qulonglong mask = (1ULL << bits) - 1ULL;
    v &= mask;
    if ((v & signBit) == 0) return static_cast<qlonglong>(v);
    const qulonglong neg = (~v + 1ULL) & mask;
    return -static_cast<qlonglong>(neg);
}

qulonglong ReverseCalculatorDialog::swapEndian(qulonglong v, int bits)
{
    const int bytes = qMax(1, bits / 8);
    qulonglong out = 0;
    for (int i = 0; i < bytes; ++i) {
        out = (out << 8) | ((v >> (i * 8)) & 0xFFULL);
    }
    return maskToWidth(out, bits);
}

bool ReverseCalculatorDialog::parseValue(const QString& text, qulonglong* outValue)
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

        // raw binary digits with spaces (currently is not good idea)
        //static const QRegularExpression rawBin(R"(^\s*([+-]?)\s*([01][01 ]+[01])\s*$)");
        //auto rm = rawBin.match(t);
        //if (rm.hasMatch()) {
        //    QString digits = rm.captured(2);
        //    digits.remove(' ');
        //    bool ok = false;
        //    qulonglong v = digits.toULongLong(&ok, 2);
        //    if (!ok) return false;
        //    const QString sign = rm.captured(1);
        //    *outValue = (sign == "-") ? static_cast<qulonglong>(-static_cast<qlonglong>(v)) : v;
        //    return true;
        //}
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

bool ReverseCalculatorDialog::parseExpression(const QString& text, qulonglong* outValue, QString* errorOut, int* lhsBase)
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

    if (lhsBase) {
        *lhsBase = detectBase(mergedTokens[0]);
    }

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

void ReverseCalculatorDialog::updateOutputs(qulonglong value, bool ok)
{
    const int bits = m_width->currentData().toInt();

    if (!ok) {
        m_hex->setText("-");
        m_decU->setText("-");
        m_decS->setText("-");
        m_bin->setText("-");
        return;
    }

    const qulonglong v = maskToWidth(value, bits);

    m_hex->setText(fmtHex(v, bits));
    m_decU->setText(QString::number(v));
    m_decS->setText(QString::number(toSigned(v, bits)));
    m_bin->setText(fmtBin(v, bits));
}

static bool looksLikeExpression(const QString& text)
{
    // After the first numeric token, is there an operator?
    static const QRegularExpression exprRe(R"((?:^|\s|\d)(?:\+|-|\*|/|%|&|\||\^|<<|>>)(?:\s|\d|$))");
    return exprRe.match(text).hasMatch();
}

void ReverseCalculatorDialog::onInputChanged()
{
    const QString text = m_input->text().trimmed();

    if (text.isEmpty()) {
        m_status->clear();
        m_hex->setText("-");
        m_decU->setText("-");
        m_decS->setText("-");
        m_bin->setText("-");
        updateSignedRowVisibility();
        return;
    }

    qulonglong v = 0;
    QString error;
    bool ok = false;

    if (looksLikeExpression(text)) {
        int lhsBase = 10;
        ok = parseExpression(text, &v, &error, &lhsBase);
        if (!ok) {
            m_status->setText(error);
            m_status->setStyleSheet("color: #ef4444;");
        }
        else {
            const int bits = m_width->currentData().toInt();
            const QString res = fmtResult(v, bits, lhsBase);
            m_status->setText(QString("%1 = %2").arg(text.simplified()).arg(res));
            m_status->setStyleSheet("color: #21c55d;");
        }
    }
    else {
        ok = parseValue(text, &v);
        if (!ok) {
            m_status->setText(tr("Invalid input"));
            m_status->setStyleSheet("color: #ef4444;");
        }
        else {
            m_status->clear();
        }
    }

    updateOutputs(v, ok);
    updateSignedRowVisibility();
}

void ReverseCalculatorDialog::onReturnPressed()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;

    qulonglong v = 0;
    QString error;
    bool ok = false;
    int lhsBase = 10;

    if (looksLikeExpression(text)) {
        ok = parseExpression(text, &v, &error, &lhsBase);
    }
    else {
        ok = parseValue(text, &v);
        lhsBase = detectBase(text);
    }

    if (ok) {
        const int bits = m_width->currentData().toInt();
        const QString res = fmtResult(v, bits, lhsBase);
        const QString historyItem = QString("%1 = %2").arg(text.simplified()).arg(res);

        if (m_historyList->count() > 0 &&
            m_historyList->item(m_historyList->count() - 1)->text() == historyItem)
            return;

        m_historyList->addItem(historyItem);
        while (m_historyList->count() > 10)
            delete m_historyList->takeItem(0);

        m_historyList->show();
        m_clearHistoryBtn->show();
        m_historyList->scrollToBottom();
    }
}

void ReverseCalculatorDialog::updateSignedRowVisibility()
{
    const bool show = m_showSigned->isChecked();
    m_form->setRowVisible(m_decS, show);
    if (QWidget* label = m_form->labelForField(m_decS))
        show ? label->show() : label->hide();
}

void ReverseCalculatorDialog::onHistoryItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    QString text = item->text();
    const int eqIdx = text.indexOf(" = ");
    if (eqIdx != -1)
        text = text.left(eqIdx);
    m_input->setText(text);
}

void ReverseCalculatorDialog::onSwapEndian()
{
    qulonglong v = 0;
    if (!parseValue(m_input->text(), &v)) return;
    const int bits = m_width->currentData().toInt();
    const qulonglong swapped = swapEndian(maskToWidth(v, bits), bits);
    m_input->setText(fmtHex(swapped, bits));
}

void ReverseCalculatorDialog::onCopyHex()
{
    QGuiApplication::clipboard()->setText(m_hex->text());
}

void ReverseCalculatorDialog::onCopyDec()
{
    QGuiApplication::clipboard()->setText(m_decU->text());
}

void ReverseCalculatorDialog::onCopyBin()
{
    QGuiApplication::clipboard()->setText(m_bin->text());
}