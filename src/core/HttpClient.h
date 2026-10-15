#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;
class CookieJar;

// Shared HTTP client with the Steam Community cookie jar attached.
//
// Requests to steamcommunity.com are throttled and backed off when Steam
// reports "too many requests", since hammering the site was a common source of
// login failures and broken idling in the original app.
class HttpClient : public QObject
{
    Q_OBJECT
public:
    static HttpClient& instance();

    QNetworkAccessManager* nam() const { return m_nam; }
    CookieJar* cookieJar() const { return m_jar; }

    // Async request; caller owns the returned reply. Not throttled (used for
    // CDN assets such as game banners).
    QNetworkReply* get(const QUrl& url);

    // Blocking GET with a local event loop and timeout. Throttled for the
    // Steam Community host. Returns the body.
    QString getSync(const QUrl& url, int timeoutMs = 20000, bool* ok = nullptr,
                    int* httpStatus = nullptr);

    // GETs the profile and confirms the signed-in SteamID matches the cookie.
    bool isLogined(const QString& profileUrl);

    // Confirms Steam actually accepts the stored session right now. Unlike
    // isLogined() (which reads a public feed) this reads the community page's
    // logged_in flag, so an expired token is detected instead of silently
    // browsing as a signed-out visitor.
    bool hasValidSession();

    // Sends steamRefresh_steam to Steam and persists a refreshed
    // steamLoginSecure token when one is returned.
    bool refreshLoginToken();

    void clearCookies();
    void refreshCookies();

    bool isRateLimited() const { return m_rateLimited; }
    void resetBackoff();

private:
    explicit HttpClient(QObject* parent = nullptr);

    void throttle();
    void noteRateLimit();
    static bool bodyLooksRateLimited(const QString& body);

    QNetworkAccessManager* m_nam;
    CookieJar* m_jar;

    qint64 m_lastRequestMs = 0;
    int m_minIntervalMs = 350;
    int m_backoffMs = 0;
    bool m_rateLimited = false;
};
