#pragma once

#include <QNetworkCookie>
#include <QNetworkCookieJar>

// Holds the Steam Community login cookies read from Settings.
class CookieJar : public QNetworkCookieJar
{
    Q_OBJECT
public:
    explicit CookieJar(QObject* parent = nullptr);

    // Rebuilds the in-memory cookie list from the stored settings.
    void refreshFromSettings();

    // Empties the cookie list (used on sign-out / invalid session).
    void clearCookies();

    static QNetworkCookie makeCookie(const QString& name, const QString& value,
                                     const QString& domain = QStringLiteral("steamcommunity.com"));

    // The value of steamLoginSecure without URL decoding, as stored.
    static QString steamLoginSecureValue();
};
