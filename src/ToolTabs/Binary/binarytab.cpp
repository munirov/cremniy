#include "binarytab.h"
#include "verticaltabstyle.h"
#include <qapplication.h>
#include <qboxlayout.h>
#include <qstackedwidget.h>
#include <qtabwidget.h>
#include <QListWidget>
#include <QTableWidget>
#include "filemanager.h"
#include "formatpagefactory.h"
#include "formatpage.h"
#include "core/ToolTabFactory.h"

static bool registered = [](){
    ToolTabFactory::instance().registerTab("2", [](FileDataBuffer* buffer){
        return new BinaryTab(buffer);
    });
    return true;
}();

BinaryTab::BinaryTab(FileDataBuffer* buffer, QWidget *parent)
    : ToolTab{buffer, parent}
{
    // - - Tab Widgets - -

    // Create Layout
    auto mainHexTabLayout = new QHBoxLayout(this);
    mainHexTabLayout->setSpacing(0);
    mainHexTabLayout->setContentsMargins(0,0,0,0);
    this->setLayout(mainHexTabLayout);

    // Create Tab Widgets
    QListWidget* pageList = new QListWidget();
    pageList->setObjectName("hexTabsList");
    pageList->setFocusPolicy(Qt::NoFocus);
    pageView = new QStackedWidget();

    // Add TabWidgets in Layout
    mainHexTabLayout->addWidget(pageView);
    mainHexTabLayout->addWidget(pageList);

    // - - Create Pages - -
    auto& formatFactory = FormatPageFactory::instance();

    qDebug() << "FormatPageFactory constr: for id in avPages";
    for (const QString& toolID : formatFactory.availablePages()){
        FormatPage* fpage = formatFactory.create(toolID);
        qDebug() << "availablePage: " << fpage->pageName();

        if (fpage) {
            pageView->addWidget(fpage);
            pageList->addItem(fpage->pageName());

            connect(fpage, &FormatPage::modifyData, this, &BinaryTab::pageModifyDataSlot);
            connect(fpage, &FormatPage::dataEqual, this, &ToolTab::dataEqual);
            connect(fpage, &FormatPage::pageDataChanged,
                    this, [this](const QByteArray& data) {
                        if (m_syncingBufferData)
                            return;

                        m_syncingBufferData = true;
                        m_dataBuffer->replaceData(data);
                        m_syncingBufferData = false;

                        if (m_dataBuffer->isModified()) {
                            setModifyIndicator(true);
                            emit modifyData();
                        } else {
                            setModifyIndicator(false);
                            emit dataEqual();
                        }
                    });
            
            // Connect selection signal from page to buffer
            connect(fpage, &FormatPage::selectionChanged, 
                    this, [this](qint64 pos, qint64 length){
                        if (m_updatingSelection) return; // Предотвращаем рекурсию
                        
                        m_updatingSelection = true;
                        m_dataBuffer->setSelection(pos, length);
                        m_updatingSelection = false;
                    });
        }
    }

    // - - End Configurate Tab Widgets - -

    // Configurate
    pageList->setCurrentRow(0);

    // - - Connects - -

    // TabList: select tab
    connect(pageList, &QListWidget::currentRowChanged,
                     pageView, &QStackedWidget::setCurrentIndex);
}


// - - override functions - -

// - public slots -

void BinaryTab::pageModifyDataSlot(){
    setModifyIndicator(true);
    emit modifyData();
}

void BinaryTab::setFile(QString filepath){
    m_fileContext = new FileContext(filepath);
}

void BinaryTab::setTabData(){
    qDebug() << "HexViewTab: setTabData(): start";

    QByteArray data = m_dataBuffer->data();
    qDebug() << "HexViewTab: setTabData(): got data from buffer";

    m_syncingBufferData = true;
    for (int pageIndex = 0; pageIndex < pageView->count(); pageIndex++){
        FormatPage* fpage = dynamic_cast<FormatPage*>(pageView->widget(pageIndex));
        qDebug() << "HexViewTab: setTabData(): start set page data for " << fpage->pageName();
        fpage->setPageData(data);
        qDebug() << "HexViewTab: setTabData(): success set page data for " << fpage->pageName();
    }
    m_syncingBufferData = false;

    if (m_dataBuffer->isModified()) {
        setModifyIndicator(true);
        emit modifyData();
    } else {
        setModifyIndicator(false);
        emit dataEqual();
    }
    qDebug() << "HexViewTab: setTabData(): success";
};

void BinaryTab::onDataChanged()
{
    if (m_syncingBufferData)
        return;

    setTabData();
}

void BinaryTab::onSelectionChanged(qint64 pos, qint64 length)
{
    if (m_updatingSelection) return; // prevent recursion
    
    m_updatingSelection = true;
    
    // Apply selection to all pages
    for (int pageIndex = 0; pageIndex < pageView->count(); pageIndex++){
        FormatPage* fpage = dynamic_cast<FormatPage*>(pageView->widget(pageIndex));
        if (fpage) {
            fpage->setSelection(pos, length);
        }
    }
    
    m_updatingSelection = false;
}

void BinaryTab::saveTabData() {
    qDebug() << "HexViewTab: saveTabData";

    FormatPage* fpage = dynamic_cast<FormatPage*>(pageView->currentWidget());
    if (fpage && !m_syncingBufferData)
        m_dataBuffer->replaceData(fpage->getPageData());

    if (!m_dataBuffer->isModified())
        return;

    if (!m_dataBuffer->saveToFile(m_fileContext->filePath()))
        return;
    
    setModifyIndicator(false);
    emit dataEqual();
    emit refreshDataAllTabsSignal();
}
