#include "CdpLoginDialog.h"

#include "../core/Settings.h"
#include "../core/WebSocketClient.h"
#include "Theme.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPushButton>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const char* kLoginUrl = "https://steamcommunity.com/login/home/?goto=";

QStringList browserCandidates()
{
    QStringList candidates;
    const QString override = qEnvironmentVariable("COPPER_BROWSER");
    if (!override.isEmpty())
        candidates << override;

    const QString pf = qEnvironmentVariable("ProgramFiles");
    const QString pf86 = qEnvironmentVariable("ProgramFiles(x86)");
    const QString local = qEnvironmentVariable("LOCALAPPDATA");

    candidates << pf86 + "/Microsoft/Edge/Application/msedge.exe"
               << pf + "/Microsoft/Edge/Application/msedge.exe"
               << pf + "/Google/Chrome/Application/chrome.exe"
               << pf86 + "/Google/Chrome/Application/chrome.exe"
               << local + "/Google/Chrome/Application/chrome.exe";
    return candidates;
}

quint16 freePort()
{
    QTcpServer server;
    if (server.listen(QHostAddress::LocalHost, 0)) {
        const quint16 port = server.serverPort();
        server.close();
        return port;
    }
    return 0;
}

} // namespace

CdpLoginDialog::CdpLoginDialog(QWidget* parent)
    : QDialog(parent)
    , m_process(new QProcess(this))
    , m_nam(new QNetworkAccessManager(this))
    , m_ws(new WebSocketClient(this))
{
    setWindowTitle(tr("Sign in to Steam"));
    setWindowIcon(Theme::appIcon());
    setModal(true);
    resize(460, 190);

    auto* root = new QVBoxLayout(this);

    auto* info = new QLabel(
        tr("A private browser window will open the Steam login page. Sign in "
           "there (including Steam Guard) and Copper Steam Idler will capture "
           "the secure cookie automatically. The browser profile is temporary."),
        this);
    info->setWordWrap(true);
    root->addWidget(info);

    m_status = new QLabel(tr("Starting browser..."), this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    root->addStretch();

    auto* row = new QHBoxLayout;
    m_btnManual = new QPushButton(tr("Enter cookie manually"), this);
    auto* btnCancel = new QPushButton(tr("&Cancel"), this);
    row->addWidget(m_btnManual);
    row->addStretch();
    row->addWidget(btnCancel);
    root->addLayout(row);

    connect(m_btnManual, &QPushButton::clicked, this, [this]() {
        m_manualRequested = true;
        reject();
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_nam, &QNetworkAccessManager::finished, this, [this](QNetworkReply* reply) {
        onDevToolsReply(reply);
    });

    connect(m_ws, &WebSocketClient::opened, this, &CdpLoginDialog::onSocketOpened);
    connect(m_ws, &WebSocketClient::textMessage, this, &CdpLoginDialog::onSocketMessage);
    connect(m_ws, &WebSocketClient::failed, this, &CdpLoginDialog::onSocketFailed);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) { onBrowserFinished(code); });

    m_devToolsTimer = new QTimer(this);
    m_devToolsTimer->setInterval(700);
    connect(m_devToolsTimer, &QTimer::timeout, this, &CdpLoginDialog::pollDevTools);

    m_cookieTimer = new QTimer(this);
    m_cookieTimer->setInterval(1500);
    connect(m_cookieTimer, &QTimer::timeout, this, &CdpLoginDialog::pollCookies);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(5 * 60 * 1000);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        fail(tr("Timed out waiting for sign-in."));
    });

    if (Settings::instance().customTheme())
        Theme::applyWidgetTheme(this, true);

    Theme::fitToScreen(this);

    QTimer::singleShot(0, this, &CdpLoginDialog::launchBrowser);
}

CdpLoginDialog::~CdpLoginDialog()
{
    stopBrowser();
}

QString CdpLoginDialog::findBrowser()
{
    const QStringList candidates = browserCandidates();
    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFileInfo::exists(path))
            return path;
    }
    return QString();
}

void CdpLoginDialog::launchBrowser()
{
    const QString exe = findBrowser();
    if (exe.isEmpty()) {
        fail(tr("No Edge or Chrome browser was found."));
        return;
    }

    m_port = freePort();
    if (m_port == 0) {
        fail(tr("Could not allocate a local debugging port."));
        return;
    }

    m_profileDir = new QTemporaryDir(QStringLiteral("copper-quicklogin-"));
    if (!m_profileDir->isValid()) {
        fail(tr("Could not create a temporary browser profile."));
        return;
    }

    QStringList args;
    args << QStringLiteral("--remote-debugging-port=%1").arg(m_port)
         << QStringLiteral("--user-data-dir=%1").arg(m_profileDir->path())
         << QStringLiteral("--no-first-run") << QStringLiteral("--no-default-browser-check")
         << QStringLiteral("--disable-extensions") << QStringLiteral("--disable-sync")
         << QStringLiteral("--new-window") << QString::fromLatin1(kLoginUrl);

    m_process->start(exe, args);
    m_status->setText(tr("Waiting for the browser debug port..."));

    m_devToolsTimer->start();
    m_timeoutTimer->start();
}

