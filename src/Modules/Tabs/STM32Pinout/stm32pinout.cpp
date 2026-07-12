#include "stm32pinout.h"
#include "core/modules/ModuleManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidgetItem>
#include <QTextStream>
#include <QMessageBox>

static QString normalizeMcuName(const QString& raw)
{
    QString name = raw;
    static QRegularExpression suffixRx("[A-Z]x$");
    name.remove(suffixRx);
    return name;
}

ChipView::ChipView(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(300, 300);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void ChipView::setPins(const QList<PinInfo>& pins, const QString& mcuName)
{
    m_pins = pins; m_mcuName = mcuName;
    m_scale = 1.0; m_offset = {};
    update();
}

void ChipView::setSelectedPin(const QString& pinName)
{
    m_selectedPin = pinName;
    update();
}

QColor ChipView::colorForSignal(const QString& signal) const
{
    if (signal.isEmpty() || signal == "-") return QColor(180,180,180);
    QString s = signal.toUpper();

    // colors stuff
    if (s.contains("USART")||s.contains("UART")) return QColor(255,165,  0);
    if (s.contains("SPI"))
        return QColor(180, 90,210);
    if (s.contains("I2C"))
        return QColor(0, 200, 200);
    if (s.contains("ADC"))
        return QColor(220, 60, 60);
    if (s.contains("TIM")||s.contains("PWM"))
        return QColor(200,200,  0);
    if (s.contains("USB"))
        return QColor(0,120,255);
    if (s.contains("CAN"))
        return QColor(255, 100, 100);
    if (s.contains("GPIO_OUTPUT"))
        return QColor(60, 180, 60);
    if (s.contains("GPIO_INPUT"))
        return QColor( 60,130,220);
    if (s.contains("GPIO"))
        return QColor(100, 180, 100);
    return QColor(100, 180, 255);
}

void ChipView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(30,30,30));

    if (m_pins.isEmpty()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "No pins loaded");
        return;
    }

    m_drawnPins.clear();

    int total = m_pins.size();
    int perSide = qMax(1, (total + 3) / 4);
    qreal pinLen = 30 * m_scale;
    qreal spacing = 22 * m_scale;
    qreal chipW = qMax(perSide * spacing, 120 * m_scale);
    qreal chipH = qMax(perSide * spacing, 120 * m_scale);
    qreal textW = 70 * m_scale;

    QPointF center = QPointF(width()/2.0, height()/2.0) + m_offset;
    QRectF chip(center.x()-chipW/2, center.y()-chipH/2, chipW, chipH);

    p.setBrush(QColor(45,45,55));
    p.setPen(QPen(QColor(100,100,120), 2));
    p.drawRect(chip);

    p.setBrush(QColor(200,200,200));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(chip.left()+10*m_scale, chip.top()+10*m_scale),
    4*m_scale, 4*m_scale);

    p.setFont(QFont("Arial", qMax(7.0, 9.0*m_scale), QFont::Bold));
    p.setPen(QColor(200,200,200));
    p.drawText(chip, Qt::AlignCenter, m_mcuName);

    p.setFont(QFont("Consolas", qMax(5.0, 7.0*m_scale)));

    for (int i = 0; i < total; ++i) {
        const PinInfo& pin = m_pins[i];
        int side = i / perSide;
        qreal t = ((i % perSide) + 0.5) / perSide;
        QColor col = colorForSignal(pin.signal);

        QPointF pinStart, pinEnd, labelPos;
        bool horiz = (side == 0 || side == 2);

        if (side == 0) {
            qreal y=chip.top()+t*chipH;
            pinStart={chip.left(),y}; pinEnd={chip.left()-pinLen,y};
            labelPos={chip.left()-pinLen-textW-4*m_scale,y};
        } else if (side == 1) {
            qreal x=chip.left()+t*chipW;
            pinStart={x,chip.bottom()}; pinEnd={x,chip.bottom()+pinLen};
            labelPos={x,chip.bottom()+pinLen+4*m_scale};
        } else if (side == 2) {
            qreal y=chip.bottom()-t*chipH;
            pinStart={chip.right(),y}; pinEnd={chip.right()+pinLen,y};
            labelPos={chip.right()+pinLen+4*m_scale,y};
        } else {
            qreal x=chip.right()-t*chipW;
            pinStart={x,chip.top()}; pinEnd={x,chip.top()-pinLen};
            labelPos={x,chip.top()-pinLen-4*m_scale};
        }

        bool selected = (pin.name == m_selectedPin);
        bool modified = m_modifiedPins.contains(pin.name);

        QColor drawCol = modified ? QColor(255, 140, 0) : col;

        p.setPen(QPen(selected ? Qt::white : drawCol.lighter(130),
        selected ? 2.5*m_scale : 1.5*m_scale));
        p.drawLine(pinStart, pinEnd);

        QRectF pinRect(pinEnd.x()-4*m_scale, pinEnd.y()-4*m_scale, 8*m_scale, 8*m_scale);
        p.setBrush(selected ? Qt::white : drawCol);
        p.setPen(Qt::NoPen);
        p.drawRect(pinRect);

        QString txt = pin.label.isEmpty() ? pin.name : pin.label;
        p.setPen(selected ? Qt::white : drawCol.lighter(160));

        if (horiz) {
            Qt::Alignment align = (side==0) ? Qt::AlignRight : Qt::AlignLeft;
            p.drawText(QRectF(labelPos.x(), labelPos.y()-8*m_scale, textW, 16*m_scale),
            align|Qt::AlignVCenter, txt);
        } else {
            p.save();
            p.translate(labelPos); p.rotate(-60);
            p.drawText(QRectF(0,-8*m_scale,textW,16*m_scale),
                    Qt::AlignLeft|Qt::AlignVCenter, txt);
            p.restore();
        }

        m_drawnPins.append({pinRect.adjusted(-4,-4,4,4), pin});
    }

    // Легенда
    struct LegendEntry { QString label; QColor color; };
    QList<LegendEntry> legend = {
        {"GPIO OUT", QColor(60,180,60)}, {"GPIO IN", QColor(60,130,220)},
        {"USART", QColor(255,165,0)}, {"SPI", QColor(180,90,210)},
        {"I2C", QColor(0,200,200)}, {"ADC", QColor(220,60,60)},
        {"TIM/PWM", QColor(200,200,0)}, {"USB", QColor(0,120,255)},
    };
    for (int i = 0; i < legend.size(); ++i) {
        p.fillRect(8, 8 + i*16, 12, 10, legend[i].color);
        p.setPen(Qt::white);
        p.drawText(24, 8 + i*16 + 9, legend[i].label);
    }
}

