#include "reversecalculatordialog.h"
#include "reversecalculatorengine.h"

#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>
#include <QString>

#include "core/modules/ModuleManager.h"

static QString displayName() {
    return QCoreApplication::translate("ReverseCalculator", "Reverse Calculator");
};

static bool registered = []() {
    ModuleManager::instance().registerModule<WindowBase>(
    &displayName, "", []() { return new ReverseCalculatorDialog(); });
    return true;
}();

namespace {
QFrame *makeSeparator(QWidget *parent) {
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Plain);
    f->setFixedHeight(1);
    f->setStyleSheet(QStringLiteral("background-color: #2D2D2D;"));
    return f;
}

QPushButton *makeCopyBtn(QWidget *parent) {
    auto *btn = new QPushButton(QObject::tr("Copy"), parent);
    btn->setFixedWidth(60);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

QLabel *makeValueLabel(QWidget *parent) {
    auto *lbl = new QLabel(QStringLiteral("None"), parent);
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lbl->setFont(QFont(QStringLiteral("monospace"), -1));
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QStringLiteral("color: #888888; font-weight: bold;"));
    return lbl;
}
} // namespace

ReverseCalculatorDialog::ReverseCalculatorDialog(QWidget *parent) : WindowBase(parent) {
    setWindowTitle(tr("Reverse Calculator"));
    setModal(false);
    setMinimumWidth(620);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(10, 10, 10, 10);

    // --- input/width ---
    auto *topRow = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Enter value (e.g., 255, 0xDEAD, 0b1010) or expression..."));
    m_input->setFont(QFont(QStringLiteral("monospace"), -1));
    topRow->addWidget(m_input, 1);

    topRow->addWidget(new QLabel(tr("Width:"), this));
    m_width = new QComboBox(this);
    for (auto [label, val] : {std::pair{"8 bit", 8}, {"16 bit", 16}, {"32 bit", 32}, {"64 bit", 64}}) {
        m_width->addItem(label, val);
    }
    m_width->setCurrentIndex(2); // default 32bit
    m_width->setMinimumWidth(80);
    topRow->addWidget(m_width);
    root->addLayout(topRow);

    m_historyList = new QListWidget(this);
    m_historyList->setMaximumHeight(100);
    m_historyList->hide();

    m_clearHistoryBtn = new QPushButton(tr("Clear"), this);
    m_clearHistoryBtn->hide();
    m_clearHistoryBtn->setFixedWidth(60);

    auto *histRow = new QHBoxLayout();
    histRow->addWidget(m_historyList, 1);
    auto *histBtns = new QVBoxLayout();
    histBtns->addWidget(m_clearHistoryBtn);
    histBtns->addStretch();
    histRow->addLayout(histBtns);
    root->addLayout(histRow);

    // --- status ---
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->hide();
    root->addWidget(m_status);

    root->addWidget(makeSeparator(this));

    // --- results ---
    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setSpacing(10);

    auto addResultRow = [&](const QString &key, const QString &label) {
        OutputRow orow;
        orow.value = makeValueLabel(this);
        orow.copyBtn = makeCopyBtn(this);

        auto *hRow = new QHBoxLayout();
        hRow->addWidget(orow.value, 1);
        hRow->addWidget(orow.copyBtn);

        form->addRow(label, hRow);
        m_rows[key] = orow;
    };

    form->setLabelAlignment(Qt::AlignLeft);
    addResultRow(QStringLiteral("hex"), tr("HEX:"));
    addResultRow(QStringLiteral("decU"), tr("DEC (UNSIGNED):"));
    addResultRow(QStringLiteral("decS"), tr("DEC (SIGNED):"));
    addResultRow(QStringLiteral("oct"), tr("OCTAL:"));
    addResultRow(QStringLiteral("bin"), tr("BINARY:"));
    addResultRow(QStringLiteral("bytes"), tr("BYTES:"));

    root->addLayout(form);
    root->addWidget(makeSeparator(this));

    auto *bottomRow = new QHBoxLayout();
    m_swapBtn = new QPushButton(tr("Swap Endian"), this);
    m_swapBtn->setToolTip(tr("Swap byte order within selected bit width"));
    bottomRow->addWidget(m_swapBtn);
    bottomRow->addStretch();

    m_copyAllBtn = new QPushButton(tr("Copy All"), this);
    bottomRow->addWidget(m_copyAllBtn);
    root->addLayout(bottomRow);

    connect(m_input, &QLineEdit::textChanged, this, &ReverseCalculatorDialog::onInputChanged);
    connect(m_input, &QLineEdit::returnPressed, this, &ReverseCalculatorDialog::onReturnPressed);
    connect(m_width, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ReverseCalculatorDialog::onInputChanged);
    connect(m_swapBtn, &QPushButton::clicked, this, &ReverseCalculatorDialog::onSwapEndian);

    for (auto it = m_rows.begin(); it != m_rows.end(); ++it) {
        const QString key = it.key();
        connect(it.value().copyBtn, &QPushButton::clicked, this, [this, key] {
            QString val = m_rows[key].value->text();
            if (key == QStringLiteral("bin")) {
                val.remove(' ');
                if (val != QStringLiteral("-") && val != QStringLiteral("None")) val.prepend(QStringLiteral("0b"));
            }
            copyText(val);
        });
    }

    connect(m_copyAllBtn, &QPushButton::clicked, this, [this] {
        if (m_rows[QStringLiteral("hex")].value->text() == QStringLiteral("None")) return;

        QString text = tr("HEX: ") % m_rows[QStringLiteral("hex")].value->text() % "\n" %
                       tr("DEC (UNSIGNED): ") % m_rows[QStringLiteral("decU")].value->text() % "\n" %
                       tr("DEC (SIGNED): ") % m_rows[QStringLiteral("decS")].value->text() % "\n" %
                       tr("OCT: ") % m_rows[QStringLiteral("oct")].value->text() % "\n" %
                       tr("BIN: 0b") % m_rows[QStringLiteral("bin")].value->text().simplified().remove(' ') % "\n" %
                       tr("BYTES: ") % m_rows[QStringLiteral("bytes")].value->text();
        copyText(text);
    });

    connect(m_historyList, &QListWidget::itemClicked, this, &ReverseCalculatorDialog::onHistoryItemClicked);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, [this] {
        m_historyList->clear();
        m_historyList->hide();
        m_clearHistoryBtn->hide();
    });

    onInputChanged();
}

