#include "WebSocketClient.h"

#include <QByteArray>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QTcpSocket>
#include <QUrl>

namespace {

QByteArray makeMask()
{
    QByteArray mask(4, 0);
    for (int i = 0; i < 4; ++i)
        mask[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return mask;
}

QByteArray makeKey()
{
    QByteArray key(16, 0);
    for (int i = 0; i < 16; ++i)
        key[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return key.toBase64();
}

} // namespace

WebSocketClient::WebSocketClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QSslSocket(this))
{
    auto* ssl = static_cast<QSslSocket*>(m_socket);

    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        // For wss:// the HTTP upgrade must wait until the TLS handshake is done;
        // QSslSocket emits connected() for the TCP connection before that.
        if (!m_useTls)
            onConnected();
    });
    connect(ssl, &QSslSocket::encrypted, this, &WebSocketClient::onConnected);
    connect(ssl, &QSslSocket::sslErrors, this, [this](const QList<QSslError>& errors) {
        if (!m_handshakeDone && !errors.isEmpty())
            emit failed(errors.first().errorString());
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &WebSocketClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        if (m_handshakeDone) {
            m_handshakeDone = false;
            emit closed();
        }
    });
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &WebSocketClient::onSocketError);
}

void WebSocketClient::open(const QUrl& url)
{
    m_buffer.clear();
    m_handshakeDone = false;
    m_messageBuffer.clear();
    m_messageInProgress = false;
    m_messageOpcode = 0;

    m_useTls = url.scheme().compare(QLatin1String("wss"), Qt::CaseInsensitive) == 0;

    m_path = url.path().isEmpty() ? QStringLiteral("/") : url.path();
    if (url.hasQuery())
        m_path += QLatin1Char('?') + url.query();
    const int defaultPort = m_useTls ? 443 : 80;
    m_hostHeader = url.host() + QLatin1Char(':') + QString::number(url.port(defaultPort));

    const quint16 port = static_cast<quint16>(url.port(defaultPort));
    if (m_useTls)
        static_cast<QSslSocket*>(m_socket)->connectToHostEncrypted(url.host(), port);
    else
        m_socket->connectToHost(url.host(), port);
}

void WebSocketClient::onConnected()
{
    const QByteArray key = makeKey();
    QByteArray request;
    request += "GET " + m_path.toUtf8() + " HTTP/1.1\r\n";
    request += "Host: " + m_hostHeader.toUtf8() + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + key + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "\r\n";
    m_socket->write(request);
}

void WebSocketClient::onSocketError()
{
    if (!m_handshakeDone)
        emit failed(m_socket->errorString());
}

void WebSocketClient::onReadyRead()
{
    m_buffer += m_socket->readAll();

    if (!m_handshakeDone) {
        const int headerEnd = m_buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        const QByteArray header = m_buffer.left(headerEnd);
        m_buffer.remove(0, headerEnd + 4);
        if (!header.startsWith("HTTP/1.1 101") && !header.contains("101 Web Socket")) {
            emit failed(QStringLiteral("WebSocket handshake failed"));
            m_socket->abort();
            return;
        }
        m_handshakeDone = true;
        emit opened();
    }

    processFrames();
}

void WebSocketClient::processFrames()
{
    while (true) {
        if (m_buffer.size() < 2)
            return;

        const quint8 b0 = static_cast<quint8>(m_buffer.at(0));
        const quint8 b1 = static_cast<quint8>(m_buffer.at(1));
        const bool fin = (b0 & 0x80) != 0;
        const quint8 opcode = b0 & 0x0f;
        const bool masked = (b1 & 0x80) != 0;
        quint64 length = b1 & 0x7f;
        int offset = 2;

        if (length == 126) {
            if (m_buffer.size() < offset + 2)
                return;
            length = (static_cast<quint8>(m_buffer.at(offset)) << 8)
                     | static_cast<quint8>(m_buffer.at(offset + 1));
            offset += 2;
        } else if (length == 127) {
            if (m_buffer.size() < offset + 8)
                return;
            length = 0;
            for (int i = 0; i < 8; ++i)
                length = (length << 8) | static_cast<quint8>(m_buffer.at(offset + i));
            offset += 8;
        }

        QByteArray mask;
        if (masked) {
            if (m_buffer.size() < offset + 4)
                return;
            mask = m_buffer.mid(offset, 4);
            offset += 4;
        }

        if (static_cast<quint64>(m_buffer.size()) < static_cast<quint64>(offset) + length)
            return;

        QByteArray payload = m_buffer.mid(offset, static_cast<int>(length));
        m_buffer.remove(0, offset + static_cast<int>(length));

        if (masked) {
            for (int i = 0; i < payload.size(); ++i)
                payload[i] = payload.at(i) ^ mask.at(i % 4);
        }

        if (opcode == 0x8) { // close
            m_socket->close();
            if (m_handshakeDone) {
                m_handshakeDone = false;
                emit closed();
            }
            return;
        }
        if (opcode == 0x9) { // ping -> pong
            sendFrame(0xA, payload);
            continue;
        }
        if (opcode == 0xA) // pong
            continue;

        handleMessageFrame(opcode, fin, payload);
    }
}

void WebSocketClient::handleMessageFrame(quint8 opcode, bool fin, const QByteArray& payload)
{
    if (opcode == 0x1 || opcode == 0x2)
        m_messageOpcode = opcode;

    m_messageBuffer += payload;
    m_messageInProgress = !fin;

    if (!fin)
        return;

    if (m_messageOpcode == 0x1)
        emit textMessage(QString::fromUtf8(m_messageBuffer));
    else if (m_messageOpcode == 0x2)
        emit binaryMessage(m_messageBuffer);

    m_messageBuffer.clear();
    m_messageOpcode = 0;
    m_messageInProgress = false;
}

void WebSocketClient::sendText(const QString& text)
{
    sendFrame(0x1, text.toUtf8());
}

void WebSocketClient::sendBinary(const QByteArray& payload)
{
    sendFrame(0x2, payload);
}

void WebSocketClient::sendFrame(quint8 opcode, const QByteArray& payload)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    QByteArray frame;
    frame.append(static_cast<char>(0x80 | opcode)); // FIN + opcode

    const int len = payload.size();
    const QByteArray mask = makeMask();
    if (len < 126) {
        frame.append(static_cast<char>(0x80 | len));
    } else if (len <= 0xFFFF) {
        frame.append(static_cast<char>(0x80 | 126));
        frame.append(static_cast<char>((len >> 8) & 0xFF));
        frame.append(static_cast<char>(len & 0xFF));
    } else {
        frame.append(static_cast<char>(0x80 | 127));
        for (int i = 7; i >= 0; --i)
            frame.append(static_cast<char>((static_cast<quint64>(len) >> (i * 8)) & 0xFF));
    }

    frame.append(mask);
    QByteArray masked = payload;
    for (int i = 0; i < masked.size(); ++i)
        masked[i] = masked.at(i) ^ mask.at(i % 4);
    frame.append(masked);

    m_socket->write(frame);
}

void WebSocketClient::close()
{
    if (!m_socket)
        return;
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        sendFrame(0x8, QByteArray());
    m_socket->close();
}
