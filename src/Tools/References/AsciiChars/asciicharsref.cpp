#include "asciicharsref.h"
#include "core/ToolsRegistry.h"
#include <qboxlayout.h>
#include <qclipboard.h>
#include <qguiapplication.h>
#include <qheaderview.h>
#include <qlineedit.h>
#include <qmenu.h>
#include <qtabbar.h>
#include <qtablewidget.h>
#include <memory>
#include <QRegularExpressionValidator>

static bool registered = registerReferenceTool<AsciiCharsRef>("ascii_chars", "ASCII / Unicode Characters");

AsciiCharsRef::AsciiCharsRef(QWidget* parent)
    : ReferenceWindow(parent)
{
    initWindow();
    initWidgets();
}

void AsciiCharsRef::initWindow() {
    this->setWindowTitle("ASCII / Unicode Characters");
    this->setMinimumSize(840, 580);
}

struct RowData {
    QString displayChar;
    int decimal = 0;
    QString hex;
};

static QVector<RowData> buildAsciiRows() {
    static const char* controlNames[33] = {
        "NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL",
        "BS","TAB","LF","VT","FF","CR","SO","SI",
        "DLE","DC1","DC2","DC3","DC4","NAK","SYN","ETB",
        "CAN","EM","SUB","ESC","FS","GS","RS","US","SPACE"
    };

    QVector<RowData> rows;
    rows.reserve(128);
    for (int i = 0; i < 128; ++i) {
        QString displayChar;
        if (i < 32) {
            displayChar = controlNames[i];
        }
        else if (i == 32) {
            displayChar = "SPACE";
        }
        else if (i == 127) {
            displayChar = "DEL";
        }
        else {
            displayChar = QString(QChar(i));
        }

        RowData row;
        row.displayChar = displayChar;
        row.decimal = i;
        row.hex = QString::number(i, 16).toUpper();
        rows.push_back(row);
    }
    return rows;
}

static void appendRange(QVector<RowData>& rows, int from, int to) {
    for (int cp = from; cp <= to; ++cp) {
        RowData row;
        row.displayChar = QString(QChar(cp));
        row.decimal = cp;
        row.hex = QString::number(cp, 16).toUpper();
        rows.push_back(row);
    }
}

static QVector<RowData> buildUnicodeRows() {
    QVector<RowData> rows;
    rows.reserve(512);

    // --- Latin ---
    appendRange(rows, 0x0100, 0x017F); // U+0100-U+017F

    // --- Cyrillic ---
    appendRange(rows, 0x0410, 0x042F); // Upper: A-Ya
    appendRange(rows, 0x0401, 0x0401); // Upper: Yo
    appendRange(rows, 0x0430, 0x044F); // Lower: a-ya
    appendRange(rows, 0x0451, 0x0451); // Lower: yo

    // --- Greek ---
    appendRange(rows, 0x0391, 0x03A9); // Upper: Alpha-Omega  U+0391-U+03A9
    appendRange(rows, 0x03B1, 0x03C9); // Lower: alpha-omega  U+03B1-U+03C9

    // --- Math Symbols ---
    appendRange(rows, 0x2200, 0x2233); // U+2200-U+2233

    // --- Arrows ---
    appendRange(rows, 0x2190, 0x21FF); // U+2190-U+21FF

    // --- Box Drawing ---
    appendRange(rows, 0x2500, 0x257F); // U+2500-U+257F

    // --- Currency Symbols ---
    appendRange(rows, 0x20A0, 0x20CF); // U+20A0-U+20CF

    // --- Misc Symbols
    appendRange(rows, 0x2600, 0x266F); // U+2600-U+266F

    return rows;
}