void ReverseCalculatorDialog::copyText(const QString &text) {
    if (text != QStringLiteral("None"))
        QGuiApplication::clipboard()->setText(text);
}

void ReverseCalculatorDialog::clearOutputs() {
    for (auto &row : m_rows) {
        row.value->setText(QStringLiteral("None"));
        row.value->setStyleSheet(QStringLiteral("color: #888888;"));
    }
}

void ReverseCalculatorDialog::updateOutputs(qulonglong value, bool ok) {
    if (!ok) {
        clearOutputs();
        return;
    }

    const int bits = m_width->currentData().toInt();
    const qulonglong v = ReverseCalculatorEngine::maskToWidth(value, bits);

    const QString style = QStringLiteral("color: #0073E5; font-weight: bold;");
    for (auto &row : m_rows) row.value->setStyleSheet(style);

    m_rows[QStringLiteral("hex")].value->setText(ReverseCalculatorEngine::formatHex(v, bits));
    m_rows[QStringLiteral("decU")].value->setText(ReverseCalculatorEngine::formatUnsigned(v, bits));
    m_rows[QStringLiteral("decS")].value->setText(ReverseCalculatorEngine::formatSigned(v, bits));
    m_rows[QStringLiteral("oct")].value->setText(ReverseCalculatorEngine::formatOct(v, bits));
    m_rows[QStringLiteral("bin")].value->setText(ReverseCalculatorEngine::formatBin(v, bits));
    m_rows[QStringLiteral("bytes")].value->setText(ReverseCalculatorEngine::formatBytes(v, bits));
}

void ReverseCalculatorDialog::onInputChanged() {
    const QString text = m_input->text().trimmed();

    if (text.isEmpty()) {
        m_status->clear();
        m_status->hide();
        clearOutputs();
        adjustSize();
        return;
    }

    auto &engine = ReverseCalculatorEngine::instance();
    const int bits = m_width->currentData().toInt();
    ParseResult res = engine.parseExpression(text, bits);

    if (!res.ok) {
        m_status->setStyleSheet(QStringLiteral("color: #FF5555;"));
        m_status->setText(tr("Error: ") % res.error);
        m_status->show();
        updateOutputs(0, false);
    } else {
        const bool isExpr = text.contains(QRegularExpression(R"([+\-*/%&|^~()])"));
        if (isExpr) {
            m_status->setStyleSheet(QStringLiteral("color: #00E500;"));
            QString fmtVal;
            if (res.base == 16) fmtVal = engine.formatHex(res.value, bits);
            else if (res.base == 2) fmtVal = QStringLiteral("0b") % engine.formatBin(res.value, bits);
            else fmtVal = engine.formatUnsigned(res.value, bits);

            m_status->setText(text.simplified() % " = " % fmtVal);
            m_status->show();
        } else {
            m_status->clear();
            m_status->hide();
        }
        updateOutputs(res.value, true);
    }
}

void ReverseCalculatorDialog::onReturnPressed() {
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;

    auto &engine = ReverseCalculatorEngine::instance();
    const int bits = m_width->currentData().toInt();
    ParseResult res = engine.parseExpression(text, bits);

    if (!res.ok) return;

    QString fmtVal;
    if (res.base == 16) fmtVal = engine.formatHex(res.value, bits);
    else if (res.base == 2) fmtVal = QStringLiteral("0b") % engine.formatBin(res.value, bits);
    else fmtVal = engine.formatUnsigned(res.value, bits);

    const QString item = text.simplified() % " = " % fmtVal;

    if (m_historyList->count() > 0 && m_historyList->item(m_historyList->count() - 1)->text() == item) return;

    m_historyList->addItem(item);
    while (m_historyList->count() > 10) delete m_historyList->takeItem(0);

    m_historyList->show();
    m_clearHistoryBtn->show();
    m_historyList->scrollToBottom();
}

void ReverseCalculatorDialog::onHistoryItemClicked(QListWidgetItem *item) {
    if (!item) return;
    QString text = item->text();
    const int eq = text.indexOf(QStringLiteral(" = "));
    if (eq != -1) text = text.left(eq);
    m_input->setText(text);
}

void ReverseCalculatorDialog::onSwapEndian() {
    const int bits = m_width->currentData().toInt();
    auto &engine = ReverseCalculatorEngine::instance();
    ParseResult res = engine.parseExpression(m_input->text(), bits);
    if (!res.ok) return;

    qulonglong swapped = engine.swapEndian(res.value, bits);
    m_input->setText(engine.formatHex(swapped, bits));
}
