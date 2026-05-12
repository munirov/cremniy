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

#include "recent_projects_page.h"
#include "../ProjectCard/project_card.h"
#include "projects_history_manager.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

RecentProjectsPage::RecentProjectsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setObjectName("CardsScrollArea");
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* cardsContainer = new QWidget();
    m_cardsLayout = new QVBoxLayout(cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(0);
    m_cardsLayout->addStretch();

    scrollArea->setWidget(cardsContainer);
    root->addWidget(scrollArea, 1);

    auto* toolbar = new QWidget();
    toolbar->setObjectName("WelcomeToolbar");
    toolbar->setFixedHeight(48);

    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 0, 12, 0);
    toolbarLayout->setSpacing(8);
    toolbarLayout->addStretch();

    auto* openBtn = new QPushButton(tr("Open..."));
    auto* createBtn = new QPushButton(tr("New Project"));

    toolbarLayout->addWidget(openBtn);
    toolbarLayout->addWidget(createBtn);

    connect(openBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Open Project"), QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (!dir.isEmpty()) {
            emit openProjectRequested(dir);
        }
    });

    connect(createBtn, &QPushButton::clicked,
            this, &RecentProjectsPage::newProjectRequested);

    root->addWidget(toolbar);

    reload();
}

void RecentProjectsPage::reload()
{
    clearCards();

    const auto projects = utils::ProjectsHistoryManager::loadProjectsHistory();

    int insertPos = 0;
    for (const auto& project : projects) {
        auto* card = new ProjectCard(project);

        connect(card, &ProjectCard::openRequested,
                this, &RecentProjectsPage::openProjectRequested);

        connect(card, &ProjectCard::removeRequested, this, [this](const QString& path) {
            utils::ProjectsHistoryManager::removeProjectFromHistory(path);
            reload();
        });

        m_cardsLayout->insertWidget(insertPos++, card);
    }
}

void RecentProjectsPage::clearCards() const {
    while (m_cardsLayout->count() > 1) {
        const auto* item = m_cardsLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}
