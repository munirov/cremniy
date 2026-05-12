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

#ifndef RECENT_PROJECTS_PAGE_H
#define RECENT_PROJECTS_PAGE_H

#include <QVBoxLayout>
#include <QWidget>

class RecentProjectsPage : public QWidget {
    Q_OBJECT

public:
    explicit RecentProjectsPage(QWidget* parent = nullptr);

    void reload();

signals:
    void openProjectRequested(const QString& path);
    void newProjectRequested();

private:
    QVBoxLayout* m_cardsLayout;

    void clearCards() const;
};

#endif /* RECENT_PROJECTS_PAGE_H */