void ChipView::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) {
        m_offset += QPointF(e->pos() - m_lastMousePos);
        m_lastMousePos = e->pos();
        update();
    }
    for (const DrawnPin& dp : m_drawnPins) {
        if (dp.rect.contains(e->pos())) {
            QToolTip::showText(e->globalPosition().toPoint(),
                QString("<b>%1</b><br>Signal: %2<br>Label: %3")
                    .arg(dp.info.name, dp.info.signal, dp.info.label), this);
            return;
        }
    }
    QToolTip::hideText();
}

void ChipView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        // проверяем клик по пину
        for (const DrawnPin& dp : m_drawnPins) {
            if (dp.rect.contains(e->pos())) {
                emit pinClicked(dp.info.name);
                return;
            }
        }
        m_dragging = true;
        m_lastMousePos = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ChipView::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
    setCursor(Qt::ArrowCursor);
}

void ChipView::wheelEvent(QWheelEvent* e)
{
    m_scale = qBound(0.3, m_scale * (e->angleDelta().y()>0 ? 1.15 : 0.87), 4.0);
    update();
}


//  STM32PinoutTab

static QString displayName() {
    return QCoreApplication::translate("STM32PinoutTab", "STM32 Pinout");
}

static bool registered = []() {
    ModuleManager::instance().registerModule<TabBase>(&displayName, "", []() { return new STM32PinoutTab(); }, 200);
    return true;
}();

