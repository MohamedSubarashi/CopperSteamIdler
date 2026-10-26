#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

class WebSocketClient;
class QNetworkAccessManager;
class QTimer;

// Talks to Steam's Connection Manager (CM) network directly, speaking the same
// protocol the Steam client uses, so games can be "played" while the Steam
// client is not running.
//
// Transport is a WebSocket (wss://<cm>/cmsocket/) which is already protected by
// TLS, so unlike the TCP CM protocol there is no channel-encryption handshake.
// Each WebSocket binary message is exactly one CM packet:
//
//   [u32 emsg | 0x80000000] [u32 header length] [CMsgProtoBufHeader] [body]
//
// Messages used here are the same ones ArchiSteamFarm relies on: ClientHello,
// ClientLogon, ClientLoggedOn, ClientGamesPlayedWithDataBlob, ClientHeartBeat.
class SteamCmClient : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Disconnected,
        FetchingServers,
        Connecting,
        Connected,
        LoggedOn,
        Failed,
    };
    Q_ENUM(State)

    explicit SteamCmClient(QObject* parent = nullptr);
    ~SteamCmClient() override;

    // Credentials. Steam's ClientLogon message calls this field "access_token"
    // but actually expects a refresh token, which is what the QR sign-in
    // returns; the client (and node-steam-user) does the same.
    void setAccessToken(const QString& token, quint64 steamId);

    // Alternative credentials: a long-lived refresh token from the QR link.
    // Steam accepts it in the logon's access-token field, and it is also what
    // the community session is minted from. Prefer this over setAccessToken():
    // access tokens expire within a day, refresh tokens do not.
    void setRefreshToken(const QString& refreshToken, quint64 steamId);

    void connectToSteam();
    void logOff();

    // Asks Steam, over the logged-on CM connection, for a short-lived access
    // token. That token doubles as the steamLoginSecure cookie value, so this is
    // how a standalone session gets access to the community pages. Called
    // automatically after a successful logon.
    void requestWebAccessToken();

    // Reports the given app IDs as being played, replacing any previous set.
    // Steam accepts up to 32 concurrently "played" games per session.
    void setGamesPlayed(const QList<quint32>& appIds);

    State state() const { return m_state; }
    QString lastError() const { return m_lastError; }
    quint64 steamId() const { return m_steamId; }

signals:
    void stateChanged(SteamCmClient::State state);
    void loggedOn(quint64 steamId);
    void loggedOff(int result);
    void errorOccurred(const QString& message);
    // A fresh access token for the community site. refreshToken is Steam's
    // rotated copy of the long-lived token and should replace the stored one.
    // Both are empty when Steam refused the request.
    void webAccessTokenReceived(const QString& accessToken, const QString& refreshToken);
    // Emitted as soon as Steam rotates the long-lived token, so it can be saved
    // even if the logon that follows fails.
    void refreshTokenRotated(const QString& refreshToken);
    // Diagnostic: emitted for every message the CM sends us.
    void messageReceived(quint32 emsg, int bodySize);

private:
    void setState(State state);
    void fail(const QString& message);

    void fetchServerList();
    void openNextServer();
    void sendHello();
    void sendLogon();
    void sendHeartbeat();

    void onBinaryMessage(const QByteArray& payload);
    void handlePacket(const QByteArray& payload);
    void handleMulti(const QByteArray& body);
    void handleMessage(quint32 emsg, const QByteArray& header, const QByteArray& body);
    void handleLogonResponse(const QByteArray& body);
    void handleLoggedOff(const QByteArray& body);
    void handleServiceMethodResponse(const QByteArray& header, const QByteArray& body);

    void sendMessage(quint32 emsg, const QByteArray& body);
    void sendJob(const QString& targetJobName, const QByteArray& body, quint64 jobId,
                 bool authenticated = true);

    // Marks the session as online so the reported games are visible on the
    // profile instead of the account showing up as offline.
    void sendChangeStatus();

    static QStringList parseWebSocketServers(const QByteArray& json);

    WebSocketClient* m_ws = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
    QTimer* m_heartbeat = nullptr;

    QStringList m_servers;
    int m_serverIndex = 0;

    State m_state = State::Disconnected;
    QString m_lastError;

    QString m_accessToken;
    QString m_refreshToken;
    quint64 m_steamId = 0;
    qint32 m_sessionId = 0;
    quint64 m_jobCounter = 0;
    quint64 m_webTokenJobId = 0;

    QList<quint32> m_games;
    int m_heartbeatSeconds = 9;
};
