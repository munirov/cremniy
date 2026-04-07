#ifndef KEYBOARDSCANCODESDIALOG_H
#define KEYBOARDSCANCODESDIALOG_H

#include <QDialog>
#include <QFrame>

class QKeyEvent;
class QShowEvent;
class QLabel;
class QTableWidget;
class QToolButton;
class KeyboardScanCodeVizWidget;

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

class KeyboardScanCodesDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit KeyboardScanCodesDialog(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
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

#endif
