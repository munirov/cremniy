#include "asciicharsref.h"
#include "ui/MenuBar/Menus/References/referencewindowfactory.h"
#include <qboxlayout.h>
#include <qheaderview.h>
#include <qlineedit.h>
#include <qtablewidget.h>
#include <QRegularExpressionValidator>

static bool registered = []() {
    ReferenceWindowFactory::instance().registerRefWin("1", []() { return new AsciiCharsRef(); });
    return true;
}();

AsciiCharsRef::AsciiCharsRef()
{
    initWindow();
    initWidgets();
}

void AsciiCharsRef::initWindow(){
    this->setWindowTitle("ASCII Characters");
    this->setMinimumSize(600, 500);
}

void AsciiCharsRef::initWidgets(){

    QVBoxLayout* layout = new QVBoxLayout(this);

    QTableWidget* table = new QTableWidget(this);
    table->setRowCount(256);
    table->setColumnCount(3);


    QStringList headers;
    headers << "Char" << "Decimal" << "Hex";
    table->setHorizontalHeaderLabels(headers);

    const char* controlNames[33] = {
        "NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL",
        "BS","TAB","LF","VT","FF","CR","SO","SI",
        "DLE","DC1","DC2","DC3","DC4","NAK","SYN","ETB",
        "CAN","EM","SUB","ESC","FS","GS","RS","US","SPACE"
    };

    for(int i = 0; i < 256; ++i){
        QString displayChar;
        if(i < 32){
            displayChar = controlNames[i];
        } else if(i == 32){
            displayChar = "SPACE";
        } else if(i == 127){
            displayChar = "DEL";
        } else {
            displayChar = QString(QChar(i));
        }

        QTableWidgetItem* charItem = new QTableWidgetItem(displayChar);
        QTableWidgetItem* decItem  = new QTableWidgetItem(QString::number(i));
        QTableWidgetItem* hexItem  = new QTableWidgetItem(QString::number(i,16).toUpper());

        charItem->setFlags(charItem->flags() & ~Qt::ItemIsEditable);
        decItem->setFlags(decItem->flags() & ~Qt::ItemIsEditable);
        hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);

        table->setItem(i,0,charItem);
        table->setItem(i,1,decItem);
        table->setItem(i,2,hexItem);
    }

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);

    layout->addWidget(table);

    QHBoxLayout* lineEditLayout = new QHBoxLayout(this);
    layout->addLayout(lineEditLayout);

    QLineEdit* symbolInput = new QLineEdit(this);
    symbolInput->setPlaceholderText("Search by Character");
    lineEditLayout->addWidget(symbolInput);

    QLineEdit* decInput = new QLineEdit(this);
    decInput->setPlaceholderText("Search by Decimal (0-255)");
    decInput->setValidator(new QIntValidator(0,255,this));
    lineEditLayout->addWidget(decInput);

    QLineEdit* hexInput = new QLineEdit(this);
    hexInput->setPlaceholderText("Search by Hex (0-FF)");
    hexInput->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f]{1,2}"),hexInput));
    lineEditLayout->addWidget(hexInput);

    this->setLayout(layout);

    QObject::connect(symbolInput, &QLineEdit::textChanged, this, [=](const QString &text){
        if(text.length() > 1){
            QString last = text.right(1);
            symbolInput->setText(last);
        }
        table->clearSelection();
        if(symbolInput->text().isEmpty()) return;
        int index = symbolInput->text()[0].unicode();
        QTableWidgetSelectionRange range(index,0,index,2);
        table->setRangeSelected(range,true);
        table->scrollToItem(table->item(index,0));
    });

    QObject::connect(decInput, &QLineEdit::textChanged, this, [=](const QString &text){
        table->clearSelection();
        if(text.isEmpty()) return;
        bool ok;
        int num = text.toInt(&ok);
        if(ok && num >= 0 && num <= 255){
            QTableWidgetSelectionRange range(num,0,num,2);
            table->setRangeSelected(range,true);
            table->scrollToItem(table->item(num,0));
        }
    });

    QObject::connect(hexInput, &QLineEdit::textChanged, this, [=](const QString &text){
        table->clearSelection();
        if(text.isEmpty()) return;
        bool ok;
        int num = text.toInt(&ok,16);
        if(ok && num >= 0 && num <= 255){
            QTableWidgetSelectionRange range(num,0,num,2);
            table->setRangeSelected(range,true);
            table->scrollToItem(table->item(num,0));
        }
    });
}