void CdpLoginDialog::pollDevTools()
{
    m_elapsedMs += m_devToolsTimer->interval();
    if (m_elapsedMs > 30000) {
        fail(tr("Could not reach the browser's debug port."));
        return;
    }
    if (m_pendingReply)
        return;

    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1/json/version").arg(m_port)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_pendingReply = m_nam->get(request);
}

void CdpLoginDialog::onDevToolsReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply == m_pendingReply)
        m_pendingReply = nullptr;

    if (m_connected || reply->error() != QNetworkReply::NoError)
        return;

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    const QString wsUrl = obj.value(QStringLiteral("webSocketDebuggerUrl")).toString();
    if (wsUrl.isEmpty())
        return;

    m_devToolsTimer->stop();
    m_status->setText(tr("Waiting for sign-in..."));
    m_ws->open(QUrl(wsUrl));
}

void CdpLoginDialog::onSocketOpened()
{
    m_connected = true;
    m_status->setText(tr("Waiting for sign-in..."));
    m_cookieTimer->start();
    pollCookies();
}

void CdpLoginDialog::pollCookies()
{
    if (!m_ws->isConnected())
        return;
    const int id = ++m_requestId;
    const QString request = QStringLiteral(
        "{\"id\":%1,\"method\":\"Storage.getCookies\",\"params\":{}}").arg(id);
    m_ws->sendText(request);
}

QString CdpLoginDialog::extractCookie(const QByteArray& json, const QString& name,
                                      const QString& domain) const
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QJsonObject result = root.value(QStringLiteral("result")).toObject();
    const QJsonArray cookies = result.value(QStringLiteral("cookies")).toArray();
    for (const QJsonValue& value : cookies) {
        const QJsonObject cookie = value.toObject();
        if (cookie.value(QStringLiteral("name")).toString() != name)
            continue;
        if (!cookie.value(QStringLiteral("domain")).toString().contains(domain))
            continue;
        const QString cookieValue = cookie.value(QStringLiteral("value")).toString();
        if (cookieValue.isEmpty() || cookieValue == QLatin1String("deleted"))
            continue;
        return cookieValue;
    }
    return QString();
}

void CdpLoginDialog::onSocketMessage(const QString& message)
{
    const QByteArray payload = message.toUtf8();
    if (m_cookie.isEmpty())
        m_cookie = extractCookie(payload, QStringLiteral("steamLoginSecure"),
                                 QStringLiteral("steamcommunity.com"));

    if (m_cookie.isEmpty())
        return;

    m_status->setText(tr("Signed in. Finishing up..."));
    stopBrowser();
    accept();
}

void CdpLoginDialog::onSocketFailed(const QString& reason)
{
    if (m_cookie.isEmpty())
        m_status->setText(tr("Connection to browser failed: %1").arg(reason));
}

void CdpLoginDialog::onBrowserFinished(int)
{
    if (!m_cookie.isEmpty())
        return;
    if (m_devToolsTimer)
        m_devToolsTimer->stop();
    if (m_cookieTimer)
        m_cookieTimer->stop();
    m_status->setText(tr("The browser was closed before sign-in completed."));
}

void CdpLoginDialog::fail(const QString& message)
{
    if (m_devToolsTimer)
        m_devToolsTimer->stop();
    if (m_cookieTimer)
        m_cookieTimer->stop();
    m_status->setText(message);
}

void CdpLoginDialog::stopBrowser()
{
    if (m_devToolsTimer)
        m_devToolsTimer->stop();
    if (m_cookieTimer)
        m_cookieTimer->stop();
    if (m_timeoutTimer)
        m_timeoutTimer->stop();

    if (m_ws)
        m_ws->close();

    if (m_process && m_process->state() != QProcess::NotRunning) {
        const qint64 pid = m_process->processId();
        if (pid > 0) {
            QProcess::execute(QStringLiteral("taskkill"),
                              {QStringLiteral("/F"), QStringLiteral("/T"),
                               QStringLiteral("/PID"), QString::number(pid)});
        }
        m_process->waitForFinished(2000);
    }

    if (m_profileDir) {
        // Best effort; the browser may still hold a lock briefly.
        delete m_profileDir;
        m_profileDir = nullptr;
    }
}
