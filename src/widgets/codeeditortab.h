#ifndef CODEEDITORTAB_H
#define CODEEDITORTAB_H

#include "QCodeEditor.hpp"
#include "tooltab.h"
#include <QWidget>
#include <qfileinfo.h>
#include <qlabel.h>
#include "utils.h"

class CodeEditorTab : public QWidget, public ToolTab
{
    Q_OBJECT

private:
    QCodeEditor* m_codeEditorWidget;
    QWidget* m_overlayWidget;
    bool forceSetData = false;
    uint dataHash = 0;

public:
    explicit CodeEditorTab(QWidget *parent, QString path);

    void saveToFile(QString path) override {

        QByteArray data = m_codeEditorWidget->getBData();
        uint newDataHash = qHash(data, 0);
        if (newDataHash == dataHash) return;
        dataHash = newDataHash;

        QFile f(path);
        if (!f.open(QFile::WriteOnly)) return;
        f.write(data);
        f.close();

        m_codeEditorWidget->document()->setModified(false);
    };

    void setTabData(QByteArray &data) override {

        if (isBinary(data) && !forceSetData){
            m_codeEditorWidget->hide();
            m_overlayWidget->show();
        }
        else{
            dataHash = qHash(data, 0);
            m_codeEditorWidget->show();
            m_overlayWidget->hide();
            m_codeEditorWidget->setBData(data);
            forceSetData = false;
        }
    };

signals:
    void modifyData(bool modified);
    void dataEqual();
    void askData();
    void setHexViewTab();
};

#endif // CODEEDITORTAB_H
