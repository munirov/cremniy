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

#include "create_project_page.h"
#include "widgets/clickablelineedit.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>

CreateProjectPage::CreateProjectPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("CreateProjectPage");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 40, 40, 40);
    root->setSpacing(0);

    /* Title */
    auto* title = new QLabel(tr("New Project"));
    title->setStyleSheet("color:#e0e0e0; font-size:16px; font-weight:bold;");
    root->addWidget(title);
    root->addSpacing(24);

    /* Grid */
    auto* grid = new QGridLayout();
    grid->setSpacing(10);
    grid->setColumnStretch(1, 1);

    m_nameLabel = new QLabel(tr("Project Name"));
    m_nameEdit  = new QLineEdit();
    m_nameEdit->setPlaceholderText("my-project");
    m_nameEdit->setValidator(
        new QRegularExpressionValidator(
            QRegularExpression("^[A-Za-z0-9_-]+$"), this)
    );
    grid->addWidget(m_nameLabel, 0, 0);
    grid->addWidget(m_nameEdit,  0, 1);

    m_langLabel = new QLabel(tr("Language"));
    m_langCombo = new QComboBox();
    m_langCombo->addItems({"C", "C++", "ASM", "C + ASM", "Custom"});
    grid->addWidget(m_langLabel, 1, 0);
    grid->addWidget(m_langCombo, 1, 1);

    m_pathLabel = new QLabel(tr("Location"));
    m_pathEdit  = new ClickableLineEdit();
    m_pathEdit->setReadOnly(true);
    m_pathEdit->setPlaceholderText(tr("Click to choose directory..."));
    connect(m_pathEdit, &ClickableLineEdit::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Choose Location"), QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (!dir.isEmpty()) m_pathEdit->setText(dir);
    });
    grid->addWidget(m_pathLabel, 2, 0);
    grid->addWidget(m_pathEdit,  2, 1);

    root->addLayout(grid);
    root->addStretch();

    /* Error label */
    m_infoLabel = new QLabel();
    m_infoLabel->setObjectName("CreateProjectInfoLabel");
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setVisible(false);
    root->addWidget(m_infoLabel);
    root->addSpacing(8);

    /* Buttons */
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    auto* backBtn   = new QPushButton(tr("Back"));
    auto* createBtn = new QPushButton(tr("Create Project"));

    btnLayout->addStretch();
    btnLayout->addWidget(backBtn);
    btnLayout->addWidget(createBtn);
    root->addLayout(btnLayout);

    connect(backBtn,   &QPushButton::clicked, this, &CreateProjectPage::backRequested);
    connect(createBtn, &QPushButton::clicked, this, &CreateProjectPage::onCreateClicked);
}

void CreateProjectPage::onCreateClicked()
{
    resetErrors();

    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        setFieldError(m_nameLabel, tr("Please enter a project name"));
        return;
    }

    if (const QFileInfo dirInfo(m_pathEdit->text()); !dirInfo.exists() || !dirInfo.isDir()) {
        setFieldError(m_pathLabel, tr("Please choose a valid directory"));
        return;
    }

    const QString newPath = m_pathEdit->text() + "/" + name;
    QDir dir;

    if (dir.exists(newPath)) {
        setFieldError(m_nameLabel, tr("A folder with this name already exists"));
        return;
    }

    if (!dir.mkdir(newPath)) {
        m_infoLabel->setText(tr("Failed to create project directory"));
        m_infoLabel->setVisible(true);
        return;
    }

    emit projectCreated(newPath, m_langCombo->currentText());
}

void CreateProjectPage::setFieldError(QLabel* label, const QString& message) const
{
    label->setProperty("error", true);
    label->style()->polish(label);
    m_infoLabel->setText(message);
    m_infoLabel->setVisible(true);
}

void CreateProjectPage::resetErrors()
{
    m_infoLabel->setVisible(false);
    for (auto* lbl : {m_nameLabel, m_langLabel, m_pathLabel}) {
        lbl->setProperty("error", false);
        lbl->style()->polish(lbl);
    }
}
