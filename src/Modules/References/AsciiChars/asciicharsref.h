#ifndef ASCIICHARSREF_H
#define ASCIICHARSREF_H

#include "core/modules/ReferenceBase.h"

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
