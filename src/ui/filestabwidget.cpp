#include "filestabwidget.h"
#include <QApplication>
#include <QCoreApplication>
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
  connect(filetab, &FileTab::removeStarSignal, this,
          &FilesTabWidget::removeStar);
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
  // Переключение вкладок

  // ALT + Mouse Wheel UP/DOWN
  if (event->type() == QEvent::Wheel) {
    QWheelEvent *we = static_cast<QWheelEvent *>(event);
    if (we->modifiers() == Qt::AltModifier) {
      int delta = we->angleDelta().y();
      if (delta == 0)
        delta = we->angleDelta().x();

      if (delta != 0 && count() > 1) {
        switchTab(delta > 0 ? 1 : -1);
        return true;
      }
    }
  }
  
  // ALT + Arrow LEFT/RIGHT
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *key = static_cast<QKeyEvent *>(event);
    if (key->modifiers() == Qt::AltModifier) {
      if (key->key() == Qt::Key_Left) {
        switchTab(-1);
        return true;
      }
      if (key->key() == Qt::Key_Right) {
        switchTab(1);
        return true;
      }
    }
  }

  /*
  Middle Mouse Button: закрытие вкладки
  todo: переписать закрытие без лишнего eventFilter, если возможно сделать это
  адекватно
  */
  if (obj == tabBar() && event->type() == QEvent::MouseButtonRelease) {
    QMouseEvent *me = static_cast<QMouseEvent *>(event);
    if (me->button() == Qt::MiddleButton) {
      int index = tabBar()->tabAt(me->pos());
      if (index != -1) {
        QWidget *w = widget(index);
        removeTab(index);
        w->deleteLater();
        return true;
      }
    }
  }
  
if (event->type() == QEvent::KeyPress) {
    QKeyEvent *key = static_cast<QKeyEvent *>(event);
    if (key->modifiers() == Qt::ControlModifier && key->key() == Qt::Key_W) {
        int index = currentIndex();
        if (index != -1) {
            QWidget *w = widget(index);
            removeTab(index);
            w->deleteLater();
        }
        return true;
    }
}
  return QTabWidget::eventFilter(obj, event);
}

void FilesTabWidget::switchTab(int page) {
  int newIdx = currentIndex() + page;
  if (newIdx < 0)
    newIdx = count() - 1;
  else if (newIdx >= count())
    newIdx = 0;
  setCurrentIndex(newIdx);
}
