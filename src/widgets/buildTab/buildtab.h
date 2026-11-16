#ifndef BUILDTAB_H
#define BUILDTAB_H

#include <QWidget>
#include "project_info_manager.h"
#include "core/buildman/buildmanager.h"

class BuildTab : public QWidget {
    Q_OBJECT
public:
    explicit BuildTab(const ProjectInfo &projInfo, QWidget* parent = nullptr);

private:
    BuildManager *buildMan;
    logView *m_logViewWidg;

public slots:
    void onBuild();
    void onStop();
    void onClear();
};

#endif// BUILDTAB_H
