#pragma once

#include "QStyleSyntaxHighlighter.hpp"

class MakefileHighlighter : public QStyleSyntaxHighlighter {
public:
    explicit MakefileHighlighter(QTextDocument* document = nullptr);

protected:
    void highlightBlock(const QString& text) override;
};
