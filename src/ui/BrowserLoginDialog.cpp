#include "BrowserLoginDialog.h"

#include "../core/BrowserCookies.h"
#include "../core/Settings.h"
#include "CdpLoginDialog.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const char* kLoginUrl = "https://steamcommunity.com/login/home/?goto=";
}

BrowserLoginDialog::BrowserLoginDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sign in to Steam"));
    setWindowIcon(Theme::appIcon());
    setModal(true);
    resize(540, 240);

    auto* root = new QVBoxLayout(this);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_status->setOpenExternalLinks(true);
    root->addWidget(m_status);
    root->addStretch();

    auto* row = new QHBoxLayout;
    m_btnOpen = new QPushButton(tr("Login with browser"), this);
    m_btnWindow = new QPushButton(tr("Open sign-in window"), this);
    m_btnManual = new QPushButton(tr("Enter cookie manually"), this);
    auto* btnCancel = new QPushButton(tr("&Cancel"), this);
    row->addWidget(m_btnOpen);
    row->addWidget(m_btnWindow);
    row->addStretch();
    row->addWidget(m_btnManual);
    row->addWidget(btnCancel);
    root->addLayout(row);

    connect(m_btnOpen, &QPushButton::clicked, this, &BrowserLoginDialog::openLoginPage);
    connect(m_btnWindow, &QPushButton::clicked, this, &BrowserLoginDialog::openSignInWindow);
    connect(m_btnManual, &QPushButton::clicked, this, &BrowserLoginDialog::useManualEntry);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_poll = new QTimer(this);
    m_poll->setInterval(2000);
    connect(m_poll, &QTimer::timeout, this, &BrowserLoginDialog::pollCookie);

    const BrowserCookies::Browser browser = BrowserCookies::defaultBrowser();
    const bool firefoxReadable =
        browser == BrowserCookies::Browser::Firefox && !BrowserCookies::firefoxProfileDir().isEmpty();

    if (firefoxReadable) {
        setStatus(tr("Click \"Login with browser\" to open the Steam login page in %1. Sign in "
                     "there (including Steam Guard); this dialog closes automatically once the "
                     "session is captured.")
                      .arg(BrowserCookies::browserName(browser)));
    } else if (browser == BrowserCookies::Browser::Chrome || browser == BrowserCookies::Browser::Edge
               || browser == BrowserCookies::Browser::Brave) {
        setStatus(tr("Your default browser is %1, which encrypts its cookies, so the session "
                     "cannot be read automatically. Use \"Open sign-in window\" or enter the "
                     "steamLoginSecure cookie manually.")
                      .arg(BrowserCookies::browserName(browser)));
    } else {
        setStatus(tr("No readable default browser was found. Use \"Open sign-in window\" or "
                     "enter the steamLoginSecure cookie manually."));
    }

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);

    Theme::fitToScreen(this);
}

void BrowserLoginDialog::setStatus(const QString& text)
{
    m_status->setText(text);
}

void BrowserLoginDialog::openLoginPage()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kLoginUrl)));

    // The cookie can only be read back from a readable (Firefox) profile.
    const bool readable =
        BrowserCookies::defaultBrowser() == BrowserCookies::Browser::Firefox
        && !BrowserCookies::firefoxProfileDir().isEmpty();
    if (!readable) {
        setStatus(tr("The page opened in your browser, but this browser's cookies cannot be read "
                     "automatically. Use \"Open sign-in window\" or enter the steamLoginSecure "
                     "cookie manually."));
        return;
    }

    m_elapsedMs = 0;
    if (!m_polling) {
        m_polling = true;
        m_poll->start();
    }
    setStatus(tr("Waiting for sign-in... finish signing in on the Steam page in your browser. "
                 "This dialog closes automatically once the session is captured."));
    pollCookie();
}

void BrowserLoginDialog::pollCookie()
{
    const QString cookie =
        BrowserCookies::firefoxCookie(QStringLiteral("steamLoginSecure"),
                                      QStringLiteral("steamcommunity.com"));
    if (!cookie.isEmpty()) {
        m_cookie = cookie;
        m_poll->stop();
        accept();
        return;
    }

    m_elapsedMs += m_poll->interval();
    if (m_elapsedMs >= 3 * 60 * 1000) {
        setStatus(tr("Still waiting for sign-in. Finish signing in on the Steam page in your "
                     "browser, or use \"Enter cookie manually\"."));
    }
}

void BrowserLoginDialog::openSignInWindow()
{
    CdpLoginDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        const QString cookie = dialog.capturedCookie();
        if (!cookie.isEmpty()) {
            m_cookie = cookie;
            accept();
            return;
        }
    }
    if (dialog.manualRequested())
        useManualEntry();
}

void BrowserLoginDialog::useManualEntry()
{
    m_manualRequested = true;
    reject();
}
