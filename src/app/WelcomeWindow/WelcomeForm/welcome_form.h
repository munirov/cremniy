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

#ifndef WELCOME_FORM_H
#define WELCOME_FORM_H

#include <QStackedWidget>
#include <QWidget>

class RecentProjectsPage;
class CreateProjectPage;

class WelcomeForm : public QWidget {
    Q_OBJECT

public:
    explicit WelcomeForm(QWidget* parent = nullptr);
    ~WelcomeForm() override = default;

private:
    QStackedWidget*     m_stack;
    RecentProjectsPage* m_recentPage;
    CreateProjectPage*  m_createPage;

    void openProject(const QString& path, const QString& language = {});
    void loadStyles();
};

#endif /* WELCOME_FORM_H */