#include "keyboardscancodesdialog.h"
#include "widgets/keyboardscancodevizwidget.h"
#include "ui/MenuBar/Menus/References/referencewindowfactory.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QFormLayout>
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

struct RefRow {
    const char *name;
    const char *set1;
    const char *notes;
};

static const RefRow kRefRows[] = {
    {"Esc", "01", "Break code: 81"},
    {"1", "02", "… 0 (top row) 0B"},
    {"2", "03", ""},
    {"3", "04", ""},
    {"4", "05", ""},
    {"5", "06", ""},
    {"6", "07", ""},
    {"7", "08", ""},
    {"8", "09", ""},
    {"9", "0A", ""},
    {"0", "0B", ""},
    {"- (minus)", "0C", ""},
    {"= (equals)", "0D", ""},
    {"Backspace", "0E", ""},
    {"Tab", "0F", ""},
    {"Q", "10", "Letter rows: set 1 make codes"},
    {"W", "11", ""},
    {"E", "12", ""},
    {"R", "13", ""},
    {"T", "14", ""},
    {"Y", "15", ""},
    {"U", "16", ""},
    {"I", "17", ""},
    {"O", "18", ""},
    {"P", "19", ""},
    {"[", "1A", ""},
    {"]", "1B", ""},
    {"Enter (main)", "1C", ""},
    {"Left Ctrl", "1D", "Right Ctrl (extended): E0 1D"},
    {"A", "1E", ""},
    {"S", "1F", ""},
    {"D", "20", ""},
    {"F", "21", ""},
    {"G", "22", ""},
    {"H", "23", ""},
    {"J", "24", ""},
    {"K", "25", ""},
    {"L", "26", ""},
    {";", "27", ""},
    {"'", "28", ""},
    {"` (grave)", "29", ""},
    {"Left Shift", "2A", "Right Shift: 36"},
    {"\\ (backslash)", "2B", "US 104-key; layout-dependent"},
    {"Z", "2C", ""},
    {"X", "2D", ""},
    {"C", "2E", ""},
    {"V", "2F", ""},
    {"B", "30", ""},
    {"N", "31", ""},
    {"M", "32", ""},
    {",", "33", ""},
    {".", "34", ""},
    {"/", "35", "Numpad / (ext.): E0 35"},
    {"Right Shift", "36", ""},
    {"* (numpad)", "37", ""},
    {"Left Alt", "38", "Right Alt / AltGr (ext.): E0 38"},
    {"Space", "39", ""},
    {"Caps Lock", "3A", ""},
    {"F1", "3B", "F10: 44"},
    {"F2", "3C", ""},
    {"F3", "3D", ""},
    {"F4", "3E", ""},
    {"F5", "3F", ""},
    {"F6", "40", ""},
    {"F7", "41", ""},
    {"F8", "42", ""},
    {"F9", "43", ""},
    {"F10", "44", ""},
    {"Num Lock", "45", ""},
    {"Scroll Lock", "46", ""},
    {"Numpad 7 / Home", "47", "Behavior depends on Num Lock"},
    {"Numpad 8 / Up", "48", ""},
    {"Numpad 9 / PgUp", "49", ""},
    {"Numpad -", "4A", ""},
    {"Numpad 4 / Left", "4B", ""},
    {"Numpad 5", "4C", ""},
    {"Numpad 6 / Right", "4D", ""},
    {"Numpad +", "4E", ""},
    {"Numpad 1 / End", "4F", ""},
    {"Numpad 2 / Down", "50", ""},
    {"Numpad 3 / PgDn", "51", ""},
    {"Numpad 0 / Ins", "52", ""},
    {"Numpad . / Del", "53", ""},
    {"F11", "57", ""},
    {"F12", "58", ""},
    {"Left Win", "E0 5B", "GUI keys (extended prefix E0)"},
    {"Right Win", "E0 5C", ""},
    {"Context / Menu", "E0 5D", ""},
    {"Numpad Enter", "E0 1C", ""},
    {"Insert", "E0 52", "Navigation cluster (E0)"},
    {"Home", "E0 47", ""},
    {"Page Up", "E0 49", ""},
    {"Delete", "E0 53", ""},
    {"End", "E0 4F", ""},
    {"Page Down", "E0 51", ""},
    {"Arrow Up", "E0 48", ""},
    {"Arrow Left", "E0 4B", ""},
    {"Arrow Down", "E0 50", ""},
    {"Arrow Right", "E0 4D", ""},
    {"Print Screen", "E0 37", "Often a multi-byte sequence; see OS docs"},
    {"Pause", "E1 1D 45 …", "Long pause make sequence; break differs"},
};

static bool s_keyboardScanCodesRefRegistered = []() {
    ReferenceWindowFactory::instance().registerRefWin(
        QStringLiteral("2"), []() { return new KeyboardScanCodesDialog(); });
    return true;
}();

