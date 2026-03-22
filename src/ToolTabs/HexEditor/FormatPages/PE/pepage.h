#ifndef PEPAGE_H
#define PEPAGE_H

#include "formatpage.h"
#include <QTableWidget>
#include <QLabel>

class PEPage : public FormatPage
{
    Q_OBJECT

private:
    QTableWidget* m_table = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_warning = nullptr;

public:
    explicit PEPage(QWidget *parent = nullptr);

    QString pageName() const override { return "PE"; }

    void setPageData(QByteArray& data) override;
    QByteArray getPageData() const override;
};

#endif // PEPAGE_H