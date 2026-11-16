#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QVector>

class QStackedWidget;
class QTreeWidget;
class QLabel;
class SettingsPage;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onExportIni();
    void onImportIni();
    void onAccept();
    void showSelectedPage();

private:
    void loadPages();

    QTreeWidget* m_categories = nullptr;
    QStackedWidget* m_pagesWidget = nullptr;
    QLabel* m_pageTitle = nullptr;
    QVector<SettingsPage*> m_pages;
};

#endif // SETTINGSDIALOG_H
