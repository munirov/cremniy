#include "filestabwidget.h"

#include <qabstractbutton.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
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
#include <QPushButton>
#include "Modules/Tabs/CodeEditor/codeeditortab.h"
#include "buildTab/buildtab.h"
#include "core/file/FileDataBuffer.h"
#include "core/file/filesynccontroller.h"
#include "core/file/filesyncdecision.h"
#include "core/file/filesyncmonitor.h"

namespace {

QString comparablePath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).replace('\\', '/');
}

bool samePath(const QString& left, const QString& right)
{
#ifdef Q_OS_WIN
    return comparablePath(left).compare(comparablePath(right), Qt::CaseInsensitive) == 0;
#else
    return comparablePath(left) == comparablePath(right);
#endif
}

} // namespace

FilesTabWidget::FilesTabWidget(QWidget *parent) {
    connect(this, &QTabWidget::currentChanged, this, &FilesTabWidget::tabSelect);
    connect(tabBar(), &QTabBar::tabMoved, this, &FilesTabWidget::onTabMoved);
    tabBar()->installEventFilter(this);
    QCoreApplication::instance()->installEventFilter(this);

    m_syncController = new FileSyncController(this);

    connect(
        m_syncController,
        &FileSyncController::reloadRequested,
        this,
        [this](const QString& filePath) {
            FileTab* tab = findTabByPath(filePath);
            if (tab) {
                reloadTabFromDisk(tab);
            }
        }
    );

    connect(
        m_syncController,
        &FileSyncController::keepCurrentRequested,
        this,
        [this](const QString& filePath) {
            FileTab* tab = findTabByPath(filePath);
            if (!tab || !tab->toolsTabWidget()) {
                return;
            }

            auto* buffer = tab->toolsTabWidget()->sharedBuffer();
            if (m_externalFileDialogActive || !buffer) {
                return;
            }

            m_externalFileDialogActive = true;
            QMessageBox question(
                QMessageBox::Question,
                tr("File changed on disk"),
                tr("\"%1\" has been changed on disk.\n"
                   "Do you want to reload it and lose your unsaved changes?")
                    .arg(QFileInfo(tab->filePath).fileName()),
                QMessageBox::NoButton,
                this
            );

            const auto reloadBtn = question.addButton(tr("Reload"), QMessageBox::YesRole);
            question.addButton(tr("Keep my changes"), QMessageBox::NoRole);
            question.exec();
            m_externalFileDialogActive = false;

            if (question.clickedButton() == reloadBtn) {
                reloadTabFromDisk(tab);
            }
        }
    );

    connect(
        m_syncController,
        &FileSyncController::closeRequested,
        this,
        [this](const QString& filePath) {
            FileTab* tab = findTabByPath(filePath);
            if (!tab || !tab->toolsTabWidget()) {
                return;
            }

            if (m_externalFileDialogActive) {
                return;
            }

            m_externalFileDialogActive = true;
            QMessageBox question(
                QMessageBox::Warning,
                tr("File deleted on disk"),
                tr("\"%1\" was deleted or renamed outside the editor.")
                    .arg(QFileInfo(tab->filePath).fileName()),
                QMessageBox::NoButton,
                this
            );

            const auto keepOpenBtn = question.addButton(tr("Keep open"), QMessageBox::YesRole);
            const auto closeTabBtn = question.addButton(tr("Close tab"), QMessageBox::DestructiveRole);
            question.exec();
            m_externalFileDialogActive = false;

            const auto reply = question.clickedButton();
            if (reply == closeTabBtn) {
                closeTab(indexOf(tab));
            } else if (reply == keepOpenBtn) {
                tab->toolsTabWidget()->setupStar();
            }
        }
    );

    connect(
        m_syncController,
        &FileSyncController::markDirtyRequested,
        this,
        [this](const QString& filePath) {
            FileTab* tab = findTabByPath(filePath);
            if (tab && tab->toolsTabWidget()) {
                tab->toolsTabWidget()->setupStar();
            }
        }
    );

    connect(this, &FilesTabWidget::openTabModule,
            this, [this](ModuleDescription<TabBase> desc){
                if (auto* tab = qobject_cast<FileTab*>(currentWidget()))
                    emit tab->openTabModule(desc);
            });
}

