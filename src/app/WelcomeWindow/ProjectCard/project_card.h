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

#ifndef PROJECT_CARD_H
#define PROJECT_CARD_H

#include "projects_history_manager.h"
#include <QWidget>

class ProjectCard : public QWidget {
    Q_OBJECT

public:
    explicit ProjectCard(const utils::RecentProject& project, QWidget* parent = nullptr);

    QString projectPath() const { return m_path; }

signals:
    void openRequested(const QString& path);
    void removeRequested(const QString& path);

private:
    QString m_path;

    static QString shortLang(const QString& lang);
};

#endif /* PROJECT_CARD_H */
