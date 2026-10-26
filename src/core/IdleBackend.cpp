#include "IdleBackend.h"

#include "Logger.h"
#include "ProcessManager.h"
#include "Settings.h"
#include "SteamAuth.h"
#include "SteamCmClient.h"

#include <QTimer>

namespace {

// Steam rejects more than 32 concurrently played games per session.
constexpr int kMaxGamesPlayed = 32;

// How long to wait before retrying a dropped standalone session.
constexpr int kReconnectDelayMs = 5000;

// Retry interval (and cap) when no access token can be obtained at all, e.g.
// while offline or after the saved session expired.
constexpr int kReconnectRetryMs = 15000;
constexpr int kMaxConnectFailures = 5;

} // namespace

IdleBackend& IdleBackend::instance()
{
    static IdleBackend backend;
    return backend;
}

IdleBackend::IdleBackend(QObject* parent)
    : QObject(parent)
    , m_cm(new SteamCmClient(this))
    , m_reconnect(new QTimer(this))
{
    m_reconnect->setSingleShot(true);

    connect(m_reconnect, &QTimer::timeout, this, [this]() {
        if (!m_reconnectWanted || m_mode != Mode::Standalone)
            return;
        if (!ensureSession()) {
            // No usable token right now (offline, or the session expired). Keep
            // trying while idling is still wanted, but do not spin.
            if (m_failures < kMaxConnectFailures)
                m_reconnect->start(kReconnectRetryMs);
        }
    });

    connect(m_cm, &SteamCmClient::loggedOn, this, [this](quint64 steamId) {
        m_connecting = false;
        m_standaloneReady = true;
        m_failures = 0;
        m_lastError.clear();
        Logger::log(QStringLiteral("IdleBackend: standalone session ready (%1)").arg(steamId));
        pushGames();
        emit standaloneStateChanged(true);
    });

    connect(m_cm, &SteamCmClient::loggedOff, this, [this](int result) {
        const bool wasReady = m_standaloneReady;
        m_standaloneReady = false;
        if (wasReady)
            emit standaloneStateChanged(false);

        // result 3 is our own disconnect; only re-establish a session we wanted.
        if (m_mode == Mode::Standalone && m_reconnectWanted && m_failures < kMaxConnectFailures) {
            Logger::log(QStringLiteral("IdleBackend: standalone session dropped, reconnecting"));
            scheduleReconnect();
        } else if (result != 3) {
            Logger::log(QStringLiteral("IdleBackend: standalone session ended (%1)").arg(result));
        }
    });

    connect(m_cm, &SteamCmClient::errorOccurred, this, [this](const QString& message) {
        m_lastError = message;
        m_connecting = false;
        m_standaloneReady = false;
        ++m_failures;
        emit standaloneFailed(message);
        if (m_mode == Mode::Standalone && m_reconnectWanted && m_failures < kMaxConnectFailures)
            scheduleReconnect();
    });
}

IdleBackend::~IdleBackend() = default;

IdleBackend::Mode IdleBackend::resolveMode()
{
    const QString method = Settings::instance().farmingMethod();
    if (method == QLatin1String("standalone"))
        return Mode::Standalone;
    if (method == QLatin1String("client"))
        return Mode::Client;

    // "auto": farm without the Steam client whenever a saved session makes that
    // possible, and fall back to the helper processes otherwise.
    return Settings::instance().hasSession() ? Mode::Standalone : Mode::Client;
}

void IdleBackend::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    Logger::log(QStringLiteral("IdleBackend: mode is now %1")
                    .arg(mode == Mode::Standalone ? QStringLiteral("standalone")
                                                  : QStringLiteral("client")));

    if (m_mode == Mode::Client) {
        // Leave no CM session (or its reconnect loop) running in client mode.
        m_reconnectWanted = false;
        m_reconnect->stop();
        if (m_standaloneReady || m_connecting)
            stopStandalone();
    }

    if (m_mode != Mode::Standalone)
        m_games.clear();
}