FilesTabWidget::~FilesTabWidget() {
    QCoreApplication::instance()->removeEventFilter(this);
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
    emit searchDocumentsChanged();
}

void FilesTabWidget::createBuildTab(const ProjectInfo &projInfo){
    BuildTab *buildTab = new BuildTab(projInfo, this);
    int new_tab_index = this->addTab(buildTab, QString("Build '%1'").arg(projInfo.name));
    this->setCurrentIndex(new_tab_index);
}

// Create new tab and open file if he is not open already
void FilesTabWidget::openFile(QString filePath, QString tabTitle) {

    // check already open
    for (int i = 0; i < this->count(); ++i) {
        FileTab *t = qobject_cast<FileTab *>(this->widget(i));
        if (t && samePath(t->filePath, filePath)) {
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
    if (filetab->toolsTabWidget() && filetab->toolsTabWidget()->sharedBuffer()) {
        connect(filetab->toolsTabWidget()->sharedBuffer(), &FileDataBuffer::dataChanged,
                this, &FilesTabWidget::searchDocumentsChanged);
    }

    connect(this, &FilesTabWidget::setWordWrapSignal, filetab, &FileTab::setWordWrapSlot);
    connect(this, &FilesTabWidget::setTabReplaceSignal, filetab, &FileTab::setTabReplaceSlot);
    connect(this, &FilesTabWidget::setTabWidthSignal, filetab, &FileTab::setTabWidthSlot);

    startFileSync(filetab);

}

/* - - Sync with disk (external changes) - - */

/* Watch the freshly opened file so external modifications can be detected */
void FilesTabWidget::startFileSync(FileTab *tab) {
    if (!tab || tab->filePath.isEmpty()) {
        return;
    }

    auto* tools = tab->toolsTabWidget();
    if (!tools || !tools->sharedBuffer()) {
        return;
    }

    const QString normalizedPath = comparablePath(tab->filePath);
    m_syncController->attachFile(normalizedPath, tools->sharedBuffer()->isModified());

    connect(
        tools->sharedBuffer(),
        &FileDataBuffer::savedToFile,
        m_syncController,
        &FileSyncController::refreshBaseline
    );

    connect(
        tools->sharedBuffer(),
        &FileDataBuffer::dataChanged,
        m_syncController,
        [this, normalizedPath, buffer = tools->sharedBuffer()]() {
            if (m_syncController && buffer) {
                m_syncController->updateFileState(normalizedPath, buffer->isModified());
            }
        }
    );
}

void FilesTabWidget::stopFileSync(const QString& filePath) {
    if (!m_syncController)
        return;

    m_syncController->detachFile(filePath);
}

FileTab* FilesTabWidget::findTabByPath(const QString& filePath) const {
    for (int i = 0; i < count(); ++i) {
        FileTab *tab = qobject_cast<FileTab *>(widget(i));
        if (tab && samePath(tab->filePath, filePath)) {
            return tab;
        }
    }
    return nullptr;
}

void FilesTabWidget::reloadTabFromDisk(FileTab *tab) {
    if (!tab || !tab->toolsTabWidget()) {
        return;
    }

    if (!tab->toolsTabWidget()->reloadFromDisk()) {
        /* The file disappeared while syncing: fall back to deletion flow */
        onFileDisappeared(tab->filePath);
        return;
    }

    emit searchDocumentsChanged();
}

void FilesTabWidget::onFileChangedOnDisk(const QString& filePath) {
    if (m_syncController) {
        m_syncController->handleFileChanged(filePath);
    }
}

void FilesTabWidget::onFileDisappeared(const QString& filePath) {
    if (m_syncController) {
        m_syncController->handleFileDisappeared(filePath);
    }
}

QVector<SearchDocument> FilesTabWidget::searchDocuments(SearchScope scope) const
{
    QVector<SearchDocument> documents;
    if (scope == SearchScope::CurrentFile) {
        auto* tab = qobject_cast<FileTab*>(currentWidget());
        if (tab && tab->toolsTabWidget() && tab->toolsTabWidget()->sharedBuffer())
            documents.append({tab->filePath, tab->toolsTabWidget()->sharedBuffer()->data()});
        return documents;
    }

    documents.reserve(count());
    for (int index = 0; index < count(); ++index) {
        auto* tab = qobject_cast<FileTab*>(widget(index));
        if (!tab || !tab->toolsTabWidget() || !tab->toolsTabWidget()->sharedBuffer())
            continue;
        documents.append({tab->filePath, tab->toolsTabWidget()->sharedBuffer()->data()});
    }
    return documents;
}

QByteArray FilesTabWidget::documentContents(const QString& filePath, bool* found) const
{
    if (found)
        *found = false;
    for (int index = 0; index < count(); ++index) {
        auto* tab = qobject_cast<FileTab*>(widget(index));
        if (!tab || !samePath(tab->filePath, filePath) || !tab->toolsTabWidget()->sharedBuffer())
            continue;
        if (found)
            *found = true;
        return tab->toolsTabWidget()->sharedBuffer()->data();
    }
    return {};
}

bool FilesTabWidget::replaceOpenDocument(const QString& filePath, const QByteArray& contents)
{
    for (int index = 0; index < count(); ++index) {
        auto* tab = qobject_cast<FileTab*>(widget(index));
        if (!tab || !samePath(tab->filePath, filePath) || !tab->toolsTabWidget()->sharedBuffer())
            continue;
        tab->toolsTabWidget()->sharedBuffer()->replaceData(contents);
        return true;
    }
    return false;
}

bool FilesTabWidget::openSearchMatch(const SearchMatch& match)
{
    openFile(match.filePath, QFileInfo(match.filePath).fileName());
    auto* tab = qobject_cast<FileTab*>(currentWidget());
    if (!tab || !tab->toolsTabWidget())
        return false;
    CodeEditorTab* editor = tab->toolsTabWidget()->codeEditorTab(true);
    return editor && editor->revealSearchMatch(match.line, match.column, match.length);
}

QString FilesTabWidget::selectedSearchText() const
{
    auto* tab = qobject_cast<FileTab*>(currentWidget());
    if (!tab || !tab->toolsTabWidget())
        return {};
    CodeEditorTab* editor = tab->toolsTabWidget()->codeEditorTab(false);
    return editor ? editor->selectedSearchText() : QString();
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
        if (currentFileTab)
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

    case QEvent::MouseMove: {
        if (obj == tabBar() && count() <= 1) {
            return true;
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
        QMessageBox question_save_file(QMessageBox::Question, tr("Save File"), tr("Do you want to save this file?"),QMessageBox::NoButton, this);
        const auto yes = question_save_file.addButton(tr("Yes") ,QMessageBox::YesRole);
        const auto no = question_save_file.addButton(tr("No"), QMessageBox::NoRole);
        const auto cancel = question_save_file.addButton(tr("Cancel"), QMessageBox::RejectRole);

        question_save_file.exec();
        const auto reply = question_save_file.clickedButton();
        if (reply == yes) tab->saveFile();
        else if (reply == cancel || reply  == nullptr) return;
    }

    removeTab(index);
    if (tab) {
        stopFileSync(comparablePath(tab->filePath));
        tab->deleteLater();
    }
    if (count() == 0)
        emit statusBarInfoChanged(QString());
    emit searchDocumentsChanged();
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
