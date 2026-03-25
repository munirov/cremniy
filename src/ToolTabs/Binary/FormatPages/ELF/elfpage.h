#ifndef ELFPAGE_H
#define ELFPAGE_H

#include "formatpage.h"
#include <QTableWidget>
#include <QLabel>

class ELFPage : public FormatPage
{
    Q_OBJECT

private:
    QTableWidget* m_table = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_warning = nullptr;

public:
    explicit ELFPage(QWidget *parent = nullptr);

    QString pageName() const override { return "ELF"; }

    void setPageData(QByteArray& data) override;
    QByteArray getPageData() const override;
    void setSelection(qint64 pos, qint64 length) override { Q_UNUSED(pos); Q_UNUSED(length); }
};

#endif // ELFPAGE_H