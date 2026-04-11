#ifndef FILECONTEXT_H
#define FILECONTEXT_H

#include <cstdint>
#include <qobject.h>
class FileContext
{
    friend class FileManager;

public:
    // Класс который хранит информацию об открытом файле. Для каждого ToolTab отдельно (codeEditor, hexView и т.д)
    FileContext(QString filepath) :
        m_filePath(filepath)
    {

    }

    // - - Getters - -
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
    // путь к файлу (ссылка на FileTab->m_filePath)
    QString m_filePath;
    // количество загруженных (текущих отображаемых) байтов
    uint64_t m_bytesCount;
    // начало в файле (номер байта)
    uint64_t m_startOffset;
    // конец в файле (номер байта)
    uint64_t m_endOffset;

};

#endif // FILECONTEXT_H
