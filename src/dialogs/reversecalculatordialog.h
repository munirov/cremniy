// SPDX-License-Identifier: MIT
#ifndef REVERSECALCULATORDIALOG_H
#define REVERSECALCULATORDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QLabel;
class QCheckBox;
class QPushButton;
class QFormLayout;

class ReverseCalculatorDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit ReverseCalculatorDialog(QWidget *parent = nullptr);

private slots:
    void onInputChanged();
    void onSwapEndian();
    void onCopyHex();
    void onCopyDec();
    void onCopyBin();

private:
    void updateOutputs(qulonglong value, bool ok);
    static bool parseValue(const QString &text, qulonglong *outValue);
    static qulonglong maskToWidth(qulonglong v, int bits);
    static qlonglong toSigned(qulonglong v, int bits);
    static qulonglong swapEndian(qulonglong v, int bits);

    QLineEdit  *m_input = nullptr;
    QComboBox  *m_width = nullptr;
    QCheckBox  *m_showSigned = nullptr;

    QFormLayout *m_form = nullptr;

    QLabel     *m_status = nullptr;
    QLabel     *m_hex = nullptr;
    QLabel     *m_decU = nullptr;
    QLabel     *m_decS = nullptr;
    QLabel     *m_bin = nullptr;

    QPushButton *m_swapBtn = nullptr;
    QPushButton *m_copyHex = nullptr;
    QPushButton *m_copyDec = nullptr;
    QPushButton *m_copyBin = nullptr;
};

#endif // REVERSECALCULATORDIALOG_H