KeyboardScanCodesDialog::KeyboardScanCodesDialog(QWidget *parent) : ReferenceWindow(parent)
{
    initWindow();
    initWidgets();
}

QString KeyboardScanCodesDialog::RefWinName()
{
    return tr("Keyboard Scan-Codes");
}

void KeyboardScanCodesDialog::initWindow()
{
    setWindowTitle(tr("Keyboard scan codes"));
    resize(980, 720);
}

void KeyboardScanCodesDialog::initWidgets()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    m_helpToggle = new QToolButton(this);
    m_helpToggle->setText(tr("Справка"));
    m_helpToggle->setCheckable(true);
    m_helpToggle->setChecked(false);
    m_helpToggle->setToolButtonStyle(Qt::ToolButtonTextOnly);
    root->addWidget(m_helpToggle, 0, Qt::AlignLeft);

    m_helpContent = new QWidget(this);
    auto *helpLay = new QVBoxLayout(m_helpContent);
    helpLay->setContentsMargins(8, 8, 8, 8);
    auto *helpText = new QLabel(
        tr("• Таблица показывает эталонные make-коды IBM PC/AT Scan Code Set 1 (hex).\n"
           "• Поле захвата отображает текущую нажатую клавишу и native-коды из Qt.\n"
           "• На визуальной клавиатуре подсвечивается соответствующая клавиша.\n"
           "• Native scan code зависит от платформы и может отличаться от Set 1."),
        m_helpContent);
    helpText->setWordWrap(true);
    helpLay->addWidget(helpText);
    m_helpContent->setVisible(false);
    m_helpContent->setStyleSheet(QStringLiteral("background: rgba(255,255,255,0.03); border: 1px solid #3f3f46; border-radius: 6px;"));
    root->addWidget(m_helpContent);
    connect(m_helpToggle, &QToolButton::toggled, m_helpContent, &QWidget::setVisible);

    m_capture = new KeyCaptureFrame(this);
    auto *capHint = new QLabel(tr("Фокус: кликните в область ниже и нажимайте клавиши"), this);
    root->addWidget(capHint);
    root->addWidget(m_capture);

    auto *statusPanel = new QWidget(this);
    auto *statusForm = new QFormLayout(statusPanel);
    statusForm->setContentsMargins(8, 6, 8, 6);
    statusForm->setLabelAlignment(Qt::AlignRight);
    statusForm->setHorizontalSpacing(14);
    statusForm->setVerticalSpacing(4);

    auto mkVal = [this]() {
        auto *v = new QLabel(QStringLiteral("—"), this);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->setStyleSheet(QStringLiteral("font-family: monospace;"));
        return v;
    };
    m_keyNameValue = mkVal();
    m_qtKeyValue = mkVal();
    m_scanValue = mkVal();
    m_vkValue = mkVal();
    m_textValue = mkVal();
    m_modsValue = mkVal();

    statusForm->addRow(tr("Key:"), m_keyNameValue);
    statusForm->addRow(tr("Qt::Key:"), m_qtKeyValue);
    statusForm->addRow(tr("Native scan:"), m_scanValue);
    statusForm->addRow(tr("Native VK:"), m_vkValue);
    statusForm->addRow(tr("Text:"), m_textValue);
    statusForm->addRow(tr("Modifiers:"), m_modsValue);
    statusPanel->setStyleSheet(QStringLiteral("background: rgba(255,255,255,0.03); border: 1px solid #3f3f46; border-radius: 6px;"));
    root->addWidget(statusPanel);

    m_status = new QLabel(tr("Ожидание нажатия клавиши"), this);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
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

    auto *scrollTable = new QScrollArea(split);
    scrollTable->setWidgetResizable(true);
    scrollTable->setWidget(m_table);
    scrollTable->setMinimumHeight(200);

    m_viz = new KeyboardScanCodeVizWidget();
    auto *scrollViz = new QScrollArea(split);
    scrollViz->setWidgetResizable(true);
    scrollViz->setWidget(m_viz);
    scrollViz->setMinimumHeight(280);

    split->addWidget(scrollTable);
    split->addWidget(scrollViz);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);

    root->addWidget(split, 1);

    connect(m_capture, &KeyCaptureFrame::keyActivity, this, &KeyboardScanCodesDialog::onKeyActivity);

    m_capture->setFocus();
}

void KeyboardScanCodesDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
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
    if (mods & Qt::ShiftModifier)
        modStr += QStringLiteral("Shift ");
    if (mods & Qt::ControlModifier)
        modStr += QStringLiteral("Ctrl ");
    if (mods & Qt::AltModifier)
        modStr += QStringLiteral("Alt ");
    if (mods & Qt::MetaModifier)
        modStr += QStringLiteral("Meta ");
    if (mods & Qt::KeypadModifier)
        modStr += QStringLiteral("Keypad ");
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
