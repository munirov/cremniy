#ifndef ASCIICHARSREF_H
#define ASCIICHARSREF_H

#include "ui/MenuBar/Menus/References/referencewindow.h"
#include <QWidget>

class AsciiCharsRef : public ReferenceWindow
{
    Q_OBJECT
public:
    explicit AsciiCharsRef();
    QString RefWinName() override { return "ASCII / Unicode Characters"; }

private:
    void initWindow() override;
    void initWidgets() override;

signals:
};

#endif // ASCIICHARSREF_H
