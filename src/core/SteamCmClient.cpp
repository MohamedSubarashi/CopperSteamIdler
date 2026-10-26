#include "SteamCmClient.h"

#include "Logger.h"
#include "SteamProto.h"
#include "WebSocketClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOperatingSystemVersion>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QCryptographicHash>
#include <QtEndian>

#include <zlib.h>

namespace {

// EMsg values (see SteamKit's emsg.steamd).
constexpr quint32 kEmsgMulti = 1;
constexpr quint32 kEmsgClientLoggedOff = 757;
constexpr quint32 kEmsgClientLogOnResponse = 751;
constexpr quint32 kEmsgClientSessionToken = 850;
constexpr quint32 kEmsgClientLogOff = 706;
constexpr quint32 kEmsgClientLogon = 5514;
constexpr quint32 kEmsgClientHello = 9805;
constexpr quint32 kEmsgClientHeartBeat = 703;
constexpr quint32 kEmsgClientPlayingSessionState = 9600;
constexpr quint32 kEmsgClientKickPlayingSession = 9601;
constexpr quint32 kEmsgClientGamesPlayedWithDataBlob = 5410;
constexpr quint32 kEmsgClientChangeStatus = 716;
constexpr quint32 kEmsgServiceMethodResponse = 147;
constexpr quint32 kEmsgServiceMethodCallFromClient = 151;
constexpr quint32 kEmsgServiceMethodCallFromClientNonAuthed = 9804;

constexpr quint32 kProtoMask = 0x80000000;

// CMsgClientLogon.protocol_version expected by current Steam clients.
constexpr quint32 kProtocolVersion = 65580;

// Steam reports at most 32 concurrently played games.
constexpr int kMaxGamesPlayed = 32;

// EOSType values from Steam's enums.
constexpr quint32 kEosWindows10 = 16;
constexpr quint32 kEosWindows11 = 20;

QString eResultName(int result)
{
    switch (result) {
    case 1: return QStringLiteral("OK");
    case 2: return QStringLiteral("Fail");
    case 3: return QStringLiteral("NoConnection");
    case 5: return QStringLiteral("InvalidPassword");
    case 6: return QStringLiteral("LoggedInElsewhere");
    case 15: return QStringLiteral("AccessDenied");
    case 20: return QStringLiteral("ServiceUnavailable");
    case 27: return QStringLiteral("Expired");
    case 34: return QStringLiteral("LogonSessionReplaced");
    case 63: return QStringLiteral("AccountLoginDeniedNeedTwoFactor");
    case 65: return QStringLiteral("AccountLoginDeniedThrottle");
    case 84: return QStringLiteral("RateLimitExceeded");
    case 85: return QStringLiteral("AccountLoginDeniedNeedEmail");
    case 88: return QStringLiteral("TwoFactorCodeMismatch");
    default: return QStringLiteral("EResult %1").arg(result);
    }
}

quint32 osType()
{
    // Steam only distinguishes broad families here; Win11 builds are 22000+.
    const QOperatingSystemVersion os = QOperatingSystemVersion::current();
    if (os.type() == QOperatingSystemVersion::Windows && os.microVersion() >= 22000)
        return kEosWindows11;
    return kEosWindows10;
}

// Steam identifies a computer with a small binary key/value blob (root key
// MessageObject with three SHA-1 "hardware" hashes). It only needs to be stable
// across runs, so the values are derived from the OS machine id.
QByteArray machineIdBlob()
{
    QByteArray seed = QSysInfo::machineUniqueId();
    if (seed.isEmpty())
        seed = QSysInfo::machineHostName().toUtf8();
    if (seed.isEmpty())
        seed = QByteArrayLiteral("copper-steam-idler");

    const auto hash = [&seed](const char* salt) {
        return QCryptographicHash::hash(seed + '|' + salt, QCryptographicHash::Sha1).toHex();
    };

    QByteArray blob;
    blob.append('\0');
    blob.append("MessageObject");
    blob.append('\0');
    for (const char* key : {"BB3", "FF2", "3B3"}) {
        blob.append('\1');
        blob.append(key);
        blob.append('\0');
        blob.append(hash(key));
        blob.append('\0');
    }
    blob.append('\x08');
    blob.append('\x08');
    return blob;
}

quint64 clientInstanceId()
{
    // One process = one client instance, like the Steam client itself.
    static const quint64 id = QRandomGenerator::global()->generate64();
    return id;
}

// Decompresses the gzip payload used by CMsgMulti. sizeHint is the exact
// uncompressed size Steam reports, so the output buffer can be sized up front.
QByteArray inflateGzip(const QByteArray& input, int sizeHint)
{
    if (input.isEmpty() || sizeHint <= 0)
        return QByteArray();

    QByteArray output(sizeHint, '\0');

    z_stream stream = {};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    // 16 + MAX_WBITS tells zlib to expect a gzip wrapper rather than raw zlib.
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
        return QByteArray();

    const int result = inflate(&stream, Z_FINISH);
    const int written = static_cast<int>(stream.total_out);
    inflateEnd(&stream);

    if (result != Z_STREAM_END)
        return QByteArray();

    output.resize(written);
    return output;
}

} // namespace

