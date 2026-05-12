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


#ifndef CREATE_PROJECT_PAGE_H
#define CREATE_PROJECT_PAGE_H

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

class ClickableLineEdit;

class CreateProjectPage : public QWidget {
    Q_OBJECT

public:
    explicit CreateProjectPage(QWidget* parent = nullptr);

    signals:
        void backRequested();
    void projectCreated(const QString& path, const QString& language);

private slots:
    void onCreateClicked();

private:
    QLineEdit*         m_nameEdit;
    QComboBox*         m_langCombo;
    ClickableLineEdit* m_pathEdit;
    QLabel*            m_infoLabel;
    QLabel*            m_nameLabel;
    QLabel*            m_langLabel;
    QLabel*            m_pathLabel;

    void setFieldError(QLabel* label, const QString& message) const;
    void resetErrors();
};

#endif /* CREATE_PROJECT_PAGE_H */