#ifndef STM32PINOUT_H
#define STM32PINOUT_H
#include "core/modules/ModuleManager.h"
#include "core/file/filecontext.h"
#include "core/modules/TabBase.h"
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
#include <QIcon>
#include <thread>
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
    QSet<QString> m_modifiedPins;
    QList<PinInfo> m_pins;
    QString m_mcuName;
    QString m_selectedPin;
    QList<DrawnPin> m_drawnPins;
    qreal m_scale = 1.0;
    QPointF m_offset;
    QPoint m_lastMousePos;
    bool m_dragging = false;
    QColor colorForSignal(const QString& signal) const;
};
// STM32PinoutTab
class STM32PinoutTab : public TabBase
{
    Q_OBJECT
public:
    explicit STM32PinoutTab(QWidget* parent = nullptr);
    ~STM32PinoutTab() override = default;
    QIcon icon() const override {
        return QIcon(":/icons/chip.png");
    }
protected slots:
    void onSelectionChanged(qint64, qint64) override {}
    void onDataChanged() override;
public slots:
    void setFile(QString filepath) override;
    void setTabData() override;
    void saveTabData() override;
    void setWordWrapSlot(bool checked) override { Q_UNUSED(checked); }
    void setTabReplaceSlot(bool checked) override { Q_UNUSED(checked); }
    void setTabWidthSlot(int width) override { Q_UNUSED(width); }
private slots:
    void onPinClickedInTree(QTreeWidgetItem* item, int column);
    void onPinClickedInChip(const QString& pinName);
    void onApplyClicked();
private:
    // layout

    QSet<QString> m_modifiedPins;
    QSplitter* m_splitter = nullptr;
    QTabWidget* m_tabs = nullptr;
    QTreeWidget* m_pinoutTree = nullptr;
    ChipView* m_chipView = nullptr;
    QLabel* m_microcontrollerInfo = nullptr;
    // edit panel
    QGroupBox* m_editGroup = nullptr;
    QLabel* m_editPinName = nullptr;
    QComboBox* m_editSignal = nullptr;
    QLineEdit* m_editLabel = nullptr;
    QPushButton* m_applyBtn = nullptr;
    // Data
    QMap<QString, QMap<QString, QString>> m_pinData;
    QString m_mcuName;
    QString m_mcuPackage;
    QString m_selectedPin;
    static QStringList getAllPinsForPackage(const QString& mcuName, const QString& package);
    
    void parseIocData(const QByteArray& raw);
    
    QByteArray buildIocBytes() const;
    void rebuildViews();
    void selectPin(const QString& pinName);
};
#endif