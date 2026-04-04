#include "filetab.h"
#include <qboxlayout.h>
#include <qdir.h>
#include <qevent.h>

FileTab::FileTab(QWidget* parent, QString path)
    : QWidget(parent),
    filePath(path)
{
    QVBoxLayout *vlayout = new QVBoxLayout(this);
    m_tooltabWidget = new ToolsTabWidget(this, path);
    m_tooltabWidget->setObjectName("toolTabWidget");
    vlayout->addWidget(m_tooltabWidget);
    vlayout->setContentsMargins(0,0,0,0);
    this->setLayout(vlayout);

    // - - Connects - -
    connect(m_tooltabWidget, &ToolsTabWidget::removeStarSignal, this, &FileTab::removeStar);
    connect(m_tooltabWidget, &ToolsTabWidget::setupStarSignal, this, &FileTab::setupStar);

    connect(this, &FileTab::saveFileSignal, m_tooltabWidget, &ToolsTabWidget::saveCurrentTabData);

}

void FileTab::setPinned(bool pinned){
    if (m_pinned == pinned) {
        return;
    }
    m_pinned = pinned;
    emit pinnedChanged(this);
}

void FileTab::removeStar(){
    m_modified = false;
    emit removeStarSignal(this);
}

void FileTab::setupStar(){
    m_modified = true;
    emit setupStarSignal(this);
}

void FileTab::saveFile(){
    qDebug() << "FileTab::saveFile()";
    emit removeStarSignal(this);
    emit saveFileSignal();
}

void FileTab::navigateToCodeLine(int lineNumber, bool selectWholeLine)
{
    m_tooltabWidget->activateCodeEditorAtLine(lineNumber, selectWholeLine);
}

void FileTab::navigateToSearchHit(int lineNumber, const QString &needle)
{
    m_tooltabWidget->activateCodeEditorSearchHit(lineNumber, needle);
}
