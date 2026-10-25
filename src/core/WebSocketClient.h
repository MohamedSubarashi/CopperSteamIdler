#pragma once

#include <QByteArray>
#include <QObject>
#include <QUrl>

class QTcpSocket;

// Minimal RFC 6455 WebSocket client (client-side masking, text and binary
// frames, ws:// and wss://).
//
// Deliberately hand-rolled instead of depending on the optional Qt WebSockets
// module. It talks to the local Chrome/Edge DevTools endpoint for login, and to
// Steam's CM servers for standalone idling, which exchanges binary frames.
class WebSocketClient : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketClient(QObject* parent = nullptr);

    void open(const QUrl& url);
    void sendText(const QString& text);
    void sendBinary(const QByteArray& payload);
    void close();

    bool isConnected() const { return m_handshakeDone; }

signals:
    void opened();
    void textMessage(const QString& message);
    void binaryMessage(const QByteArray& payload);
    void closed();
    void failed(const QString& reason);

private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError();

private:
    void sendFrame(quint8 opcode, const QByteArray& payload);
    void processFrames();
    void handleMessageFrame(quint8 opcode, bool fin, const QByteArray& payload);

    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;
    bool m_handshakeDone = false;
    QByteArray m_messageBuffer;
    bool m_messageInProgress = false;
    quint8 m_messageOpcode = 0;
    bool m_useTls = false;
    QString m_path;
    QString m_hostHeader;
};
