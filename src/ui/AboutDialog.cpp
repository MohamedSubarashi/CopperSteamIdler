#include "AboutDialog.h"

#include "../core/Settings.h"
#include "Theme.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFont>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("&About"));
    setWindowIcon(Theme::appIcon());
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setAlignment(Qt::AlignHCenter);

    auto* logo = new QLabel(this);
    logo->setPixmap(QPixmap(QStringLiteral(":/app.png"))
                        .scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignHCenter);
    root->addWidget(logo);

    auto* title = new QLabel(QStringLiteral("Copper Steam Idler"), this);
    title->setAlignment(Qt::AlignHCenter);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto* subtitle = new QLabel(tr("A Qt port of Idle Master Extended"), this);
    subtitle->setAlignment(Qt::AlignHCenter);
    root->addWidget(subtitle);

    auto* credits = new QLabel(tr("Originally by jshackles and JonasNilson"), this);
    credits->setAlignment(Qt::AlignHCenter);
    root->addWidget(credits);

    auto* developer = new QLabel(
        tr("Developed by <a href=\"https://github.com/MohamedSubarashi\">MohamedSubarashi</a>"), this);
    developer->setOpenExternalLinks(true);
    developer->setAlignment(Qt::AlignHCenter);
    root->addWidget(developer);

    auto* group = new QLabel(
        tr("Steam group: <a href=\"https://steamcommunity.com/groups/coppersteamidler\">Copper Steam Idler</a>"),
        this);
    group->setOpenExternalLinks(true);
    group->setAlignment(Qt::AlignHCenter);
    root->addWidget(group);

    auto* version = new QLabel(this);
    version->setText(QStringLiteral("<a href=\"https://github.com/MohamedSubarashi/CopperSteamIdler/releases\">"
                                    "Copper Steam Idler v%1</a>")
                         .arg(QApplication::applicationVersion()));
    version->setOpenExternalLinks(true);
    version->setAlignment(Qt::AlignHCenter);
    root->addWidget(version);

    root->addSpacing(8);
    auto* btnDonate = new QPushButton(tr("&Donate"), this);
    connect(btnDonate, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://ko-fi.com/mohamedsubarashi")));
    });
    root->addWidget(btnDonate);

    auto* btnOk = new QPushButton(tr("&OK"), this);
    connect(btnOk, &QPushButton::clicked, this, &AboutDialog::accept);
    root->addWidget(btnOk);

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);
}
