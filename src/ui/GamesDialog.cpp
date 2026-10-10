#include "GamesDialog.h"

#include "../core/HttpClient.h"
#include "../core/Settings.h"
#include "../core/SteamHtml.h"
#include "../core/SteamProfile.h"
#include "Theme.h"

#include <QApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

GamesDialog::GamesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select Games"));
    setWindowIcon(Theme::appIcon());
    setModal(true);
    resize(460, 520);

    auto* root = new QVBoxLayout(this);

    auto* modeGroup = new QGroupBox(tr("Games to idle"), this);
    auto* modeLayout = new QVBoxLayout(modeGroup);
    m_radAll = new QRadioButton(tr("All games with card drops remaining"), modeGroup);
    m_radPicked = new QRadioButton(tr("Only the picked games below"), modeGroup);
    m_radLibrary = new QRadioButton(tr("My whole Steam library"), modeGroup);
    modeLayout->addWidget(m_radAll);
    modeLayout->addWidget(m_radPicked);
    modeLayout->addWidget(m_radLibrary);
    root->addWidget(modeGroup);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    root->addWidget(m_list, 1);

    auto* addGroup = new QGroupBox(tr("Add a game"), this);
    auto* addRow = new QHBoxLayout(addGroup);
    m_appId = new QLineEdit(addGroup);
    m_appId->setPlaceholderText(tr("Steam App ID"));
    auto* btnAdd = new QPushButton(tr("&Add"), addGroup);
    auto* btnLoad = new QPushButton(tr("Load my library"), addGroup);
    m_btnRemove = new QPushButton(tr("Remove"), addGroup);
    addRow->addWidget(m_appId, 1);
    addRow->addWidget(btnAdd);
    addRow->addWidget(btnLoad);
    addRow->addWidget(m_btnRemove);
    root->addWidget(addGroup);

    auto* hint = new QLabel(tr("Check the games to idle in \"Only the picked games\" mode."),
                            this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* buttonRow = new QHBoxLayout;
    auto* btnOk = new QPushButton(tr("&Accept"), this);
    auto* btnCancel = new QPushButton(tr("&Cancel"), this);
    buttonRow->addStretch();
    buttonRow->addWidget(btnOk);
    buttonRow->addWidget(btnCancel);
    root->addLayout(buttonRow);

    connect(btnAdd, &QPushButton::clicked, this, &GamesDialog::addManual);
    connect(m_appId, &QLineEdit::returnPressed, this, &GamesDialog::addManual);
    connect(btnLoad, &QPushButton::clicked, this, &GamesDialog::loadLibrary);
    connect(m_btnRemove, &QPushButton::clicked, this, &GamesDialog::removeSelected);
    connect(btnOk, &QPushButton::clicked, this, [this]() { save(); });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    // Populate from the stored picked list first.
    const Settings& s = Settings::instance();
    for (const QString& id : s.whitelist())
        addEntry(id, QStringLiteral("App ID: %1").arg(id), true);

    const QString mode = s.selectionMode();
    if (mode == QLatin1String("picked"))
        m_radPicked->setChecked(true);
    else if (mode == QLatin1String("library"))
        m_radLibrary->setChecked(true);
    else
        m_radAll->setChecked(true);

    if (s.customTheme())
        Theme::applyWidgetTheme(this, true);

    Theme::fitToScreen(this);
}

void GamesDialog::addEntry(const QString& appId, const QString& name, bool checked)
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == appId) {
            m_list->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            return;
        }
    }
    auto* item = new QListWidgetItem(name, m_list);
    item->setData(Qt::UserRole, appId);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

void GamesDialog::addManual()
{
    bool ok = false;
    const int appId = m_appId->text().trimmed().toInt(&ok);
    if (!ok || appId <= 0)
        return;
    addEntry(QString::number(appId), QStringLiteral("App ID: %1").arg(appId), true);
    m_appId->clear();
}

void GamesDialog::removeSelected()
{
    const auto items = m_list->selectedItems();
    for (QListWidgetItem* item : items)
        delete item;
}

void GamesDialog::loadLibrary()
{
    const QString id = SteamProfile::steamId();
    if (id.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QUrl url(QStringLiteral(
        "https://steamcommunity.com/profiles/%1/games?tab=all&xml=1").arg(id));
    bool ok = false;
    const QString xml = HttpClient::instance().getSync(url, 30000, &ok);
    QApplication::restoreOverrideCursor();
    if (!ok || xml.isEmpty())
        return;

    const QStringList picked = Settings::instance().whitelist();
    for (const SteamHtml::OwnedGame& game : SteamHtml::parseOwnedGames(xml))
        addEntry(game.appId, game.name, picked.contains(game.appId));
}

void GamesDialog::save()
{
    Settings& s = Settings::instance();

    if (m_radLibrary->isChecked())
        s.setSelectionMode(QStringLiteral("library"));
    else if (m_radPicked->isChecked())
        s.setSelectionMode(QStringLiteral("picked"));
    else
        s.setSelectionMode(QStringLiteral("all"));

    QStringList picked;
    for (int i = 0; i < m_list->count(); ++i) {
        const QListWidgetItem* item = m_list->item(i);
        if (item->checkState() == Qt::Checked)
            picked.append(item->data(Qt::UserRole).toString());
    }
    s.setWhitelist(picked);
    s.save();
    accept();
}
