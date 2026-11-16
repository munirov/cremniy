#pragma once

#include "findtypes.h"

class FindEngine final {
public:
    static SearchReport searchDocuments(const QVector<SearchDocument>& documents,
                                        const SearchOptions& options);
    static SearchReport searchProject(const QString& projectPath,
                                      const QVector<SearchDocument>& openDocuments,
                                      const SearchOptions& options);
    static ReplacementResult replaceAll(const QByteArray& contents,
                                        const SearchOptions& options);
    static ReplacementResult replaceMatch(const QByteArray& contents,
                                          const SearchMatch& match,
                                          const SearchOptions& options);

private:
    FindEngine() = delete;
};