SteamCmClient::SteamCmClient(QObject* parent)
    : QObject(parent)
    , m_ws(new WebSocketClient(this))
    , m_nam(new QNetworkAccessManager(this))
    , m_heartbeat(new QTimer(this))
{
    connect(m_ws, &WebSocketClient::opened, this, [this]() {
        setState(State::Connected);
        sendHello();
        if (m_accessToken.isEmpty())
            Logger::log(QStringLiteral("Steam CM: connected without logging on"));
        else
            sendLogon();
    });
    connect(m_ws, &WebSocketClient::binaryMessage, this, &SteamCmClient::onBinaryMessage);
    connect(m_ws, &WebSocketClient::closed, this, [this]() {
        m_heartbeat->stop();
        if (m_state == State::Failed)
            return;
        setState(State::Disconnected);
        emit loggedOff(3);
    });
    connect(m_ws, &WebSocketClient::failed, this, [this](const QString& reason) {
        // Try the next CM before giving up; individual CMs are frequently busy.
        if (m_state == State::Connecting) {
            ++m_serverIndex;
            if (m_serverIndex < m_servers.size()) {
                openNextServer();
                return;
            }
        }
        fail(reason);
    });

    m_heartbeat->setSingleShot(false);
    connect(m_heartbeat, &QTimer::timeout, this, &SteamCmClient::sendHeartbeat);
}

SteamCmClient::~SteamCmClient() = default;

void SteamCmClient::setAccessToken(const QString& token, quint64 steamId)
{
    m_accessToken = token;
    m_refreshToken.clear();
    m_steamId = steamId;
}

void SteamCmClient::setRefreshToken(const QString& refreshToken, quint64 steamId)
{
    // The CM accepts a refresh token in the same field as an access token, which
    // is what node-steam-user does. Keep a copy so the web session can be minted
    // after logon, and because Steam rotates it on every use.
    m_refreshToken = refreshToken;
    m_accessToken = refreshToken;
    m_steamId = steamId;
}

void SteamCmClient::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void SteamCmClient::fail(const QString& message)
{
    m_lastError = message;
    m_heartbeat->stop();
    setState(State::Failed);
    Logger::log(QStringLiteral("Steam CM error: %1").arg(message));
    emit errorOccurred(message);
}

void SteamCmClient::connectToSteam()
{
    // A token is required to log on, but connecting without one is still useful
    // (it exercises server discovery and the TLS transport).
    if (m_accessToken.isEmpty())
        Logger::log(QStringLiteral("Steam CM: no access token, connecting without logon"));

    m_serverIndex = 0;
    m_servers.clear();
    setState(State::FetchingServers);
    fetchServerList();
}

void SteamCmClient::logOff()
{
    m_heartbeat->stop();
    if (m_state == State::LoggedOn) {
        sendMessage(kEmsgClientLogOff, QByteArray());
    }
    m_games.clear();
    m_ws->close();
    setState(State::Disconnected);
}

// ---------------------------------------------------------------------------
// Server discovery
// ---------------------------------------------------------------------------

void SteamCmClient::fetchServerList()
{
    QUrl url(QStringLiteral(
        "https://api.steampowered.com/ISteamDirectory/GetCMList/v1/"
        "?cellid=0&format=json"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("CopperSteamIdler"));

    QNetworkReply* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(QStringLiteral("Could not reach the Steam directory: %1")
                     .arg(reply->errorString()));
            return;
        }

        const QByteArray body = reply->readAll();
        m_servers = parseWebSocketServers(body);
        if (m_servers.isEmpty()) {
            fail(QStringLiteral("Steam returned no WebSocket CM servers"));
            return;
        }

        Logger::log(QStringLiteral("Steam CM: %1 WebSocket endpoints available")
                        .arg(m_servers.size()));
        m_serverIndex = 0;
        openNextServer();
    });
}

