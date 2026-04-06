#include "filestabwidget.h"
#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTabBar>
#include <QWheelEvent>
#include <qboxlayout.h>
#include <qfileinfo.h>

FilesTabWidget::FilesTabWidget(QWidget *parent) {
    connect(this, &QTabWidget::currentChanged, this, &FilesTabWidget::tabSelect);
    tabBar()->installEventFilter(this);
    QCoreApplication::instance()->installEventFilter(this);
}

void FilesTabWidget::tabSelect(int index) {
    FileTab *tab = qobject_cast<FileTab *>(widget(index));
    if (!tab)
        return;
}

// Create new tab and open file if he is not open already
void FilesTabWidget::openFile(QString filePath, QString tabTitle) {

    // check already open
    for (int i = 0; i < this->count(); ++i) {
        FileTab *t = qobject_cast<FileTab *>(this->widget(i));
        if (t && t->filePath == filePath) {
            this->setCurrentIndex(i);
            return;
        }
    }

    // else if file is not opened
    FileTab *filetab = new FileTab(this, filePath);
    int new_tab_index = this->addTab(filetab, tabTitle);
    this->setCurrentIndex(new_tab_index);

    // - - Connects - -
    connect(filetab, &FileTab::removeStarSignal, this, &FilesTabWidget::removeStar);
    connect(filetab, &FileTab::setupStarSignal, this, &FilesTabWidget::setupStar);
}

void FilesTabWidget::removeStar(FileTab *tab) {
    int index = indexOf(tab);
    if (index != -1) {
        QFileInfo finfo(tab->filePath);
        setTabText(index, finfo.fileName());
    }
}

void FilesTabWidget::setupStar(FileTab *tab) {
    int index = indexOf(tab);
    if (index != -1) {
        QFileInfo finfo(tab->filePath);
        setTabText(index, finfo.fileName() + "*");
    }
}

void FilesTabWidget::saveFileSlot() {
    qDebug() << "FilesTabWidget::saveFileSlot()";
    if (count() > 0) {
        FileTab *currentFileTab = dynamic_cast<FileTab *>(currentWidget());
        currentFileTab->saveFile();
    }
}

bool FilesTabWidget::eventFilter(QObject *obj, QEvent *event) {
    switch (event->type()) {

    // ALT + Mouse Wheel UP/DOWN: для переключения между вкладками
    case QEvent::Wheel: {
        auto *we = static_cast<QWheelEvent *>(event);
        if (we->modifiers() == Qt::AltModifier && count() > 1) {
            int delta = we->angleDelta().y();
            if (delta == 0) {
                delta = we->angleDelta().x();
            }
            if (delta != 0) {
                switchTab(delta > 0 ? 1 : -1);
                return true;
            }
        }
        break;
    }

    case QEvent::KeyPress: {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        // ALT + Arrows: для переключения между вкладками
        if (keyEvent->modifiers() == Qt::AltModifier) {
            if (keyEvent->key() == Qt::Key_Left) {
                switchTab(-1);
                return true;
            } else if (keyEvent->key() == Qt::Key_Right) {
                switchTab(1);
                return true;
            }
            // CTRL + W: для закрытия вкладки
        } else if (keyEvent->modifiers() == Qt::ControlModifier && keyEvent->key() == Qt::Key_W) {
            closeTab(currentIndex());
            return true;
        }
        break;
    }

    // Mouse Middle Button: для закрытия вкладки
    case QEvent::MouseButtonRelease: {
        if (obj == tabBar()) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::MiddleButton) {
                closeTab(tabBar()->tabAt(me->pos()));
                return true;
            }
        }
        break;
    }

    default:
        break;
    }
    return QTabWidget::eventFilter(obj, event);
}

void FilesTabWidget::closeTab(int index) {
    if (index < 0 || index >= count()) {
        return;
    }

    FileTab *tab = qobject_cast<FileTab *>(widget(index));
    if (tab && tab->isFileUnsaved()) {
        const auto replay = QMessageBox::question(this, "Save File", "Do you want to save this file?",
                                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        switch (replay) {
        case QMessageBox::Yes:
            tab->saveFile();
            break;
        case QMessageBox::No:
            break;
        case QMessageBox::Cancel:
            return;
        }
    }

    removeTab(index);
    if (tab)
        tab->deleteLater();
}

void FilesTabWidget::switchTab(int page) {
    int newIdx = currentIndex() + page;
    if (newIdx < 0)
        newIdx = count() - 1;
    else if (newIdx >= count())
        newIdx = 0;
    setCurrentIndex(newIdx);
}
