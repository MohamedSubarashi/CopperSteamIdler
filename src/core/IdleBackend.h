#pragma once

#include <QList>
#include <QObject>
#include <QString>

class QTimer;
class SteamCmClient;

// Routes idling to one of two backends behind the small interface the rest of
// the app already uses (start/stop one AppID at a time, ask what is idling):
//
//   Client      spawns one copper-steam-idle helper per AppID, so the Steam
//               client has to be running. This is the historical behaviour.
//   Standalone  keeps one Steam Connection Manager session open and reports the
//               selected games as "played". It logs on with the refresh token
//               from the QR link, so neither the Steam client nor a website
//               sign-in is needed.
//
// Callers do not need to know which backend is active; Badge and MainWindow go
// through this class instead of talking to ProcessManager directly.
class IdleBackend : public QObject
{
    Q_OBJECT
public:
    enum class Mode
    {
        Client,
        Standalone,
    };
    Q_ENUM(Mode)

    static IdleBackend& instance();

    // Mode selected by the user, with "auto" resolved: standalone is used when a
    // saved Steam session exists, otherwise the client helper is used.
    static Mode resolveMode();
    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

    // Backend-agnostic idling interface.
    bool isIdling(int appId) const;
    void startIdle(int appId);
    void stopIdle(int appId);
    void stopAll();
    int idlingCount() const;
    QList<int> idlingAppIds() const;
    void killRogueHelpers();

    // Standalone session lifecycle. startStandalone() logs on to the CM with the
    // linked account's refresh token; the handshake is asynchronous. The session
    // is usable once standaloneReady() is true and standaloneStateChanged() has
    // been emitted. Returns false only when there is no linked account.
    bool startStandalone();
    void stopStandalone();
    bool standaloneReady() const { return m_standaloneReady; }
    bool standaloneConnecting() const { return m_connecting; }
    QString lastError() const { return m_lastError; }

    // Asks Steam for a fresh community (web) access token over the CM session.
    // The token behind the steamLoginSecure cookie expires daily, so this is how
    // the app recovers without making the user link or sign in again.
    void refreshWebSession();

signals:
    // Emitted whenever the standalone session becomes ready or drops.
    void standaloneStateChanged(bool ready);
    void standaloneFailed(const QString& reason);
    // A fresh community session has been installed; it is safe to load badges.
    void webSessionReady();

private:
    explicit IdleBackend(QObject* parent = nullptr);
    ~IdleBackend() override;

    void pushGames();          // sends the current set to the CM session
    void scheduleReconnect();
    bool ensureSession();

    SteamCmClient* m_cm = nullptr;
    QTimer* m_reconnect = nullptr;

    Mode m_mode = Mode::Client;
    QList<int> m_games;              // AppIDs currently reported as played

    bool m_standaloneReady = false;
    bool m_connecting = false;
    bool m_reconnectWanted = false;
    int m_failures = 0;
    QString m_lastError;
};
