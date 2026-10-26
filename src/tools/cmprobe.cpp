// Console probe for the standalone Steam CM backend.
//
// Not part of the shipped app. It exists so the protocol code can be exercised
// outside the GUI: server discovery, the wss:// transport, ClientHello, and -
// when an access token is supplied - ClientLogon and ClientGamesPlayed.
//
// Build with:  cmake --preset mingw -DCSI_BUILD_CM_PROBE=ON
// Run with:    cmprobe --token <jwt> --steamid <id64> --app 440 --seconds 30
// or, to verify only the transport:  cmprobe --seconds 8

#include "HttpClient.h"
#include "Logger.h"
#include "Settings.h"
#include "SteamAuth.h"
#include "SteamCmClient.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QList>
#include <QTextStream>
#include <QTimer>

namespace {

QTextStream& out()
{
    static QTextStream stream(stdout);
    return stream;
}

void report(const QString& line)
{
    out() << line << Qt::endl;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("cmprobe"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Probe the Steam CM WebSocket protocol."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption tokenOption(
        QStringLiteral("token"),
        QStringLiteral("Steam access token (JWT). Omit to test the transport only."),
        QStringLiteral("jwt"));
    const QCommandLineOption steamIdOption(
        QStringLiteral("steamid"),
        QStringLiteral("SteamID64 the token belongs to."),
        QStringLiteral("id"));
    const QCommandLineOption accountOption(
        QStringLiteral("account"),
        QStringLiteral("Steam account name sent with the logon."),
        QStringLiteral("name"));
    const QCommandLineOption refreshOption(
        QStringLiteral("refresh"),
        QStringLiteral("Refresh token to exchange over the CM before logging on."),
        QStringLiteral("jwt"));
    const QCommandLineOption appOption(
        QStringLiteral("app"),
        QStringLiteral("App ID to report as played (repeatable)."),
        QStringLiteral("appid"));
    const QCommandLineOption secondsOption(
        QStringLiteral("seconds"),
        QStringLiteral("How long to stay connected before exiting."),
        QStringLiteral("n"),
        QStringLiteral("15"));
    const QCommandLineOption sessionOption(
        QStringLiteral("from-session"),
        QStringLiteral("Log on with the refresh token saved by the app."));
    const QCommandLineOption webOption(
        QStringLiteral("web"),
        QStringLiteral("After logon, mint a community session and read the badge page."));
    const QCommandLineOption qrOption(
        QStringLiteral("qr"),
        QStringLiteral("Link the account with a QR code, then log on with the token it returns."));

    parser.addOption(tokenOption);
    parser.addOption(steamIdOption);
    parser.addOption(accountOption);
    parser.addOption(refreshOption);
    parser.addOption(appOption);
    parser.addOption(secondsOption);
    parser.addOption(sessionOption);
    parser.addOption(webOption);
    parser.addOption(qrOption);
    parser.process(app);

    QString token = parser.value(tokenOption);
    quint64 steamId = parser.value(steamIdOption).toULongLong();
    QString account = parser.value(accountOption);
    const int seconds = qMax(1, parser.value(secondsOption).toInt());
    const bool checkWeb = parser.isSet(webOption);

    if (parser.isSet(sessionOption)) {
        report(QStringLiteral("reading the linked refresh token from settings..."));
        Settings& settings = Settings::instance();
        token = settings.clientRefreshToken();
        steamId = SteamAuth::steamIdFromToken(token);
        if (account.isEmpty())
            account = settings.clientAccountName();
        if (token.isEmpty() || steamId == 0) {
            report(QStringLiteral("[auth] no linked account in %1/settings.ini")
                       .arg(Settings::dataDirectory()));
            return 1;
        }
        report(QStringLiteral("[auth] refresh token found for SteamID %1").arg(steamId));
    }

    QList<quint32> apps;
    for (const QString& value : parser.values(appOption))
        apps.append(value.toUInt());

    if (parser.isSet(qrOption)) {
        SteamAuth::QrChallenge challenge;
        QString error;
        if (!SteamAuth::beginQrChallenge(&challenge, &error)) {
            report(QStringLiteral("[qr] failed: %1").arg(error));
            return 1;
        }

        report(QStringLiteral("[qr] open this link on your phone and approve it:"));
        report(QStringLiteral("[qr] %1").arg(challenge.challengeUrl));
        report(QStringLiteral("[qr] waiting for approval (polling every %1 ms)...")
                   .arg(challenge.intervalMs));

        auto* poll = new QTimer(&app);
        QString shownUrl = challenge.challengeUrl;
        QObject::connect(poll, &QTimer::timeout, [&]() {
            SteamAuth::QrApproval approval;
            QString pollError;
            const SteamAuth::QrPoll result =
                SteamAuth::pollQrChallenge(&challenge, &approval, &pollError);

            if (result == SteamAuth::QrPoll::Pending) {
                if (challenge.challengeUrl != shownUrl) {
                    shownUrl = challenge.challengeUrl;
                    report(QStringLiteral("[qr] Steam issued a new code, scan this one:"));
                    report(QStringLiteral("[qr] %1").arg(shownUrl));
                }
                report(QStringLiteral("[qr] ."));
                return;
            }

            poll->stop();
            if (result == SteamAuth::QrPoll::Approved) {
                report(QStringLiteral("[qr] approved, account %1 linked")
                           .arg(approval.accountName.isEmpty()
                                    ? QString::number(approval.steamId)
                                    : approval.accountName));
                app.exit(0);
            } else {
                report(QStringLiteral("[qr] failed%1")
                           .arg(pollError.isEmpty() ? QString()
                                                    : QStringLiteral(": ") + pollError));
                app.exit(1);
            }
        });

        poll->start(challenge.intervalMs);
        return app.exec();
    }

    report(QStringLiteral("== Copper Steam Idler CM probe =="));
    report(token.isEmpty()
               ? QStringLiteral("mode: transport only (no access token)")
               : QStringLiteral("mode: logon (SteamID %1)").arg(steamId));
    if (!apps.isEmpty()) {
        QStringList ids;
        for (quint32 id : apps)
            ids.append(QString::number(id));
        report(QStringLiteral("games: %1").arg(ids.join(QLatin1Char(','))));
    }

    SteamCmClient client;
    const QString refreshToken = parser.value(refreshOption);
    if (!refreshToken.isEmpty()) {
        steamId = SteamAuth::steamIdFromToken(refreshToken);
        client.setRefreshToken(refreshToken, steamId);
        report(QStringLiteral("mode: refresh-token logon (SteamID %1)").arg(steamId));
    } else if (!token.isEmpty()) {
        client.setAccessToken(token, steamId);
    }
    if (!account.isEmpty())
        report(QStringLiteral("account: %1").arg(account));

    int exitCode = 0;
    bool reachedLoggedOn = false;

    QObject::connect(&client, &SteamCmClient::stateChanged,
                     [](SteamCmClient::State state) {
                         QString name;
                         switch (state) {
                         case SteamCmClient::State::Disconnected:
                             name = QStringLiteral("Disconnected");
                             break;
                         case SteamCmClient::State::FetchingServers:
                             name = QStringLiteral("FetchingServers");
                             break;
                         case SteamCmClient::State::Connecting:
                             name = QStringLiteral("Connecting");
                             break;
                         case SteamCmClient::State::Connected:
                             name = QStringLiteral("Connected (transport up)");
                             break;
                         case SteamCmClient::State::LoggedOn:
                             name = QStringLiteral("LoggedOn");
                             break;
                         case SteamCmClient::State::Failed:
                             name = QStringLiteral("Failed");
                             break;
                         }
                         report(QStringLiteral("[state] %1").arg(name));
                     });

    QObject::connect(&client, &SteamCmClient::errorOccurred, [&](const QString& message) {
        report(QStringLiteral("[error] %1").arg(message));
        exitCode = 1;
    });

    QObject::connect(&client, &SteamCmClient::loggedOn, [&](quint64 id) {
        reachedLoggedOn = true;
        report(QStringLiteral("[logon] ok, SteamID %1").arg(id));
        if (!apps.isEmpty()) {
            report(QStringLiteral("[games] reporting %1 app(s) as played").arg(apps.size()));
            client.setGamesPlayed(apps);
        }
    });

    QObject::connect(&client, &SteamCmClient::loggedOff, [&](int result) {
        report(QStringLiteral("[logoff] result %1").arg(result));
    });

    QObject::connect(&client, &SteamCmClient::webAccessTokenReceived,
                     [&](const QString& accessToken, const QString& refreshToken) {
                         report(QStringLiteral("[web] token received: %1 chars, rotated refresh: %2")
                                    .arg(accessToken.size())
                                    .arg(refreshToken.isEmpty() ? QStringLiteral("no")
                                                                : QStringLiteral("yes")));
                         if (accessToken.isEmpty()) {
                             exitCode = 1;
                             if (checkWeb)
                                 report(QStringLiteral("[web] RESULT: FAILED (no token)"));
                             return;
                         }

                         if (!checkWeb)
                             return;

                         HttpClient::instance().adoptWebSession(steamId, accessToken);
                         const QString page = HttpClient::instance().getSync(
                             QUrl(QStringLiteral("https://steamcommunity.com/profiles/%1/badges/?p=1")
                                      .arg(steamId)),
                             20000);
                         const bool loggedIn = page.contains(QStringLiteral("g_steamID = \""))
                                               || page.contains(QStringLiteral("g_bLoggedIn = true"));
                         const int drops = page.count(QStringLiteral("card drops remaining"));
                         const int marker = page.indexOf(QStringLiteral("g_steamID"));
                         report(QStringLiteral("[web] marker at %1: %2")
                                    .arg(marker)
                                    .arg(marker >= 0 ? page.mid(marker, 120)
                                                     : QStringLiteral("<absent>")));
                         report(QStringLiteral("[web] badge page: %1 bytes, logged in: %2, "
                                               "drops remaining markers: %3")
                                    .arg(page.size())
                                    .arg(loggedIn ? QStringLiteral("yes") : QStringLiteral("no"))
                                    .arg(drops));
                         if (!loggedIn)
                             exitCode = 1;
                     });

    QObject::connect(&client, &SteamCmClient::messageReceived, [&](quint32 emsg, int bodySize) {
        report(QStringLiteral("[recv] emsg %1 (%2 body bytes)").arg(emsg).arg(bodySize));
    });

    client.connectToSteam();

    QTimer::singleShot(seconds * 1000, &app, [&]() {
        if (token.isEmpty() && refreshToken.isEmpty()) {
            // Transport-only run: reaching Connected is the success criterion.
            if (client.state() == SteamCmClient::State::Failed) {
                report(QStringLiteral("RESULT: transport check FAILED"));
                exitCode = 1;
            } else {
                report(QStringLiteral("RESULT: transport check OK "
                                      "(discovery + TLS + ClientHello)"));
            }
        } else if (!reachedLoggedOn) {
            report(QStringLiteral("RESULT: logon did not complete"));
            exitCode = 1;
        } else {
            report(QStringLiteral("RESULT: logged on and game list sent"));
        }
        client.logOff();
        app.quit();
    });

    return app.exec();
}
