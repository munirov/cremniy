#ifndef FILECREATEDIALOG_H
#define FILECREATEDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>

class FileCreateDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileCreateDialog(QWidget *parent = nullptr, const QString& path = {}, bool is_dir = false);
    QString dir_path;
private:
    bool is_dir = false;
private slots:
    void onCreateClicked();

private:
    QLineEdit *lineEdit;
};

#endif // FILECREATEDIALOG_H
