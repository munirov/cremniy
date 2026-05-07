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
        /* Populate widget text from FileDialog */
        QString dir = QFileDialog::getExistingDirectory(
            this,
            "Choose Directory",
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