QStringList SteamCmClient::parseWebSocketServers(const QByteArray& json)
{
    QStringList servers;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return servers;

    const QJsonObject response = doc.object().value(QStringLiteral("response")).toObject();
    const QJsonArray list = response.value(QStringLiteral("serverlist_websockets")).toArray();
    for (const QJsonValue& value : list) {
        const QString entry = value.toString();
        if (!entry.isEmpty())
            servers.append(entry);
    }
    return servers;
}

void SteamCmClient::openNextServer()
{
    if (m_serverIndex >= m_servers.size()) {
        fail(QStringLiteral("All Steam CM servers refused the connection"));
        return;
    }

    const QString entry = m_servers.at(m_serverIndex);
    QString host = entry;
    int port = 443;
    const int colon = entry.lastIndexOf(QLatin1Char(':'));
    if (colon > 0) {
        host = entry.left(colon);
        port = entry.mid(colon + 1).toInt();
    }

    QUrl url;
    url.setScheme(QStringLiteral("wss"));
    url.setHost(host);
    url.setPort(port);
    url.setPath(QStringLiteral("/cmsocket/"));

    Logger::log(QStringLiteral("Steam CM: connecting to %1:%2").arg(host).arg(port));
    setState(State::Connecting);
    m_ws->open(url);
}

// ---------------------------------------------------------------------------
// Packet I/O
// ---------------------------------------------------------------------------

void SteamCmClient::sendMessage(quint32 emsg, const QByteArray& body)
{
    proto::Writer header;
    if (m_steamId != 0)
        header.fixed64Field(1, m_steamId); // steamid
    if (m_sessionId != 0)
        header.int32Field(2, m_sessionId); // client_sessionid

    const QByteArray headerBytes = header.data();

    QByteArray packet;
    packet.reserve(8 + headerBytes.size() + body.size());

    const quint32 rawEmsg = emsg | kProtoMask;
    char emsgBytes[4];
    qToLittleEndian<quint32>(rawEmsg, emsgBytes);
    packet.append(emsgBytes, 4);

    char lengthBytes[4];
    qToLittleEndian<quint32>(static_cast<quint32>(headerBytes.size()), lengthBytes);
    packet.append(lengthBytes, 4);

    packet.append(headerBytes);
    packet.append(body);

    m_ws->sendBinary(packet);
}

// A service-method call: the unified Steam services (Authentication, Player,
// ...) are reachable over the CM with a job header instead of a WebAPI key.
void SteamCmClient::sendJob(const QString& targetJobName, const QByteArray& body, quint64 jobId,
                            bool authenticated)
{
    proto::Writer header;
    if (authenticated && m_steamId != 0)
        header.fixed64Field(1, m_steamId); // steamid
    if (authenticated && m_sessionId != 0)
        header.int32Field(2, m_sessionId); // client_sessionid
    header.fixed64Field(10, jobId);        // jobid_source
    header.stringField(12, targetJobName); // target_job_name
    header.varintField(32, 1);             // realm

    const QByteArray headerBytes = header.data();

    QByteArray packet;
    packet.reserve(8 + headerBytes.size() + body.size());

    const quint32 callEmsg = authenticated ? kEmsgServiceMethodCallFromClient
                                           : kEmsgServiceMethodCallFromClientNonAuthed;
    const quint32 rawEmsg = callEmsg | kProtoMask;
    char emsgBytes[4];
    qToLittleEndian<quint32>(rawEmsg, emsgBytes);
    packet.append(emsgBytes, 4);

    char lengthBytes[4];
    qToLittleEndian<quint32>(static_cast<quint32>(headerBytes.size()), lengthBytes);
    packet.append(lengthBytes, 4);

    packet.append(headerBytes);
    packet.append(body);

    m_ws->sendBinary(packet);
}

