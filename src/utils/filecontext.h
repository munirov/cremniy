#ifndef FILECONTEXT_H
#define FILECONTEXT_H

#include <cstdint>
#include <qobject.h>
class FileContext
{
    friend class FileManager;

public:
    /* A class that stores state for an open file. Unique per ToolTab (codeEditor, hexView, etc.) */
    FileContext(QString filepath) :
        m_filePath(filepath)
    {

    }

    /* - - Getters - - */
    QString filePath(){
        return m_filePath;
    }

    uint64_t bytesCount(){
        return m_bytesCount;
    }

    uint64_t startOffset(){
        return m_startOffset;
    }

    uint64_t endOffset(){
        return m_endOffset;
    }

private:
    /* file path (ref to FileTab->m_filePath) */
    QString m_filePath;
    /* number of loaded (currently displayed) bytes */
    uint64_t m_bytesCount;
    /* start in file (byte number) */
    uint64_t m_startOffset;
    /* end in file (byte number) */
    uint64_t m_endOffset;

};

#endif // FILECONTEXT_H
