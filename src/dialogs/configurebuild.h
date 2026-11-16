#ifndef CONFIGUREBUILD_H
#define CONFIGUREBUILD_H

#include <QDialog>
#include <qlabel.h>
#include <qlineedit.h>
#include "project_info_manager.h"

class ConfigureBuild : public QDialog {
    Q_OBJECT

    public:
        ConfigureBuild(ProjectInfo &projInfo, QWidget *parent);

    private:
        QLabel* m_buildCommandLabel;
        QLineEdit* m_buildCommandEdit;
        ProjectInfo* m_projectInfo;

        void saveConfigureClicked();
        void cancelClicked();

};

#endif// CONFIGUREBUILD_H
