#include "pepage.h"
#include "formatpagefactory.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QtEndian>

static bool registered = [](){
    FormatPageFactory::instance().registerPage("3", [](){
        return new PEPage();
    });
    return true;
}();

PEPage::PEPage(QWidget *parent)
    : FormatPage(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    m_title = new QLabel("PE Header", this);
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

void PEPage::setPageData(QByteArray& data)
{
    m_dataHash = qHash(data, 0);
    m_table->setRowCount(0);

    if (data.size() < 64) {
        m_warning->setText("Файл слишком мал для PE заголовка");
        m_warning->show();
        emit dataEqual();
        return;
    }

    const uchar* bytes = reinterpret_cast<const uchar*>(data.constData());

    bool valid = (bytes[0] == 'M' && bytes[1] == 'Z');
    if (!valid) {
        m_warning->setText("Нет DOS-сигнатуры MZ → не является PE файлом");
        m_warning->show();
        emit dataEqual();
        return;
    }

    uint32_t peOffset = qFromLittleEndian<uint32_t>(bytes + 0x3C);
    if ((int)(peOffset + 24) >= data.size()) {
        m_warning->setText("Некорректное смещение PE заголовка");
        m_warning->show();
        emit dataEqual();
        return;
    }

    bool peValid = (bytes[peOffset]   == 'P' && bytes[peOffset+1] == 'E' &&
                    bytes[peOffset+2] == 0   && bytes[peOffset+3] == 0);
    if (!peValid) {
        m_warning->setText("Нет PE-сигнатуры → повреждённый файл");
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

    uint16_t machine = qFromLittleEndian<uint16_t>(bytes + peOffset + 4);
    QString machineStr;
    switch(machine) {
        case 0x014C: machineStr = "x86 (i386)"; break;
        case 0x8664: machineStr = "x86-64 (AMD64)"; break;
        case 0xAA64: machineStr = "AArch64"; break;
        default: machineStr = QString("0x%1").arg(machine, 4, 16, QChar('0'));
    }
    addRow("Machine", machineStr);

    uint16_t sections = qFromLittleEndian<uint16_t>(bytes + peOffset + 6);
    addRow("Number of Sections", QString::number(sections));

    uint16_t chars = qFromLittleEndian<uint16_t>(bytes + peOffset + 22);
    QStringList charList;
    if (chars & 0x0002) charList << "Executable";
    if (chars & 0x0020) charList << "Large address aware";
    if (chars & 0x2000) charList << "DLL";
    addRow("Characteristics", charList.isEmpty() ?
               QString("0x%1").arg(chars, 4, 16, QChar('0')) :
               charList.join(", "));

    emit dataEqual();
}

QByteArray PEPage::getPageData() const
{
    return QByteArray();
}