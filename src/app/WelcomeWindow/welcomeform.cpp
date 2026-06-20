#include "welcomeform.h"
#include "app/IDEWindow/idewindow.h"
#include <qboxlayout.h>
#include <qdir.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <QFileInfo>
#include <QListView>
#include <QFileDialog>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QLabel>
#include <QComboBox>
#include <QJsonArray>
#include <qstandardpaths.h>
#include <qstringlistmodel.h>
#include <QPainter>
#include <QStyledItemDelegate>
#include <qtimer.h>

#include "projectshistorymanager.h"

namespace {

constexpr int kSidebarWidth = 240;
constexpr int kProjectItemHeight = 64;

QPushButton* createWelcomeButton(const QString& text, QWidget* parent, const QString& objectName = QString())
{
    auto* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(32);
    button->setMinimumWidth(80);
    if (!objectName.isEmpty())
        button->setObjectName(objectName);
    return button;
}

class RecentProjectDelegate final : public QStyledItemDelegate {
public:
    explicit RecentProjectDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(1, kProjectItemHeight);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect itemRect = option.rect.adjusted(12, 3, -12, -3);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);

        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor("#2e4c82") : QColor("#262626"));
            painter->drawRoundedRect(itemRect, 6, 6);
        }

        const QString path = index.data(Qt::DisplayRole).toString();
        QString name = QFileInfo(path).fileName();
        if (name.isEmpty())
            name = path;
        if (name.isEmpty())
            name = QObject::tr("Untitled");

        const QRect badgeRect(itemRect.left() + 24, itemRect.top() + 17, 24, 24);
        QLinearGradient badgeGradient(badgeRect.topLeft(), badgeRect.bottomRight());
        badgeGradient.setColorAt(0.0, QColor("#a822f5"));
        badgeGradient.setColorAt(1.0, QColor("#7e0bc1"));
        painter->setBrush(badgeGradient);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(badgeRect, 6, 6);

        QFont badgeFont = option.font;
        badgeFont.setPixelSize(14);
        painter->setFont(badgeFont);
        painter->setPen(Qt::white);
        painter->drawText(badgeRect, Qt::AlignCenter, name.left(1).toUpper());

        const int textLeft = badgeRect.right() + 13;
        const int closeWidth = 34;
        const QRect titleRect(textLeft, itemRect.top() + 13, itemRect.width() - textLeft + itemRect.left() - closeWidth, 18);
        const QRect pathRect(textLeft, itemRect.top() + 32, titleRect.width(), 18);

        QFont titleFont = option.font;
        titleFont.setPixelSize(14);
        painter->setFont(titleFont);
        painter->setPen(Qt::white);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, painter->fontMetrics().elidedText(name, Qt::ElideRight, titleRect.width()));

        QFont pathFont = option.font;
        pathFont.setPixelSize(14);
        painter->setFont(pathFont);
        painter->setPen(QColor(255, 255, 255, 128));
        painter->drawText(pathRect, Qt::AlignLeft | Qt::AlignVCenter, painter->fontMetrics().elidedText(path, Qt::ElideMiddle, pathRect.width()));

        const QRect closeRect(itemRect.right() - 36, itemRect.top() + 18, 22, 22);
        painter->setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(closeRect.left() + 6, closeRect.top() + 6, closeRect.right() - 6, closeRect.bottom() - 6);
        painter->drawLine(closeRect.right() - 6, closeRect.top() + 6, closeRect.left() + 6, closeRect.bottom() - 6);

        painter->restore();
    }
};

}

