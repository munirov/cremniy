#ifndef KEYBOARDSCANCODESREF_H
#define KEYBOARDSCANCODESREF_H

#include "core/modules/ReferenceBase.h"
#include <QFrame>

class QKeyEvent;

class KeyboardScanCodeVizWidget;
class QLabel;
class QTableWidget;
class QToolButton;

class KeyCaptureFrame final : public QFrame
{
    Q_OBJECT
public:
    explicit KeyCaptureFrame(QWidget *parent = nullptr);

signals:
    void keyActivity(int qtKey, quint32 nativeScan, quint32 nativeVirtualKey, const QString &text,
                     Qt::KeyboardModifiers mods, bool isRelease);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
};

class QShowEvent;

class KeyboardScancodesRef final : public ReferenceBase
{
    Q_OBJECT
public:
    explicit KeyboardScancodesRef(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void initWindow() override;
    void initWidgets() override;
    void fillReferenceTable();
    void onKeyActivity(int qtKey, quint32 nativeScan, quint32 nativeVirtualKey, const QString &text,
                       Qt::KeyboardModifiers mods, bool isRelease);

    KeyCaptureFrame *m_capture = nullptr;
    QToolButton *m_helpToggle = nullptr;
    QWidget *m_helpContent = nullptr;
    QLabel *m_status = nullptr;
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
