#include "welcomeform.h"
#include "app/IDEWindow/idewindow.h"
#include <qboxlayout.h>
#include <qdir.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <QListView>
#include <QFileDialog>
#include <QStackedWidget>
#include <QLabel>
#include <QComboBox>
#include <QJsonArray>
#include <qstandardpaths.h>
#include <qstringlistmodel.h>
#include <qtimer.h>

#include "projectshistorymanager.h"

WelcomeForm::WelcomeForm(QWidget *parent)
    : QWidget(parent)
{
    qDebug("WelcomeForm::WelcomeForm(QWidget *parent)");

    this->setWindowTitle("Cremniy");
    this->setBaseSize(400, 300);
    this->resize(400, 300);

    // Base
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    stack = new QStackedWidget(this);
    mainLayout->addWidget(stack);

    // Page "Welcome"
    QWidget *pageWelcome = new QWidget();
    QVBoxLayout *l1 = new QVBoxLayout(pageWelcome);

    RecentProjectsList = new QListView(pageWelcome);
    l1->addWidget(RecentProjectsList);
    RecentProjectsList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    RecentProjectsList->setSelectionMode(QAbstractItemView::SingleSelection);
    SetProjectHistoryList();

    QHBoxLayout *btnLayout = new QHBoxLayout();

    open_recent_proj_btn = new QPushButton(tr("Open"), pageWelcome);
    btnLayout->addWidget(open_recent_proj_btn);
    open_recent_proj_btn->setEnabled(false);

    QPushButton *open_browse_proj_btn = new QPushButton(tr("Open..."), pageWelcome);
    btnLayout->addWidget(open_browse_proj_btn);

    QPushButton *create_proj_btn = new QPushButton(tr("Create"), pageWelcome);
    btnLayout->addWidget(create_proj_btn);

    remove_recent_proj_btn = new QPushButton("×", pageWelcome);
    remove_recent_proj_btn->setFixedWidth(28);
    remove_recent_proj_btn->setStyleSheet("QPushButton { padding: 5px 0px; }");
    btnLayout->addWidget(remove_recent_proj_btn);
    remove_recent_proj_btn->setEnabled(false);

    l1->addLayout(btnLayout);

    stack->addWidget(pageWelcome);

    // Page "Create Project"
    QWidget *pageCreate = new QWidget();

    QVBoxLayout *l2 = new QVBoxLayout(pageCreate);

    // --- Grid layout для текста и полей ---
    QGridLayout *grid = new QGridLayout();

    // Первая строка: "Текст" + QLineEdit
    proj_name_label = new QLabel(tr("Project Name:"));
    proj_name_lineEdit = new QLineEdit();
    QRegularExpression re("^[A-Za-z0-9_-]+$");
    QValidator *validator = new QRegularExpressionValidator(re, this);
    proj_name_lineEdit->setValidator(validator);
    grid->addWidget(proj_name_label, 0, 0);
    grid->addWidget(proj_name_lineEdit, 0, 1);

    // Вторая строка: "Текст" + QComboBox
    language_label = new QLabel(tr("Language:"));
    language_comboBox = new QComboBox();
    language_comboBox->addItems({"C", "C++", "ASM", "C + ASM", "Custom"});
    grid->addWidget(language_label, 1, 0);
    grid->addWidget(language_comboBox, 1, 1);

    path_label = new QLabel(tr("Path:"));
    path_lineEdit = new ClickableLineEdit();
    path_lineEdit->setReadOnly(true);
    grid->addWidget(path_label, 2, 0);
    grid->addWidget(path_lineEdit, 2, 1);

    // Добавляем grid в основной вертикальный layout
    l2->addLayout(grid);
    l2->addStretch(1);

    info_label = new QLabel();
    info_label->setVisible(false);
    info_label->setStyleSheet("color: #bf3131; font-weight: bold;");
    info_label->setAlignment(Qt::AlignCenter);
    l2->addWidget(info_label);

    // --- Горизонтальный layout для кнопок ---
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *createButton = new QPushButton(tr("Create"));
    QPushButton *backButton = new QPushButton(tr("Back"));

    buttonLayout->addWidget(createButton);
    buttonLayout->addWidget(backButton);

    // Добавляем кнопки в основной вертикальный layout
    l2->addLayout(buttonLayout);

    stack->addWidget(pageCreate);

    // Set Default Page (Welcome)
    stack->setCurrentIndex(0);

    // Events
    connect(RecentProjectsList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &WelcomeForm::SelectProjectInList);

    connect(open_recent_proj_btn, &QPushButton::clicked, this, &WelcomeForm::OpenRecentProjectHandler);
    connect(remove_recent_proj_btn, &QPushButton::clicked, this, &WelcomeForm::RemoveRecentProjectHandler);
    connect(open_browse_proj_btn, &QPushButton::clicked, this, &WelcomeForm::OpenProjectHandler);
    connect(create_proj_btn, &QPushButton::clicked, this, &WelcomeForm::CreateProjectHandler);

    connect(backButton, &QPushButton::clicked, this, &WelcomeForm::L2BackButton);
    connect(createButton, &QPushButton::clicked, this, &WelcomeForm::L2CreateButton);

    connect(RecentProjectsList, &QListView::doubleClicked, this, &WelcomeForm::OpenRecentProjectHandler);
}