WelcomeForm::WelcomeForm(QWidget *parent)
    : QWidget(parent)
{
    qDebug("WelcomeForm::WelcomeForm(QWidget *parent)");

    this->setWindowTitle("Cremniy");
    this->setMinimumSize(800, 568);
    this->resize(800, 568);
    this->setObjectName("welcomeWindow");

    // Base
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    stack = new QStackedWidget(this);
    mainLayout->addWidget(stack);

    // Page "Welcome"
    QWidget *pageWelcome = new QWidget();
    pageWelcome->setObjectName("welcomePage");
    QHBoxLayout *welcomeLayout = new QHBoxLayout(pageWelcome);
    welcomeLayout->setContentsMargins(0, 0, 0, 0);
    welcomeLayout->setSpacing(0);

    QWidget *sidebar = new QWidget(pageWelcome);
    sidebar->setObjectName("welcomeSidebar");
    sidebar->setFixedWidth(kSidebarWidth);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(12, 12, 12, 12);
    sidebarLayout->setSpacing(12);

    QWidget *brand = new QWidget(sidebar);
    QHBoxLayout *brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(0, 12, 0, 12);
    brandLayout->setSpacing(8);

    QLabel *logo = new QLabel(brand);
    logo->setPixmap(QIcon(":/icons/icon.svg").pixmap(32, 32));
    logo->setFixedSize(32, 32);
    brandLayout->addWidget(logo);

    QVBoxLayout *brandTextLayout = new QVBoxLayout();
    brandTextLayout->setContentsMargins(0, 0, 0, 0);
    brandTextLayout->setSpacing(4);
    QLabel *appName = new QLabel("Cremniy", brand);
    appName->setObjectName("welcomeBrandName");
    QLabel *appVersion = new QLabel("v0.2.0", brand);
    appVersion->setObjectName("welcomeBrandVersion");
    brandTextLayout->addWidget(appName);
    brandTextLayout->addWidget(appVersion);
    brandLayout->addLayout(brandTextLayout);
    brandLayout->addStretch(1);
    sidebarLayout->addWidget(brand);

    QPushButton *projectsNavButton = createWelcomeButton(tr("Projects"), sidebar, "welcomeNavSelected");
    QPushButton *settingsNavButton = createWelcomeButton(tr("Settings"), sidebar, "welcomeNavButton");
    QPushButton *docsNavButton = createWelcomeButton(tr("Docs"), sidebar, "welcomeNavButton");
    sidebarLayout->addWidget(projectsNavButton);
    sidebarLayout->addWidget(settingsNavButton);
    sidebarLayout->addWidget(docsNavButton);
    sidebarLayout->addStretch(1);
    settingsNavButton->setEnabled(false);
    docsNavButton->setEnabled(false);

    QWidget *content = new QWidget(pageWelcome);
    content->setObjectName("welcomeContent");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    QWidget *topBar = new QWidget(content);
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(12, 12, 12, 12);
    topBarLayout->setSpacing(6);
    topBarLayout->addStretch(1);

    QPushButton *create_proj_btn = createWelcomeButton(tr("New Project"), topBar, "welcomeTopButton");
    QPushButton *open_browse_proj_btn = createWelcomeButton(tr("Open"), topBar, "welcomeTopButton");
    topBarLayout->addWidget(create_proj_btn);
    topBarLayout->addWidget(open_browse_proj_btn);
    contentLayout->addWidget(topBar);

    QWidget *projectsPanel = new QWidget(content);
    projectsPanel->setObjectName("welcomeProjectsPanel");
    QVBoxLayout *projectsLayout = new QVBoxLayout(projectsPanel);
    projectsLayout->setContentsMargins(0, 9, 0, 0);
    projectsLayout->setSpacing(0);

    RecentProjectsList = new QListView(projectsPanel);
    RecentProjectsList->setObjectName("welcomeProjectsList");
    projectsLayout->addWidget(RecentProjectsList);
    RecentProjectsList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    RecentProjectsList->setSelectionMode(QAbstractItemView::SingleSelection);
    RecentProjectsList->setMouseTracking(true);
    RecentProjectsList->setItemDelegate(new RecentProjectDelegate(RecentProjectsList));
    RecentProjectsList->viewport()->installEventFilter(this);
    SetProjectHistoryList();
    contentLayout->addWidget(projectsPanel, 1);

    open_recent_proj_btn = new QPushButton(pageWelcome);
    open_recent_proj_btn->hide();
    open_recent_proj_btn->setEnabled(false);

    remove_recent_proj_btn = new QPushButton("×", pageWelcome);
    remove_recent_proj_btn->hide();
    remove_recent_proj_btn->setEnabled(false);

    welcomeLayout->addWidget(sidebar);
    welcomeLayout->addWidget(content, 1);

    stack->addWidget(pageWelcome);

    // Page "Create Project"
    QWidget *pageCreate = new QWidget();
    pageCreate->setObjectName("welcomePage");

    QVBoxLayout *l2 = new QVBoxLayout(pageCreate);
    l2->setContentsMargins(0, 0, 0, 0);
    l2->setSpacing(0);

    QWidget *createFormPanel = new QWidget(pageCreate);
    createFormPanel->setObjectName("welcomeCreateFormPanel");
    QVBoxLayout *createFormPanelLayout = new QVBoxLayout(createFormPanel);
    createFormPanelLayout->setContentsMargins(12, 12, 12, 12);
    createFormPanelLayout->setSpacing(0);

    // --- Grid layout для текста и полей ---
    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->setColumnMinimumWidth(0, 94);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);

    // Первая строка: "Текст" + QLineEdit
    proj_name_label = new QLabel(tr("Project Name:"));
    proj_name_label->setObjectName("welcomeFormLabel");
    proj_name_lineEdit = new QLineEdit();
    proj_name_lineEdit->setObjectName("welcomeProjectInput");
    proj_name_lineEdit->setMinimumHeight(26);
    QRegularExpression re("^[A-Za-z0-9_-]+$");
    QValidator *validator = new QRegularExpressionValidator(re, this);
    proj_name_lineEdit->setValidator(validator);
    grid->addWidget(proj_name_label, 0, 0, Qt::AlignVCenter);
    grid->addWidget(proj_name_lineEdit, 0, 1);

    // Вторая строка: "Текст" + QComboBox
    language_label = new QLabel(tr("Language:"));
    language_label->setObjectName("welcomeFormLabel");
    language_comboBox = new QComboBox();
    language_comboBox->setObjectName("welcomeProjectCombo");
    language_comboBox->setMinimumHeight(26);
    language_comboBox->addItems({"C", "C++", "ASM", "C + ASM", "Custom"});
    language_comboBox->setCurrentText("C++");
    grid->addWidget(language_label, 1, 0, Qt::AlignVCenter);
    grid->addWidget(language_comboBox, 1, 1);

    path_label = new QLabel(tr("Location:"));
    path_label->setObjectName("welcomeFormLabel");
    path_lineEdit = new QLineEdit();
    path_lineEdit->setObjectName("welcomeProjectInput");
    path_lineEdit->setMinimumHeight(26);
    QAction *chooseProjectLocationAction = path_lineEdit->addAction(QIcon(":/icons/welcome-folder.svg"), QLineEdit::TrailingPosition);
    connect(chooseProjectLocationAction, &QAction::triggered, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this,
            tr("Choose Directory"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (!dir.isEmpty())
            path_lineEdit->setText(dir);
    });
    grid->addWidget(path_label, 2, 0, Qt::AlignVCenter);
    grid->addWidget(path_lineEdit, 2, 1);

    // Добавляем grid в основной вертикальный layout
    createFormPanelLayout->addLayout(grid);
    createFormPanelLayout->addStretch(1);
    l2->addWidget(createFormPanel, 1);

    info_label = new QLabel();
    info_label->setObjectName("welcomeCreateInfo");
    info_label->setVisible(false);
    info_label->setAlignment(Qt::AlignCenter);

    // --- Горизонтальный layout для кнопок ---
    QWidget *buttonPanel = new QWidget(pageCreate);
    buttonPanel->setObjectName("welcomeCreateButtonPanel");
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonPanel);
    buttonLayout->setContentsMargins(12, 12, 12, 12);
    buttonLayout->setSpacing(6);
    QPushButton *createButton = createWelcomeButton(tr("Create"), pageCreate, "welcomeTopButton");
    QPushButton *backButton = createWelcomeButton(tr("Cancel"), pageCreate, "welcomeTopButton");

    buttonLayout->addWidget(info_label);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(createButton);
    buttonLayout->addWidget(backButton);

    // Добавляем кнопки в основной вертикальный layout
    l2->addWidget(buttonPanel);

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

    this->setStyleSheet(R"(
        QWidget#welcomeWindow,
        QWidget#welcomePage {
            background: #262626;
        }

        QWidget#welcomeSidebar {
            background: #262626;
        }

        QWidget#welcomeContent {
            background: #262626;
        }

        QWidget#welcomeProjectsPanel {
            background: #1f1f1f;
            border-top-left-radius: 6px;
        }

        QLabel#welcomeBrandName {
            color: #ffffff;
            font-size: 14px;
        }

        QLabel#welcomeBrandVersion {
            color: rgba(255, 255, 255, 128);
            font-size: 10px;
        }

        QPushButton#welcomeNavSelected,
        QPushButton#welcomeNavButton {
            border: none;
            border-radius: 8px;
            color: #ffffff;
            font-size: 14px;
            padding: 12px 24px;
            text-align: left;
        }

        QPushButton#welcomeNavSelected {
            background: #2e4c82;
        }

        QPushButton#welcomeNavButton {
            background: transparent;
        }

        QPushButton#welcomeNavButton:hover {
            background: #333333;
        }

        QPushButton#welcomeNavButton:disabled {
            color: #ffffff;
            background: transparent;
        }

        QPushButton#welcomeTopButton {
            background: #1f1f1f;
            border: none;
            border-radius: 6px;
            color: #ffffff;
            font-size: 14px;
            padding: 6px 12px;
        }

        QPushButton#welcomeTopButton:hover {
            background: #2e4c82;
        }

        QPushButton#welcomeTopButton:disabled {
            color: rgba(255, 255, 255, 90);
            background: #1f1f1f;
        }

        QListView#welcomeProjectsList {
            background: #1f1f1f;
            border: none;
            outline: none;
        }

        QWidget#welcomeCreateFormPanel {
            background: #262626;
        }

        QWidget#welcomeCreateButtonPanel {
            background: #262626;
        }

        QLabel#welcomeFormLabel {
            color: #ffffff;
            font-size: 14px;
            min-width: 94px;
        }

        QLabel#welcomeCreateInfo {
            color: #bf3131;
            font-size: 14px;
            font-weight: bold;
        }

        QLineEdit#welcomeProjectInput,
        QComboBox#welcomeProjectCombo {
            background: #1f1f1f;
            border: none;
            border-radius: 6px;
            color: #ffffff;
            font-size: 14px;
            padding: 6px 12px;
            selection-background-color: #2e4c82;
        }

        QLineEdit#welcomeProjectInput:hover,
        QComboBox#welcomeProjectCombo:hover {
            background: #242424;
        }

        QLineEdit#welcomeProjectInput:focus,
        QComboBox#welcomeProjectCombo:focus {
            border: 1px solid #2e4c82;
        }

        QComboBox#welcomeProjectCombo::drop-down {
            border: none;
            width: 30px;
        }

        QComboBox#welcomeProjectCombo::down-arrow {
            image: url(:/icons/welcome-caret-down.svg);
            width: 14px;
            height: 14px;
        }

        QComboBox#welcomeProjectCombo QAbstractItemView {
            background: #1f1f1f;
            border: 1px solid #333333;
            color: #ffffff;
            selection-background-color: #2e4c82;
        }
    )");
}

