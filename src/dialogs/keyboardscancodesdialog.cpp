#include "keyboardscancodesdialog.h"
#include "widgets/keyboardscancodevizwidget.h"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

KeyCaptureFrame::KeyCaptureFrame(QWidget *parent) : QFrame(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::StyledPanel);
    setMinimumHeight(80);
    setStyleSheet(QStringLiteral(
        "KeyCaptureFrame { border: 2px dashed #525252; border-radius: 6px; background-color: rgba(0,0,0,0.15); }"));
}

void KeyCaptureFrame::keyPressEvent(QKeyEvent *event)
{
    emit keyActivity(event->key(), static_cast<quint32>(event->nativeScanCode()),
                     static_cast<quint32>(event->nativeVirtualKey()), event->text(), event->modifiers(), false);
    event->accept();
}

void KeyCaptureFrame::keyReleaseEvent(QKeyEvent *event)
{
    emit keyActivity(event->key(), static_cast<quint32>(event->nativeScanCode()),
                     static_cast<quint32>(event->nativeVirtualKey()), QString(), event->modifiers(), true);
    event->accept();
}

struct RefRow { const char *name; const char *set1; const char *notes; };
static const RefRow kRefRows[] = {
    {"Esc","01",""}, {"1","02","...0 -> 0B"}, {"A","1E",""}, {"Enter","1C",""},
    {"Space","39",""}, {"F1","3B","...F10:44"}, {"F11","57",""}, {"F12","58",""},
    {"Left Ctrl","1D","Right Ctrl: E0 1D"}, {"Left Alt","38","AltGr: E0 38"},
    {"Left Shift","2A","Right Shift: 36"}, {"Backspace","0E",""}, {"Tab","0F",""},
    {"Caps Lock","3A",""}, {"Num Lock","45",""}, {"Scroll Lock","46",""},
    {"Arrow Up","E0 48",""}, {"Arrow Left","E0 4B",""}, {"Arrow Down","E0 50",""},
    {"Arrow Right","E0 4D",""}, {"Insert","E0 52",""}, {"Delete","E0 53",""},
    {"Home","E0 47",""}, {"End","E0 4F",""}, {"Page Up","E0 49",""}, {"Page Down","E0 51",""},
    {"Left Win","E0 5B",""}, {"Right Win","E0 5C",""}, {"Menu","E0 5D",""},
    {"Numpad Enter","E0 1C",""}, {"Print Screen","E0 37",""}, {"Pause","E1 1D 45 ...",""},
};

