#include "tooltabwidget.h"
#include <QCodeEditor.hpp>
#include <QFile>
#include <QSyntaxStyle.hpp>

#include <QCodeEditor.hpp>
#include <QCECompleter.hpp>
#include <QSyntaxStyle.hpp>
#include <QCXXHighlighter.hpp>
#include <QJSONHighlighter.hpp>
#include <qboxlayout.h>
#include <qfileinfo.h>

ToolTabWidget::ToolTabWidget(QWidget *parent, QString path)
    {

    // Tabs
    m_codeEditorTab = new CodeEditorTab(this, path);
    m_hexViewTab = new HexViewTab(this, path);
    m_hexViewTab->setObjectName("hexViewTab");
    m_disassemblerTab = new DisassemblerTab(this, path);

    // Tab Icons
    QIcon codeIcon(":/icons/code.png");
    QIcon hexIcon(":/icons/hex.png");
    QIcon disasmIcon(":/icons/dasm.png");

    // Add Tabs
    this->addTab(m_codeEditorTab, codeIcon, "Code");
    this->addTab(m_hexViewTab, hexIcon, "Hex");
    this->addTab(m_disassemblerTab, disasmIcon, "Disassembler");

    // - - Connects - -

    // Code Editor Tab
    connect(m_codeEditorTab, &CodeEditorTab::modifyData, this, &ToolTabWidget::setupStar);
    connect(m_codeEditorTab, &CodeEditorTab::dataEqual, this, &ToolTabWidget::removeStar);
    connect(m_codeEditorTab, &CodeEditorTab::askData, this, &ToolTabWidget::giveData);
    connect(m_codeEditorTab, &CodeEditorTab::setHexViewTab, this, &ToolTabWidget::setHexViewTab);

    // Hex View Tab
    connect(m_hexViewTab, &HexViewTab::modifyData, this, &ToolTabWidget::setupStar);

    // Disassembler Tab
    connect(m_disassemblerTab, &DisassemblerTab::modifyData, this, &ToolTabWidget::setupStar);
}

void ToolTabWidget::removeStar(){

    qDebug() << "ToolTabWidget: removeStar()";

    // remove star at sender
    QObject* obj = sender();
    QWidget* widget = qobject_cast<QWidget*>(obj);

    if (!widget) return;

    int index = indexOf(widget);
    if (index < 0) return;

    QString text = tabText(index);
    text.replace("*", "");
    setTabText(index, text);

    // check other tabs
    if (!tabText(this->indexOf(m_codeEditorTab)).contains("*") &&
        !tabText(this->indexOf(m_hexViewTab)).contains("*") &&
        !tabText(this->indexOf(m_disassemblerTab)).contains("*") ){
        emit removeStarSignal();
    }

}

void ToolTabWidget::setupStar(bool modified){

    qDebug() << "ToolTabWidget: setupStar()";

    // setup star on tab
    QObject* obj = sender();
    QWidget* widget = qobject_cast<QWidget*>(obj);

    if (!widget) return;

    int index = indexOf(widget);
    if (index < 0) return;

    QString text = tabText(index);
    if (!text.endsWith("*")){
        setTabText(index, text + "*");
    }

    // signal "setup star" to up
    emit setupStarSignal();

}

void ToolTabWidget::setHexViewTab(){
    int index = indexOf(m_hexViewTab);
    setCurrentIndex(index);
}

void ToolTabWidget::giveData(){
    QObject* s = sender();
    int index = -1;
    if (s == m_codeEditorTab)
        index = this->indexOf(m_codeEditorTab);
    else if (s == m_hexViewTab)
        index = this->indexOf(m_hexViewTab);
    else if (s == m_disassemblerTab)
        index = this->indexOf(m_disassemblerTab);

    if (index >= 0){
        emit askData(index);
    }
}

int ToolTabWidget::saveToFileCurrentTab(QString path){
    QWidget* w = currentWidget();
    int index = currentIndex();
    if (!w) return -1;

    ToolTab* tab = dynamic_cast<ToolTab*>(w);
    if (!tab) return -1;

    tab->saveToFile(path);
    QString text = tabText(index);
    text.replace("*", "");
    setTabText(index, text);
    return index;
}

void ToolTabWidget::setDataInTabs(QByteArray &data, int index, int excluded_index){
    if (index >= 0){
        QWidget* w = widget(index);
        if (!w) return;

        ToolTab* tab = dynamic_cast<ToolTab*>(w);
        if (!tab) return;

        tab->setTabData(data);
    }
    else{
        for (int i = 0; i < count(); ++i) {
            if (i == excluded_index) continue;
            QWidget* w = widget(i);
            if (!w) return;

            ToolTab* tab = dynamic_cast<ToolTab*>(w);
            if (!tab) return;

            tab->setTabData(data);
        }
    }
}
