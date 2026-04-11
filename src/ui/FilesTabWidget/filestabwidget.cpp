#include "filestabwidget.h"
#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTabBar>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QIcon>
#include <QPainter>
#include <QFontMetrics>
#include <QPixmap>
#include <qboxlayout.h>
#include <qfileinfo.h>

FilesTabWidget::FilesTabWidget(QWidget *parent) {
    connect(this, &QTabWidget::currentChanged, this, &FilesTabWidget::tabSelect);
    connect(tabBar(), &QTabBar::tabMoved, this, &FilesTabWidget::onTabMoved);
    tabBar()->installEventFilter(this);
    QCoreApplication::instance()->installEventFilter(this);

    connect(this, &FilesTabWidget::openTabModule,
            this, [this](ModuleDescription<TabBase> desc){
                if (auto* tab = qobject_cast<FileTab*>(currentWidget()))
                    emit tab->openTabModule(desc);
            });
}

void FilesTabWidget::tabSelect(int index) {
    FileTab *tab = qobject_cast<FileTab *>(widget(index));
    if (!tab || !tab->toolsTabWidget()) {
        emit statusBarInfoChanged(QString());
        return;
    }
    QWidget* currentTool = tab->toolsTabWidget()->currentWidget();
    QString lastInfo = currentTool ? currentTool->property("lastStatusBarInfo").toString() : QString();
    emit statusBarInfoChanged(lastInfo);
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
    connect(filetab, &FileTab::pinnedChanged, this, &FilesTabWidget::updatePinnedState);
    connect(filetab, &FileTab::statusBarInfoChanged, this, &FilesTabWidget::statusBarInfoChanged);

    connect(this, &FilesTabWidget::setWordWrapSignal, filetab, &FileTab::setWordWrapSlot);
    connect(this, &FilesTabWidget::setTabReplaceSignal, filetab, &FileTab::setTabReplaceSlot);
    connect(this, &FilesTabWidget::setTabWidthSignal, filetab, &FileTab::setTabWidthSlot);

}

void FilesTabWidget::removeStar(FileTab *tab) {
    int index = indexOf(tab);
    if (index != -1) {
        setPinnedTabText(index, tab);
    }
}

void FilesTabWidget::setupStar(FileTab *tab) {
    int index = indexOf(tab);
    if (index != -1) {
        setPinnedTabText(index, tab);
    }
}

void FilesTabWidget::updatePinnedState(FileTab *tab) {
    int index = indexOf(tab);
    if (index != -1) {
        if (tab->isPinned()) {
            const int targetIndex = 0;
            if (index != targetIndex) {
                m_adjustingTabMove = true;
                tabBar()->moveTab(index, targetIndex);
                m_adjustingTabMove = false;
                index = targetIndex;
            }
        }
        setPinnedTabText(index, tab);
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

    case QEvent::ContextMenu: {
        if (obj == tabBar()) {
            auto *ce = static_cast<QContextMenuEvent *>(event);
            int index = tabBar()->tabAt(ce->pos());
            if (index >= 0) {
                FileTab *tab = qobject_cast<FileTab *>(widget(index));
                if (tab) {
                    QMenu menu(this);
                    QAction *pinAction = menu.addAction(tab->isPinned() ? tr("Unpin") : tr("Pin"));
                    QAction *chosen = menu.exec(ce->globalPos());
                    if (chosen == pinAction) {
                        tab->setPinned(!tab->isPinned());
                        return true;
                    }
                }
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
    if (tab && tab->isPinned()) {
        return;
    }

    if (tab && tab->isFileUnsaved()) {
        QMessageBox question_save_file(QMessageBox::Question, tr("Save File"), tr("Do you want to save this file?"),
                                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
        question_save_file.setButtonText(QMessageBox::Yes, tr("Yes"));
        question_save_file.setButtonText(QMessageBox::No, tr("No"));
        question_save_file.setButtonText(QMessageBox::Cancel, tr("Cancel"));

        const auto replay = question_save_file.exec();
        switch (replay) {
        case QMessageBox::Yes:
            tab->saveFile();
            break;
        case QMessageBox::No:
            break;
        case QMessageBox::Cancel:
            return;
        default:
            break;
        }
    }

    removeTab(index);
    if (tab)
        tab->deleteLater();
    if (count() == 0)
        emit statusBarInfoChanged(QString());
}

void FilesTabWidget::switchTab(int page) {
    int newIdx = currentIndex() + page;
    if (newIdx < 0)
        newIdx = count() - 1;
    else if (newIdx >= count())
        newIdx = 0;
    setCurrentIndex(newIdx);
}

void FilesTabWidget::onTabMoved(int from, int to) {
    if (m_adjustingTabMove) {
        return;
    }

    FileTab *tab = qobject_cast<FileTab *>(widget(to));
    if (!tab) {
        return;
    }

    const int pinned = pinnedCount();
    const bool isPinned = tab->isPinned();
    const bool toPinnedZone = to < pinned;
    const bool fromPinnedZone = from < pinned;

    if (isPinned) {
        if (fromPinnedZone && toPinnedZone) {
            return;
        }
        m_adjustingTabMove = true;
        tabBar()->moveTab(to, from);
        m_adjustingTabMove = false;
        return;
    }

    if (!isPinned && toPinnedZone) {
        m_adjustingTabMove = true;
        tabBar()->moveTab(to, pinned);
        m_adjustingTabMove = false;
    }
}

void FilesTabWidget::setPinnedTabText(int index, FileTab *tab) {
    static const QIcon pinIcon = []() {
        QFont font;
        font.setPixelSize(12);
        QFontMetrics fm(font);
        const int size = fm.height();
        QPixmap pix(size, size);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QStringLiteral("📌"));
        painter.end();
        return QIcon(pix);
    }();

    QFileInfo finfo(tab->filePath);
    QString text = finfo.fileName();
    if (tab->isFileUnsaved()) {
        text += "*";
    }
    if (tab->isPinned()) {
        QIcon themed = QIcon::fromTheme(QStringLiteral("emblem-pinned"));
        setTabIcon(index, themed.isNull() ? pinIcon : themed);
    } else {
        setTabIcon(index, QIcon());
    }
    setTabText(index, text);
}

int FilesTabWidget::pinnedCount() const {
    int countPinned = 0;
    for (int i = 0; i < count(); ++i) {
        FileTab *tab = qobject_cast<FileTab *>(widget(i));
        if (tab && tab->isPinned()) {
            ++countPinned;
        }
    }
    return countPinned;
}

void FilesTabWidget::setWordWrapSlot(bool checked){
    emit setWordWrapSignal(checked);
}

void FilesTabWidget::setTabReplaceSlot(bool checked){
    emit setTabReplaceSignal(checked);
}

void FilesTabWidget::setTabWidthSlot(int width){
    emit setTabWidthSignal(width);
}
