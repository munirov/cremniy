#include "filetab.h"
#include <qboxlayout.h>
#include <qdir.h>
#include <qevent.h>

FileTab::FileTab(QWidget* parent, QString path)
    : QWidget(parent),
    filePath(path)
{
    qDebug() << "FileTab ctor: this=" << this << " parent=" << parent << " path=" << path;
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

void FileTab::removeStar(){
    qDebug() << "FileTab::removeStar this=" << this << " filePath=" << filePath;
    emit removeStarSignal(this);
    qDebug() << "FileTab::removeStar returned this=" << this;
}

void FileTab::setupStar(){
    qDebug() << "FileTab::setupStar this=" << this << " filePath=" << filePath;
    emit setupStarSignal(this);
}

void FileTab::saveFile(){
    qDebug() << "FileTab::saveFile()";
    emit saveFileSignal();
}
