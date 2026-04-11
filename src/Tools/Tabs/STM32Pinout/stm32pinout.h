#ifndef STM32PINOUT_H
#define STM32PINOUT_H
#include "ui/ToolsTabWidget/ToolTab.h"
// #include "utils/filecontext.h"
#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QTabWidget>
#include <QSplitter>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QRegularExpression>
#include <QtMath>
#include <QSet>
#include <QComboBox>

// ChipView
class ChipView : public QWidget
{
    Q_OBJECT
public:
    struct PinInfo {
        QString name;
        QString signal;
        QString label;
    };

    explicit ChipView(QWidget* parent = nullptr);
    void setPins(const QList<PinInfo>& pins, const QString& mcuName);
    void setSelectedPin(const QString& pinName);
    void setModifiedPins(const QSet<QString>& pins);

signals:
    void pinClicked(const QString& pinName);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    
    struct DrawnPin { QRectF rect; PinInfo info; };
    QSet<QString>   m_modifiedPins;
    QList<PinInfo>  m_pins;
    QString         m_mcuName;
    QString         m_selectedPin;
    QList<DrawnPin> m_drawnPins;
    qreal           m_scale = 1.0;
    QPointF         m_offset;
    QPoint          m_lastMousePos;
    bool            m_dragging = false;

    QColor colorForSignal(const QString& signal) const;
};

// STM32PinoutTab
class STM32PinoutTab : public ToolTab
{
    Q_OBJECT
private:
    // Layout
    // QWidget*     m_mainWidget;
    QSet<QString> m_modifiedPins;
    QSplitter*   m_splitter;
    QTabWidget*  m_tabs;
    QTreeWidget* m_pinoutTree;
    ChipView*    m_chipView;
    QLabel*      m_microcontrollerInfo;

    // Edit panel
    QGroupBox*   m_editGroup;
    QLabel*      m_editPinName;
    QComboBox* m_editSignal;
    QLineEdit*   m_editLabel;
    QPushButton* m_applyBtn;
    QPushButton* m_saveBtn;

    // Data
    FileContext* m_fileContext = nullptr;
    QString      m_currentFile;
    QMap<QString, QMap<QString, QString>> m_pinData;
    QString      m_mcuName;
    QString      m_mcuPackage;
    QString      m_selectedPin;
    QStringList getAllPinsForPackage(const QString& mcuName, const QString& package) const;
    void parseIocFile(const QString& filepath);
    void rebuildViews();
    void selectPin(const QString& pinName);

public:
    explicit STM32PinoutTab(FileDataBuffer* buffer, QWidget* parent = nullptr);

    QString toolName() const override { return "STM32 Pinout"; }
    QIcon   toolIcon() const override { return QIcon(":/icons/chip.png"); }

protected slots:
    void onSelectionChanged(qint64, qint64) override {}
    void onDataChanged() override;

public slots:
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;
    void setWordWrapSlot(bool checked) override {}
    void setTabReplaceSlot(bool checked) override {}
    void setTabWidthSlot(int width) override {}

private slots:
    void onPinClickedInTree(QTreeWidgetItem* item, int column);
    void onPinClickedInChip(const QString& pinName);
    void onApplyClicked();
    void onSaveClicked();
};

#endif 