KeyboardScanCodesDialog::KeyboardScanCodesDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Keyboard scan codes"));
    setModal(false);
    resize(980, 720);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    m_helpToggle = new QToolButton(this);
    m_helpToggle->setText(tr("Справка"));
    m_helpToggle->setCheckable(true);
    root->addWidget(m_helpToggle, 0, Qt::AlignLeft);

    m_helpContent = new QWidget(this);
    auto *helpLay = new QVBoxLayout(m_helpContent);
    helpLay->setContentsMargins(8, 8, 8, 8);
    auto *helpText = new QLabel(
        tr("• Таблица — эталонные make-коды Set 1 (hex).\n"
           "• Поле захвата — текущая нажатая клавиша.\n"
           "• Визуальная клавиатура показывает подсветку клавиши.\n"
           "• Native scan зависит от платформы."),
        m_helpContent);
    helpText->setWordWrap(true);
    helpLay->addWidget(helpText);
    m_helpContent->setVisible(false);
    m_helpContent->setStyleSheet(QStringLiteral("background: rgba(255,255,255,0.03); border: 1px solid #3f3f46; border-radius: 6px;"));
    root->addWidget(m_helpContent);
    connect(m_helpToggle, &QToolButton::toggled, m_helpContent, &QWidget::setVisible);

    auto *capHint = new QLabel(tr("Фокус: кликните в область ниже и нажимайте клавиши"), this);
    root->addWidget(capHint);

    m_capture = new KeyCaptureFrame(this);
    root->addWidget(m_capture);

    auto *statusPanel = new QWidget(this);
    auto *statusForm = new QFormLayout(statusPanel);
    statusForm->setContentsMargins(8, 6, 8, 6);
    statusForm->setLabelAlignment(Qt::AlignRight);
    auto mkVal = [this]() {
        auto *v = new QLabel(QStringLiteral("—"), this);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->setStyleSheet(QStringLiteral("font-family: monospace;"));
        return v;
    };
    m_keyNameValue = mkVal(); m_qtKeyValue = mkVal(); m_scanValue = mkVal();
    m_vkValue = mkVal(); m_textValue = mkVal(); m_modsValue = mkVal();
    statusForm->addRow(tr("Key:"), m_keyNameValue);
    statusForm->addRow(tr("Qt::Key:"), m_qtKeyValue);
    statusForm->addRow(tr("Native scan:"), m_scanValue);
    statusForm->addRow(tr("Native VK:"), m_vkValue);
    statusForm->addRow(tr("Text:"), m_textValue);
    statusForm->addRow(tr("Modifiers:"), m_modsValue);
    statusPanel->setStyleSheet(QStringLiteral("background: rgba(255,255,255,0.03); border: 1px solid #3f3f46; border-radius: 6px;"));
    root->addWidget(statusPanel);

    m_status = new QLabel(tr("Ожидание нажатия клавиши"), this);
    m_status->setStyleSheet(QStringLiteral("color: #a1a1aa;"));
    root->addWidget(m_status);

    auto *split = new QSplitter(Qt::Vertical, this);
    m_table = new QTableWidget();
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Key"), tr("Set 1 make"), tr("Notes")});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fillReferenceTable();
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(1, Qt::AscendingOrder);
    m_table->resizeColumnsToContents();

    auto *tableArea = new QScrollArea(split);
    tableArea->setWidgetResizable(true);
    tableArea->setWidget(m_table);
    m_viz = new KeyboardScanCodeVizWidget();
    auto *vizArea = new QScrollArea(split);
    vizArea->setWidgetResizable(true);
    vizArea->setWidget(m_viz);

    split->addWidget(tableArea);
    split->addWidget(vizArea);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);

    connect(m_capture, &KeyCaptureFrame::keyActivity, this, &KeyboardScanCodesDialog::onKeyActivity);
}

void KeyboardScanCodesDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (m_capture)
        m_capture->setFocus();
}

void KeyboardScanCodesDialog::fillReferenceTable()
{
    const int n = int(sizeof(kRefRows) / sizeof(kRefRows[0]));
    m_table->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(QString::fromUtf8(kRefRows[i].name)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::fromUtf8(kRefRows[i].set1)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::fromUtf8(kRefRows[i].notes)));
    }
}

void KeyboardScanCodesDialog::onKeyActivity(int qtKey, quint32 nativeScan, quint32 nativeVk, const QString &text,
                                            Qt::KeyboardModifiers mods, bool isRelease)
{
    if (isRelease) {
        m_viz->clearHighlight();
        return;
    }

    QKeyEvent ev(QEvent::KeyPress, qtKey, mods, text);
    m_viz->applyHighlight(&ev);

    QString modStr;
    if (mods & Qt::ShiftModifier) modStr += "Shift ";
    if (mods & Qt::ControlModifier) modStr += "Ctrl ";
    if (mods & Qt::AltModifier) modStr += "Alt ";
    if (mods & Qt::MetaModifier) modStr += "Meta ";
    if (mods & Qt::KeypadModifier) modStr += "Keypad ";
    modStr = modStr.trimmed();

    const QString keyName = QKeySequence(qtKey).toString(QKeySequence::PortableText);
    m_keyNameValue->setText(keyName.isEmpty() ? QStringLiteral("Unknown") : keyName);
    m_qtKeyValue->setText(QString::number(qtKey));
    m_scanValue->setText(QStringLiteral("0x") + QString::number(nativeScan, 16).toUpper());
    m_vkValue->setText(QStringLiteral("0x") + QString::number(nativeVk, 16).toUpper());
    m_textValue->setText(text.isEmpty() ? QStringLiteral("—") : text);
    m_modsValue->setText(modStr.isEmpty() ? QStringLiteral("—") : modStr);
    m_status->setText(tr("Последняя клавиша: %1").arg(keyName.isEmpty() ? QStringLiteral("Unknown") : keyName));
}
