#include "keyboardscancodesref.h"
#include "keyboardscancodevizwidget.h"
#include "core/modules/ModuleManager.h"
#include <QApplication>
#include <QGroupBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

static QString displayName() {
    return QCoreApplication::translate("KeyboardScancodesRef", "Keyboard Scancodes");
}

static bool registered = []() {
    ModuleManager::instance().registerModule<ReferenceBase>(&displayName, "", []() { return new KeyboardScancodesRef(); });
    return true;
}();

struct RefRow {
    const char *name;
    const char *set1;
    const char *notes;
};

static const RefRow kRefRows[] = {
    {"Esc", "01", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","Break code: 81")},
    {"1", "02", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","… 0 (top row) 0B")},
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
    {"Q", "10", QT_TRANSLATE_NOOP("KeyboardScanCodesRef", "Letter rows: set 1 make codes")},
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
    {"Left Ctrl", "1D", QT_TRANSLATE_NOOP("KeyboardScanCodesRef", "Right Ctrl (extended): E0 1D")},
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
    {"Left Shift", "2A", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","Right Shift: 36")},
    {"\\ (backslash)", "2B", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","US 104-key; layout-dependent")},
    {"Z", "2C", ""},
    {"X", "2D", ""},
    {"C", "2E", ""},
    {"V", "2F", ""},
    {"B", "30", ""},
    {"N", "31", ""},
    {"M", "32", ""},
    {",", "33", ""},
    {".", "34", ""},
    {"/", "35", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","Numpad / (ext.): E0 35")},
    {"Right Shift", "36", ""},
    {"* (numpad)", "37", ""},
    {"Left Alt", "38", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","Right Alt / AltGr (ext.): E0 38")},
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
    {"Numpad 7 / Home", "47", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","Behavior depends on Num Lock")},
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
    {"Left Win", "E0 5B", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","GUI keys (extended prefix E0)")},
    {"Right Win", "E0 5C", ""},
    {"Context / Menu", "E0 5D", ""},
    {"Numpad Enter", "E0 1C", ""},
    {"Insert", "E0 52", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","Navigation cluster (E0)")},
    {"Home", "E0 47", ""},
    {"Page Up", "E0 49", ""},
    {"Delete", "E0 53", ""},
    {"End", "E0 4F", ""},
    {"Page Down", "E0 51", ""},
    {"Arrow Up", "E0 48", ""},
    {"Arrow Left", "E0 4B", ""},
    {"Arrow Down", "E0 50", ""},
    {"Arrow Right", "E0 4D", ""},
    {"Print Screen", "E0 37", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","Often a multi-byte sequence; see OS docs")},
    {"Pause", "E1 1D 45 …", QT_TRANSLATE_NOOP("KeyboardScanCodesRef","Long pause make sequence; break differs")},
};

KeyboardScancodesRef::KeyboardScancodesRef(QWidget *parent)
{
    initWindow();
    initWidgets();
    qApp->installEventFilter(this);
}

KeyboardScancodesRef::~KeyboardScancodesRef()
{
    qApp->removeEventFilter(this);
}

void KeyboardScancodesRef::initWindow()
{
    setWindowTitle(tr("Keyboard scan codes"));
    resize(1100, 700);
    setMinimumSize(800, 500);
}

void KeyboardScancodesRef::initWidgets()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // Top: key info + reference table side by side
    auto *topSplitter = new QSplitter(Qt::Horizontal, this);

    // Key info group
    auto *infoGroup = new QGroupBox(tr("Last Key"));
    auto *infoGrid = new QGridLayout(infoGroup);
    infoGrid->setContentsMargins(10, 10, 10, 10);
    infoGrid->setHorizontalSpacing(14);
    infoGrid->setVerticalSpacing(6);

    auto mkLabel = [this]() {
        auto *v = new QLabel(QStringLiteral("None"));
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 12px; color: #71717a;"));
        return v;
    };

    auto addRow = [&](int row, const QString &name, QLabel *&value) {
        auto *lbl = new QLabel(name);
        lbl->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 12px; color: #a1a1aa;"));
        value = mkLabel();
        infoGrid->addWidget(lbl, row, 0, Qt::AlignRight);
        infoGrid->addWidget(value, row, 1);
    };

    addRow(0, tr("Key:"), m_keyNameValue);
    addRow(1, tr("Qt::Key:"), m_qtKeyValue);
    addRow(2, tr("Native scan:"), m_scanValue);
    addRow(3, tr("Native VK:"), m_vkValue);
    addRow(4, tr("Text:"), m_textValue);
    addRow(5, tr("Modifiers:"), m_modsValue);

    infoGroup->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: 600; border: 1px solid #3f3f46; border-radius: 6px; "
        "margin-top: 10px; padding-top: 14px; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }"));
    topSplitter->addWidget(infoGroup);

    // Reference table
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

    auto *scrollTable = new QScrollArea();
    scrollTable->setWidgetResizable(true);
    scrollTable->setWidget(m_table);
    topSplitter->addWidget(scrollTable);

    topSplitter->setStretchFactor(0, 1);
    topSplitter->setStretchFactor(1, 2);

    root->addWidget(topSplitter, 1);

    // Bottom: visual keyboard
    m_viz = new KeyboardScanCodeVizWidget();
    auto *scrollViz = new QScrollArea();
    scrollViz->setWidgetResizable(true);
    scrollViz->setWidget(m_viz);
    scrollViz->setMinimumHeight(240);
    scrollViz->setMaximumHeight(340);
    root->addWidget(scrollViz, 0);
}

bool KeyboardScancodesRef::eventFilter(QObject *obj, QEvent *event)
{
    if (!isVisible())
        return ReferenceBase::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(event);
        // Ignore shortcuts and system keys that should propagate
        if (ke->modifiers() & Qt::AltModifier && (ke->key() == Qt::Key_F4 || ke->key() == Qt::Key_Space))
            return ReferenceBase::eventFilter(obj, event);
        onKeyPress(ke);
        return false;
    }

    if (event->type() == QEvent::KeyRelease) {
        auto *ke = static_cast<QKeyEvent*>(event);
        onKeyRelease(ke);
        return false;
    }

    return ReferenceBase::eventFilter(obj, event);
}

void KeyboardScancodesRef::fillReferenceTable()
{
    const int n = int(sizeof(kRefRows) / sizeof(kRefRows[0]));
    m_table->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(QString::fromUtf8(kRefRows[i].name)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::fromUtf8(kRefRows[i].set1)));
        m_table->setItem(i, 2, new QTableWidgetItem(QCoreApplication::translate("KeyboardScanCodesRef",kRefRows[i].notes)));
    }
}

void KeyboardScancodesRef::onKeyPress(QKeyEvent *event)
{
    QKeyEvent ev(QEvent::KeyPress, event->key(), event->modifiers(), event->text());
    m_viz->applyHighlight(&ev);

    const Qt::KeyboardModifiers mods = event->modifiers();
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

    const QString keyName = QKeySequence(event->key()).toString(QKeySequence::PortableText);
    m_keyNameValue->setText(keyName.isEmpty() ? QStringLiteral("Unknown") : keyName);
    m_qtKeyValue->setText(QString::number(event->key()));
    m_scanValue->setText(QStringLiteral("0x") + QString::number(event->nativeScanCode(), 16).toUpper());
    m_vkValue->setText(QStringLiteral("0x") + QString::number(event->nativeVirtualKey(), 16).toUpper());
    m_textValue->setText(event->text().isEmpty() ? QStringLiteral("None") : event->text());
    m_modsValue->setText(modStr.isEmpty() ? QStringLiteral("None") : modStr);
}

void KeyboardScancodesRef::onKeyRelease(QKeyEvent *event)
{
    Q_UNUSED(event);
    m_viz->clearHighlight();
}
