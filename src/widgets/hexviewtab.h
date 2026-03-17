#ifndef HEXVIEWTAB_H
#define HEXVIEWTAB_H

#include "QHexView/qhexview.h"
#include "tooltab.h"
#include <QWidget>
#include <qfileinfo.h>

class HexViewTab : public QWidget, public ToolTab
{
    Q_OBJECT

private:
    QHexView* m_hexViewWidget;
    QWidget* createPage();
    uint m_dataHash = 0;

public:
    explicit HexViewTab(QWidget *parent, QString path);

    void saveToFile(QString path) override {

        QByteArray data = m_hexViewWidget->getBData();
        uint newDataHash = qHash(data, 0);
        if (newDataHash == m_dataHash) return;
        m_dataHash = newDataHash;

        QFile f(path);
        if (!f.open(QFile::WriteOnly)) return;
        f.write(data);
        f.close();

    };

    void setTabData(QByteArray &data) override {
        m_dataHash = qHash(data, 0);
        m_hexViewWidget->setBData(data);
        emit dataEqual();
    };

signals:
    void modifyData(bool modified);
    void dataEqual();
};

#endif // HEXVIEWTAB_H
