#include "configurebuild.h"
#include <qboxlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>


ConfigureBuild::ConfigureBuild(ProjectInfo &projInfo, QWidget *parent)
    : QDialog(parent),
      m_projectInfo(&projInfo)
{

    setWindowTitle(tr("Configure Build"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 40, 40, 40);
    root->setSpacing(0);

    /* Title */
    auto* title = new QLabel(tr("Build"));
    title->setStyleSheet("color:#e0e0e0; font-size:16px; font-weight:bold;");
    root->addWidget(title);
    root->addSpacing(24);

    /* Grid */
    auto* grid = new QGridLayout();
    grid->setSpacing(10);
    grid->setColumnStretch(1, 1);

    m_buildCommandLabel = new QLabel(tr("Build Command"));
    m_buildCommandEdit  = new QLineEdit();

    if (m_projectInfo->buildCommand.isEmpty()) m_buildCommandEdit->setPlaceholderText(tr("e.g. cmake --build . (executed in the project root directory)"));
    else m_buildCommandEdit->setText(m_projectInfo->buildCommand);

    grid->addWidget(m_buildCommandLabel, 0, 0);
    grid->addWidget(m_buildCommandEdit,  0, 1);

    root->addLayout(grid);

    root->addSpacing(16);
    root->addStretch();

    /* Buttons */
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    auto* cancelBtn   = new QPushButton(tr("Cancel"));
    auto* applyBtn = new QPushButton(tr("Apply"));

    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(applyBtn);
    root->addLayout(btnLayout);

    connect(cancelBtn,   &QPushButton::clicked, this, &ConfigureBuild::cancelClicked);
    connect(applyBtn, &QPushButton::clicked, this, &ConfigureBuild::saveConfigureClicked);

    layout()->setSizeConstraint(QLayout::SetMinimumSize);
    resize(480, sizeHint().height());

}


void ConfigureBuild::saveConfigureClicked(){
    m_projectInfo->buildCommand = m_buildCommandEdit->text().trimmed();
    accept();
}

void ConfigureBuild::cancelClicked(){
    reject();
}
