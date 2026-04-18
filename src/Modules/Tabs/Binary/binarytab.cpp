#include "binarytab.h"
#include <qapplication.h>
#include <qboxlayout.h>
#include <qstackedwidget.h>
#include <qtabwidget.h>
#include <QListWidget>
#include <QTableWidget>
#include "formatpagefactory.h"
#include "formatpage.h"
#include "core/modules/ModuleManager.h"

static QString displayName() {
    return QCoreApplication::translate("BinaryTab", "Binary");
}

static bool registered = []() {
    ModuleManager::instance().registerModule<TabBase>(&displayName, "always", []() { return new BinaryTab(); }, 200);
    return true;
}();

namespace {
void syncCurrentFormatPage(QStackedWidget* pageView, FileDataBuffer* dataBuffer)
{
    if (!pageView || !dataBuffer)
        return;

    auto* currentPage = dynamic_cast<FormatPage*>(pageView->currentWidget());
    if (!currentPage)
        return;

    currentPage->setSharedBuffer(dataBuffer);
}
}

BinaryTab::BinaryTab(QWidget *parent)
    : TabBase{parent}
{
    // - - Tab Widgets - -
    qDebug() << "BinaryTab ctor start";
    // Create Layout
    auto mainHexTabLayout = new QHBoxLayout(this);
    mainHexTabLayout->setSpacing(0);
    mainHexTabLayout->setContentsMargins(0,0,0,0);
    this->setLayout(mainHexTabLayout);

    // Create Tab Widgets
    m_pageList = new QListWidget();
    m_pageList->setObjectName("hexTabsList");
    m_pageList->setFocusPolicy(Qt::NoFocus);
    pageView = new QStackedWidget();

    // Add TabWidgets in Layout
    mainHexTabLayout->addWidget(pageView);
    mainHexTabLayout->addWidget(m_pageList);

    // - - End Configurate Tab Widgets - -

}


// - - override functions - -

void BinaryTab::setFileDataBuffer(FileDataBuffer* newFileDataBuffer){
    m_dataBuffer = newFileDataBuffer;

    connect(m_dataBuffer, &FileDataBuffer::byteChanged,
            this, &BinaryTab::onByteChanged);
    connect(m_dataBuffer, &FileDataBuffer::bytesChanged,
            this, &BinaryTab::onBytesChanged);
    connect(m_dataBuffer, &FileDataBuffer::selectionChanged,
            this, &BinaryTab::onSelectionChanged);
    connect(m_dataBuffer, &FileDataBuffer::dataChanged,
            this, &BinaryTab::onDataChanged);

    createPages();

    // TabList: select tab
    connect(m_pageList, &QListWidget::currentRowChanged,
            this, [this](int row) {
                pageView->setCurrentIndex(row);
                if (row >= 0)
                    syncCurrentFormatPage(pageView, m_dataBuffer);
                m_pageDataDirty = false;
            });

    connect(m_dataBuffer,
            &FileDataBuffer::selectionChanged,
            this,
            &BinaryTab::onSelectionChanged
            );
}

void BinaryTab::createPages(){

    auto& formatFactory = FormatPageFactory::instance();

    qDebug() << "FormatPageFactory constr: for id in avPages";
    for (const QString& toolID : formatFactory.availablePages()){
        FormatPage* fpage = formatFactory.create(toolID);
        qDebug() << "availablePage: " << fpage->pageName();

        if (fpage) {
            pageView->addWidget(fpage);
            m_pageList->addItem(fpage->pageName());

            connect(fpage, &FormatPage::modifyData, this, &BinaryTab::pageModifyDataSlot);
            connect(fpage, &FormatPage::dataEqual, this, &TabBase::dataEqual);
            connect(fpage,
                    &FormatPage::pageDataChanged,
                    this,
                    [this](const QByteArray& data) {
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

            // Forward status bar info from format page
            connect(fpage, &FormatPage::statusBarInfoChanged,
                    this, &BinaryTab::statusBarInfoChanged);

            // Подключаем сигнал выделения от страницы к буферу
            connect(fpage,
                    &FormatPage::selectionChanged,
                    this,
                    [this](qint64 pos, qint64 length){
                        if (m_updatingSelection) return; // Предотвращаем рекурсию

                        m_updatingSelection = true;
                        m_dataBuffer->setSelection(pos, length);
                        m_updatingSelection = false;
                    });
        }
    }
    m_pageList->setCurrentRow(0);
}

void BinaryTab::pageModifyDataSlot(){
    setModifyIndicator(true);
    emit modifyData();
}

void BinaryTab::setFile(QString filepath){
    m_fileContext = new FileContext(filepath);
}

void BinaryTab::setTabData(){
    qDebug() << "HexViewTab: setTabData(): start";

    m_syncingBufferData = true;
    if (auto* currentPage = dynamic_cast<FormatPage*>(pageView->currentWidget()); currentPage) {
        qDebug() << "HexViewTab: setTabData(): start set page data for " << currentPage->pageName();
        currentPage->setSharedBuffer(m_dataBuffer);
        qDebug() << "HexViewTab: setTabData(): success set page data for " << currentPage->pageName();
    }
    m_syncingBufferData = false;
    m_pageDataDirty = false;

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

    m_pageDataDirty = true;
    setTabData();
}

void BinaryTab::onSelectionChanged(qint64 pos, qint64 length)
{
    if (m_updatingSelection) return; // Предотвращаем рекурсию
    
    m_updatingSelection = true;
    
    // Устанавливаем выделение на всех страницах
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

    if (!m_dataBuffer->isModified())
        return;

    if (!m_dataBuffer->saveToFile(m_fileContext->filePath()))
        return;
    
    setModifyIndicator(false);
    emit dataEqual();
    emit refreshDataAllTabsSignal();
}

void BinaryTab::openFindDialog()
{
    if (auto* currentPage = dynamic_cast<FormatPage*>(pageView->currentWidget()); currentPage && currentPage->showFind()) {
        return;
    }

    for (int pageIndex = 0; pageIndex < pageView->count(); ++pageIndex) {
        if (auto* page = dynamic_cast<FormatPage*>(pageView->widget(pageIndex)); page) {
            pageView->setCurrentIndex(pageIndex);
            if (page->showFind())
                return;
        }
    }
}

bool BinaryTab::showFind()
{
    openFindDialog();
    return true;
}
