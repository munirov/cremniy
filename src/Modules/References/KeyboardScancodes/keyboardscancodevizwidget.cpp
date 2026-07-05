#include "keyboardscancodevizwidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <tuple>

using KeyDef = std::tuple<const char *, const char *, int, int, int8_t, int>;

static QFrame *makeKeyCellFrame(const QString &label, const QString &codeHex, QWidget *parent, int minWidth)
{
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::StyledPanel);
    f->setMinimumWidth(minWidth);
    f->setMinimumHeight(44);
    f->setMaximumHeight(54);
    f->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *lay = new QVBoxLayout(f);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(0);
    auto *top = new QLabel(label, f);
    top->setAlignment(Qt::AlignCenter);
    top->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 10px;"));
    auto *bot = new QLabel(codeHex, f);
    bot->setAlignment(Qt::AlignCenter);
    bot->setStyleSheet(QStringLiteral("color: #60a5fa; font-size: 9px; font-family: monospace;"));
    lay->addWidget(top);
    lay->addWidget(bot);
    return f;
}

static void pushKeyRow(QVBoxLayout *col, std::initializer_list<KeyDef> keys,
                        QVector<KeyboardScanCodeVizWidget::Cell> &cells, QWidget *parent)
{
    auto *row = new QHBoxLayout();
    row->setSpacing(2);
    for (const auto &t : keys) {
        const char *lb = std::get<0>(t);
        const char *cd = std::get<1>(t);
        const int qk = std::get<2>(t);
        const int qkAlt = std::get<3>(t);
        const int8_t kpf = std::get<4>(t);
        const int wmul = std::get<5>(t);
        if (!lb[0]) {
            row->addSpacing(38 * wmul);
            continue;
        }
        QFrame *fr = makeKeyCellFrame(QString::fromUtf8(lb), QString::fromUtf8(cd), parent, 38 * wmul);
        row->addWidget(fr);
        cells.push_back({qk, qkAlt, kpf, fr});
    }
    row->addStretch(1);
    col->addLayout(row);
}

static void pushKeyRowH(QHBoxLayout *row, std::initializer_list<KeyDef> keys,
                          QVector<KeyboardScanCodeVizWidget::Cell> &cells, QWidget *parent)
{
    for (const auto &t : keys) {
        const char *lb = std::get<0>(t);
        const char *cd = std::get<1>(t);
        const int qk = std::get<2>(t);
        const int qkAlt = std::get<3>(t);
        const int8_t kpf = std::get<4>(t);
        const int wmul = std::get<5>(t);
        if (!lb[0]) {
            row->addSpacing(38 * wmul);
            continue;
        }
        QFrame *fr = makeKeyCellFrame(QString::fromUtf8(lb), QString::fromUtf8(cd), parent, 38 * wmul);
        row->addWidget(fr);
        cells.push_back({qk, qkAlt, kpf, fr});
    }
}

