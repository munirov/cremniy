#ifndef BLAMETOOLTIP_H
#define BLAMETOOLTIP_H

#include <QWidget>
#include "EditorBlameLineInfo.h"

/**
 * @brief A beautiful, semi-transparent tooltip for Git Blame information.
 */
class BlameTooltip : public QWidget {
    Q_OBJECT

public:
    explicit BlameTooltip(QWidget* parent = nullptr);

    void setBlameInfo(const EditorBlameLineInfo& info);
    void showAt(const QPoint& globalPos);
    QString currentCommitHash() const { return m_info.fullOid; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    EditorBlameLineInfo m_info;

    QString formatAuthor() const;
    QString formatDate() const;
};

#endif // BLAMETOOLTIP_H