void AsciiCharsRef::initWidgets() {

    QVBoxLayout* layout = new QVBoxLayout(this);

    QTabBar* tabBar = new QTabBar(this);
    tabBar->addTab("ASCII (0-127)");
    tabBar->addTab("Unicode");
    tabBar->setExpanding(false);
    layout->addWidget(tabBar);

    QTableWidget* table = new QTableWidget(this);
    table->setColumnCount(3);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setContextMenuPolicy(Qt::CustomContextMenu);

    QStringList asciiHeaders;
    asciiHeaders << "Char" << "Decimal" << "Hex";
    table->setHorizontalHeaderLabels(asciiHeaders);

    auto currentRows = std::make_shared<QVector<RowData>>(buildAsciiRows());
    auto isUnicodeMode = std::make_shared<bool>(false);

    auto renderTable = [table, isUnicodeMode](const QVector<RowData>& rows) {
        bool unicode = *isUnicodeMode;
        int cols = unicode ? 4 : 3;
        table->setColumnCount(cols);

        QStringList headers;
        if (unicode)
            headers << "Char" << "Code" << "Decimal" << "Hex";
        else
            headers << "Char" << "Decimal" << "Hex";
        table->setHorizontalHeaderLabels(headers);

        table->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i) {
            QTableWidgetItem* charItem = new QTableWidgetItem(rows[i].displayChar);
            charItem->setFlags(charItem->flags() & ~Qt::ItemIsEditable);
            table->setItem(i, 0, charItem);

            if (unicode) {
                QString code = QString("U+%1").arg(rows[i].decimal, 4, 16, QChar('0')).toUpper();
                QTableWidgetItem* codeItem = new QTableWidgetItem(code);
                codeItem->setFlags(codeItem->flags() & ~Qt::ItemIsEditable);
                table->setItem(i, 1, codeItem);
            }

            int decCol = unicode ? 2 : 1;
            int hexCol = unicode ? 3 : 2;

            QTableWidgetItem* decItem = new QTableWidgetItem(QString::number(rows[i].decimal));
            QTableWidgetItem* hexItem = new QTableWidgetItem(rows[i].hex);
            decItem->setFlags(decItem->flags() & ~Qt::ItemIsEditable);
            hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);
            table->setItem(i, decCol, decItem);
            table->setItem(i, hexCol, hexItem);
        }

        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        };

    renderTable(*currentRows);

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);

    layout->addWidget(table);

    QHBoxLayout* lineEditLayout = new QHBoxLayout();
    layout->addLayout(lineEditLayout);

    QLineEdit* symbolInput = new QLineEdit(this);
    symbolInput->setPlaceholderText("Search by Character");
    lineEditLayout->addWidget(symbolInput);

    QLineEdit* decInput = new QLineEdit(this);
    decInput->setPlaceholderText("Search by Decimal (0-127)");
    decInput->setValidator(new QIntValidator(0, 127, this));
    lineEditLayout->addWidget(decInput);

    QLineEdit* hexInput = new QLineEdit(this);
    hexInput->setPlaceholderText("Search by Hex (0-7F)");
    hexInput->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f]{1,2}"), hexInput));
    lineEditLayout->addWidget(hexInput);

    this->setLayout(layout);

    QObject::connect(symbolInput, &QLineEdit::textChanged, this, [=](const QString& text) {
        if (text.length() > 1) {
            QString last = text.right(1);
            symbolInput->setText(last);
        }
        table->clearSelection();
        if (symbolInput->text().isEmpty()) return;
        QString target = symbolInput->text().left(1);
        for (int i = 0; i < currentRows->size(); ++i) {
            if ((*currentRows)[i].displayChar == target) {
                QTableWidgetSelectionRange range(i, 0, i, table->columnCount() - 1);
                table->setRangeSelected(range, true);
                table->scrollToItem(table->item(i, 0));
                break;
            }
        }
        });

    QObject::connect(decInput, &QLineEdit::textChanged, this, [=](const QString& text) {
        table->clearSelection();
        if (text.isEmpty()) return;
        bool ok;
        int num = text.toInt(&ok);
        if (ok) {
            for (int i = 0; i < currentRows->size(); ++i) {
                if ((*currentRows)[i].decimal == num) {
                    QTableWidgetSelectionRange range(i, 0, i, table->columnCount() - 1);
                    table->setRangeSelected(range, true);
                    table->scrollToItem(table->item(i, 0));
                    break;
                }
            }
        }
        });

    QObject::connect(hexInput, &QLineEdit::textChanged, this, [=](const QString& text) {
        table->clearSelection();
        if (text.isEmpty()) return;
        bool ok;
        int num = text.toInt(&ok, 16);
        if (ok) {
            for (int i = 0; i < currentRows->size(); ++i) {
                if ((*currentRows)[i].decimal == num) {
                    QTableWidgetSelectionRange range(i, 0, i, 2);
                    table->setRangeSelected(range, true);
                    table->scrollToItem(table->item(i, 0));
                    break;
                }
            }
        }
        });

    QObject::connect(tabBar, &QTabBar::currentChanged, this, [=](int index) {
        if (index == 0) {
            *isUnicodeMode = false;
            *currentRows = buildAsciiRows();
            decInput->setPlaceholderText("Search by Decimal (0-127)");
            decInput->setValidator(new QIntValidator(0, 127, decInput));
            hexInput->setPlaceholderText("Search by Hex (0-7F)");
            hexInput->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f]{1,2}"), hexInput));
        }
        else {
            *isUnicodeMode = true;
            *currentRows = buildUnicodeRows();
            decInput->setPlaceholderText("Search by Decimal (Unicode)");
            decInput->setValidator(new QIntValidator(0, 0xFFFF, decInput));
            hexInput->setPlaceholderText("Search by Hex (Unicode)");
            hexInput->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f]{1,4}"), hexInput));
        }

        symbolInput->clear();
        decInput->clear();
        hexInput->clear();
        table->clearSelection();
        renderTable(*currentRows);
        });

    QObject::connect(table, &QTableWidget::customContextMenuRequested, this, [=](const QPoint& pos) {
        QTableWidgetItem* item = table->itemAt(pos);
        if (!item) return;
        int row = item->row();
        bool unicode = *isUnicodeMode;

        QMenu menu(table);
        QAction* copyChar = menu.addAction("Copy Char");
        QAction* copyCode = unicode ? menu.addAction("Copy Code") : nullptr;
        QAction* copyDec = menu.addAction("Copy Decimal");
        QAction* copyHex = menu.addAction("Copy Hex");
        QAction* copyRow = menu.addAction("Copy Row");

        QAction* chosen = menu.exec(table->viewport()->mapToGlobal(pos));
        if (!chosen) return;

        int decCol = unicode ? 2 : 1;
        int hexCol = unicode ? 3 : 2;

        QString textToCopy;
        if (chosen == copyChar) {
            textToCopy = table->item(row, 0)->text();
        }
        else if (unicode && chosen == copyCode) {
            textToCopy = table->item(row, 1)->text();
        }
        else if (chosen == copyDec) {
            textToCopy = table->item(row, decCol)->text();
        }
        else if (chosen == copyHex) {
            textToCopy = table->item(row, hexCol)->text();
        }
        else if (chosen == copyRow) {
            QStringList parts;
            for (int c = 0; c < table->columnCount(); ++c)
                parts << table->item(row, c)->text();
            textToCopy = "| " + parts.join(" | ") + " |";
        }

        if (!textToCopy.isEmpty()) {
            QGuiApplication::clipboard()->setText(textToCopy);
        }
        });
}

