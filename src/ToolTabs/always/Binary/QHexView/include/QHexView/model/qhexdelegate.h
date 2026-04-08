#pragma once

#include <QColor>
#include <QHexView/model/qhexoptions.h>
#include <QHexView/model/qhexutils.h>
#include <QObject>

class QHexView;
class QPainter;

class QHexDelegate: public QObject {
    Q_OBJECT

public:
    explicit QHexDelegate(QObject* parent = nullptr);
    virtual ~QHexDelegate() = default;
    virtual QString addressHeader(const QHexView* hexview) const;
    virtual QString hexHeader(const QHexView* hexview) const;
    virtual QString asciiHeader(const QHexView* hexview) const;
    virtual void renderAddress(quint64 address, QHexCharFormat& cf,
                               const QHexView* hexview) const;
    virtual bool renderByte(quint64 offset, quint8 b, QHexCharFormat& outcf,
                            const QHexView* hexview) const;
    virtual bool paintSeparator(QPainter* painter, QLineF line,
                                const QHexView* hexview) const;
    virtual void paint(QPainter* painter, const QHexView* hexview) const;
};
