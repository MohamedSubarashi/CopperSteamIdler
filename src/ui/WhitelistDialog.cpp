#include "WhitelistDialog.h"

#include "../core/Settings.h"
#include "Theme.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

WhitelistDialog::WhitelistDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage Idle Master Whitelist"));
    setWindowIcon(Theme::appIcon());
    setModal(true);

    auto* root = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    m_list->setSortingEnabled(true);
    m_list->addItems(Settings::instance().whitelist());
    m_list->sortItems();
    root->addWidget(m_list);

    auto* addGroup = new QGroupBox(tr("Add Game to Whitelist"), this);
    auto* addRow = new QHBoxLayout(addGroup);
    m_appId = new QLineEdit(addGroup);
    auto* btnAdd = new QPushButton(tr("&Add"), addGroup);
    addRow->addWidget(m_appId);
    addRow->addWidget(btnAdd);
    root->addWidget(addGroup);

    auto* buttonRow = new QHBoxLayout;
    auto* btnRemove = new QPushButton(tr("Remove"), this);
    btnRemove->setIcon(Theme::icon(QStringLiteral("trash-2x")));
    auto* btnSave = new QPushButton(tr("&Save"), this);
    buttonRow->addStretch();
    buttonRow->addWidget(btnRemove);
    buttonRow->addWidget(btnSave);
    root->addLayout(buttonRow);

    connect(btnAdd, &QPushButton::clicked, this, &WhitelistDialog::add);
    connect(m_appId, &QLineEdit::returnPressed, this, &WhitelistDialog::add);
    connect(btnRemove, &QPushButton::clicked, this, &WhitelistDialog::removeSelected);
    connect(btnSave, &QPushButton::clicked, this, &WhitelistDialog::save);

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);

    Theme::fitToScreen(this);
}

void WhitelistDialog::add()
{
    bool ok = false;
    const int appId = m_appId->text().trimmed().toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Copper Steam Idler"),
                             tr("Please enter a valid App ID."));
        return;
    }
    const QString value = QString::number(appId);
    if (!m_list->findItems(value, Qt::MatchExactly).isEmpty())
        return;
    m_list->addItem(value);
    m_appId->clear();
}

void WhitelistDialog::removeSelected()
{
    qDeleteAll(m_list->selectedItems());
}

void WhitelistDialog::save()
{
    QStringList items;
    for (int i = 0; i < m_list->count(); ++i)
        items.append(m_list->item(i)->text());
    Settings::instance().setWhitelist(items);
    Settings::instance().save();
    accept();
}
