#include "BlacklistDialog.h"

#include "../core/Settings.h"
#include "Theme.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

BlacklistDialog::BlacklistDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage Copper Steam Idler Blacklist"));
    setWindowIcon(Theme::appIcon());
    setModal(true);

    auto* root = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    m_list->setSortingEnabled(true);
    m_list->addItems(Settings::instance().blacklist());
    m_list->sortItems();
    root->addWidget(m_list);

    auto* addGroup = new QGroupBox(tr("Add Game to Blacklist"), this);
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

    connect(btnAdd, &QPushButton::clicked, this, &BlacklistDialog::add);
    connect(m_appId, &QLineEdit::returnPressed, this, &BlacklistDialog::add);
    connect(btnRemove, &QPushButton::clicked, this, &BlacklistDialog::removeSelected);
    connect(btnSave, &QPushButton::clicked, this, &BlacklistDialog::save);

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);

    Theme::fitToScreen(this);
}

void BlacklistDialog::add()
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

void BlacklistDialog::removeSelected()
{
    qDeleteAll(m_list->selectedItems());
}

void BlacklistDialog::save()
{
    QStringList items;
    for (int i = 0; i < m_list->count(); ++i)
        items.append(m_list->item(i)->text());
    Settings::instance().setBlacklist(items);
    Settings::instance().save();
    accept();
}
