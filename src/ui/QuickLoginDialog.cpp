#include "QuickLoginDialog.h"

#include "../core/Settings.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkCookie>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineView>

QuickLoginDialog::QuickLoginDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sign in to Steam"));
    setWindowIcon(Theme::appIcon());
    setModal(true);
    resize(520, 680);

    auto* root = new QVBoxLayout(this);

    auto* info = new QLabel(tr("Sign in with your Steam account. Copper Steam Idler captures "
                               "the secure login cookie automatically."),
                            this);
    info->setWordWrap(true);
    root->addWidget(info);

    // Off-the-record profile: credentials are not written to a persistent
    // browser cache and are discarded when the dialog closes.
    m_profile = new QWebEngineProfile(this);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    m_view = new QWebEngineView(this);
    m_view->setPage(new QWebEnginePage(m_profile, m_view));
    root->addWidget(m_view, 1);

    m_status = new QLabel(tr("Waiting for sign-in..."), this);
    root->addWidget(m_status);

    auto* row = new QHBoxLayout;
    row->addStretch();
    m_btnDone = new QPushButton(tr("I've signed in"), this);
    auto* btnCancel = new QPushButton(tr("&Cancel"), this);
    row->addWidget(m_btnDone);
    row->addWidget(btnCancel);
    root->addLayout(row);

    connect(m_profile->cookieStore(), &QWebEngineCookieStore::cookieAdded, this,
            &QuickLoginDialog::onCookieAdded);
    connect(m_view, &QWebEngineView::loadFinished, this, &QuickLoginDialog::onLoadFinished);
    connect(m_btnDone, &QPushButton::clicked, this, &QuickLoginDialog::probeCookies);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_view->load(QUrl(QStringLiteral("https://steamcommunity.com/login/home/?goto=")));

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);
}

QuickLoginDialog::~QuickLoginDialog() = default;

void QuickLoginDialog::onCookieAdded(const QNetworkCookie& cookie)
{
    if (cookie.name() != QByteArrayLiteral("steamLoginSecure"))
        return;
    if (!cookie.domain().contains(QLatin1String("steamcommunity.com")))
        return;

    const QString value = QString::fromUtf8(cookie.value());
    if (value.isEmpty() || value == QLatin1String("deleted"))
        return;

    m_cookie = value;
    m_status->setText(tr("Signed in. Finishing up..."));
    // Let a potentially split Set-Cookie settle before accepting.
    QTimer::singleShot(300, this, [this]() {
        if (!m_cookie.isEmpty())
            accept();
    });
}

void QuickLoginDialog::probeCookies()
{
    m_profile->cookieStore()->loadAllCookies();
    if (!m_cookie.isEmpty()) {
        accept();
        return;
    }
    m_status->setText(tr("No session cookie found yet. Complete the sign-in first."));
}

void QuickLoginDialog::onLoadFinished(bool ok)
{
    if (!ok)
        m_status->setText(tr("Could not reach Steam. Check your connection."));
}
