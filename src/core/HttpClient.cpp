#include "HttpClient.h"

#include "CookieJar.h"
#include "Logger.h"
#include "Settings.h"

#include <QDateTime>
#include <QEventLoop>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace {

QString steamIdFromCookie()
{
    QString secure = Settings::instance().steamLoginSecure();
    if (secure.isEmpty())
        return QString();
    secure = QUrl::fromPercentEncoding(secure.toUtf8());
    const int pipe = secure.indexOf(QLatin1Char('|'));
    if (pipe >= 0)
        secure = secure.left(pipe);
    return secure.trimmed();
}

} // namespace

HttpClient::HttpClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_jar(new CookieJar(this))
{
    m_nam->setCookieJar(m_jar);
    m_jar->setParent(m_nam);

    // Restore the saved Steam session up front. Without this the cookie jar
    // starts empty on every launch, so the first badge fetch is served to a
    // signed-out visitor: no card-drop data is returned, nothing is found to
    // idle, and the app looks like it never re-analysed the account.
    refreshCookies();
    if (Settings::instance().hasSession())
        Logger::log(QStringLiteral("Restored the saved Steam session from settings."));
}

HttpClient& HttpClient::instance()
{
    static HttpClient client;
    return client;
}

QNetworkReply* HttpClient::get(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                     "AppleWebKit/537.36 (KHTML, like Gecko) "
                                     "Chrome/124.0 Safari/537.36 CopperSteamIdler/2.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return m_nam->get(request);
}

void HttpClient::throttle()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 wait = 0;
    if (m_lastRequestMs != 0)
        wait = m_minIntervalMs - (now - m_lastRequestMs);
    if (m_backoffMs > 0)
        wait = qMax<qint64>(wait, m_backoffMs);
    if (wait > 0)
        QThread::msleep(static_cast<unsigned long>(wait));
    m_lastRequestMs = QDateTime::currentMSecsSinceEpoch();
}

void HttpClient::noteRateLimit()
{
    m_rateLimited = true;
    if (m_backoffMs == 0)
        m_backoffMs = 30000;
    else
        m_backoffMs = qMin(m_backoffMs * 2, 15 * 60 * 1000);
    Logger::log(QStringLiteral("Rate limited by Steam; backing off %1 ms").arg(m_backoffMs));
}

void HttpClient::resetBackoff()
{
    m_backoffMs = 0;
    m_rateLimited = false;
}

bool HttpClient::bodyLooksRateLimited(const QString& body)
{
    return body.contains(QLatin1String("too many requests"), Qt::CaseInsensitive)
           || body.contains(QLatin1String("You've made too many requests"), Qt::CaseInsensitive);
}

QString HttpClient::getSync(const QUrl& url, int timeoutMs, bool* ok, int* httpStatus)
{
    if (ok)
        *ok = false;
    if (httpStatus)
        *httpStatus = 0;

    const bool steamHost = url.host().contains(QLatin1String("steamcommunity.com"));
    if (steamHost)
        throttle();

    QNetworkReply* reply = get(url);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (reply->isRunning())
            reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus)
        *httpStatus = status;

    QString body;
    bool success = reply->error() == QNetworkReply::NoError;

    if (success)
        body = QString::fromUtf8(reply->readAll());

    // A rate limited response often still returns HTTP 200 with an error page.
    if (status == 429 || (success && bodyLooksRateLimited(body))) {
        noteRateLimit();
        success = false;
    } else if (success) {
        resetBackoff();
    }

    if (!success && status != 429)
        Logger::log(QStringLiteral("HTTP error for %1: %2").arg(url.toString(), reply->errorString()));

    // Steam invalidates the session by setting steamLoginSecure=deleted.
    const QVariant setCookies = reply->header(QNetworkRequest::SetCookieHeader);
    if (setCookies.isValid()) {
        const QList<QNetworkCookie> cookies =
            setCookies.value<QList<QNetworkCookie>>();
        for (const QNetworkCookie& cookie : cookies) {
            if (cookie.name() == "steamLoginSecure"
                && cookie.value() == QByteArrayLiteral("deleted")) {
                Logger::log(QStringLiteral("Steam invalidated the session cookie; signing out."));
                clearCookies();
                break;
            }
        }
    }

    reply->deleteLater();
    if (ok)
        *ok = success;
    return body;
}

bool HttpClient::isLogined(const QString& profileUrl)
{
    const QString id = steamIdFromCookie();
    if (id.isEmpty())
        return false;

    const QUrl xmlUrl(QStringLiteral("https://steamcommunity.com/profiles/%1/?xml=1").arg(id));
    int status = 0;
    bool ok = false;
    const QString xml = getSync(xmlUrl, 20000, &ok, &status);

    // Network trouble / rate limiting must not be mistaken for "logged out".
    if (status == 429 || isRateLimited())
        return false;
    if (!ok || xml.isEmpty())
        return false;

    static const QRegularExpression id64Re(QStringLiteral("<steamID64>(\\d+)</steamID64>"));
    const QRegularExpressionMatch match = id64Re.match(xml);
    if (match.hasMatch())
        return match.captured(1) == id;

    // Secondary signal for older/edge responses.
    if (profileUrl.isEmpty())
        return false;
    bool pageOk = false;
    const QString page = getSync(QUrl(profileUrl), 20000, &pageOk);
    return pageOk && page.contains(QLatin1String("notification_count"));
}

bool HttpClient::hasValidSession()
{
    if (steamIdFromCookie().isEmpty())
        return false;

    // Every community page carries the real authentication state in its
    // data-userinfo blob ("logged_in":true/false). The public ?xml=1 feed is
    // served to signed-out visitors too, so it can never tell a live session
    // apart from an expired one -- this flag can.
    static const QRegularExpression loggedInRe(
        QStringLiteral("logged_in(?:&quot;|\")\\s*:\\s*(true|false)"));
    int status = 0;
    bool ok = false;
    const QString page = getSync(QUrl(QStringLiteral("https://steamcommunity.com/my/")),
                                 20000, &ok, &status);

    // Ambiguous answers (rate limiting, network trouble, an unexpected page
    // shape) must never sign the user out; only a definite "logged_in":false does.
    if (status == 429 || isRateLimited())
        return true;
    if (!ok || page.isEmpty())
        return true;

    const QRegularExpressionMatch loggedIn = loggedInRe.match(page);
    if (!loggedIn.hasMatch())
        return true;
    return loggedIn.captured(1) == QLatin1String("true");
}

void HttpClient::refreshCookies()
{
    m_jar->refreshFromSettings();
}

void HttpClient::clearCookies()
{
    m_jar->clearCookies();

    Settings& s = Settings::instance();
    s.setSteamLoginSecure(QString());
    s.setMyProfileURL(QString());
    s.save();
}

void HttpClient::adoptWebSession(quint64 steamId, const QString& accessToken)
{
    if (steamId == 0 || accessToken.isEmpty())
        return;

    // Same shape the Steam client writes: "<steamid>||<access token>", encoded
    // so the pipe characters cannot confuse the Cookie header.
    const QString value = QString::fromUtf8(
        QUrl::toPercentEncoding(QStringLiteral("%1||%2").arg(steamId).arg(accessToken)));

    Settings& s = Settings::instance();
    s.setSteamLoginSecure(value);
    s.setMyProfileURL(QStringLiteral("https://steamcommunity.com/profiles/%1").arg(steamId));
    s.save();

    m_jar->refreshFromSettings();

    Logger::log(QStringLiteral("Community session adopted from the linked Steam account"));
}
