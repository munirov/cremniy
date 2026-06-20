#ifndef WELCOMEFORM_H
#define WELCOMEFORM_H

#include <QWidget>
#include <qcombobox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlistview.h>
#include <qstackedwidget.h>

class QPushButton;

class WelcomeForm : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeForm(QWidget *parent = nullptr);
    ~WelcomeForm();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QStackedWidget *stack;
    QPushButton *open_recent_proj_btn;
    QPushButton *remove_recent_proj_btn;
    QLabel *proj_name_label;
    QLineEdit *proj_name_lineEdit;
    QComboBox *language_comboBox;
    QLabel *language_label;
    QLabel *info_label;
    QLabel *path_label;
    QLineEdit *path_lineEdit;
    QListView *RecentProjectsList;
    void L2CreateProject(QString name, QString path, QString language);
    void SetProjectHistoryList();
    void OpenProject(QString path);

private slots:
    void SelectProjectInList();
    void OpenRecentProjectHandler();
    void RemoveRecentProjectHandler();
    void OpenProjectHandler();
    void CreateProjectHandler();
    void L2BackButton();
    void L2CreateButton();

};

#endif // WELCOMEFORM_H
