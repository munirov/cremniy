#ifndef FILESTABWIDGET_H
#define FILESTABWIDGET_H

#include "core/ToolStatusState.h"
#include <QTabWidget>
#include <filetab.h>

class FilesTabWidget : public QTabWidget {
  Q_OBJECT
public:
  FilesTabWidget(QWidget *parent = nullptr);

  ToolStatusState currentStatusState() const;
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
  
private:
  void onFileTabActiveStatusStateChanged(const ToolStatusState &state);
  void switchTab(int page);
  void setPinnedTabText(int index, FileTab *tab);
  int pinnedCount() const;
  bool m_adjustingTabMove = false;

signals:
  void activeStatusStateChanged(const ToolStatusState &state);
};

#endif // FILESTABWIDGET_H
