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

static const bool registeredBinaryTab =
    registerAlwaysToolTab<BinaryTab>(QStringLiteral("binary"), QStringLiteral("Binary"), ToolTabOrder::Binary);

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

FormatPage* currentFormatPage(QStackedWidget* pageView)
{
    if (!pageView)
        return nullptr;

    return dynamic_cast<FormatPage*>(pageView->currentWidget());
}
}

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

                        updateStatusState();
                    });
            
            // Подключаем сигнал выделения от страницы к буферу
            connect(fpage, &FormatPage::selectionChanged, 
                    this, [this](qint64 pos, qint64 length){
                        if (m_updatingSelection) return; // Предотвращаем рекурсию
                        
                        m_updatingSelection = true;
                        m_dataBuffer->setSelection(pos, length);
                        m_updatingSelection = false;

                        updateStatusState();
                    });
        }
    }

    // - - End Configurate Tab Widgets - -

    // Configurate
    pageList->setCurrentRow(0);

    // - - Connects - -

    // TabList: select tab
    connect(pageList, &QListWidget::currentRowChanged,
            this, [this](int row) {
                pageView->setCurrentIndex(row);
                if (row >= 0)
                    syncCurrentFormatPage(pageView, m_dataBuffer);
                m_pageDataDirty = false;
                updateStatusState();
            });

    m_findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(m_findShortcut, &QShortcut::activated, this, &BinaryTab::openFindDialog);

    updateStatusState();
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

    updateStatusState();
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
    updateStatusState();
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

void BinaryTab::updateStatusState()
{
    auto* page = currentFormatPage(pageView);
    if (!page) {
        setStatusState({"Binary", "", ""});
        return;
    }

    setStatusState(page->statusState());
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
