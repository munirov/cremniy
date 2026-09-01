#include "buildtab.h"
#include "core/buildman/buildmanager.h"
#include "logView/logview.h"
#include <qboxlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>



BuildTab::BuildTab(const ProjectInfo &projInfo, QWidget* parent)
    : QWidget{parent} {

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0,0,0,0);

    /* Log */
    auto* logLayout = new QVBoxLayout();
    m_logViewWidg = new logView();

    logLayout->addWidget(m_logViewWidg);

    root->addLayout(logLayout);

    /* Buttons */
    auto* btnLayout = new QVBoxLayout();
    btnLayout->setContentsMargins(10, 10, 10, 10);

    auto* buildBtn = new QPushButton(tr("Build"), this);
    auto* stopBtn = new QPushButton(tr("Stop"), this);
    auto* clearBtn = new QPushButton(tr("Clear"), this);

    btnLayout->addWidget(buildBtn);
    btnLayout->addWidget(stopBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(clearBtn);

    root->addLayout(btnLayout);

    connect(buildBtn, &QPushButton::clicked, this, &BuildTab::onBuild);
    connect(stopBtn, &QPushButton::clicked, this, &BuildTab::onStop);
    connect(clearBtn, &QPushButton::clicked, this, &BuildTab::onClear);

    layout()->setSizeConstraint(QLayout::SetMinimumSize);
    resize(480, sizeHint().height());

    buildMan = new BuildManager(projInfo.path, projInfo.buildCommand, m_logViewWidg);
    buildMan->build();

}

void BuildTab::onBuild(){
    buildMan->build();
}

void BuildTab::onStop(){
    buildMan->stop();
}

void BuildTab::onClear(){
    m_logViewWidg->clear();
}