void SteamCmClient::requestWebAccessToken()
{
    if (m_state != State::LoggedOn) {
        Logger::log(QStringLiteral("Steam CM: cannot request a web token before logging on"));
        emit webAccessTokenReceived(QString(), QString());
        return;
    }

    // Callers can ask twice in quick succession (logon and the idle pipeline);
    // one in-flight request is enough, and a second job would overwrite the id
    // the response is matched against.
    if (m_webTokenJobId != 0) {
        Logger::log(QStringLiteral("Steam CM: web session token already requested"));
        return;
    }

    // CAuthentication_AccessToken_GenerateForApp_Request.
    proto::Writer request;
    const QString& refresh = m_refreshToken.isEmpty() ? m_accessToken : m_refreshToken;
    request.stringField(1, refresh); // refresh_token
    if (m_steamId != 0)
        request.fixed64Field(2, m_steamId); // steamid

    const quint64 jobId = ++m_jobCounter;
    m_webTokenJobId = jobId;
    sendJob(QStringLiteral("Authentication.GenerateAccessTokenForApp#1"),
            request.data(), jobId);
    Logger::log(QStringLiteral("Steam CM: requested a web session token"));
}

void SteamCmClient::sendHello()
{
    proto::Writer body;
    body.varintField(1, kProtocolVersion); // protocol_version
    sendMessage(kEmsgClientHello, body.data());
}

void SteamCmClient::sendLogon()
{
    proto::Writer body;
    body.varintField(1, kProtocolVersion);      // protocol_version
    body.varintField(2, 0);                     // deprecated_obfustucated_private_ip
    body.varintField(3, 0);                     // cell_id
    body.varintField(5, 1771);                  // client_package_version
    body.stringField(6, QStringLiteral("english")); // client_language
    body.varintField(7, osType());              // client_os_type
    body.boolField(8, true);                    // should_remember_password
    body.varintField(31, 0);                    // launcher_type
    body.varintField(32, 4);                    // ui_mode
    body.varintField(33, 2);                    // chat_mode
    body.bytesField(30, machineIdBlob());       // machine_id
    body.stringField(96, QStringLiteral("CopperSteamIdler")); // machine_name
    body.varintField(100, clientInstanceId());  // client_instance_id
    body.boolField(102, true);                  // supports_rate_limit_response
    body.boolField(109, false);                 // is_chrome_os
    if (m_steamId != 0)
        body.fixed64Field(22, m_steamId);       // client_supplied_steam_id
    body.stringField(108, m_accessToken);       // access_token

    sendMessage(kEmsgClientLogon, body.data());
    Logger::log(QStringLiteral("Steam CM: logon request sent"));
}

void SteamCmClient::sendHeartbeat()
{
    if (m_state != State::LoggedOn)
        return;
    proto::Writer body;
    body.boolField(1, false); // send_reply
    sendMessage(kEmsgClientHeartBeat, body.data());
}

void SteamCmClient::onBinaryMessage(const QByteArray& payload)
{
    handlePacket(payload);
}

void SteamCmClient::handlePacket(const QByteArray& payload)
{
    if (payload.size() < 8)
        return;

    const quint32 rawEmsg = qFromLittleEndian<quint32>(payload.constData());
    const quint32 headerLength = qFromLittleEndian<quint32>(payload.constData() + 4);

    if (!(rawEmsg & kProtoMask)) {
        // Struct message; the only one that matters here has a fixed header.
        if (payload.size() < 20)
            return;
        handleMessage(rawEmsg, QByteArray(), payload.mid(20));
        return;
    }

    if (8 + static_cast<int>(headerLength) > payload.size())
        return;

    const quint32 emsg = rawEmsg & ~kProtoMask;
    const QByteArray header = payload.mid(8, static_cast<int>(headerLength));
    const QByteArray body = payload.mid(8 + static_cast<int>(headerLength));
    handleMessage(emsg, header, body);
}

void SteamCmClient::handleMulti(const QByteArray& body)
{
    proto::Reader reader(body);
    quint32 field = 0;
    quint32 wire = 0;

    int sizeUnzipped = 0;
    QByteArray messageBody;

    while (reader.nextTag(&field, &wire)) {
        if (field == 1 && wire == proto::Varint) {
            quint64 value = 0;
            reader.readVarint(&value);
            sizeUnzipped = static_cast<int>(value);
        } else if (field == 2 && wire == proto::LengthDelimited) {
            reader.readBytes(&messageBody);
        } else if (!reader.skip(wire)) {
            break;
        }
    }

    QByteArray payload = messageBody;
    if (sizeUnzipped > 0) {
        payload = inflateGzip(messageBody, sizeUnzipped);
        if (payload.isEmpty()) {
            Logger::log(QStringLiteral("Steam CM: could not decompress a Multi message"));
            return;
        }
    }

    // A Multi payload is a sequence of [int32 size][complete packet] entries.
    int offset = 0;
    while (offset + 4 <= payload.size()) {
        const qint32 subSize = qFromLittleEndian<qint32>(payload.constData() + offset);
        offset += 4;
        if (subSize <= 0 || offset + subSize > payload.size())
            break;
        handlePacket(payload.mid(offset, subSize));
        offset += subSize;
    }
}