WelcomeForm::~WelcomeForm()
{
}

void WelcomeForm::SelectProjectInList(){
    open_recent_proj_btn->setEnabled(true);
    open_recent_proj_btn->setProperty("state", "green");
    open_recent_proj_btn->style()->polish(open_recent_proj_btn);

    remove_recent_proj_btn->setEnabled(true);
    remove_recent_proj_btn->setProperty("state", "red");
    remove_recent_proj_btn->style()->polish(remove_recent_proj_btn);
}

void WelcomeForm::OpenRecentProjectHandler(){
    QModelIndex index = RecentProjectsList->currentIndex();

    if (index.isValid())
        OpenProject(index.data().toString());
}

void WelcomeForm::RemoveRecentProjectHandler(){
    QModelIndex index = RecentProjectsList->currentIndex();

    if (index.isValid()) {
        QString projectPath = index.data(Qt::DisplayRole).toString();
        RecentProjectsList->model()->removeRow(index.row());
        utils::ProjectsHistoryManager::removeProjectFromHistory(projectPath);
    }

    index = RecentProjectsList->currentIndex();
    if (!index.isValid()) {
        open_recent_proj_btn->setEnabled(false);
        open_recent_proj_btn->setProperty("state", QVariant());
        open_recent_proj_btn->style()->polish(open_recent_proj_btn);

        remove_recent_proj_btn->setEnabled(false);
        remove_recent_proj_btn->setProperty("state", QVariant());
        remove_recent_proj_btn->style()->polish(remove_recent_proj_btn);
    }
}


void WelcomeForm::OpenProjectHandler()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Choose Directory"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
    if (dir.isEmpty()) return;
    OpenProject(dir);
}

void WelcomeForm::OpenProject(QString path){
    if (!QDir(path).exists()) return;

    utils::ProjectsHistoryManager::saveProjectsHistory(path);

    this->hide();

    IDEWindow *mw = new IDEWindow(path, nullptr);
    mw->setAttribute(Qt::WA_DeleteOnClose);
    mw->setWindowState(Qt::WindowMaximized);

    connect(mw, &IDEWindow::CloseProject, this, [this, mw]() {
        RecentProjectsList->clearSelection();
        open_recent_proj_btn->setEnabled(false);
        open_recent_proj_btn->setProperty("state", "");
        open_recent_proj_btn->style()->polish(open_recent_proj_btn);
        this->show();
        SetProjectHistoryList();
    });

    mw->show();

}

void WelcomeForm::CreateProjectHandler()
{
    stack->setCurrentIndex(1);
}

void WelcomeForm::L2BackButton()
{
    stack->setCurrentIndex(0);
}

void WelcomeForm::L2CreateButton()
{
    info_label->setVisible(false);
    proj_name_label->setStyleSheet("color: #ffffff;");
    language_label->setStyleSheet("color: #ffffff;");
    path_label->setStyleSheet("color: #ffffff;");

    QString project_name = proj_name_lineEdit->text().trimmed();
    // Check Project Name
    if (project_name.isEmpty()) {
        proj_name_label->setStyleSheet("color: #bf3131;");
        info_label->setText(tr("Please enter project name"));
        info_label->setVisible(true);
        return;
    }

    // Check Directory Path
    QFileInfo dirinfo(path_lineEdit->text());
    if (!dirinfo.exists() || !dirinfo.isDir()) {
        path_label->setStyleSheet("color: #bf3131;");
        info_label->setText(tr("Directory is invalid"));
        info_label->setVisible(true);
        return;
    }

    QString new_project_path = path_lineEdit->text() + "/" + project_name;

    QDir dir;
    if (dir.exists(new_project_path)){
        path_label->setStyleSheet("color: #bf3131;");
        proj_name_label->setStyleSheet("color: #bf3131;");
        info_label->setText(tr("Directory is exists!"));
        info_label->setVisible(true);
        return;
    }

    if (!dir.mkdir(new_project_path)) {
        info_label->setText(tr("Failed to create project directory!"));
        info_label->setVisible(true);
        return;
    }

    IDEWindow *mw = new IDEWindow(new_project_path, nullptr);
    mw->show();
    this->destroy();
}

void WelcomeForm::L2CreateProject(QString name, QString path, QString language){

}


void WelcomeForm::SetProjectHistoryList(){
    const QStringList history = utils::ProjectsHistoryManager::loadProjectsHistory();

    QStringListModel *model = new QStringListModel(this);
    model->setStringList(history);
    RecentProjectsList->setModel(model);
}