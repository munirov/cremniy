#ifndef FILESTABWIDGET_H
#define FILESTABWIDGET_H

#include <QTabWidget>
#include <filetab.h>

class FilesTabWidget : public QTabWidget {
  Q_OBJECT
public:
  FilesTabWidget(QWidget *parent = nullptr);

  void tabSelect(int index);
  void openFile(QString fullPath, QString fileName, int lineNumber = -1,
                const QString &searchNeedle = QString());

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
  void switchTab(int page);
  void setPinnedTabText(int index, FileTab *tab);
  int pinnedCount() const;
  bool m_adjustingTabMove = false;
};

#endif // FILESTABWIDGET_H
