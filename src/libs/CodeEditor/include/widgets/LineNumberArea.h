#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include <QWidget>

class CustomCodeEditor;

/**
 * @brief Widget for displaying line numbers
 * 
 * Displays line numbers on the left side of the code editor.
 * Width adjusts automatically based on the number of lines.
 */
class LineNumberArea : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param editor Pointer to the parent editor
     */
    explicit LineNumberArea(CustomCodeEditor* editor);
    
    /**
     * @brief Get the recommended size for this widget
     * @return Size hint based on line count
     */
    QSize sizeHint() const override;
    
    /**
     * @brief Calculate the width needed for line numbers
     * @return Width in pixels
     */
    int calculateWidth() const;

protected:
    /**
     * @brief Paint event handler
     * @param event Paint event
     */
    void paintEvent(QPaintEvent* event) override;

private:
    CustomCodeEditor* m_editor;
};

#endif // LINENUMBERAREA_H
