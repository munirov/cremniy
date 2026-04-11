#ifndef FILESTABWIDGET_H
#define FILESTABWIDGET_H

#include <QTabWidget>
#include "widgets/filetab.h"

class FilesTabWidget : public QTabWidget {
    Q_OBJECT
public:
    FilesTabWidget(QWidget *parent = nullptr);

    void tabSelect(int index);
    void openFile(QString fullPath, QString fileName);

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
  
private:
    void switchTab(int page);
    void setPinnedTabText(int index, FileTab *tab);
    int pinnedCount() const;
    bool m_adjustingTabMove = false;

signals:
    void setWordWrapSignal(bool checked);
    void setTabReplaceSignal(bool checked);
    void setTabWidthSignal(int width);
};

#endif // FILESTABWIDGET_H
