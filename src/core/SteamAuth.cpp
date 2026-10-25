#include "SteamAuth.h"

#include "HttpClient.h"
#include "Logger.h"
#include "Settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace {

QString decodeSessionValue(const QString& value)
{
    return QUrl::fromPercentEncoding(value.toUtf8());
}

} // namespace

quint64 SteamAuth::steamIdFromSession()
{
    const QString raw = Settings::instance().steamLoginSecure();
    if (raw.isEmpty())
        return 0;

    QString decoded = decodeSessionValue(raw);
    const int pipe = decoded.indexOf(QLatin1Char('|'));
    if (pipe >= 0)
        decoded = decoded.left(pipe);
    return decoded.trimmed().toULongLong();
}

SteamAuth::Token SteamAuth::fetchToken()
{
    Token token;

    if (!Settings::instance().hasSession()) {
        token.error = QStringLiteral("Not signed in to Steam.");
        return token;
    }

    token.steamId = steamIdFromSession();

    // Steam mints a short-lived web-session token here. The response also
    // reports logged_in, so an expired session is distinguishable from a
    // network failure.
    bool ok = false;
    int status = 0;
    const QString body = HttpClient::instance().getSync(
        QUrl(QStringLiteral("https://steamcommunity.com/chat/clientjstoken")),
        20000, &ok, &status);

    if (ok) {
        const QJsonObject json = QJsonDocument::fromJson(body.toUtf8()).object();
        const QString jwt = json.value(QStringLiteral("token")).toString();
        const quint64 jsonSteamId =
            json.value(QStringLiteral("steamid")).toString().toULongLong();

        if (!jwt.isEmpty()) {
            token.accessToken = jwt;
            if (jsonSteamId != 0)
                token.steamId = jsonSteamId;
            return token;
        }

        if (json.contains(QStringLiteral("logged_in"))
            && !json.value(QStringLiteral("logged_in")).toBool()) {
            token.error = QStringLiteral("The saved Steam session has expired; sign in again.");
            return token;
        }
    }

    // Fallback: the JWT part of steamLoginSecure. Shaped like
    // "<steamid>||<jwt>", so keep everything after the last separator.
    const QString raw = Settings::instance().steamLoginSecure();
    if (!raw.isEmpty()) {
        const QString decoded = decodeSessionValue(raw);
        const int pipe = decoded.lastIndexOf(QLatin1Char('|'));
        if (pipe >= 0) {
            const QString jwt = decoded.mid(pipe + 1).trimmed();
            // Steam tokens are JWTs: header.payload.signature, base64url encoded.
            if (jwt.startsWith(QLatin1String("eyJ")) && jwt.count(QLatin1Char('.')) == 2) {
                token.accessToken = jwt;
                return token;
            }
        }
    }

    if (token.error.isEmpty()) {
        token.error = ok ? QStringLiteral("Steam returned no access token.")
                         : QStringLiteral("Could not reach Steam (HTTP %1).").arg(status);
    }
    return token;
}
