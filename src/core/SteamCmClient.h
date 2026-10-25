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

    // Credentials: a Steam access token (JWT) and the SteamID64 it belongs to.
    void setAccessToken(const QString& token, quint64 steamId);

    void connectToSteam();
    void logOff();

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

    void sendMessage(quint32 emsg, const QByteArray& body);

    static QStringList parseWebSocketServers(const QByteArray& json);

    WebSocketClient* m_ws = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
    QTimer* m_heartbeat = nullptr;

    QStringList m_servers;
    int m_serverIndex = 0;

    State m_state = State::Disconnected;
    QString m_lastError;

    QString m_accessToken;
    quint64 m_steamId = 0;
    qint32 m_sessionId = 0;
    quint64 m_jobCounter = 0;

    QList<quint32> m_games;
    int m_heartbeatSeconds = 9;
};
