#include "QrLoginDialog.h"

#include "../core/Logger.h"
#include "../core/Settings.h"
#include "QrImage.h"
#include "Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// Steam expires the challenge after a few minutes; stop waiting before the
// server starts answering with errors.
constexpr int kChallengeLifetimeMs = 4 * 60 * 1000;

constexpr int kQrPixels = 320;

} // namespace

QrLoginDialog::QrLoginDialog(QWidget* parent)
    : QDialog(parent)
    , m_poll(new QTimer(this))
{
    setWindowTitle(tr("Link Steam Account"));
    setWindowIcon(Theme::appIcon());
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    auto* title = new QLabel(tr("Farm cards without the Steam client"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);

    auto* info = new QLabel(
        tr("Scan this code with the Steam mobile app (Steam > Sign in with QR code) "
           "and approve the sign-in. No password is stored on this computer."),
        this);
    info->setWordWrap(true);
    root->addWidget(info);

    m_qr = new QLabel(this);
    m_qr->setAlignment(Qt::AlignCenter);
    m_qr->setMinimumSize(kQrPixels + 8, kQrPixels + 8);
    m_qr->setFrameShape(QFrame::NoFrame);
    root->addWidget(m_qr, 1);

    auto* linkRow = new QHBoxLayout;
    m_link = new QLineEdit(this);
    m_link->setReadOnly(true);
    m_link->setPlaceholderText(tr("Waiting for Steam..."));
    m_btnCopy = new QPushButton(tr("&Copy"), this);
    linkRow->addWidget(m_link, 1);
    linkRow->addWidget(m_btnCopy);
    root->addLayout(linkRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    m_btnRetry = new QPushButton(tr("New Code"), this);
    m_btnUnlink = new QPushButton(tr("&Unlink"), this);
    m_btnClose = new QPushButton(tr("&Close"), this);
    m_btnRetry->setEnabled(false);
    buttons->addWidget(m_btnRetry);
    buttons->addWidget(m_btnUnlink);
    buttons->addStretch();
    buttons->addWidget(m_btnClose);
    root->addLayout(buttons);

    m_poll->setSingleShot(false);
    connect(m_poll, &QTimer::timeout, this, &QrLoginDialog::poll);
    connect(m_btnRetry, &QPushButton::clicked, this, &QrLoginDialog::beginChallenge);
    connect(m_btnCopy, &QPushButton::clicked, this, &QrLoginDialog::copyLink);
    connect(m_btnUnlink, &QPushButton::clicked, this, &QrLoginDialog::unlink);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);

    m_btnUnlink->setEnabled(Settings::instance().hasClientToken());

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);

    Theme::fitToScreen(this);

    // Start the (blocking) request only once the event loop runs, so the
    // dialog is already on screen while Steam is contacted.
    QTimer::singleShot(0, this, &QrLoginDialog::beginChallenge);
}

void QrLoginDialog::beginChallenge()
{
    stopPolling();
    m_btnRetry->setEnabled(false);
    m_qr->clear();
    m_link->clear();
    setStatus(tr("Contacting Steam..."));

    SteamAuth::QrChallenge challenge;
    QString error;
    if (!SteamAuth::beginQrChallenge(&challenge, &error)) {
        setStatus(error);
        m_btnRetry->setEnabled(true);
        return;
    }

    showChallenge(challenge);

    m_elapsed.restart();
    m_poll->start(challenge.intervalMs);
}

void QrLoginDialog::showChallenge(const SteamAuth::QrChallenge& challenge)
{
    m_challenge = challenge;
    m_link->setText(challenge.challengeUrl);
    m_link->setCursorPosition(0);

    // Render whole pixels per module so the code stays crisp and scannable;
    // honour the screen's pixel ratio and only shrink if a long link makes the
    // code larger than the dialog allows.
    const qreal dpr = devicePixelRatioF();
    const int modulePixels = qMax(4, qRound(8 * dpr));
    QPixmap pixmap = QrImage::pixmap(challenge.challengeUrl, modulePixels, 4);
    if (pixmap.isNull()) {
        setStatus(tr("Steam sent a code that could not be displayed. Try a new code."));
        m_btnRetry->setEnabled(true);
        return;
    }
    pixmap.setDevicePixelRatio(dpr);
    if (pixmap.width() / dpr > kQrPixels)
        pixmap = pixmap.scaled(kQrPixels, kQrPixels, Qt::KeepAspectRatio,
                               Qt::FastTransformation);
    m_qr->setPixmap(pixmap);

    setStatus(tr("Waiting for approval on your phone..."));
}

void QrLoginDialog::poll()
{
    if (m_challenge.clientId.isEmpty()) {
        stopPolling();
        return;
    }

    const QString shownUrl = m_challenge.challengeUrl;

    SteamAuth::QrApproval approval;
    QString error;
    switch (SteamAuth::pollQrChallenge(&m_challenge, &approval, &error)) {
    case SteamAuth::QrPoll::Pending:
        if (m_elapsed.elapsed() >= kChallengeLifetimeMs) {
            stopPolling();
            setStatus(tr("This code expired. Click \"New Code\" to try again."));
            m_btnRetry->setEnabled(true);
            return;
        }
        // Steam sometimes reissues the code while it is on screen; show the
        // current one instead of a link that no longer works.
        if (m_challenge.challengeUrl != shownUrl)
            showChallenge(m_challenge);
        return;

    case SteamAuth::QrPoll::Approved: {
        stopPolling();
        m_btnRetry->setEnabled(true);
        m_btnUnlink->setEnabled(true);
        const QString who = approval.accountName.isEmpty()
                                ? tr("this account")
                                : approval.accountName;
        setStatus(tr("Signed in to %1. Card farming can now run without the Steam client.")
                      .arg(who));
        m_qr->clear();
        m_link->clear();
        m_btnClose->setDefault(true);
        m_btnClose->setFocus();
        QTimer::singleShot(1200, this, &QDialog::accept);
        return;
    }

    case SteamAuth::QrPoll::Failed:
        stopPolling();
        if (!error.isEmpty())
            Logger::log(QStringLiteral("SteamAuth: QR poll failed: %1").arg(error));
        setStatus(tr("Could not finish linking with Steam. Click \"New Code\" to try again."));
        m_btnRetry->setEnabled(true);
        return;
    }
}

void QrLoginDialog::copyLink()
{
    if (m_link->text().isEmpty())
        return;

    QApplication::clipboard()->setText(m_link->text());
    setStatus(tr("Link copied. You can also open it in the Steam app on your phone."));
}

void QrLoginDialog::unlink()
{
    if (QMessageBox::question(this, QStringLiteral("Copper Steam Idler"),
                              tr("Unlink this account? Card farming without the Steam client "
                                 "will stop working until you link it again."))
        != QMessageBox::Yes)
        return;

    SteamAuth::forgetClientToken();
    m_btnUnlink->setEnabled(false);
    stopPolling();
    m_qr->clear();
    m_link->clear();
    m_btnRetry->setEnabled(true);
    setStatus(tr("The account is no longer linked. Scan a new code to link it again."));
}

void QrLoginDialog::stopPolling()
{
    m_poll->stop();
    m_challenge = SteamAuth::QrChallenge();
}

void QrLoginDialog::setStatus(const QString& text)
{
    m_status->setText(text);
}
