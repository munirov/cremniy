#ifndef FILESTABWIDGET_H
#define FILESTABWIDGET_H

#include <QTabWidget>
#include "core/modules/ModuleManager.h"
#include "core/FindEngine/findtypes.h"
#include "project_info_manager.h"
#include "widgets/filetab.h"

class FilesTabWidget : public QTabWidget {
    Q_OBJECT
public:
    FilesTabWidget(QWidget *parent = nullptr);
    ~FilesTabWidget() override;

    void tabSelect(int index);
    void createBuildTab(const ProjectInfo &projInfo);
    void openFile(QString fullPath, QString fileName);
    QVector<SearchDocument> searchDocuments(SearchScope scope) const;
    QByteArray documentContents(const QString& filePath, bool* found) const;
    bool replaceOpenDocument(const QString& filePath, const QByteArray& contents);
    bool openSearchMatch(const SearchMatch& match);
    QString selectedSearchText() const;
    bool gitBlameEnabled() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void removeStar(FileTab *tab);
    void setupStar(FileTab *tab);
    void updatePinnedState(FileTab *tab);
    void saveFileSlot();
    void closeTab(int index);
    void onTabMoved(int from, int to);

    void setWordWrapSlot(bool checked);
    void setTabReplaceSlot(bool checked);
    void setTabWidthSlot(int width);
    void setGitBlameSlot(bool checked);
  
private:
    void switchTab(int page);
    void setPinnedTabText(int index, FileTab *tab);
    int pinnedCount() const;
    bool m_adjustingTabMove = false;

signals:
    void searchDocumentsChanged();
    void setWordWrapSignal(bool checked);
    void setTabReplaceSignal(bool checked);
    void setTabWidthSignal(int width);
    void setGitBlameSignal(bool checked);
    void gitBlameEnabledChanged(bool enabled);
    void statusBarInfoChanged(const QString& info);
    void openTabModule(ModuleDescription<TabBase> desc);
};

#endif // FILESTABWIDGET_H
