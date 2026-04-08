#include "BuildSetupDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>

BuildSetupDialog::BuildSetupDialog(const BuildConfig& initial, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Build Configuration");
    setMinimumWidth(420);

    auto* form = new QFormLayout;
    m_qtPathEdit = new QLineEdit(initial.qtPath);
    m_buildEdit = new QLineEdit(initial.build);
    m_runEdit   = new QLineEdit(initial.run);
    m_cleanEdit = new QLineEdit(initial.clean);

    auto* qtPathRow = new QWidget(this);
    auto* qtPathLayout = new QHBoxLayout(qtPathRow);
    qtPathLayout->setContentsMargins(0,0,0,0);
    qtPathLayout->addWidget(m_qtPathEdit);
    auto* browseQtPath = new QPushButton("Browse...", qtPathRow);
    qtPathLayout->addWidget(browseQtPath);

    connect(browseQtPath, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, "Select Qt prefix path", m_qtPathEdit->text());
        if (!dir.isEmpty())
            m_qtPathEdit->setText(dir);
    });

    m_qtPathEdit->setPlaceholderText("e.g. E:/DevTools/Qt/6.10.2/msvc2022_64");
    m_buildEdit->setPlaceholderText("e.g. cmake -S . -B build -DCMAKE_PREFIX_PATH=\"C:/Qt/...\" && cmake --build build --config Release");
    m_runEdit->setPlaceholderText("e.g. .\\build\\Release\\myapp.exe");
    m_runEdit->setToolTip("Commands are executed from the project root directory");
    m_cleanEdit->setPlaceholderText("e.g. cmake --build build --config Release --target clean");

    form->addRow("Qt path:", qtPathRow);
    form->addRow("Build command:", m_buildEdit);
    form->addRow("Run command:", m_runEdit);
    auto* runHint = new QLabel("Commands are executed relative to the project directory. For CMake projects the run command uses the target name from CMakeLists.txt when it can be detected. Qt path is passed to the build process through environment variables (CMAKE_PREFIX_PATH, QT_DIR, Qt6_DIR/Qt5_DIR). ");
    runHint->setStyleSheet("color: gray; font-size: 11px;");
    runHint->setWordWrap(true);
    form->addRow("", runHint);
    form->addRow("Clean command:", m_cleanEdit);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* label = new QLabel(
        "Configure how to build and run your project.\n"
        "Settings will be saved to <b>cremniy.json</b> in the project folder.");
    label->setTextFormat(Qt::RichText);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

BuildConfig BuildSetupDialog::result() const {
    return { m_buildEdit->text().trimmed(),
             m_runEdit->text().trimmed(),
             m_cleanEdit->text().trimmed(),
             m_qtPathEdit->text().trimmed() };
}
