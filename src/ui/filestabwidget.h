#ifndef FILESTABWIDGET_H
#define FILESTABWIDGET_H

#include <QTabWidget>
#include <filetab.h>

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
  void saveFileSlot();
  void closeTab(int index);
  
private:
  void switchTab(int page);
};

#endif // FILESTABWIDGET_H
