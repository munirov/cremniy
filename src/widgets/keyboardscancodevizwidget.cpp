#include "keyboardscancodevizwidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <tuple>

static QFrame *makeKeyCellFrame(const QString &label, const QString &codeHex, QWidget *parent, int minWidth)
{
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::StyledPanel);
    f->setMinimumWidth(minWidth);
    f->setMinimumHeight(50);
    f->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *lay = new QVBoxLayout(f);
    lay->setContentsMargins(3, 2, 3, 2);
    lay->setSpacing(0);
    auto *top = new QLabel(label, f);
    top->setAlignment(Qt::AlignCenter);
    top->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 11px;"));
    auto *bot = new QLabel(codeHex, f);
    bot->setAlignment(Qt::AlignCenter);
    bot->setStyleSheet(QStringLiteral("color: #60a5fa; font-size: 10px; font-family: monospace;"));
    lay->addWidget(top);
    lay->addWidget(bot);
    return f;
}

KeyboardScanCodeVizWidget::KeyboardScanCodeVizWidget(QWidget *parent) : QWidget(parent)
{
    static constexpr int u = 46;

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto *mainCol = new QVBoxLayout();
    mainCol->setSpacing(4);

    auto pushRow = [&](std::initializer_list<std::tuple<const char *, const char *, int, int8_t, int>> keys) {
        auto *row = new QHBoxLayout();
        row->setSpacing(3);
        for (const auto &t : keys) {
            const char *lb = std::get<0>(t);
            const char *cd = std::get<1>(t);
            const int qk = std::get<2>(t);
            const int8_t kpf = std::get<3>(t);
            const int wmul = std::get<4>(t);
            if (!lb[0]) {
                row->addSpacing(u * wmul);
                continue;
            }
            QFrame *fr = makeKeyCellFrame(QString::fromUtf8(lb), QString::fromUtf8(cd), this, u * wmul);
            row->addWidget(fr);
            m_cells.push_back({qk, kpf, fr});
        }
        row->addStretch(1);
        mainCol->addLayout(row);
    };

    pushRow({{"Esc", "01", Qt::Key_Escape, -1, 1},
             {"", "", 0, -1, 1},
             {"F1", "3B", Qt::Key_F1, -1, 1},
             {"F2", "3C", Qt::Key_F2, -1, 1},
             {"F3", "3D", Qt::Key_F3, -1, 1},
             {"F4", "3E", Qt::Key_F4, -1, 1},
             {"F5", "3F", Qt::Key_F5, -1, 1},
             {"F6", "40", Qt::Key_F6, -1, 1},
             {"F7", "41", Qt::Key_F7, -1, 1},
             {"F8", "42", Qt::Key_F8, -1, 1},
             {"F9", "43", Qt::Key_F9, -1, 1},
             {"F10", "44", Qt::Key_F10, -1, 1},
             {"F11", "57", Qt::Key_F11, -1, 1},
             {"F12", "58", Qt::Key_F12, -1, 1}});

    pushRow({{"`", "29", Qt::Key_QuoteLeft, 0, 1},
             {"1", "02", Qt::Key_1, 0, 1},
             {"2", "03", Qt::Key_2, 0, 1},
             {"3", "04", Qt::Key_3, 0, 1},
             {"4", "05", Qt::Key_4, 0, 1},
             {"5", "06", Qt::Key_5, 0, 1},
             {"6", "07", Qt::Key_6, 0, 1},
             {"7", "08", Qt::Key_7, 0, 1},
             {"8", "09", Qt::Key_8, 0, 1},
             {"9", "0A", Qt::Key_9, 0, 1},
             {"0", "0B", Qt::Key_0, 0, 1},
             {"-", "0C", Qt::Key_Minus, 0, 1},
             {"=", "0D", Qt::Key_Equal, 0, 1},
             {"Bksp", "0E", Qt::Key_Backspace, -1, 2}});

    pushRow({{"Tab", "0F", Qt::Key_Tab, -1, 2},
             {"Q", "10", Qt::Key_Q, -1, 1},
             {"W", "11", Qt::Key_W, -1, 1},
             {"E", "12", Qt::Key_E, -1, 1},
             {"R", "13", Qt::Key_R, -1, 1},
             {"T", "14", Qt::Key_T, -1, 1},
             {"Y", "15", Qt::Key_Y, -1, 1},
             {"U", "16", Qt::Key_U, -1, 1},
             {"I", "17", Qt::Key_I, -1, 1},
             {"O", "18", Qt::Key_O, -1, 1},
             {"P", "19", Qt::Key_P, -1, 1},
             {"[", "1A", Qt::Key_BracketLeft, -1, 1},
             {"]", "1B", Qt::Key_BracketRight, -1, 1},
             {"\\", "2B", Qt::Key_Backslash, -1, 1}});

    pushRow({{"Caps", "3A", Qt::Key_CapsLock, -1, 2},
             {"A", "1E", Qt::Key_A, -1, 1},
             {"S", "1F", Qt::Key_S, -1, 1},
             {"D", "20", Qt::Key_D, -1, 1},
             {"F", "21", Qt::Key_F, -1, 1},
             {"G", "22", Qt::Key_G, -1, 1},
             {"H", "23", Qt::Key_H, -1, 1},
             {"J", "24", Qt::Key_J, -1, 1},
             {"K", "25", Qt::Key_K, -1, 1},
             {"L", "26", Qt::Key_L, -1, 1},
             {";", "27", Qt::Key_Semicolon, -1, 1},
             {"'", "28", Qt::Key_Apostrophe, -1, 1},
             {"Enter", "1C", Qt::Key_Return, -1, 2}});

    pushRow({{"Shift", "2A", Qt::Key_Shift, -1, 3},
             {"Z", "2C", Qt::Key_Z, -1, 1},
             {"X", "2D", Qt::Key_X, -1, 1},
             {"C", "2E", Qt::Key_C, -1, 1},
             {"V", "2F", Qt::Key_V, -1, 1},
             {"B", "30", Qt::Key_B, -1, 1},
             {"N", "31", Qt::Key_N, -1, 1},
             {"M", "32", Qt::Key_M, -1, 1},
             {",", "33", Qt::Key_Comma, -1, 1},
             {".", "34", Qt::Key_Period, -1, 1},
             {"/", "35", Qt::Key_Slash, -1, 1},
             {"Shift", "36", Qt::Key_Shift, -1, 3}});

    pushRow({{"Ctrl", "1D", Qt::Key_Control, -1, 2},
             {"Win", "E0 5B", Qt::Key_Super_L, -1, 1},
             {"Alt", "38", Qt::Key_Alt, -1, 1},
             {"Space", "39", Qt::Key_Space, -1, 6},
             {"AltGr", "E0 38", Qt::Key_AltGr, -1, 1},
             {"Win", "E0 5C", Qt::Key_Super_R, -1, 1},
             {"Menu", "E0 5D", Qt::Key_Menu, -1, 1},
             {"Ctrl", "E0 1D", Qt::Key_Control, -1, 2}});

    auto *numCol = new QVBoxLayout();
    numCol->setSpacing(4);
    auto addNumRow = [&](std::initializer_list<std::tuple<const char *, const char *, int, int8_t, int>> keys) {
        auto *row = new QHBoxLayout();
        row->setSpacing(3);
        for (const auto &t : keys) {
            QFrame *fr = makeKeyCellFrame(QString::fromUtf8(std::get<0>(t)), QString::fromUtf8(std::get<1>(t)), this,
                                          u * std::get<4>(t));
            row->addWidget(fr);
            m_cells.push_back({std::get<2>(t), std::get<3>(t), fr});
        }
        row->addStretch(1);
        numCol->addLayout(row);
    };

    addNumRow({{"Num", "45", Qt::Key_NumLock, 1, 1},
               {"/", "E0 35", Qt::Key_Slash, 1, 1},
               {"*", "37", Qt::Key_Asterisk, 1, 1},
               {"-", "4A", Qt::Key_Minus, 1, 1}});
    addNumRow({{"7", "47", Qt::Key_7, 1, 1},
               {"8", "48", Qt::Key_8, 1, 1},
               {"9", "49", Qt::Key_9, 1, 1},
               {"+", "4E", Qt::Key_Plus, 1, 1}});
    addNumRow({{"4", "4B", Qt::Key_4, 1, 1},
               {"5", "4C", Qt::Key_5, 1, 1},
               {"6", "4D", Qt::Key_6, 1, 1},
               {"", "", 0, -1, 1}});
    addNumRow({{"1", "4F", Qt::Key_1, 1, 1},
               {"2", "50", Qt::Key_2, 1, 1},
               {"3", "51", Qt::Key_3, 1, 1},
               {"Ent", "E0 1C", Qt::Key_Enter, 1, 1}});
    addNumRow({{"0", "52", Qt::Key_0, 1, 2},
               {".", "53", Qt::Key_Period, 1, 1},
               {"", "", 0, -1, 1}});

    auto *navCol = new QVBoxLayout();
    navCol->setSpacing(4);
    auto addNavRow = [&](std::initializer_list<std::tuple<const char *, const char *, int, int8_t, int>> keys) {
        auto *row = new QHBoxLayout();
        row->setSpacing(3);
        for (const auto &t : keys) {
            if (!std::get<0>(t)[0]) {
                row->addSpacing(u * std::get<4>(t));
                continue;
            }
            QFrame *fr = makeKeyCellFrame(QString::fromUtf8(std::get<0>(t)), QString::fromUtf8(std::get<1>(t)), this,
                                          u * std::get<4>(t));
            row->addWidget(fr);
            m_cells.push_back({std::get<2>(t), std::get<3>(t), fr});
        }
        row->addStretch(1);
        navCol->addLayout(row);
    };

    addNavRow({{"PrtSc", "E0 37", Qt::Key_Print, -1, 1},
               {"ScrLk", "46", Qt::Key_ScrollLock, -1, 1},
               {"Pause", "E1 1D 45", Qt::Key_Pause, -1, 1}});
    addNavRow({{"Ins", "E0 52", Qt::Key_Insert, 0, 1},
               {"Home", "E0 47", Qt::Key_Home, 0, 1},
               {"PgUp", "E0 49", Qt::Key_PageUp, 0, 1}});
    addNavRow({{"Del", "E0 53", Qt::Key_Delete, 0, 1},
               {"End", "E0 4F", Qt::Key_End, 0, 1},
               {"PgDn", "E0 51", Qt::Key_PageDown, 0, 1}});
    addNavRow({{"", "", 0, -1, 1},
               {"↑", "E0 48", Qt::Key_Up, 0, 1},
               {"", "", 0, -1, 1}});
    addNavRow({{"←", "E0 4B", Qt::Key_Left, 0, 1},
               {"↓", "E0 50", Qt::Key_Down, 0, 1},
               {"→", "E0 4D", Qt::Key_Right, 0, 1}});

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
        if (c.qtKey == 0 || c.qtKey != k)
            continue;
        if (c.keypadFilter == 0 && kp)
            continue;
        if (c.keypadFilter == 1 && !kp)
            continue;
        c.frame->setStyleSheet(
            QStringLiteral("QFrame { border: 2px solid #60a5fa; background-color: rgba(96, 165, 250, 0.18); "
                           "border-radius: 4px; }"));
        m_highlighted.push_back(c.frame);
    }
}
