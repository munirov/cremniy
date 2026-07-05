#ifndef KEYBOARDSCANCODESREF_H
#define KEYBOARDSCANCODESREF_H

#include "core/modules/ReferenceBase.h"

class QKeyEvent;
class KeyboardScanCodeVizWidget;
class QLabel;
class QTableWidget;

class KeyboardScancodesRef final : public ReferenceBase
{
    Q_OBJECT
public:
    explicit KeyboardScancodesRef(QWidget *parent = nullptr);
    ~KeyboardScancodesRef() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void initWindow() override;
    void initWidgets() override;
    void fillReferenceTable();
    void onKeyPress(QKeyEvent *event);
    void onKeyRelease(QKeyEvent *event);

    QLabel *m_keyNameValue = nullptr;
    QLabel *m_qtKeyValue = nullptr;
    QLabel *m_scanValue = nullptr;
    QLabel *m_vkValue = nullptr;
    QLabel *m_textValue = nullptr;
    QLabel *m_modsValue = nullptr;
    QTableWidget *m_table = nullptr;
    KeyboardScanCodeVizWidget *m_viz = nullptr;
};

#endif // KEYBOARDSCANCODESREF_H
