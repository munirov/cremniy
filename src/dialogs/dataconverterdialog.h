    #ifndef DATACONVERTERDIALOG_H
#define DATACONVERTERDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QFormLayout;

class DataConverterDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit DataConverterDialog(QWidget* parent = nullptr);

private slots:
    void onInputChanged();
    void onSourceUnitChanged();

private:
    void updateOutputs(double bytes);
    void copyRow(int rowIndex);
    void copyAll();

    static double toBytes(double value, int unitIndex);
    static double fromBytes(double bytes, int unitIndex);
    static QString formatValue(double value);

    static bool parseValue(const QString& text, qulonglong* outValue);
    static bool parseExpression(const QString& text, qulonglong* outValue, QString* errorOut);
    static bool looksLikeExpression(const QString& text);

    QLineEdit* m_input = nullptr;
    QComboBox* m_sourceUnit = nullptr;
    QFormLayout* m_form = nullptr;
    QLabel* m_status = nullptr;

    // one label + copy button per unit (must stay in sync with kUnitCount in .cpp)
    QLabel* m_labels[10] = {};
    QPushButton* m_copies[10] = {};
};

#endif // DATACONVERTERDIALOG_H