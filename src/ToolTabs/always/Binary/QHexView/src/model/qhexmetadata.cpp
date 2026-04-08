#include <QHexView/model/qhexcursor.h>
#include <QHexView/model/qhexmetadata.h>

QHexMetadata::QHexMetadata(const QHexOptions* options, QObject* parent)
    : QObject(parent), m_options(options) {}

const QHexMetadataLine* QHexMetadata::find(qint64 line) const {
    auto it = m_metadata.find(line);
    return it != m_metadata.end() ? std::addressof(it.value()) : nullptr;
}

QString QHexMetadata::getComment(qint64 line, qint64 column) const {
    auto* metadataline = this->find(line);
    if(!metadataline)
        return QString();

    auto offset = QHexUtils::positionToOffset(m_options, {line, column});
    QStringList comments;

    for(auto& mi : *metadataline) {
        if((offset < mi.begin || offset > mi.end) || mi.comment.isEmpty())
            continue;
        comments.push_back(mi.comment);
    }

    return comments.join("\n");
}

void QHexMetadata::removeMetadata(qint64 line) {
    auto it = m_metadata.find(line);
    if(it == m_metadata.end())
        return;

    m_metadata.erase(it);
    Q_EMIT changed();
}

void QHexMetadata::removeBackground(qint64 line) {
    this->clearMetadata(line, [](QHexMetadataItem& mi) -> bool {
        if(mi.format.background == Qt::NoBrush)
            return false;

        if(mi.format.foreground.isValid() || !mi.comment.isEmpty()) {
            mi.format.background = QBrush{};
            return false;
        }

        return true;
    });
}

void QHexMetadata::removeForeground(qint64 line) {
    this->clearMetadata(line, [](QHexMetadataItem& mi) -> bool {
        if(!mi.format.foreground.isValid())
            return false;

        if(mi.format.background != Qt::NoBrush || !mi.comment.isEmpty()) {
            mi.format.foreground = QColor{};
            return false;
        }

        return true;
    });
}

void QHexMetadata::removeComments(qint64 line) {
    this->clearMetadata(line, [](QHexMetadataItem& mi) -> bool {
        if(mi.comment.isEmpty())
            return false;

        if(mi.format.foreground.isValid() ||
           mi.format.background != Qt::NoBrush) {
            mi.comment.clear();
            return false;
        }

        return true;
    });
}

void QHexMetadata::unhighlight(qint64 line) {
    this->clearMetadata(line, [](QHexMetadataItem& mi) -> bool {
        if(!mi.format.foreground.isValid() &&
           mi.format.background == Qt::NoBrush)
            return false;

        if(!mi.comment.isEmpty()) {
            mi.format.foreground = QColor{};
            mi.format.background = QBrush{};
            return false;
        }

        return true;
    });
}

void QHexMetadata::clear() {
    m_metadata.clear();
    Q_EMIT changed();
}

void QHexMetadata::copy(const QHexMetadata* metadata) {
    m_metadata = metadata->m_metadata;
}

void QHexMetadata::clearMetadata(qint64 line, ClearMetadataCallback&& cb) {
    auto iit = m_metadata.find(line);
    if(iit == m_metadata.end())
        return;

    auto oldsize = iit->size();

    for(auto it = iit->begin(); it != iit->end();) {
        if(cb(*it))
            it = iit->erase(it);
        else
            it++;
    }

    if(iit->empty()) {
        this->removeMetadata(line);
        return;
    }

    if(oldsize != iit->size())
        Q_EMIT changed();
}

void QHexMetadata::setMetadata(const QHexMetadataItem& mi) {
    if(!m_options->line_length)
        return;

    const qint64 firstline = mi.begin / m_options->line_length;
    const qint64 lastline = mi.end / m_options->line_length;
    bool notify = false;

    for(auto line = firstline; line <= lastline; line++) {
        auto start = line == firstline ? mi.begin % m_options->line_length : 0;
        auto length = line == lastline
                          ? (mi.end % m_options->line_length) - start
                          : m_options->line_length;
        if(length <= 0)
            continue;

        notify = true;
        m_metadata[line].push_back(mi);
    }

    if(notify)
        Q_EMIT changed();
}

void QHexMetadata::invalidate() {
    auto oldmetadata = m_metadata;
    m_metadata.clear();

    for(const QHexMetadataLine& line : oldmetadata)
        for(const QHexMetadataItem& mi : line)
            this->setMetadata(mi);
}
