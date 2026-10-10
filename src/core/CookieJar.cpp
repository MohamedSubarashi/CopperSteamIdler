#include "CookieJar.h"

#include "Settings.h"

CookieJar::CookieJar(QObject* parent)
    : QNetworkCookieJar(parent)
{
}

QNetworkCookie CookieJar::makeCookie(const QString& name, const QString& value, const QString& domain)
{
    QNetworkCookie cookie(name.toUtf8(), value.toUtf8());
    cookie.setDomain(domain);
    cookie.setPath(QStringLiteral("/"));
    return cookie;
}

QString CookieJar::steamLoginSecureValue()
{
    return Settings::instance().steamLoginSecure();
}

void CookieJar::refreshFromSettings()
{
    QList<QNetworkCookie> cookies;

    // steamLoginSecure alone authenticates the community GET requests we make.
    const QString secure = Settings::instance().steamLoginSecure();
    if (!secure.isEmpty())
        cookies.append(makeCookie(QStringLiteral("steamLoginSecure"), secure));

    setAllCookies(cookies);
}

void CookieJar::clearCookies()
{
    setAllCookies({});
}
