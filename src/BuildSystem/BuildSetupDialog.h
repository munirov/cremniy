#pragma once
#include "BuildConfig.h"
#include <QDialog>

class QLineEdit;
class QLabel;

class BuildSetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit BuildSetupDialog(const BuildConfig& initial, QWidget* parent = nullptr);

    BuildConfig result() const;

private:
    QLineEdit* m_qtPathEdit;
    QLineEdit* m_buildEdit;
    QLineEdit* m_runEdit;
    QLineEdit* m_cleanEdit;
};
