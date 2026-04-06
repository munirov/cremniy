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

ReverseCalculatorDialog::ReverseCalculatorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Reverse Calculator"));
    setModal(false);
    setMinimumWidth(560);

    auto *root = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout();
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
    // Ensure items are visible in dark themes.
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

    m_status = new QLabel(this);
    m_status->setStyleSheet("color: #ef4444;");
    root->addWidget(m_status);

    m_form = new QFormLayout();
    m_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_hex  = new QLabel("-", this);
    m_decU = new QLabel("-", this);
    m_decS = new QLabel("-", this);
    m_bin  = new QLabel("-", this);

    for (QLabel *l : {m_hex, m_decU, m_decS, m_bin}) {
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->setStyleSheet("color: #60a5fa;"); // blue, no gray/black
    }
    m_hex->setStyleSheet("color: #21c55d;");  // green
    m_decU->setStyleSheet("color: #60a5fa;"); // blue
    m_decS->setStyleSheet("color: #ef4444;"); // red
    m_bin->setStyleSheet("color: #21c55d;");  // green

    m_form->addRow(tr("Hex"), m_hex);
    m_form->addRow(tr("Dec (unsigned)"), m_decU);
    m_form->addRow(tr("Dec (signed)"), m_decS);
    m_form->addRow(tr("Bin"), m_bin);
    root->addLayout(m_form);

    auto *btnRow = new QHBoxLayout();
    m_swapBtn = new QPushButton(tr("Swap endian"), this);
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
    connect(m_width, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ReverseCalculatorDialog::onInputChanged);
    connect(m_showSigned, &QCheckBox::toggled, this, &ReverseCalculatorDialog::onInputChanged);
    connect(m_swapBtn, &QPushButton::clicked, this, &ReverseCalculatorDialog::onSwapEndian);
    connect(m_copyHex, &QPushButton::clicked, this, &ReverseCalculatorDialog::onCopyHex);
    connect(m_copyDec, &QPushButton::clicked, this, &ReverseCalculatorDialog::onCopyDec);
    connect(m_copyBin, &QPushButton::clicked, this, &ReverseCalculatorDialog::onCopyBin);

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
    // two's complement
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

bool ReverseCalculatorDialog::parseValue(const QString &text, qulonglong *outValue)
{
    if (!outValue) return false;
    const QString t = text.trimmed();
    if (t.isEmpty()) return false;

    // Accept: -123, 123, 0x..., 0b...
    static const QRegularExpression re(
        R"(^\s*([+-]?)\s*(0x[0-9a-fA-F]+|0b[01]+|\d+)\s*$)");
    auto m = re.match(t);
    if (!m.hasMatch()) return false;

    const QString sign = m.captured(1);
    const QString num  = m.captured(2);

    bool ok = false;
    qulonglong v = 0;
    if (num.startsWith("0x", Qt::CaseInsensitive)) {
        v = num.mid(2).toULongLong(&ok, 16);
    } else if (num.startsWith("0b", Qt::CaseInsensitive)) {
        v = num.mid(2).toULongLong(&ok, 2);
    } else {
        v = num.toULongLong(&ok, 10);
    }
    if (!ok) return false;

    if (sign == "-") {
        // Keep as 64-bit two's complement; width masking applied later.
        *outValue = static_cast<qulonglong>(-static_cast<qlonglong>(v));
    } else {
        *outValue = v;
    }
    return true;
}

void ReverseCalculatorDialog::updateOutputs(qulonglong value, bool ok) {
  const int bits = m_width->currentData().toInt();

  if (!ok) {
    m_status->setText(tr("Invalid input"));
    m_hex->setText("-");
    m_decU->setText("-");
    m_decS->setText("-");
    m_bin->setText("-");
    return;
  }

  m_status->clear();

  const qulonglong v = maskToWidth(value, bits);

  m_hex->setText(fmtHex(v, bits));
  m_decU->setText(QString::number(v));
  m_decS->setText(QString::number(toSigned(v, bits)));
  m_bin->setText(fmtBin(v, bits));
}

void ReverseCalculatorDialog::onInputChanged() {
  qulonglong v = 0;
  const bool isInputEmpty = m_input->text().trimmed().isEmpty();
  const bool ok = parseValue(m_input->text(), &v);
  
  if (isInputEmpty) {
    m_status->clear();
    m_hex->setText("-");
    m_decU->setText("-");
    m_decS->setText("-");
    m_bin->setText("-");
  } else {
    updateOutputs(v, ok); 
  }

  if (m_showSigned->isChecked()) {
    m_form->setRowVisible(m_decS, true);
    m_decS->setVisible(true);
    if (QWidget *label = m_form->labelForField(m_decS)) {
      label->show();
    }
  } else {
    m_form->setRowVisible(m_decS, false);
    if (QWidget *label = m_form->labelForField(m_decS)) {
      label->hide();
    }
  }
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

