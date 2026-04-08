#ifndef CLICKABLELINEEDIT_H
#define CLICKABLELINEEDIT_H

#include <QLineEdit>
#include <qfiledialog.h>

class ClickableLineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit ClickableLineEdit();
    using QLineEdit::QLineEdit;

protected:
    void mousePressEvent(QMouseEvent *event) override {
        // Здесь будем вызывать FIleDialog и полученное значение из диалога подставлять в Text данного виждета
        QString dir = QFileDialog::getExistingDirectory(
            this,
            tr("Choose Directory"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
            );

        if (!dir.isEmpty()) {
            this->setText(dir);
        }

        QLineEdit::mousePressEvent(event);
    }
};

#endif // CLICKABLELINEEDIT_H