STM32PinoutTab::STM32PinoutTab(QWidget* parent)
    : TabBase{parent}
{
    // TabBase::m_dataBuffer объявлен без "= nullptr", поэтому в свежесозданном
    // объекте там мусор из кучи, а не гарантированный nullptr. Если IDE вызовет
    // setFile()/setTabData() до setFileDataBuffer(), проверка "if (m_dataBuffer)"
    // без явного зануления здесь может пройти по мусорному указателю -> segfault
    // внутри FileDataBuffer::data() (см. QMutexLocker на невалидном m_mutex).
    m_dataBuffer = nullptr;

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal);

    // Левая часть: label + tabs
    m_pinoutTree = new QTreeWidget();
    m_pinoutTree->setHeaderLabels({"Pin", "Label", "Signal", "Mode"});
    m_pinoutTree->setColumnCount(4);
    m_pinoutTree->setAlternatingRowColors(true);

    m_chipView = new ChipView();

    m_tabs = new QTabWidget();
    m_tabs->addTab(m_pinoutTree, "Table");
    m_tabs->addTab(m_chipView, "Graphic");

    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(2);

    m_microcontrollerInfo = new QLabel("Open a .ioc file to view pinout configuration");
    m_microcontrollerInfo->setStyleSheet("font-weight: bold; padding: 2px 8px;");
    leftLayout->addWidget(m_microcontrollerInfo);
    leftLayout->addWidget(m_tabs);

    m_splitter->addWidget(leftWidget);

    // правая часть: редактор пина
    m_editGroup = new QGroupBox("Pin Editor");
    m_editGroup->setMinimumWidth(200);
    m_editGroup->setMaximumWidth(280);

    QVBoxLayout* editLayout = new QVBoxLayout(m_editGroup);

    m_editPinName = new QLabel("Select a pin");
    m_editPinName->setStyleSheet("font-weight: bold; font-size: 13px; padding: 4px;");
    editLayout->addWidget(m_editPinName);

    QFormLayout* form = new QFormLayout();
    m_editSignal = new QComboBox();
    m_editSignal->setEditable(true);
    m_editSignal->addItem("");
    m_editSignal->addItems({
        "GPIO_Output", "GPIO_Input",
        "USART1_TX", "USART1_RX", "USART2_TX", "USART2_RX",
        "USART3_TX", "USART3_RX",
        "SPI1_SCK", "SPI1_MISO", "SPI1_MOSI", "SPI1_NSS",
        "SPI2_SCK", "SPI2_MISO", "SPI2_MOSI", "SPI2_NSS",
        "I2C1_SCL", "I2C1_SDA", "I2C2_SCL", "I2C2_SDA",
        "ADC1_IN0", "ADC1_IN1", "ADC1_IN2", "ADC1_IN3",
        "ADC1_IN4", "ADC1_IN5", "ADC1_IN6", "ADC1_IN7",
        "TIM1_CH1", "TIM1_CH2", "TIM2_CH1", "TIM2_CH2",
        "TIM3_CH1", "TIM3_CH2", "TIM4_CH1", "TIM4_CH2",
        "USB_DM", "USB_DP",
        "CAN_RX", "CAN_TX",
        "RCC_OSC_IN","RCC_OSC_OUT"
    });
    form->addRow("Signal:", m_editSignal);
    m_editLabel = new QLineEdit();
    m_editLabel->setPlaceholderText("e.g. DEBUG_TX");
    form->addRow("Label:", m_editLabel);
    editLayout->addLayout(form);

    m_applyBtn = new QPushButton("Apply");
    m_applyBtn->setEnabled(false);
    editLayout->addWidget(m_applyBtn);

    editLayout->addStretch();

    m_splitter->addWidget(m_editGroup);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter);
    setLayout(mainLayout);

    connect(m_pinoutTree, &QTreeWidget::itemClicked, this, &STM32PinoutTab::onPinClickedInTree);
    connect(m_chipView, &ChipView::pinClicked, this, &STM32PinoutTab::onPinClickedInChip);
    connect(m_applyBtn, &QPushButton::clicked, this, &STM32PinoutTab::onApplyClicked);
}


// загрузка
void ChipView::setModifiedPins(const QSet<QString>& pins)
{
    m_modifiedPins = pins;
    update();
}
void STM32PinoutTab::setFile(QString filepath)
{
    delete m_fileContext;
    m_fileContext = new FileContext(filepath);

    if (!filepath.endsWith(".ioc", Qt::CaseInsensitive)) {
        m_microcontrollerInfo->setText("Not a .ioc file.");
        return;
    }

    if (m_dataBuffer)
        setTabData();
}

