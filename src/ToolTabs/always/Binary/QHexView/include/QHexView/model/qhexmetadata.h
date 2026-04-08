#pragma once

#include <QColor>
#include <QHash>
#include <QHexView/model/qhexoptions.h>
#include <QList>
#include <QObject>
#include <functional>

struct QHexMetadataItem {
    qint64 begin, end;
    QHexCharFormat format;
    QString comment;
};

using QHexMetadataLine = QList<QHexMetadataItem>;

class QHexMetadata: public QObject {
    Q_OBJECT

private:
    using ClearMetadataCallback = std::function<bool(QHexMetadataItem&)>;

private:
    explicit QHexMetadata(const QHexOptions* options,
                          QObject* parent = nullptr);

public:
    const QHexMetadataLine* find(qint64 line) const;
    QString getComment(qint64 line, qint64 column) const;
    void removeMetadata(qint64 line);
    void removeBackground(qint64 line);
    void removeForeground(qint64 line);
    void removeComments(qint64 line);
    void unhighlight(qint64 line);
    void clear();

public:
    void setMetadata(qint64 begin, qint64 end, const QColor& fg,
                     const QBrush& bg, const QString& comment) {
        this->setMetadata({begin, end, {bg, fg, {}}, comment});
    }

    void setForeground(qint64 begin, qint64 end, const QColor& fg) {
        this->setMetadata(begin, end, fg, Qt::NoBrush, QString{});
    }

    void setBackground(qint64 begin, qint64 end, const QBrush& bg) {
        this->setMetadata(begin, end, QColor{}, bg, QString{});
    }

    void setComment(qint64 begin, qint64 end, const QString& comment) {
        this->setMetadata(begin, end, QColor{}, Qt::NoBrush, comment);
    }

    void setMetadataSize(qint64 begin, qint64 length, const QColor& fg,
                         const QBrush& bg, const QString& comment) {
        this->setMetadata({begin, begin + length, {bg, fg, {}}, comment});
    }

    void setForegroundSize(qint64 begin, qint64 length, const QColor& fg) {
        this->setForeground(begin, begin + length, fg);
    }

    void setBackgroundSize(qint64 begin, qint64 length, const QBrush& bg) {
        this->setBackground(begin, begin + length, bg);
    }

    void setCommentSize(qint64 begin, qint64 length, const QString& comment) {
        this->setComment(begin, begin + length, comment);
    }

private:
    void copy(const QHexMetadata* metadata);
    void clearMetadata(qint64 line, ClearMetadataCallback&& cb);
    void setMetadata(const QHexMetadataItem& mi);
    void invalidate();

Q_SIGNALS:
    void changed();
    void cleared();

private:
    QHash<qint64, QHexMetadataLine> m_metadata;
    const QHexOptions* m_options;

    friend class QHexView;
};
