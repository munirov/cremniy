#include "elfpage.h"
#include "formatpagefactory.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QtEndian>

static bool registered = [](){
    FormatPageFactory::instance().registerPage("2", [](){
        return new ELFPage();
    });
    return true;
}();

ELFPage::ELFPage(QWidget *parent)
    : FormatPage(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    m_title = new QLabel("ELF Header", this);
    m_title->setStyleSheet("font-size: 18px; font-weight: bold; margin: 10px;");
    m_title->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_title);

    m_warning = new QLabel(this);
    m_warning->setStyleSheet("color: #ff8800; font-size: 14px; margin: 10px;");
    m_warning->hide();
    layout->addWidget(m_warning);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({"Поле", "Значение"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    layout->addStretch(1);
}

void ELFPage::setPageData(QByteArray& data)
{
    m_dataHash = qHash(data, 0);
    m_table->setRowCount(0);

    if (data.size() < 64) {
        m_warning->setText("Файл слишком мал для ELF заголовка");
        m_warning->show();
        emit dataEqual();
        return;
    }

    const uchar* bytes = reinterpret_cast<const uchar*>(data.constData());

    bool valid = (bytes[0] == 0x7F && bytes[1] == 'E' &&
                  bytes[2] == 'L'  && bytes[3] == 'F');
    if (!valid) {
        m_warning->setText("Нет ELF-сигнатуры (7F 45 4C 46) → не является ELF файлом");
        m_warning->show();
        emit dataEqual();
        return;
    }

    m_warning->hide();

    auto addRow = [&](const QString& field, const QString& value) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(field));
        m_table->setItem(row, 1, new QTableWidgetItem(value));
    };

    QString cls = bytes[4] == 1 ? "32-bit" : bytes[4] == 2 ? "64-bit" : "Unknown";
    addRow("Class", cls);

    QString endian = bytes[5] == 1 ? "Little Endian" : bytes[5] == 2 ? "Big Endian" : "Unknown";
    addRow("Data", endian);

    uint16_t type = qFromLittleEndian<uint16_t>(bytes + 16);
    QString typeStr;
    switch(type) {
        case 1: typeStr = "Relocatable (ET_REL)"; break;
        case 2: typeStr = "Executable (ET_EXEC)"; break;
        case 3: typeStr = "Shared object (ET_DYN)"; break;
        case 4: typeStr = "Core (ET_CORE)"; break;
        default: typeStr = QString("Unknown (0x%1)").arg(type, 4, 16, QChar('0'));
    }
    addRow("Type", typeStr);

    uint16_t machine = qFromLittleEndian<uint16_t>(bytes + 18);
    QString machineStr;
    switch(machine) {
        case 0x03: machineStr = "x86"; break;
        case 0x3E: machineStr = "x86-64"; break;
        case 0x28: machineStr = "ARM"; break;
        case 0xB7: machineStr = "AArch64"; break;
        default: machineStr = QString("0x%1").arg(machine, 4, 16, QChar('0'));
    }
    addRow("Machine", machineStr);

    emit dataEqual();
}

QByteArray ELFPage::getPageData() const
{
    return QByteArray();
}