void SteamCmClient::handleMessage(quint32 emsg, const QByteArray& header, const QByteArray& body)
{
    emit messageReceived(emsg, body.size());

    switch (emsg) {
    case kEmsgMulti:
        handleMulti(body);
        break;
    case kEmsgServiceMethodResponse:
        handleServiceMethodResponse(header, body);
        break;
    case kEmsgClientLogOnResponse:
        handleLogonResponse(body);
        break;
    case kEmsgClientLoggedOff:
        handleLoggedOff(body);
        break;
    case kEmsgClientSessionToken: {
        // The AM hands out the session token / instance id. Not needed for
        // idling, but the session id is useful for subsequent messages.
        proto::Reader reader(body);
        quint32 field = 0;
        quint32 wire = 0;
        while (reader.nextTag(&field, &wire)) {
            if (field == 1 && wire == proto::Varint) {
                quint64 token = 0;
                reader.readVarint(&token);
                Logger::log(QStringLiteral("Steam CM: session token received"));
            } else if (!reader.skip(wire)) {
                break;
            }
        }
        break;
    }
    case kEmsgClientKickPlayingSession:
        Logger::log(QStringLiteral("Steam CM: another session started playing; "
                                   "this client's game list may be cleared"));
        break;
    case kEmsgClientPlayingSessionState:
        break;
    default:
        break;
    }
}

void SteamCmClient::handleLogonResponse(const QByteArray& body)
{
    proto::Reader reader(body);
    quint32 field = 0;
    quint32 wire = 0;

    int eresult = 2;
    int extended = 0;
    int heartbeat = 9;
    qint32 sessionId = 0;
    quint64 steamId = 0;

    while (reader.nextTag(&field, &wire)) {
        if (field == 1 && wire == proto::Varint) {
            quint64 value = 0;
            reader.readVarint(&value);
            eresult = static_cast<int>(value);
        } else if (field == 2 && wire == proto::Varint) {
            quint64 value = 0;
            reader.readVarint(&value);
            extended = static_cast<int>(value);
        } else if (field == 3 && wire == proto::Varint) {
            quint64 value = 0;
            reader.readVarint(&value);
            heartbeat = static_cast<int>(value);
        } else if (field == 20 && wire == proto::Fixed64) {
            reader.readFixed64(&steamId);
        } else if (field == 27 && wire == proto::Varint) {
            quint64 value = 0;
            reader.readVarint(&value);
            sessionId = static_cast<qint32>(value);
        } else if (!reader.skip(wire)) {
            break;
        }
    }

    if (eresult != 1) {
        fail(QStringLiteral("Steam logon failed: %1 (extended %2)")
                 .arg(eResultName(eresult))
                 .arg(extended));
        return;
    }

    if (steamId != 0)
        m_steamId = steamId;
    m_sessionId = sessionId;

    if (heartbeat > 0)
        m_heartbeatSeconds = heartbeat;
    m_heartbeat->start(qMax(5, m_heartbeatSeconds / 2) * 1000);

    setState(State::LoggedOn);
    Logger::log(QStringLiteral("Steam CM: logged on as %1").arg(m_steamId));
    emit loggedOn(m_steamId);

    // A freshly logged-on session starts out invisible ("Offline" on the profile).
    // The official client marks itself online right after logon, and being online
    // is what makes the reported games show up on the profile and in friends'
    // lists.
    sendChangeStatus();

    // Re-apply whatever was requested before the logon completed.
    if (!m_games.isEmpty())
        setGamesPlayed(m_games);

    // A community session can only be minted over an authenticated connection,
    // so it is requested once the logon has succeeded.
    requestWebAccessToken();
}

