#include "SteamAuth.h"

#include "HttpClient.h"
#include "Logger.h"
#include "Settings.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {

const char* const kAuthBase =
    "https://api.steampowered.com/IAuthenticationService/";

QString decodeSessionValue(const QString& value)
{
    return QUrl::fromPercentEncoding(value.toUtf8());
}

// Blocking form POST against the Steam Web API.
QString postForm(const QUrl& url, const QUrlQuery& form, int timeoutMs, bool* ok, int* httpStatus)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("CopperSteamIdler/2.0"));

    QNetworkAccessManager* nam = HttpClient::instance().nam();
    QNetworkReply* reply = nam->post(request, form.toString(QUrl::FullyEncoded).toUtf8());

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const bool good = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();

    if (httpStatus)
        *httpStatus = status;
    if (ok)
        *ok = good && status >= 200 && status < 300;

    return QString::fromUtf8(payload);
}

QJsonObject decodeJwtPayload(const QString& jwt)
{
    const QStringList parts = jwt.split(QLatin1Char('.'));
    if (parts.size() != 3)
        return {};

    QByteArray payload = parts.at(1).toUtf8();
    payload.replace('-', '+');
    payload.replace('_', '/');
    while (payload.size() % 4 != 0)
        payload.append('=');

    return QJsonDocument::fromJson(QByteArray::fromBase64(payload)).object();
}

// Speaks to one IAuthenticationService method and returns its "response" object.
QJsonObject callAuth(const QString& method, const QUrlQuery& form, bool* ok, int* status)
{
    const QString body = postForm(QUrl(QString::fromLatin1(kAuthBase) + method),
                                  form, 25000, ok, status);
    const QJsonObject root = QJsonDocument::fromJson(body.toUtf8()).object();
    return root.value(QStringLiteral("response")).toObject();
}

// Steam's JSON encodes 64-bit ids as strings, but tolerate numbers too.
QString jsonIdString(const QJsonObject& object, const char* key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(static_cast<quint64>(value.toDouble()));
    return QString();
}

} // namespace

quint64 SteamAuth::steamIdFromToken(const QString& jwt)
{
    return decodeJwtPayload(jwt).value(QStringLiteral("sub")).toString().toULongLong();
}

qint64 SteamAuth::tokenExpiry(const QString& jwt)
{
    return static_cast<qint64>(decodeJwtPayload(jwt).value(QStringLiteral("exp")).toDouble());
}

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

bool SteamAuth::hasClientToken()
{
    return !Settings::instance().clientRefreshToken().isEmpty();
}

void SteamAuth::forgetClientToken()
{
    Settings& s = Settings::instance();
    s.setClientRefreshToken(QString());
    s.setClientAccessToken(QString());
    s.setClientAccountName(QString());
}

bool SteamAuth::beginQrChallenge(QrChallenge* challenge, QString* error)
{
    if (!challenge)
        return false;

    // Mirrors node-steam-session's SteamClient QR request exactly: no
    // website_id (Steam derives it from the platform type) and a desktop
    // gaming_device_type. Deviating from this makes Steam issue a token that is
    // not accepted for CM logon.
    const QString device = QStringLiteral(
        "{\"device_friendly_name\":\"Copper Steam Idler\","
        "\"platform_type\":1,\"os_type\":20,\"gaming_device_type\":1}");
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("input_json"),
                      QStringLiteral("{\"device_details\":%1}").arg(device));

    bool ok = false;
    int status = 0;
    const QJsonObject response = callAuth(QStringLiteral("BeginAuthSessionViaQR/v1/"),
                                          form, &ok, &status);

    const QString url = response.value(QStringLiteral("challenge_url")).toString();
    const QString clientId = response.value(QStringLiteral("client_id")).toString();
    const QString requestId = response.value(QStringLiteral("request_id")).toString();

    if (url.isEmpty() || clientId.isEmpty() || requestId.isEmpty()) {
        if (error) {
            *error = ok ? QStringLiteral("Steam did not return a sign-in code (HTTP %1).").arg(status)
                        : QStringLiteral("Could not reach Steam (HTTP %1).").arg(status);
        }
        Logger::log(QStringLiteral("SteamAuth: BeginAuthSessionViaQR failed (HTTP %1)").arg(status));
        return false;
    }

    challenge->challengeUrl = url;
    challenge->clientId = clientId;
    challenge->requestId = requestId;
    challenge->intervalMs =
        qMax(2000, static_cast<int>(response.value(QStringLiteral("interval")).toDouble() * 1000));
    return true;
}

SteamAuth::QrPoll SteamAuth::pollQrChallenge(QrChallenge* challenge, QrApproval* approval,
                                             QString* error)
{
    if (!challenge || challenge->clientId.isEmpty()) {
        if (error)
            *error = QStringLiteral("The sign-in session is not valid.");
        return QrPoll::Failed;
    }

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), challenge->clientId);
    form.addQueryItem(QStringLiteral("request_id"), challenge->requestId);

    bool ok = false;
    int status = 0;
    const QJsonObject response = callAuth(QStringLiteral("PollAuthSessionStatus/v1/"),
                                          form, &ok, &status);

    if (!ok && response.isEmpty()) {
        if (error)
            *error = QStringLiteral("Could not reach Steam (HTTP %1).").arg(status);
        return QrPoll::Failed;
    }

    // While the code is on screen Steam may move the pending sign-in to a new
    // client id and/or publish a fresh challenge URL (for example when the
    // session was picked up on another device). Neither is a failure: keep
    // polling with the new values, exactly like SteamKit's AuthSession does.
    const QString newClientId = jsonIdString(response, "new_client_id");
    if (!newClientId.isEmpty() && newClientId != QLatin1String("0"))
        challenge->clientId = newClientId;

    const QString newUrl = response.value(QStringLiteral("new_challenge_url")).toString();
    if (!newUrl.isEmpty())
        challenge->challengeUrl = newUrl;

    // Authentication is only complete once Steam hands over the long-lived
    // refresh token; an empty response means the user has not approved yet.
    const QString refresh = response.value(QStringLiteral("refresh_token")).toString();
    const QString access = response.value(QStringLiteral("access_token")).toString();

    if (refresh.isEmpty()) {
        if (access.isEmpty())
            return QrPoll::Pending;

        // Should not happen for a client login, and without the refresh token
        // the link would silently stop working after a day.
        if (error)
            *error = QStringLiteral("Steam approved the sign-in but did not return a "
                                    "long-lived token. Please try again.");
        return QrPoll::Failed;
    }

    if (!approval)
        return QrPoll::Approved;

    approval->refreshToken = refresh;
    approval->accessToken = access;
    approval->accountName = response.value(QStringLiteral("account_name")).toString();
    approval->steamId = access.isEmpty() ? steamIdFromToken(refresh) : steamIdFromToken(access);

    Settings& s = Settings::instance();
    s.setClientRefreshToken(refresh);
    s.setClientAccountName(approval->accountName);
    if (!access.isEmpty())
        s.setClientAccessToken(access);
    s.save();

    Logger::log(QStringLiteral("SteamAuth: account linked for standalone farming (%1)")
                    .arg(approval->accountName.isEmpty()
                             ? QString::number(approval->steamId)
                             : approval->accountName));
    return QrPoll::Approved;
}
