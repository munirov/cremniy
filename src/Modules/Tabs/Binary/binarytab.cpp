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

    // find
    m_findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(m_findShortcut, &QShortcut::activated, this, &BinaryTab::openFindDialog);

    connect(m_pageList, &QListWidget::currentRowChanged,
            this, [this](int row) {
                pageView->setCurrentIndex(row);
                if (row >= 0 && m_dataBuffer)
                    syncCurrentFormatPage(pageView, m_dataBuffer);
                m_pageDataDirty = false;
            });
}


// - - override functions - -

void BinaryTab::setFileDataBuffer(FileDataBuffer* newFileDataBuffer) {
    if (m_dataBuffer == newFileDataBuffer)
        return;

    TabBase::setFileDataBuffer(newFileDataBuffer);
    if (!m_dataBuffer)
        return;

    if (pageView->count() == 0) {
        createPages();
        return;
    }

    // The buffer has changed and pages already exist, so refresh them all
    for (int pageIndex = 0; pageIndex < pageView->count(); ++pageIndex) {
        auto* fpage = dynamic_cast<FormatPage*>(pageView->widget(pageIndex));
        if (fpage)
            fpage->setSharedBuffer(m_dataBuffer);
    }
}

bool BinaryTab::revealSearchMatch(int oneBasedLine, int zeroBasedColumn, int length)
{
    if (!m_dataBuffer || oneBasedLine < 1 || zeroBasedColumn < 0 || length <= 0)
        return false;

    const QByteArray data = m_dataBuffer->data();
    const QString text = QString::fromUtf8(data);
    int textOffset = 0;
    for (int line = 1; line < oneBasedLine; ++line) {
        const int newline = text.indexOf(QLatin1Char('\n'), textOffset);
        if (newline < 0)
            return false;
        textOffset = newline + 1;
    }
    textOffset += zeroBasedColumn;
    if (textOffset < 0 || textOffset >= text.size())
        return false;

    const qint64 byteOffset = text.left(textOffset).toUtf8().size();
    const qint64 byteLength = text.mid(textOffset, length).toUtf8().size();
    m_dataBuffer->setSelection(byteOffset, qMin<qint64>(byteLength, data.size() - byteOffset));
    return true;
}

void BinaryTab::createPages(){

    auto& formatFactory = FormatPageFactory::instance();

    qDebug() << "FormatPageFactory constr: for id in avPages";
    for (const QString& toolID : formatFactory.availablePages()){
        FormatPage* fpage = formatFactory.create(toolID);
        if (!fpage)
            continue;

        qDebug() << "availablePage: " << fpage->pageName();
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

        // Forward selection changes from the page to the buffer
        connect(fpage,
                &FormatPage::selectionChanged,
                this,
                [this](qint64 pos, qint64 length){
                    if (m_updatingSelection) return; // Prevent recursion

                    m_updatingSelection = true;
                    m_dataBuffer->setSelection(pos, length);
                    m_updatingSelection = false;
                });
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

    // Guard against re-entrancy: setTabData() may trigger a new dataChanged
    // signal while refreshing pages, which would otherwise cause recursion.
    m_pageDataDirty = true;
    m_syncingBufferData = true;
    setTabData();
    m_syncingBufferData = false;
}

void BinaryTab::onSelectionChanged(qint64 pos, qint64 length)
{
    if (m_updatingSelection) return; // Prevent recursion
    
    m_updatingSelection = true;
    
    // Set selection on all pages
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
