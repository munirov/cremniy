#ifndef ASCIICHARSREF_H
#define ASCIICHARSREF_H

#include "ui/MenuBar/Menus/References/referencewindow.h"
#include <QWidget>

class AsciiCharsRef : public ReferenceBase
{
    Q_OBJECT
public:
    explicit AsciiCharsRef();

private:
    void initWindow() override;
    void initWidgets() override;

signals:
};

#endif // ASCIICHARSREF_H
