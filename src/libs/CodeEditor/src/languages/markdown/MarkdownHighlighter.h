#pragma once

#include "QStyleSyntaxHighlighter.hpp"

class MarkdownHighlighter : public QStyleSyntaxHighlighter {
public:
    explicit MarkdownHighlighter(QTextDocument* document = nullptr);

protected:
    void highlightBlock(const QString& text) override;
};
