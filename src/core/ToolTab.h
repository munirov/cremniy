#ifndef TOOLTAB_H
#define TOOLTAB_H

#include "FileDataBuffer.h"
#include "utils/filecontext.h"
#include <QWidget>

class ToolTab : public QWidget {
    Q_OBJECT

protected:
    /**
     * @brief Shared data buffer
     *
     * Stores file data and ensures sync between tabs
     */
    FileDataBuffer* m_dataBuffer;

    /**
     * @brief File context
     */
    FileContext* m_fileContext = nullptr;

    /**
     * @brief Data modified flag
     *
     * If true - data modified, false - data equials data in file
     */
    bool m_modifyIndicator = false;

public:
    /**
     * @brief Class constructor
     *
     * @param buffer Pointer to shared data buffer
     * @param parent Parent widget
     */
    explicit ToolTab(FileDataBuffer* buffer, QWidget* parent = nullptr)
        : QWidget(parent), m_dataBuffer(buffer)
    {
        /* Connect to buffer signals */ 
        connect(m_dataBuffer, &FileDataBuffer::byteChanged,
            this, &ToolTab::onByteChanged);
        connect(m_dataBuffer, &FileDataBuffer::bytesChanged,
            this, &ToolTab::onBytesChanged);
        connect(m_dataBuffer, &FileDataBuffer::selectionChanged,
            this, &ToolTab::onSelectionChanged);
        connect(m_dataBuffer, &FileDataBuffer::dataChanged,
            this, &ToolTab::onDataChanged);
    }

    /**
     * @brief Get tool name for the tab
     */
    virtual QString toolName() const = 0;

    /**
     * @brief Get tool icon for the tab
     */
    virtual QIcon toolIcon() const = 0;

    /**
     * @brief Check modified status
     */
    bool getModifyIndicator() {
        return m_modifyIndicator;
    }

    /**
     * @brief Set the data modified flag
     */
    void setModifyIndicator(bool value) {
        m_modifyIndicator = value;
    }

protected slots:
    /**
     * @brief Byte change handler
     *
     * Triggered when a byte in the buffer is modified
     * @param pos position of the modified byte
     */
    virtual void onByteChanged(qint64 pos) { Q_UNUSED(pos); }

    /**
     * @brief Byte range change handler
     *
     * Triggered when a range of bytes in the buffer is modified
     * @param pos start position
     * @param length range length
     */
    virtual void onBytesChanged(qint64 pos, qint64 length) { Q_UNUSED(pos); Q_UNUSED(length); }

    /**
     * @brief Selection change handler
     *
     * Triggered when the selection within the buffer is modified
     * @param pos start position of the selection
     * @param length selection length
     */
    virtual void onSelectionChanged(qint64 pos, qint64 length) { Q_UNUSED(pos); Q_UNUSED(length); }

    /**
     * @brief Full data change handler
     *
     * Triggered when a new file is loaded
     */
    virtual void onDataChanged() {}

public slots:
    /**
     * @brief Set file-tool
     *
     * @param filepath path to file
     */
    virtual void setFile(QString filepath) = 0;

    /**
     * @brief Set data from the file in the tab
     */
    virtual void setTabData() = 0;

    /**
     * @brief Save tab data to a file
     */
    virtual void saveTabData() = 0;

signals:
    /**
     * @brief Update file data across all tabs
     *
     * Emitted when interface tabs need to synchronize their data
     * For example, when saving the current ToolTab to a file all
     * other ToolTabs must be updated to reflect the changes
     */
    void refreshDataAllTabsSignal();

    /**
     * @brief Original data modification signal
     *
     * Emitted whenever data changes and no longer matches the original state
     */
    void modifyData();

    /**
     * @brief Data reverted to original state
     *
     * Emitted whenever data changes and matches the original state. 
     * Also emitted after calling setTabData to clear the modified indicator (asterisk)
     */
    void dataEqual();
};

#endif // TOOLTAB_H
