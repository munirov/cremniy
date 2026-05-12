/*
 * This file is part of the Cremniy IDE source code.
 *
 * Copyright (c) 2026 Cremniy IDE
 * SPDX-License-Identifier: GPL-3.0 license
 *
 * Repository:
 * https://github.com/munirov/cremniy
 *
 * Modified by Ilya (https://github.com/kykyrudza) on 2026-05-12
 */

#include "welcome_form.h"
#include "../RecentProjectsPage/recent_projects_page.h"
#include "../CreateProjectPage/create_project_page.h"
#include "app/IDEWindow/idewindow.h"
#include "projects_history_manager.h"

#include <QDir>
#include <QFile>

WelcomeForm::WelcomeForm(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Cremniy");
    resize(500, 380);

    loadStyles();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    m_recentPage = new RecentProjectsPage();
    m_createPage = new CreateProjectPage();

    m_stack->addWidget(m_recentPage); /* 0 */
    m_stack->addWidget(m_createPage); /* 1 */

    connect(m_recentPage, &RecentProjectsPage::openProjectRequested,
            this, [this](const QString& path) {
                openProject(path);
            });

    connect(m_recentPage, &RecentProjectsPage::newProjectRequested, this, [this]() {
        m_stack->setCurrentIndex(1);
    });

    connect(m_createPage, &CreateProjectPage::backRequested, this, [this]() {
        m_stack->setCurrentIndex(0);
    });

    connect(m_createPage, &CreateProjectPage::projectCreated,
            this, &WelcomeForm::openProject);
}

void WelcomeForm::openProject(const QString& path, const QString& language)
{
    if (!QDir(path).exists()) return;

    utils::RecentProject project;
    project.path     = path;
    project.name     = QDir(path).dirName();
    project.language = language;

    utils::ProjectsHistoryManager::saveProjectHistory(project);

    hide();

    auto* mw = new IDEWindow(path, nullptr);
    mw->setAttribute(Qt::WA_DeleteOnClose);
    mw->setWindowState(Qt::WindowMaximized);

    connect(mw, &IDEWindow::CloseProject, this, [this]() {
        m_recentPage->reload();
        show();
    });

    mw->show();
}

void WelcomeForm::loadStyles()
{
    QString styles;

    for (const QString& path : {":/styles/base.qss", ":/styles/welcome.qss"}) {
        if (QFile f(path); f.open(QIODevice::ReadOnly)) {
            styles += f.readAll();
            f.close();
        }
    }

    setStyleSheet(styles);
}