#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QPushButton;
class QTemporaryDir;
class QTimer;
class WebSocketClient;

// Automatic Steam login by driving the user's installed Chromium browser
// (Edge/Chrome) over the Chrome DevTools Protocol. The browser is launched with
// a throwaway profile and a local debugging port; once the user finishes
// signing in the steamLoginSecure cookie is read via Storage.getCookies.
//
// This is used when Qt WebEngine is not available.
class CdpLoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CdpLoginDialog(QWidget* parent = nullptr);
    ~CdpLoginDialog() override;

    QString capturedCookie() const { return m_cookie; }

    // True when the user asked to enter the cookie manually instead.
    bool manualRequested() const { return m_manualRequested; }

    // Absolute path to a usable Chromium browser (empty when none found).
    static QString findBrowser();

private slots:
    void pollDevTools();
    void onDevToolsReply(QNetworkReply* reply);
    void onSocketOpened();
    void onSocketMessage(const QString& message);
    void onSocketFailed(const QString& reason);
    void pollCookies();
    void onBrowserFinished(int exitCode);

private:
    void launchBrowser();
    void stopBrowser();
    void fail(const QString& message);
    QString extractCookie(const QByteArray& json) const;

    QProcess* m_process = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_pendingReply = nullptr;
    WebSocketClient* m_ws = nullptr;
    QTemporaryDir* m_profileDir = nullptr;
    QTimer* m_devToolsTimer = nullptr;
    QTimer* m_cookieTimer = nullptr;
    QTimer* m_timeoutTimer = nullptr;

    QLabel* m_status = nullptr;
    QPushButton* m_btnManual = nullptr;

    quint16 m_port = 0;
    int m_requestId = 0;
    int m_elapsedMs = 0;
    bool m_connected = false;
    bool m_manualRequested = false;
    QString m_cookie;
};