WelcomeForm::~WelcomeForm()
{
}

bool WelcomeForm::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == RecentProjectsList->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QModelIndex index = RecentProjectsList->indexAt(mouseEvent->pos());
        if (index.isValid()) {
            const QRect itemRect = RecentProjectsList->visualRect(index).adjusted(12, 3, -12, -3);
            const QRect closeRect(itemRect.right() - 36, itemRect.top() + 18, 22, 22);
            if (closeRect.contains(mouseEvent->pos())) {
                RecentProjectsList->setCurrentIndex(index);
                RemoveRecentProjectHandler();
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void WelcomeForm::SelectProjectInList(){
    open_recent_proj_btn->setEnabled(true);

    remove_recent_proj_btn->setEnabled(true);
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

        remove_recent_proj_btn->setEnabled(false);
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
        Q_UNUSED(mw);
        RecentProjectsList->clearSelection();
        open_recent_proj_btn->setEnabled(false);
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
    const QString normalLabelStyle = "color: #ffffff; font-size: 14px;";
    const QString errorLabelStyle = "color: #bf3131; font-size: 14px;";

    info_label->setVisible(false);
    proj_name_label->setStyleSheet(normalLabelStyle);
    language_label->setStyleSheet(normalLabelStyle);
    path_label->setStyleSheet(normalLabelStyle);

    QString project_name = proj_name_lineEdit->text().trimmed();
    // Check Project Name
    if (project_name.isEmpty()) {
        proj_name_label->setStyleSheet(errorLabelStyle);
        info_label->setText(tr("Please enter project name"));
        info_label->setVisible(true);
        return;
    }

    // Check Directory Path
    QFileInfo dirinfo(path_lineEdit->text());
    if (!dirinfo.exists() || !dirinfo.isDir()) {
        path_label->setStyleSheet(errorLabelStyle);
        info_label->setText(tr("Directory is invalid"));
        info_label->setVisible(true);
        return;
    }

    QString new_project_path = path_lineEdit->text() + "/" + project_name;

    QDir dir;
    if (dir.exists(new_project_path)){
        path_label->setStyleSheet(errorLabelStyle);
        proj_name_label->setStyleSheet(errorLabelStyle);
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
