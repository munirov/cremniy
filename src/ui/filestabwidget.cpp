#include "filestabwidget.h"
#include <qboxlayout.h>
#include <qfileinfo.h>

FilesTabWidget::FilesTabWidget(QWidget *parent) {
    connect(this, &QTabWidget::currentChanged, this, &FilesTabWidget::tabSelect);
}

void FilesTabWidget::tabSelect(int index){
    FileTab *tab = qobject_cast<FileTab*>(widget(index));
    if (!tab) return;
}

// Create new tab and open file if he is not open already
void FilesTabWidget::openFile(QString filePath, QString tabTitle){

    qDebug() << "FilesTabWidget::openFile path=" << filePath << " title=" << tabTitle << " currentCount=" << count();

    // check already open
    for (int i = 0; i < this->count(); ++i)
    {
        FileTab *t = qobject_cast<FileTab*>(this->widget(i));
        if (t && t->filePath == filePath)
        {
            this->setCurrentIndex(i);
            return;
        }
    }

    // else if file is not opened
    FileTab *filetab = new FileTab(this, filePath);
    int new_tab_index = this->addTab(filetab, tabTitle);
    this->setCurrentIndex(new_tab_index);
    qDebug() << "FilesTabWidget::openFile created filetab=" << filetab << " index=" << new_tab_index;

    // - - Connects - -
    connect(filetab, &FileTab::removeStarSignal, this, &FilesTabWidget::removeStar);
    connect(filetab, &FileTab::setupStarSignal, this, &FilesTabWidget::setupStar);
}

void FilesTabWidget::removeStar(FileTab* tab){
    int index = indexOf(tab);
    qDebug() << "FilesTabWidget::removeStar tab=" << tab << " index=" << index << " count=" << count();
    if (index != -1) {
        QFileInfo finfo(tab->filePath);
        setTabText(index, finfo.fileName());
        qDebug() << "FilesTabWidget::removeStar updated title for" << finfo.fileName();
    }
}

void FilesTabWidget::setupStar(FileTab* tab){
    int index = indexOf(tab);
    qDebug() << "FilesTabWidget::setupStar tab=" << tab << " index=" << index << " count=" << count();
    if (index != -1) {
        QFileInfo finfo(tab->filePath);
        setTabText(index, finfo.fileName() + "*");
    }
}

void FilesTabWidget::saveFileSlot(){
    qDebug() << "FilesTabWidget::saveFileSlot()";
    if (count() > 0){
        FileTab* currentFileTab = dynamic_cast<FileTab*>(currentWidget());
        currentFileTab->saveFile();
    }
}
