#include "AdvancedDialog.h"

#include "../core/HttpClient.h"
#include "../core/Settings.h"
#include "../core/SteamProfile.h"
#include "Theme.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

AdvancedDialog::AdvancedDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Steam Authentication"));
    setWindowIcon(Theme::appIcon());
    setModal(true);
    resize(560, 250);

    auto* root = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    m_secure = new QLineEdit(this);
    m_secure->setEchoMode(QLineEdit::Password);
    m_secure->setPlaceholderText(tr("Paste your steamLoginSecure cookie value"));
    form->addRow(QStringLiteral("steamLoginSecure:"), m_secure);
    root->addLayout(form);

    auto* help = new QLabel(this);
    help->setWordWrap(true);
    help->setText(QStringLiteral(
        "<a href=\"https://github.com/MohamedSubarashi/CopperSteamIdler/wiki#finding-your-steamloginsecure-cookie\">%1</a>")
                      .arg(tr("How do I find this value?")));
    help->setOpenExternalLinks(true);
    root->addWidget(help);

    auto* buttonRow = new QHBoxLayout;
    m_btnView = new QPushButton(tr("View"), this);
    m_btnView->setIcon(Theme::icon(QStringLiteral("eye-2x")));
    m_btnUpdate = new QPushButton(tr("&Update"), this);
    m_btnUpdate->setEnabled(false);
    auto* btnCancel = new QPushButton(tr("&Cancel"), this);
    buttonRow->addWidget(m_btnView);
    buttonRow->addStretch();
    buttonRow->addWidget(m_btnUpdate);
    buttonRow->addWidget(btnCancel);
    root->addLayout(buttonRow);

    m_secure->setText(Settings::instance().steamLoginSecure());

    connect(m_btnView, &QPushButton::clicked, this, &AdvancedDialog::toggleView);
    connect(m_btnUpdate, &QPushButton::clicked, this, &AdvancedDialog::update);
    connect(btnCancel, &QPushButton::clicked, this, &AdvancedDialog::reject);
    connect(m_secure, &QLineEdit::textChanged, this, &AdvancedDialog::markDirty);

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);

    Theme::fitToScreen(this);
}

void AdvancedDialog::setMasked(bool masked)
{
    m_masked = masked;
    m_secure->setEchoMode(masked ? QLineEdit::Password : QLineEdit::Normal);
}

void AdvancedDialog::toggleView()
{
    setMasked(!m_masked);
}

void AdvancedDialog::markDirty()
{
    m_btnUpdate->setEnabled(true);
}

void AdvancedDialog::update()
{
    Settings& s = Settings::instance();
    s.setSteamLoginSecure(m_secure->text().trimmed());

    const QString id = SteamProfile::steamId();
    if (!id.isEmpty())
        s.setMyProfileURL(QStringLiteral("https://steamcommunity.com/profiles/%1").arg(id));

    HttpClient::instance().refreshCookies();

    m_btnUpdate->setText(tr("Validating..."));
    m_btnUpdate->setEnabled(false);

    if (!id.isEmpty() && HttpClient::instance().isLogined(s.myProfileURL())) {
        s.save();
        QDialog::accept();
        return;
    }

    m_btnUpdate->setText(tr("&Update"));
    QMessageBox::warning(this, QStringLiteral("Copper Steam Idler"),
                         tr("The value you entered isn't valid. Please try again."));
    m_secure->clear();
    setMasked(true);
}
