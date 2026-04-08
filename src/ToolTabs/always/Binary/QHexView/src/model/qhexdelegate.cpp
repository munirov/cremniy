#include <QHexView/model/qhexdelegate.h>
#include <QHexView/qhexview.h>

QHexDelegate::QHexDelegate(QObject* parent): QObject{parent} {}

QString QHexDelegate::addressHeader(const QHexView* hexview) const {
    Q_UNUSED(hexview);
    return QString();
}

QString QHexDelegate::hexHeader(const QHexView* hexview) const {
    Q_UNUSED(hexview);
    return QString();
}

QString QHexDelegate::asciiHeader(const QHexView* hexview) const {
    Q_UNUSED(hexview);
    return QString();
}

void QHexDelegate::renderAddress(quint64 address, QHexCharFormat& cf,
                                 const QHexView* hexview) const {
    Q_UNUSED(address);
    Q_UNUSED(hexview);
    Q_UNUSED(cf);
    Q_UNUSED(hexview);
}

bool QHexDelegate::renderByte(quint64 offset, quint8 b, QHexCharFormat& outcf,
                              const QHexView* hexview) const {
    Q_UNUSED(offset);
    Q_UNUSED(b);
    Q_UNUSED(outcf);
    Q_UNUSED(hexview);

    return false;
}

bool QHexDelegate::paintSeparator(QPainter* painter, QLineF line,
                                  const QHexView* hexview) const {
    Q_UNUSED(painter);
    Q_UNUSED(line);
    Q_UNUSED(hexview);
    return false;
}

void QHexDelegate::paint(QPainter* painter, const QHexView* hexview) const {
    Q_UNUSED(hexview);
    hexview->paint(painter);
}
