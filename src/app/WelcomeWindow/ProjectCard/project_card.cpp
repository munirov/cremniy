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


#include "project_card.h"

#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ProjectCard::ProjectCard(const utils::RecentProject& project, QWidget* parent)
    : QWidget(parent)
    , m_path(project.path)
{
    setFixedHeight(72);
    setObjectName("ProjectCard");

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(14, 8, 14, 8);
    root->setSpacing(12);

    const QString language = project.language.isEmpty() ? "?" : project.language;

    auto* badge = new QLabel(shortLang(language));
    badge->setObjectName("ProjectCardBadge");
    badge->setFixedSize(42, 42);
    badge->setAlignment(Qt::AlignCenter);
    badge->setProperty("language", language);
    root->addWidget(badge);

    auto* info = new QVBoxLayout();
    info->setSpacing(3);
    info->setContentsMargins(0, 0, 0, 0);

    const QString displayName = project.name.isEmpty()
        ? QFileInfo(project.path).fileName()
        : project.name;

    auto* nameLabel = new QLabel(displayName);
    nameLabel->setObjectName("ProjectCardName");

    auto* pathLabel = new QLabel(project.path);
    pathLabel->setObjectName("ProjectCardPath");
    pathLabel->setToolTip(project.path);

    QString lastOpened = project.lastOpenedAt;
    if (!lastOpened.isEmpty()) {
        lastOpened = lastOpened.replace("T", " ").left(16);
    }

    auto* lastLabel = new QLabel(lastOpened);
    lastLabel->setObjectName("ProjectCardLastOpened");

    info->addWidget(nameLabel);
    info->addWidget(pathLabel);
    info->addWidget(lastLabel);
    root->addLayout(info, 1);

    auto* openBtn = new QPushButton(tr("Open"));
    openBtn->setObjectName("ProjectCardOpenBtn");
    openBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(openBtn, 0, Qt::AlignVCenter);

    auto* removeBtn = new QPushButton(tr("Remove"));
    removeBtn->setObjectName("ProjectCardRemoveBtn");
    removeBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(removeBtn, 0, Qt::AlignVCenter);

    connect(openBtn, &QPushButton::clicked, this, [this]() {
        emit openRequested(m_path);
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        emit removeRequested(m_path);
    });
}

QString ProjectCard::shortLang(const QString& lang)
{
    if (lang == "C++") return "CPP";
    if (lang == "C") return "C";
    if (lang == "ASM") return "ASM";
    if (lang == "C + ASM") return "C+ASM";
    if (lang == "Custom") return "USR";
    if (lang == "?") return "?";
    return lang;
}