void STM32PinoutTab::setTabData()
{
    if (!m_dataBuffer) {
        m_microcontrollerInfo->setText("No file loaded");
        return;
    }
    parseIocData(m_dataBuffer->data());
}

void STM32PinoutTab::saveTabData()
{
    if (!m_dataBuffer) return;

    if (!m_dataBuffer->saveToFile()) {
        QMessageBox::critical(this, "Save Error",
            QString("Cannot write to file:\n%1").arg(m_dataBuffer->filePath()));
        return;
    }

    m_modifiedPins.clear();
    setModifyIndicator(false);
    emit dataEqual();
    emit refreshDataAllTabsSignal();
    rebuildViews();
}

void STM32PinoutTab::onDataChanged()
{
    if (!m_dataBuffer) return;

    setTabData();

    const bool modified = m_dataBuffer->isModified();
    setModifyIndicator(modified);
    if (modified)
        emit modifyData();
    else
        emit dataEqual();
}

void STM32PinoutTab::parseIocData(const QByteArray& raw)
{
    m_pinData.clear();

    QTextStream in(raw);
    static QRegularExpression pinRx("^P[A-K]\\d+(-.*)?$");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith('[')) continue;

        int eq = line.indexOf('=');
        if (eq == -1) continue;

        QString key = line.left(eq).trimmed();
        QString value = line.mid(eq+1).trimmed();

        if (key == "Mcu.Name"){
            m_mcuName = value;
            continue;
        }
        if (key == "Mcu.Package") {
            m_mcuPackage = value;
            continue;
        }

        int dot = key.indexOf('.');
        if (dot == -1)
        continue;

        QString pinName = key.left(dot);
        QString param = key.mid(dot+1);
        if (pinRx.match(pinName).hasMatch())
            m_pinData[pinName][param] = value;
    }

    m_microcontrollerInfo->setText(
        QString("MCU: %1 | Package: %2").arg(m_mcuName, m_mcuPackage));

    rebuildViews();
}
void STM32PinoutTab::rebuildViews()
{
    m_pinoutTree->clear();
    QMap<QString, QTreeWidgetItem*> portItems;
    QList<ChipView::PinInfo> viewPins;

    // получаем весь список пинов для данного пакета
    QStringList allPins = getAllPinsForPackage(m_mcuName, m_mcuPackage);

    // добавляем пины из m_pinData которых нет в allPins
    for (const QString& p : m_pinData.keys())
        if (!allPins.contains(p)) allPins << p;

    std::sort(allPins.begin(), allPins.end(), [](const QString& a, const QString& b){
        QString portA = a.left(2), portB = b.left(2);
        if (portA != portB) return portA < portB;
        return a.mid(2).toInt() < b.mid(2).toInt();
    });

    for (const QString& pinName : allPins) {
        QString port = pinName.left(2);
        QString signal = m_pinData.value(pinName).value("Signal", "");
        QString label = m_pinData.value(pinName).value("GPIO_Label", "");
        QString mode = m_pinData.value(pinName).value("GPIO_ModeDefaultOutputPP", "");

        // Таблица
        if (!portItems.contains(port)) {
            QTreeWidgetItem* g = new QTreeWidgetItem(m_pinoutTree);
            g->setText(0, port);
            g->setFont(0, QFont("Arial", 10, QFont::Bold));
            portItems[port] = g;
        }

        QTreeWidgetItem* item = new QTreeWidgetItem(portItems[port]);
        item->setText(0, pinName);
        item->setText(1, label);
        item->setText(2, signal.isEmpty() ? "-" : signal);
        item->setText(3, mode);
        item->setData(0, Qt::UserRole, pinName);

        if (!signal.isEmpty())
            item->setForeground(2, QColor(0, 180, 0));
        else
            item->setForeground(2, QColor(120, 120, 120)); // серый — не используется

        // графика
        viewPins.append({pinName, signal, label});
    }

    m_pinoutTree->expandAll();
    for (int i=0; i<4; ++i)
        m_pinoutTree->resizeColumnToContents(i);

    m_chipView->setPins(viewPins, normalizeMcuName(m_mcuName));
    m_chipView->setModifiedPins(m_modifiedPins);
    m_chipView->setSelectedPin(m_selectedPin);
}
// выбор пина

