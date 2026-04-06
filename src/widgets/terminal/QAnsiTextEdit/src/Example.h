#pragma once

#include "QAnsiTextEdit.h"
#include <QWidget>

class Example : public QAnsiTextEdit {

    Q_OBJECT

    public:
        explicit Example(QWidget* parent = 0);
        ~Example();

    private:

};