void SteamCmClient::handleServiceMethodResponse(const QByteArray& header,
                                               const QByteArray& body)
{
    // Header: jobid_target (11) identifies the call; eresult (13) is its result.
    quint64 jobId = 0;
    qint64 eresult = 1;

    proto::Reader headerReader(header);
    quint32 field = 0;
    quint32 wire = 0;
    while (headerReader.nextTag(&field, &wire)) {
        if (field == 11) {
            // Steam sends jobid_target as a fixed64.
            if (wire == proto::Fixed64) {
                quint64 value = 0;
                if (headerReader.readFixed64(&value))
                    jobId = value;
            } else if (wire == proto::Varint) {
                headerReader.readVarint(&jobId);
            } else if (!headerReader.skip(wire)) {
                break;
            }
        } else if (field == 13 && wire == proto::Varint) {
            quint64 value = 0;
            headerReader.readVarint(&value);
            eresult = static_cast<qint64>(value);
        } else if (!headerReader.skip(wire)) {
            break;
        }
    }

    const bool isWebToken = m_webTokenJobId != 0 && jobId == m_webTokenJobId;
    if (!isWebToken)
        return;

    m_webTokenJobId = 0;

    if (eresult != 1) {
        Logger::log(QStringLiteral("Steam CM: web session token refused (%1)")
                        .arg(eResultName(static_cast<int>(eresult))));
        emit webAccessTokenReceived(QString(), QString());
        return;
    }

    // CAuthentication_AccessToken_GenerateForApp_Response.
    QString accessToken;
    QString refreshToken;
    proto::Reader reader(body);
    while (reader.nextTag(&field, &wire)) {
        if (wire == proto::LengthDelimited) {
            QByteArray value;
            if (!reader.readBytes(&value))
                break;
            if (field == 1)
                accessToken = QString::fromUtf8(value);
            else if (field == 2)
                refreshToken = QString::fromUtf8(value);
        } else if (!reader.skip(wire)) {
            break;
        }
    }

    if (accessToken.isEmpty()) {
        Logger::log(QStringLiteral("Steam CM: web session token missing from the response"));
        emit webAccessTokenReceived(QString(), QString());
        return;
    }

    // Steam rotates the long-lived token on every use; keep the newest copy so
    // the saved credentials stay valid.
    if (!refreshToken.isEmpty()) {
        m_refreshToken = refreshToken;
        emit refreshTokenRotated(refreshToken);
    }

    Logger::log(QStringLiteral("Steam CM: web session token received"));
    emit webAccessTokenReceived(accessToken, refreshToken);
}

void SteamCmClient::handleLoggedOff(const QByteArray& body)
{
    proto::Reader reader(body);
    quint32 field = 0;
    quint32 wire = 0;
    int eresult = 2;
    while (reader.nextTag(&field, &wire)) {
        if (field == 1 && wire == proto::Varint) {
            quint64 value = 0;
            reader.readVarint(&value);
            eresult = static_cast<int>(value);
        } else if (!reader.skip(wire)) {
            break;
        }
    }

    m_heartbeat->stop();
    Logger::log(QStringLiteral("Steam CM: logged off (%1)").arg(eResultName(eresult)));
    m_ws->close();
    setState(State::Disconnected);
    emit loggedOff(eresult);
}

// ---------------------------------------------------------------------------
// Games played
// ---------------------------------------------------------------------------

// CMsgClientChangeStatus: persona_state in field 1, player_name in field 2.
// Online is 1; the name is left out so Steam keeps the account's profile name.
void SteamCmClient::sendChangeStatus()
{
    proto::Writer body;
    body.varintField(1, 1); // persona_state = Online
    sendMessage(kEmsgClientChangeStatus, body.data());
    Logger::log(QStringLiteral("Steam CM: announced the session as online"));
}

void SteamCmClient::setGamesPlayed(const QList<quint32>& appIds){
    m_games = appIds;

    if (m_state != State::LoggedOn) {
        // Applied once the logon response arrives.
        return;
    }

    proto::Writer body;
    body.varintField(2, osType()); // client_os_type

    int sent = 0;
    for (quint32 appId : appIds) {
        if (appId == 0)
            continue;
        if (sent >= kMaxGamesPlayed)
            break;

        // CMsgClientGamesPlayed.GamePlayed: game_id lives in field 2 as a
        // fixed64 GameID. A normal app is just the app id: the type nibble is
        // in the high bits and stays zero.
        proto::Writer game;
        game.fixed64Field(2, static_cast<quint64>(appId));
        body.rawMessageField(1, game.data());
        ++sent;
    }

    sendMessage(kEmsgClientGamesPlayedWithDataBlob, body.data());

    if (appIds.isEmpty())
        Logger::log(QStringLiteral("Steam CM: cleared the played game list"));
    else
        Logger::log(QStringLiteral("Steam CM: reporting %1 game(s) as played").arg(sent));
}