KeyboardScanCodeVizWidget::KeyboardScanCodeVizWidget(QWidget *parent) : QWidget(parent)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    auto *mainCol = new QVBoxLayout();
    mainCol->setSpacing(3);

    // F-row
    pushKeyRow(mainCol, {
        {"Esc", "01", Qt::Key_Escape, 0, -1, 1},
        {"", "", 0, 0, -1, 1},
        {"F1", "3B", Qt::Key_F1, 0, -1, 1}, {"F2", "3C", Qt::Key_F2, 0, -1, 1},
        {"F3", "3D", Qt::Key_F3, 0, -1, 1}, {"F4", "3E", Qt::Key_F4, 0, -1, 1},
        {"F5", "3F", Qt::Key_F5, 0, -1, 1}, {"F6", "40", Qt::Key_F6, 0, -1, 1},
        {"F7", "41", Qt::Key_F7, 0, -1, 1}, {"F8", "42", Qt::Key_F8, 0, -1, 1},
        {"F9", "43", Qt::Key_F9, 0, -1, 1}, {"F10", "44", Qt::Key_F10, 0, -1, 1},
        {"F11", "57", Qt::Key_F11, 0, -1, 1}, {"F12", "58", Qt::Key_F12, 0, -1, 1}
    }, m_cells, this);

    // Number row — Russian: ё Б У К Е Н Г Ш Щ З Х Ъ І
    pushKeyRow(mainCol, {
        {"`", "29", Qt::Key_QuoteLeft, 0x0451, 0, 1},
        {"1", "02", Qt::Key_1, 0x0411, 0, 1},
        {"2", "03", Qt::Key_2, 0x0423, 0, 1},
        {"3", "04", Qt::Key_3, 0x041A, 0, 1},
        {"4", "05", Qt::Key_4, 0x0415, 0, 1},
        {"5", "06", Qt::Key_5, 0x041D, 0, 1},
        {"6", "07", Qt::Key_6, 0x0413, 0, 1},
        {"7", "08", Qt::Key_7, 0x0448, 0, 1},
        {"8", "09", Qt::Key_8, 0x0429, 0, 1},
        {"9", "0A", Qt::Key_9, 0x0417, 0, 1},
        {"0", "0B", Qt::Key_0, 0x0425, 0, 1},
        {"-", "0C", Qt::Key_Minus, 0x042A, 0, 1},
        {"=", "0D", Qt::Key_Equal, 0x0406, 0, 1},
        {"Bksp", "0E", Qt::Key_Backspace, 0, -1, 2}
    }, m_cells, this);

    // Q row — Russian: Ф Ц У К Е Н Г Ш Щ З Х Ъ
    pushKeyRow(mainCol, {
        {"Tab", "0F", Qt::Key_Tab, 0, -1, 2},
        {"Q", "10", Qt::Key_Q, 0x0424, -1, 1},
        {"W", "11", Qt::Key_W, 0x0426, -1, 1},
        {"E", "12", Qt::Key_E, 0x0423, -1, 1},
        {"R", "13", Qt::Key_R, 0x041A, -1, 1},
        {"T", "14", Qt::Key_T, 0x0415, -1, 1},
        {"Y", "15", Qt::Key_Y, 0x041D, -1, 1},
        {"U", "16", Qt::Key_U, 0x0413, -1, 1},
        {"I", "17", Qt::Key_I, 0x0448, -1, 1},
        {"O", "18", Qt::Key_O, 0x0429, -1, 1},
        {"P", "19", Qt::Key_P, 0x0417, -1, 1},
        {"[", "1A", Qt::Key_BracketLeft, 0x0425, -1, 1},
        {"]", "1B", Qt::Key_BracketRight, 0x042A, -1, 1},
        {"\\", "2B", Qt::Key_Backslash, 0x042D, -1, 1}
    }, m_cells, this);

    // A row — Russian: Ф Ы В А П Р О Л Д Ж Э
    pushKeyRow(mainCol, {
        {"Caps", "3A", Qt::Key_CapsLock, 0, -1, 2},
        {"A", "1E", Qt::Key_A, 0x0424, -1, 1},
        {"S", "1F", Qt::Key_S, 0x042B, -1, 1},
        {"D", "20", Qt::Key_D, 0x0412, -1, 1},
        {"F", "21", Qt::Key_F, 0x0410, -1, 1},
        {"G", "22", Qt::Key_G, 0x041F, -1, 1},
        {"H", "23", Qt::Key_H, 0x0420, -1, 1},
        {"J", "24", Qt::Key_J, 0x041E, -1, 1},
        {"K", "25", Qt::Key_K, 0x041B, -1, 1},
        {"L", "26", Qt::Key_L, 0x0414, -1, 1},
        {";", "27", Qt::Key_Semicolon, 0x0416, -1, 1},
        {"'", "28", Qt::Key_Apostrophe, 0x042D, -1, 1},
        {"Enter", "1C", Qt::Key_Return, 0, -1, 2}
    }, m_cells, this);

    // Z row — Russian: Я Ч С М И Т Ь Б Ю
    pushKeyRow(mainCol, {
        {"Shift", "2A", Qt::Key_Shift, 0, -1, 3},
        {"Z", "2C", Qt::Key_Z, 0x042F, -1, 1},
        {"X", "2D", Qt::Key_X, 0x0427, -1, 1},
        {"C", "2E", Qt::Key_C, 0x0421, -1, 1},
        {"V", "2F", Qt::Key_V, 0x041C, -1, 1},
        {"B", "30", Qt::Key_B, 0x0418, -1, 1},
        {"N", "31", Qt::Key_N, 0x0422, -1, 1},
        {"M", "32", Qt::Key_M, 0x042C, -1, 1},
        {",", "33", Qt::Key_Comma, 0x0411, -1, 1},
        {".", "34", Qt::Key_Period, 0x042E, -1, 1},
        {"/", "35", Qt::Key_Slash, 0x042E, -1, 1},
        {"Shift", "36", Qt::Key_Shift, 0, -1, 3}
    }, m_cells, this);

    // Bottom row
    pushKeyRow(mainCol, {
        {"Ctrl", "1D", Qt::Key_Control, 0, -1, 2},
        {"Win", "E0 5B", Qt::Key_Super_L, 0, -1, 1},
        {"Alt", "38", Qt::Key_Alt, 0, -1, 1},
        {"Space", "39", Qt::Key_Space, 0, -1, 6},
        {"AltGr", "E0 38", Qt::Key_AltGr, 0, -1, 1},
        {"Win", "E0 5C", Qt::Key_Super_R, 0, -1, 1},
        {"Menu", "E0 5D", Qt::Key_Menu, 0, -1, 1},
        {"Ctrl", "E0 1D", Qt::Key_Control, 0, -1, 2}
    }, m_cells, this);

    // Numpad
    auto *numCol = new QVBoxLayout();
    numCol->setSpacing(3);

    auto numRow = [&](std::initializer_list<KeyDef> keys) {
        auto *row = new QHBoxLayout();
        row->setSpacing(2);
        pushKeyRowH(row, keys, m_cells, this);
        row->addStretch(1);
        numCol->addLayout(row);
    };

    numRow({{"Num", "45", Qt::Key_NumLock, 0, 1, 1},
            {"/", "E0 35", Qt::Key_Slash, 0, 1, 1},
            {"*", "37", Qt::Key_Asterisk, 0, 1, 1},
            {"-", "4A", Qt::Key_Minus, 0, 1, 1}});
    numRow({{"7", "47", Qt::Key_7, 0, 1, 1},
            {"8", "48", Qt::Key_8, 0, 1, 1},
            {"9", "49", Qt::Key_9, 0, 1, 1},
            {"+", "4E", Qt::Key_Plus, 0, 1, 1}});
    numRow({{"4", "4B", Qt::Key_4, 0, 1, 1},
            {"5", "4C", Qt::Key_5, 0, 1, 1},
            {"6", "4D", Qt::Key_6, 0, 1, 1},
            {"", "", 0, 0, -1, 1}});
    numRow({{"1", "4F", Qt::Key_1, 0, 1, 1},
            {"2", "50", Qt::Key_2, 0, 1, 1},
            {"3", "51", Qt::Key_3, 0, 1, 1},
            {"Ent", "E0 1C", Qt::Key_Enter, 0, 1, 1}});
    numRow({{"0", "52", Qt::Key_0, 0, 1, 2},
            {".", "53", Qt::Key_Period, 0, 1, 1},
            {"", "", 0, 0, -1, 1}});

    // Navigation cluster
    auto *navCol = new QVBoxLayout();
    navCol->setSpacing(3);

    auto navRow = [&](std::initializer_list<KeyDef> keys) {
        auto *row = new QHBoxLayout();
        row->setSpacing(2);
        pushKeyRowH(row, keys, m_cells, this);
        row->addStretch(1);
        navCol->addLayout(row);
    };

    navRow({{"PrtSc", "E0 37", Qt::Key_Print, 0, -1, 1},
            {"ScrLk", "46", Qt::Key_ScrollLock, 0, -1, 1},
            {"Pause", "E1 1D 45", Qt::Key_Pause, 0, -1, 1}});
    navRow({{"Ins", "E0 52", Qt::Key_Insert, 0, 0, 1},
            {"Home", "E0 47", Qt::Key_Home, 0, 0, 1},
            {"PgUp", "E0 49", Qt::Key_PageUp, 0, 0, 1}});
    navRow({{"Del", "E0 53", Qt::Key_Delete, 0, 0, 1},
            {"End", "E0 4F", Qt::Key_End, 0, 0, 1},
            {"PgDn", "E0 51", Qt::Key_PageDown, 0, 0, 1}});
    navRow({{"", "", 0, 0, -1, 1},
            {"\xe2\x86\x91", "E0 48", Qt::Key_Up, 0, 0, 1},
            {"", "", 0, 0, -1, 1}});
    navRow({{"\xe2\x86\x90", "E0 4B", Qt::Key_Left, 0, 0, 1},
            {"\xe2\x86\x93", "E0 50", Qt::Key_Down, 0, 0, 1},
            {"\xe2\x86\x92", "E0 4D", Qt::Key_Right, 0, 0, 1}});

    root->addLayout(mainCol, 1);
    root->addLayout(numCol, 0);
    root->addLayout(navCol, 0);
}

void KeyboardScanCodeVizWidget::clearHighlight()
{
    for (QFrame *f : m_highlighted) {
        f->setStyleSheet(QString());
    }
    m_highlighted.clear();
}

void KeyboardScanCodeVizWidget::applyHighlight(const QKeyEvent *e)
{
    clearHighlight();
    const int k = e->key();
    const bool kp = (e->modifiers() & Qt::KeypadModifier) != 0;

    for (const Cell &c : m_cells) {
        if (c.qtKey == 0)
            continue;

        if (c.qtKey != k && c.qtKeyAlt != k)
            continue;

        if (c.keypadFilter == 0 && kp)
            continue;
        if (c.keypadFilter == 1 && !kp)
            continue;

        c.frame->setStyleSheet(
            QStringLiteral("QFrame { border: 2px solid #60a5fa; background-color: rgba(96, 165, 250, 0.35); "
                           "border-radius: 4px; }"));
        m_highlighted.push_back(c.frame);
    }
}