void STM32PinoutTab::selectPin(const QString& pinName)
{
    m_selectedPin = pinName;
    m_editPinName->setText(pinName);
    m_editSignal->setCurrentText(m_pinData.value(pinName).value("Signal"));
    m_editLabel->setText( m_pinData.value(pinName).value("GPIO_Label"));
    m_applyBtn->setEnabled(true);
    m_chipView->setSelectedPin(pinName);
}

void STM32PinoutTab::onPinClickedInTree(QTreeWidgetItem* item, int)
{
    QString pinName = item->data(0, Qt::UserRole).toString();
    if (!pinName.isEmpty()) selectPin(pinName);
}

void STM32PinoutTab::onPinClickedInChip(const QString& pinName)
{
    selectPin(pinName);
    m_tabs->setCurrentIndex(0);
}

// применяем изменения

void STM32PinoutTab::onApplyClicked()
{
    if (m_selectedPin.isEmpty() || !m_dataBuffer) return;

    QString signal = m_editSignal->currentText().trimmed();
    QString label = m_editLabel->text().trimmed();

    m_pinData[m_selectedPin]["Signal"] = signal;
    m_pinData[m_selectedPin]["GPIO_Label"] = label;

    if (!signal.isEmpty())
        m_modifiedPins.insert(m_selectedPin);
    else
        m_modifiedPins.remove(m_selectedPin);

    m_dataBuffer->replaceData(buildIocBytes());
}

// значения из m_pinData
QByteArray STM32PinoutTab::buildIocBytes() const
{
    QStringList lines;
    {
        QTextStream in(m_dataBuffer ? m_dataBuffer->data() : QByteArray());
        while (!in.atEnd())
            lines << in.readLine();
    }

    QMap<QString, QString> toWrite;
    for (auto pit = m_pinData.begin(); pit != m_pinData.end(); pit++){
        for (auto vit = pit.value().begin(); vit != pit.value().end(); vit++){
            QString key = pit.key() + "." + vit.key();

            if (!vit.value().isEmpty()){
                toWrite[key] = vit.value();
            }
        }
    }

    QSet<QString> writtenKeys;
    for (QString& line: lines){
        if(line.isEmpty() || line.startsWith('#') || line.startsWith('[')){
            continue;
        }
        int eq = line.indexOf('=');
        if (eq == -1) {
            continue;
        }
        QString key = line.left(eq).trimmed();

        if (toWrite.contains(key)) {

            QString newValue = toWrite[key];
            if (!newValue.isEmpty()) {
                line = key + "=" + newValue;
                writtenKeys.insert(key);
            } else {
                line.clear();
                writtenKeys.insert(key);
            }
        }
    }

    for (auto it = toWrite.begin(); it != toWrite.end(); it++){
        if (!writtenKeys.contains(it.key()) && !it.value().isEmpty()){
            lines << it.key() + "=" + it.value();
        }
    }
    lines.removeAll(QString());

    QByteArray out;
    for (const QString& line : lines)
        out += line.toUtf8() + '\n';
    return out;
}

QStringList STM32PinoutTab::getAllPinsForPackage(const QString& mcuName, const QString& package)
{
    QStringList pins;
    QString pkg = package.toUpper();
    // определяем набор портов по пакету
    auto addPort = [&pins](const QString& port){
        for(int i = 0; i<=15; i++){
            pins<<QString("%1%2").arg(port).arg(i);
        }
    };

    QMap<QString, QStringList> portMap;
    portMap["48"] = {"PA", "PB", "PC", "PD"};
    portMap["64"] = {"PA", "PB", "PC", "PD"};
    portMap["100"] = {"PA", "PB", "PC", "PD", "PE"};
    portMap["144"] = {"PA", "PB", "PC", "PD", "PE", "PF", "PG"};
    portMap["176"] = {"PA", "PB", "PC", "PD", "PE", "PF", "PG", "PH"};

    bool found = false;
    for (auto it = portMap.begin(); it != portMap.end(); it++){
        if(pkg.contains(it.key())){
            for (const QString& port : it.value()){
                addPort(port);
            }
            found = true;
            break;
            }
        }
            if (!found) {
        addPort("PA");
        addPort("PB");
        addPort("PC");
    }

    return pins;
}