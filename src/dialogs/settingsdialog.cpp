#include "settingsdialog.h"

#include <QBoxLayout>
#include <QColor>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QHash>
#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTreeWidget>

#include "core/settings/appsettings.h"
#include "core/settings/settingspage.h"
#include "core/settings/settingsregistry.h"

namespace {
class SettingsCategoriesTree : public QTreeWidget
{
public:
    explicit SettingsCategoriesTree(QWidget* parent = nullptr)
        : QTreeWidget(parent)
    {
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        auto* item = itemAt(event->position().toPoint());
        if (item && !item->parent() && item->childCount() > 0) {
            item->setExpanded(!item->isExpanded());
            event->accept();
            return;
        }
        QTreeWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        auto* item = itemAt(event->position().toPoint());
        if (item && !item->parent() && item->childCount() > 0) {
            event->accept();
            return;
        }
        QTreeWidget::mouseDoubleClickEvent(event);
    }
};
}

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName("settingsDialog");
    setWindowTitle(tr("Settings"));
    setModal(true);
    setMinimumSize(780, 560);
    setSizeGripEnabled(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName("settingsHeader");
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(20, 18, 20, 14);
    auto* title = new QLabel(tr("Settings"), header);
    title->setObjectName("settingsDialogTitle");
    headerLayout->addWidget(title);
    root->addWidget(header);

    auto* contentWidget = new QWidget(this);
    auto* content = new QHBoxLayout(contentWidget);
    content->setContentsMargins(20, 0, 20, 0);
    content->setSpacing(16);
    auto* navigation = new QFrame(contentWidget);
    navigation->setObjectName("settingsNavigation");
    navigation->setFixedWidth(205);
    auto* navigationLayout = new QVBoxLayout(navigation);
    navigationLayout->setContentsMargins(0, 12, 12, 12);
    navigationLayout->setSpacing(8);
    auto* navigationTitle = new QLabel(tr("Categories"), navigation);
    navigationTitle->setObjectName("settingsNavigationTitle");
    m_categories = new SettingsCategoriesTree(navigation);
    m_categories->setObjectName("settingsCategories");
    m_categories->setHeaderHidden(true);
    m_categories->setIndentation(12);
    m_categories->setRootIsDecorated(true);
    m_categories->setExpandsOnDoubleClick(false);
    m_categories->setItemsExpandable(true);
    navigationLayout->addWidget(navigationTitle);
    navigationLayout->addWidget(m_categories, 1);

    auto* pageArea = new QFrame(contentWidget);
    pageArea->setObjectName("settingsPageArea");
    auto* pageLayout = new QVBoxLayout(pageArea);
    pageLayout->setContentsMargins(4, 14, 0, 12);
    pageLayout->setSpacing(12);
    m_pageTitle = new QLabel(pageArea);
    m_pageTitle->setObjectName("settingsPageTitle");
    m_pagesWidget = new QStackedWidget(pageArea);
    pageLayout->addWidget(m_pageTitle);
    auto* pageSeparator = new QFrame(pageArea);
    pageSeparator->setObjectName("settingsPageSeparator");
    pageSeparator->setFrameShape(QFrame::HLine);
    pageLayout->addWidget(pageSeparator);
    pageLayout->addWidget(m_pagesWidget, 1);
    content->addWidget(navigation, 0);
    content->addWidget(pageArea, 1);
    root->addWidget(contentWidget, 1);

    auto* footer = new QFrame(this);
    footer->setObjectName("settingsFooter");
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(20, 12, 20, 14);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        footer
    );
    buttons->setObjectName("settingsButtonBox");
    auto* importButton = new QPushButton(tr("Import…"), footer);
    auto* exportButton = new QPushButton(tr("Export…"), footer);
    auto* okButton = buttons->button(QDialogButtonBox::Ok);
    importButton->setObjectName("settingsSecondaryButton");
    exportButton->setObjectName("settingsSecondaryButton");
    okButton->setObjectName("settingsAcceptButton");
    okButton->setDefault(true);
    footerLayout->addWidget(importButton);
    footerLayout->addWidget(exportButton);
    footerLayout->addStretch(1);
    footerLayout->addWidget(buttons);
    root->addWidget(footer);

    connect(m_categories, &QTreeWidget::currentItemChanged, this, [this]() { showSelectedPage(); });
    connect(importButton, &QPushButton::clicked, this, &SettingsDialog::onImportIni);
    connect(exportButton, &QPushButton::clicked, this, &SettingsDialog::onExportIni);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadPages();
}

void SettingsDialog::loadPages()
{
    QHash<QString, QTreeWidgetItem*> categories;
    for (const auto& descriptor : SettingsRegistry::instance().pages()) {
        auto* category = categories.value(descriptor.categoryId);
        if (!category) {
            category = new QTreeWidgetItem(m_categories, {descriptor.categoryTitle()});
            category->setFlags(category->flags() & ~Qt::ItemIsSelectable);
            QFont categoryFont = category->font(0);
            categoryFont.setBold(true);
            if (categoryFont.pointSizeF() > 1.0)
                categoryFont.setPointSizeF(categoryFont.pointSizeF() - 1.0);
            category->setFont(0, categoryFont);
            category->setForeground(0, QColor(QStringLiteral("#9D9D9D")));
            category->setExpanded(true);
            categories.insert(descriptor.categoryId, category);
        }

        auto* page = descriptor.createPage(m_pagesWidget);
        page->load();
        m_pages.append(page);

        auto* scroll = new QScrollArea(m_pagesWidget);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(page);
        const int pageIndex = m_pagesWidget->addWidget(scroll);

        auto* item = new QTreeWidgetItem(category, {descriptor.pageTitle()});
        item->setData(0, Qt::UserRole, pageIndex);
    }

    m_categories->expandAll();
    if (m_categories->topLevelItemCount() > 0) {
        auto* firstCategory = m_categories->topLevelItem(0);
        if (firstCategory->childCount() > 0)
            m_categories->setCurrentItem(firstCategory->child(0));
    }
}

void SettingsDialog::showSelectedPage()
{
    auto* item = m_categories->currentItem();
    if (!item || !item->parent())
        return;
    m_pagesWidget->setCurrentIndex(item->data(0, Qt::UserRole).toInt());
    m_pageTitle->setText(item->text(0));
}

void SettingsDialog::onExportIni()
{
    const QString file = QFileDialog::getSaveFileName(this, tr("Export settings"), QString(), tr("INI files (*.ini)"));
    if (file.isEmpty())
        return;

    QString error;
    if (!AppSettings::exportToIni(file, &error)) {
        QMessageBox::warning(this, tr("Export failed"), error);
        return;
    }
    QMessageBox::information(this, tr("Export"), tr("Settings exported to:\n%1").arg(file));
}

void SettingsDialog::onImportIni()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Import settings"), QString(), tr("INI files (*.ini)"));
    if (file.isEmpty())
        return;

    QString error;
    if (!AppSettings::importFromIni(file, &error)) {
        QMessageBox::warning(this, tr("Import failed"), error);
        return;
    }
    for (auto* page : m_pages)
        page->load();
    QMessageBox::information(this, tr("Import"), tr("Settings imported from:\n%1").arg(file));
}

void SettingsDialog::onAccept()
{
    for (const auto* page : m_pages) {
        QString error;
        if (!page->validate(&error)) {
            QMessageBox::warning(this, tr("Invalid settings"), error);
            return;
        }
    }
    for (auto* page : m_pages)
        page->apply();
    accept();
}
