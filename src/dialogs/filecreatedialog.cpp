#include "filecreatedialog.h"

#include "QIODevice"
#include "QFile"
#include <qdir.h>

FileCreateDialog::FileCreateDialog(QWidget *parent, QString path, bool _is_dir): QDialog(parent) {

    this->dir_path = path;
    this->is_dir = _is_dir;

    lineEdit = new QLineEdit(this);

    if (is_dir) {
        setWindowTitle(tr("Create folder"));
        lineEdit->setPlaceholderText(tr("Enter folder name..."));
    }
    else {
        setWindowTitle("Create file");
        lineEdit->setPlaceholderText(tr("Enter file name..."));
    }

    setFixedSize(300, 100); // маленькое окно

    QVBoxLayout *layout = new QVBoxLayout(this);

    // текстовое поле
    layout->addWidget(lineEdit);

    // кнопка
    QPushButton *button = new QPushButton(tr("Create"), this);
    layout->addWidget(button);

    connect(button, &QPushButton::clicked, this, &FileCreateDialog::onCreateClicked);
}

void FileCreateDialog::onCreateClicked() {
    QString fileName = lineEdit->text();
    if(fileName.isEmpty()) {
        if (is_dir) QMessageBox::warning(this, tr("Error"), tr("Enter folder name!"));
        else QMessageBox::warning(this, tr("Error"), tr("Enter file name!"));
        return;
    }

    // тут можно создать файл
    QString fullPath = QString("%1/%2").arg(dir_path).arg(fileName);

    if (is_dir) {
        QDir dir;
        if (!dir.mkpath(fullPath)) {
            QMessageBox::critical(this, tr("Error"), tr("Failed to create directory!"));
        }
    }
    else {
        QFile file(fullPath);
        if(file.open(QIODevice::WriteOnly)) {
            file.close();
            accept(); // закрыть диалог
        } else {
            QMessageBox::critical(this, tr("Error"), tr("Failed to create file!"));
        }
    }

    this->destroy();
}