// ---------------------------------------------------------------------------
// Idling
// ---------------------------------------------------------------------------

bool IdleBackend::isIdling(int appId) const
{
    if (m_mode == Mode::Standalone)
        return m_games.contains(appId);
    return ProcessManager::instance().isIdling(appId);
}

void IdleBackend::startIdle(int appId)
{
    if (m_mode != Mode::Standalone) {
        ProcessManager::instance().startIdle(appId);
        return;
    }

    if (appId <= 0 || m_games.contains(appId))
        return;
    if (m_games.size() >= kMaxGamesPlayed)
        return;

    m_games.append(appId);
    m_reconnectWanted = true;
    pushGames();

    if (!m_standaloneReady)
        ensureSession();}

void IdleBackend::stopIdle(int appId)
{
    if (m_mode != Mode::Standalone) {
        ProcessManager::instance().stopIdle(appId);
        return;
    }

    if (m_games.removeOne(appId))
        pushGames();
}

void IdleBackend::stopAll()
{
    m_games.clear();

    // Idling has been stopped, so any pending (re)connect is no longer wanted.
    // This also prevents a reconnect storm between two idle cycles.
    m_reconnectWanted = false;
    m_reconnect->stop();

    // Clear the helper processes too: switching backends mid-run must not leave
    // stale helpers behind.
    ProcessManager::instance().stopAll();

    if (m_standaloneReady)
        pushGames();
}

int IdleBackend::idlingCount() const
{
    if (m_mode == Mode::Standalone)
        return m_games.size();
    return ProcessManager::instance().idlingCount();
}

QList<int> IdleBackend::idlingAppIds() const
{
    if (m_mode == Mode::Standalone)
        return m_games;
    return ProcessManager::instance().idlingAppIds();
}

void IdleBackend::killRogueHelpers()
{
    // Helpers belong to client mode; killing leftovers is always safe.
    ProcessManager::instance().killRogueHelpers();
}

// ---------------------------------------------------------------------------
// Standalone session
// ---------------------------------------------------------------------------

bool IdleBackend::startStandalone()
{
    m_mode = Mode::Standalone;
    m_reconnectWanted = true;
    // A new idle cycle gets a fresh retry budget.
    m_failures = 0;
    return ensureSession();
}

void IdleBackend::stopStandalone()
{
    m_reconnectWanted = false;
    m_reconnect->stop();
    m_connecting = false;

    // Drop any games the session was reporting before closing it, so Steam does
    // not keep showing this account in-game after the app stops.
    m_cm->setGamesPlayed({});
    m_cm->logOff();

    const bool wasReady = m_standaloneReady;
    m_standaloneReady = false;
    if (wasReady)
        emit standaloneStateChanged(false);
}

bool IdleBackend::ensureSession()
{
    if (m_standaloneReady)
        return true;
    if (m_connecting)
        return true;

    const SteamAuth::Token token = SteamAuth::fetchToken();
    if (!token.isValid()) {
        m_lastError = token.error.isEmpty()
                          ? QStringLiteral("Not signed in to Steam.")
                          : token.error;
        ++m_failures;
        Logger::log(QStringLiteral("IdleBackend: cannot open standalone session: %1")
                        .arg(m_lastError));
        emit standaloneFailed(m_lastError);
        return false;
    }

    m_connecting = true;
    m_cm->setAccessToken(token.accessToken, token.steamId);
    m_cm->connectToSteam();
    return true;
}

void IdleBackend::scheduleReconnect()
{
    if (m_reconnect->isActive())
        return;
    m_connecting = false;
    m_reconnect->start(kReconnectDelayMs);
}

void IdleBackend::pushGames()
{
    if (m_mode != Mode::Standalone)
        return;

    QList<quint32> ids;
    ids.reserve(m_games.size());
    for (int appId : m_games) {
        if (appId > 0)
            ids.append(static_cast<quint32>(appId));
    }
    m_cm->setGamesPlayed(ids);
}
