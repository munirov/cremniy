#ifndef TABBASE_H
#define TABBASE_H

#include "core/file/FileDataBuffer.h"
#include "core/file/filecontext.h"
#include <QWidget>

class TabBase : public QWidget {
    Q_OBJECT

protected:
    /**
     * @brief Общий буфер данных
     *
     * Хранит все данные файла и обеспечивает синхронизацию между вкладками
     */
    FileDataBuffer* m_dataBuffer;

    /**
     * @brief Контекст файла
     */
    FileContext* m_fileContext = nullptr;

    /**
     * @brief Флаг означающий изменения в данных
     *
     * Если true - данные изменены, false - данные равны данным в файле
     */
    bool m_modifyIndicator = false;

public:
    /**
     * @brief Конструктор класса
     *
     * @param buffer указатель на общий буфер данных
     * @param parent родительский виджет
     */
    explicit TabBase(QWidget* parent = nullptr)
        : QWidget(parent)
    {}

    /**
     * @brief Получить лого инструмента для вкладки
     */
    virtual QIcon icon() const = 0;

    /**
     * @brief Получить значение индикатора изменений
     */
    bool getModifyIndicator() {
        return m_modifyIndicator;
    }

    /**
     * @brief Установить значение индикатора изменений
     */
    void setModifyIndicator(bool value) {
        m_modifyIndicator = value;
    }

    /**
     * @brief Установить общий буффер данных
     *
     * @param newFileDataBuffer указатель на общий буффер
     */
    virtual void setFileDataBuffer(FileDataBuffer* newFileDataBuffer){
        m_dataBuffer = newFileDataBuffer;

        connect(m_dataBuffer, &FileDataBuffer::byteChanged,
                this, &TabBase::onByteChanged);
        connect(m_dataBuffer, &FileDataBuffer::bytesChanged,
                this, &TabBase::onBytesChanged);
        connect(m_dataBuffer, &FileDataBuffer::selectionChanged,
                this, &TabBase::onSelectionChanged);
        connect(m_dataBuffer, &FileDataBuffer::dataChanged,
                this, &TabBase::onDataChanged);
    }

protected slots:
    /**
     * @brief Обработчик изменения байта
     *
     * Вызывается при изменении байта в буфере
     * @param pos позиция измененного байта
     */
    virtual void onByteChanged(qint64 pos) { Q_UNUSED(pos); }

    /**
     * @brief Обработчик изменения диапазона байтов
     *
     * Вызывается при изменении диапазона байтов в буфере
     * @param pos начальная позиция
     * @param length длина диапазона
     */
    virtual void onBytesChanged(qint64 pos, qint64 length) { Q_UNUSED(pos); Q_UNUSED(length); }

    /**
     * @brief Обработчик изменения выделения
     *
     * Вызывается при изменении выделения в буфере
     * @param pos начальная позиция выделения
     * @param length длина выделения
     */
    virtual void onSelectionChanged(qint64 pos, qint64 length) { Q_UNUSED(pos); Q_UNUSED(length); }

    /**
     * @brief Обработчик полного изменения данных
     *
     * Вызывается при загрузке нового файла
     */
    virtual void onDataChanged() {}

public slots:
    /**
     * @brief Установить файл инструменту
     *
     * @param filepath путь к файлу
     */
    virtual void setFile(QString filepath) = 0;

    /**
     * @brief Установить данные из файла во вкладку
     */
    virtual void setTabData() = 0;

    /**
     * @brief Сохраняет данные из вкладки в файл
     */
    virtual void saveTabData() = 0;

    /**
     * @brief Включение или отключение Word Wrap в зависимости от checked
     */
    virtual void setWordWrapSlot(bool checked) = 0;

    /**
     * @brief Включение или отключение замены TAB на пробелы в зависимости от checked
     */
    virtual void setTabReplaceSlot(bool checked) = 0;

    /**
     * @brief Устанавливает размер TAB в зависимости от width
     */
    virtual void setTabWidthSlot(int width) = 0;

signals:
    /**
     * @brief Status bar information changed
     *
     * @param info text to display in the status bar
     */
    void statusBarInfoChanged(const QString& info);

    /**
     * @brief Обновить данные из файла во всех вкладках
     *
     * Эмиттируется, когда нужно обновить данные на всех вкладках интерфейса
     * Например, при сохранении данных текущего ToolTab в файл, необходимо обновить данные на всех остальных ToolTab
     */
    void refreshDataAllTabsSignal();

    /**
     * @brief Изменение изначальных данных
     *
     * Эмиттится, каждый раз когда происходит изменение данных и они не равны изначальным данным
     */
    void modifyData();

    /**
     * @brief Изначальные данные не отличаются от текущих
     *
     * Эмиттится, каждый раз когда происходит изменение данных и они равны изначальным данным
     * Также эмиттится после вызова функции setTabData, чтобы убрать звезду
     */
    void dataEqual();
};

#endif // TABBASE_H
