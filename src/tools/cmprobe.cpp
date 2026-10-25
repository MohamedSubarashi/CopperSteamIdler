// Console probe for the standalone Steam CM backend.
//
// Not part of the shipped app. It exists so the protocol code can be exercised
// outside the GUI: server discovery, the wss:// transport, ClientHello, and -
// when an access token is supplied - ClientLogon and ClientGamesPlayed.
//
// Build with:  cmake --preset mingw -DCSI_BUILD_CM_PROBE=ON
// Run with:    cmprobe --token <jwt> --steamid <id64> --app 440 --seconds 30
// or, to verify only the transport:  cmprobe --seconds 8

#include "Logger.h"
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
    const QCommandLineOption appOption(
        QStringLiteral("app"),
        QStringLiteral("App ID to report as played (repeatable)."),
        QStringLiteral("appid"));
    const QCommandLineOption secondsOption(
        QStringLiteral("seconds"),
        QStringLiteral("How long to stay connected before exiting."),
        QStringLiteral("n"),
        QStringLiteral("15"));

    parser.addOption(tokenOption);
    parser.addOption(steamIdOption);
    parser.addOption(appOption);
    parser.addOption(secondsOption);
    parser.process(app);

    const QString token = parser.value(tokenOption);
    const quint64 steamId = parser.value(steamIdOption).toULongLong();
    const int seconds = qMax(1, parser.value(secondsOption).toInt());

    QList<quint32> apps;
    for (const QString& value : parser.values(appOption))
        apps.append(value.toUInt());

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
    if (!token.isEmpty())
        client.setAccessToken(token, steamId);

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

    QObject::connect(&client, &SteamCmClient::messageReceived, [&](quint32 emsg, int bodySize) {
        report(QStringLiteral("[recv] emsg %1 (%2 body bytes)").arg(emsg).arg(bodySize));
    });

    client.connectToSteam();

    QTimer::singleShot(seconds * 1000, &app, [&]() {
        if (token.isEmpty()) {
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
