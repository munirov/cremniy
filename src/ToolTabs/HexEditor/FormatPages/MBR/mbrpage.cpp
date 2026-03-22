#include "mbrpage.h"
#include "formatpagefactory.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QtEndian>

static bool registered = [](){
    FormatPageFactory::instance().registerPage("MBRPage", [](){
        return new MBRPage();
    });
    return true;
}();

MBRPage::MBRPage(QWidget *parent)
    : FormatPage(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    m_title = new QLabel("Master Boot Record (MBR)", this);
    m_title->setStyleSheet("font-size: 18px; font-weight: bold; margin: 10px;");
    m_title->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_title);

    m_warning = new QLabel(this);
    m_warning->setStyleSheet("color: #ff8800; font-size: 14px; margin: 10px;");
    m_warning->hide();
    layout->addWidget(m_warning);

    m_table = new QTableWidget(4, 6, this);
    m_table->setHorizontalHeaderLabels({"№", "Active", "Type (hex)", "Start LBA", "Size (sectors)", "Тип"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    layout->addStretch(1);
}

void MBRPage::setPageData(QByteArray& data)
{
    m_dataHash = qHash(data, 0);

    if (data.size() < 512) {
        m_warning->setText("Файл слишком мал для MBR (< 512 байт)");
        m_warning->show();
        return;
    }

    const uchar* bytes = reinterpret_cast<const uchar*>(data.constData());

    bool valid = (bytes[510] == 0x55 && bytes[511] == 0xAA);
    if (!valid) {
        m_warning->setText("Нет сигнатуры 55 AA на позициях 510–511 → не похоже на MBR");
        m_warning->show();
    } else {
        m_warning->hide();
    }

    for (int i = 0; i < 4; ++i) {
        int off = 0x1BE + i * 16;
        uchar boot = bytes[off];
        uchar type = bytes[off + 4];
        uint32_t lba  = qFromLittleEndian<uint32_t>(bytes + off + 8);
        uint32_t size = qFromLittleEndian<uint32_t>(bytes + off + 12);

        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));

        QString active = (boot == 0x80) ? "Yes" : (boot == 0 ? "No" : QString("0x%1").arg(boot, 2, 16, QChar('0')));
        m_table->setItem(i, 1, new QTableWidgetItem(active));
        m_table->setItem(i, 2, new QTableWidgetItem(QString("0x%1").arg(type, 2, 16, QChar('0'))));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(lba)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(size)));

        QString desc = "Unknown";
        if (type == 0x00) desc = "Empty";
        else if (type == 0x01) desc = "FAT12";
        else if (type == 0x04 || type == 0x06 || type == 0x0E) desc = "FAT16";
        else if (type == 0x0B || type == 0x0C) desc = "FAT32";
        else if (type == 0x07) desc = "NTFS / exFAT";
        else if (type == 0x83) desc = "Linux";
        else if (type == 0xEE) desc = "GPT protective";
        m_table->setItem(i, 5, new QTableWidgetItem(desc));

        if (boot == 0x80) {
            for (int c = 0; c < 6; ++c)
                m_table->item(i, c)->setBackground(QColor(0, 100, 0, 60));
        }
    }

    emit dataEqual();
}

QByteArray MBRPage::getPageData() const
{
    return QByteArray();
}