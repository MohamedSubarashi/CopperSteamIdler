#include "MainWindow.h"

#include "../core/Badge.h"
#include "../core/HttpClient.h"
#include "../core/IdleBackend.h"
#include "../core/Logger.h"
#include "../core/PowerManager.h"
#include "../core/ProcessManager.h"
#include "../core/Settings.h"
#include "../core/SteamHtml.h"
#include "../core/SteamProfile.h"
#include "AboutDialog.h"
#include "AdvancedDialog.h"
#include "BlacklistDialog.h"
#include "BrowserLoginDialog.h"
#include "GamesDialog.h"
#include "SettingsDialog.h"
#include "StatisticsDialog.h"
#include "Theme.h"
#include "WhitelistDialog.h"

#ifdef QUICK_LOGIN_ENABLED
#  include "QuickLoginDialog.h"
#endif

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QMovie>
#include <QNetworkReply>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QProgressBar>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const char* kWikiUrl = "https://github.com/MohamedSubarashi/CopperSteamIdler/wiki";
const char* kReleasesUrl = "https://github.com/MohamedSubarashi/CopperSteamIdler/releases";
const char* kGroupUrl = "https://steamcommunity.com/groups/coppersteamidler";
const char* kDonateUrl = "https://ko-fi.com/mohamedsubarashi";

int boundedDelay(int base, int spread)
{
    return base + QRandomGenerator::global()->bounded(spread);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Copper Steam Idler"));
    setWindowIcon(Theme::appIcon());

    buildUi();
    buildMenus();
    setupTimers();
    setupTray();
    restoreGeometrySettings();

    m_lblCurrentStatus->setText(tr("Not in game"));
    m_lblDrops->hide();
    m_lblIdle->hide();
    m_lblSignedOnAs->hide();
    m_lblHoursPlayed->hide();
    m_lblCurrentRemaining->hide();
    showSpinner(m_picReadingPage, false);
    showSpinner(m_picIdleStatus, false);
    m_picApp->hide();
    m_gamesState->hide();
    m_footer->hide();
    m_btnPause->hide();
    m_btnResume->hide();
    m_btnSkip->hide();
    m_lnkSignIn->hide();
    m_lnkSignOut->hide();

    setTheme();
    applyIcons();

    m_lblSteamStatus->setText(tr("Steam is not running"));
    m_lblCookieStatus->setText(tr("Copper Steam Idler is not connected to Steam"));
    m_lblLatestRelease->setText(QStringLiteral("<a href=\"%1\">v%2</a>")
                                    .arg(QString::fromLatin1(kReleasesUrl),
                                         QApplication::applicationVersion()));

    Logger::log(QStringLiteral("Copper Steam Idler started"));

    // A standalone session opens asynchronously; when it becomes ready, pick the
    // idle cycle back up where it left off.
    connect(&IdleBackend::instance(), &IdleBackend::standaloneStateChanged, this,
            [this](bool ready) {
                if (ready && !m_idlingStarted)
                    startIdle();
            });
    connect(&IdleBackend::instance(), &IdleBackend::standaloneFailed, this,
            [this](const QString& reason) {
                if (m_idlingStarted)
                    return;
                m_lblCurrentStatus->setText(tr("Could not connect to Steam: %1").arg(reason));
            });

    onCheckSteam();
    onCheckCookieData();
    m_tmrCheckSteam->start();
    m_tmrCheckCookieData->start();
    m_tmrReadyToGo->start();
}

MainWindow::~MainWindow()
{
    // Close the standalone CM session first so Steam stops showing this account
    // in-game, then make sure no helper process is left behind.
    IdleBackend::instance().stopAll();
    IdleBackend::instance().stopStandalone();
    clearBadges();
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(6);

    auto* steamRow = new QHBoxLayout;
    m_picSteamStatus = new QLabel(central);
    m_picSteamStatus->setFixedSize(16, 16);
    m_lblSteamStatus = new QLabel(central);
    m_lblLatestRelease = new QLabel(central);
    m_lblLatestRelease->setOpenExternalLinks(true);
    steamRow->addWidget(m_picSteamStatus);
    steamRow->addWidget(m_lblSteamStatus);
    steamRow->addStretch();
    steamRow->addWidget(m_lblLatestRelease);
    root->addLayout(steamRow);

    auto* cookieRow = new QHBoxLayout;
    m_picCookieStatus = new QLabel(central);
    m_picCookieStatus->setFixedSize(16, 16);
    m_lblCookieStatus = new QLabel(central);
    m_lnkSignIn = new QLabel(central);
    m_lnkSignIn->setText(QStringLiteral("<a href=\"signin\">(%1)</a>").arg(tr("Sign In")));
    m_lnkSignIn->setCursor(Qt::PointingHandCursor);
    m_lnkSignOut = new QLabel(central);
    m_lnkSignOut->setText(QStringLiteral("<a href=\"signout\">(%1)</a>").arg(tr("Sign Out")));
    m_lnkSignOut->setCursor(Qt::PointingHandCursor);
    cookieRow->addWidget(m_picCookieStatus);
    cookieRow->addWidget(m_lblCookieStatus);
    cookieRow->addStretch();
    cookieRow->addWidget(m_lnkSignIn);
    cookieRow->addWidget(m_lnkSignOut);
    root->addLayout(cookieRow);

    m_lblSignedOnAs = new QLabel(central);
    root->addWidget(m_lblSignedOnAs);

    auto* readRow = new QHBoxLayout;
    m_picReadingPage = new QLabel(central);
    m_picReadingPage->setFixedSize(16, 16);
    m_lblDrops = new QLabel(central);
    m_lblIdle = new QLabel(central);
    readRow->addWidget(m_picReadingPage);
    readRow->addWidget(m_lblDrops);
    readRow->addWidget(m_lblIdle);
    readRow->addStretch();
    root->addLayout(readRow);

    auto* gameRow = new QHBoxLayout;
    m_lblCurrentStatus = new QLabel(central);
    m_lblGameName = new QLabel(central);
    m_lblGameName->setOpenExternalLinks(false);
    m_lblGameName->setCursor(Qt::PointingHandCursor);
    m_btnPause = new QToolButton(central);
    m_btnPause->setAutoRaise(true);
    m_btnPause->setFixedSize(20, 20);
    m_btnPause->setToolTip(tr("Pause Idling"));
    m_btnResume = new QToolButton(central);
    m_btnResume->setAutoRaise(true);
    m_btnResume->setFixedSize(20, 20);
    m_btnResume->setToolTip(tr("Resume Idling"));
    m_btnSkip = new QToolButton(central);
    m_btnSkip->setAutoRaise(true);
    m_btnSkip->setFixedSize(20, 20);
    m_btnSkip->setToolTip(tr("Skip Current Game"));
    gameRow->addWidget(m_lblCurrentStatus);
    gameRow->addWidget(m_lblGameName);
    gameRow->addStretch();
    gameRow->addWidget(m_btnPause);
    gameRow->addWidget(m_btnResume);
    gameRow->addWidget(m_btnSkip);
    root->addLayout(gameRow);

    // Header image / games list share the same slot.
    auto* stack = new QStackedWidget(central);
    m_picApp = new QLabel(stack);
    m_picApp->setMinimumSize(275, 139);
    m_picApp->setAlignment(Qt::AlignCenter);
    m_gamesState = new QTreeWidget(stack);
    m_gamesState->setColumnCount(2);
    m_gamesState->setHeaderLabels({tr("Name"), tr("Hours")});
    m_gamesState->setRootIsDecorated(false);
    m_gamesState->setUniformRowHeights(true);
    m_gamesState->setMinimumHeight(139);
    stack->addWidget(m_picApp);
    stack->addWidget(m_gamesState);
    root->addWidget(stack, 1);

    auto* remRow = new QHBoxLayout;
    m_lblCurrentRemaining = new QLabel(central);
    m_lblCurrentRemaining->setCursor(Qt::PointingHandCursor);
    m_lblHoursPlayed = new QLabel(central);
    m_picIdleStatus = new QLabel(central);
    m_picIdleStatus->setFixedSize(16, 16);
    remRow->addWidget(m_lblCurrentRemaining);
    remRow->addStretch();
    remRow->addWidget(m_lblHoursPlayed);
    remRow->addWidget(m_picIdleStatus);
    root->addLayout(remRow);

    m_footer = new QWidget(central);
    auto* footRow = new QHBoxLayout(m_footer);
    footRow->setContentsMargins(0, 0, 0, 0);
    m_pbIdle = new QProgressBar(m_footer);
    m_pbIdle->setTextVisible(false);
    m_pbIdle->setRange(0, 1);
    m_pbIdle->setValue(0);
    auto* nextCheck = new QLabel(tr("Next check"), m_footer);
    nextCheck->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lblTimer = new QLabel(QStringLiteral("00:00"), m_footer);
    footRow->addWidget(m_pbIdle, 1);
    footRow->addWidget(nextCheck);
    footRow->addWidget(m_lblTimer);
    root->addWidget(m_footer);

    setCentralWidget(central);
    setMinimumSize(360, 430);
    resize(380, 480);

    connect(m_lnkSignIn, &QLabel::linkActivated, this, &MainWindow::onQuickLogin);
    connect(m_lnkSignOut, &QLabel::linkActivated, this, &MainWindow::onSignOut);
    connect(m_lblGameName, &QLabel::linkActivated, this, &MainWindow::onGameNameClicked);
    connect(m_lblCurrentRemaining, &QLabel::linkActivated, this,
            &MainWindow::onCurrentRemainingClicked);
    connect(m_btnPause, &QToolButton::clicked, this, &MainWindow::onPause);
    connect(m_btnResume, &QToolButton::clicked, this, &MainWindow::onResume);
    connect(m_btnSkip, &QToolButton::clicked, this, &MainWindow::onSkip);
}

void MainWindow::buildMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    m_actQuickLogin = fileMenu->addAction(tr("&Sign In..."));
    m_actSettings = fileMenu->addAction(tr("&Settings"));
    m_actBlacklist = fileMenu->addAction(tr("&Blacklist"));
    m_actWhitelist = fileMenu->addAction(tr("&Whitelist"));
    m_actSelectGames = fileMenu->addAction(tr("&Select Games..."));
    fileMenu->addSeparator();
    m_actDonate = fileMenu->addAction(tr("&Donate"));
    m_actExit = fileMenu->addAction(tr("E&xit"));

    QMenu* gameMenu = menuBar()->addMenu(tr("&Game"));
    m_actPause = gameMenu->addAction(tr("&Pause Idling"));
    m_actResume = gameMenu->addAction(tr("&Resume Idling"));
    m_actSkip = gameMenu->addAction(tr("&Skip Current Game"));
    gameMenu->addSeparator();
    m_actBlacklistCurrent = gameMenu->addAction(tr("&Blacklist Current Game"));

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    m_actWiki = helpMenu->addAction(tr("&Wiki Page"));
    m_actChangelog = helpMenu->addAction(tr("&Release Notes"));
    m_actGroup = helpMenu->addAction(tr("&Official Group"));
    m_actStatistics = helpMenu->addAction(tr("&Statistics"));
    helpMenu->addSeparator();
    m_actAbout = helpMenu->addAction(tr("&About"));

    m_actResume->setEnabled(false);

    connect(m_actQuickLogin, &QAction::triggered, this, &MainWindow::onQuickLogin);
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);
    connect(m_actBlacklist, &QAction::triggered, this, &MainWindow::onOpenBlacklist);
    connect(m_actWhitelist, &QAction::triggered, this, &MainWindow::onOpenWhitelist);
    connect(m_actSelectGames, &QAction::triggered, this, &MainWindow::onOpenGames);
    connect(m_actDonate, &QAction::triggered, this, [this]() {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kDonateUrl)));
    });
    connect(m_actExit, &QAction::triggered, this, &QWidget::close);
    connect(m_actPause, &QAction::triggered, this, &MainWindow::onPause);
    connect(m_actResume, &QAction::triggered, this, &MainWindow::onResume);
    connect(m_actSkip, &QAction::triggered, this, &MainWindow::onSkip);
    connect(m_actBlacklistCurrent, &QAction::triggered, this, &MainWindow::onBlacklistCurrentGame);
    connect(m_actWiki, &QAction::triggered, this, [this]() {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kWikiUrl)));
    });
    connect(m_actChangelog, &QAction::triggered, this, [this]() {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kReleasesUrl)));
    });
    connect(m_actGroup, &QAction::triggered, this, [this]() {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kGroupUrl)));
    });
    connect(m_actStatistics, &QAction::triggered, this, &MainWindow::onOpenStatistics);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::onOpenAbout);
}

void MainWindow::setupTimers()
{
    m_tmrCheckSteam = new QTimer(this);
    m_tmrCheckSteam->setInterval(500);
    connect(m_tmrCheckSteam, &QTimer::timeout, this, &MainWindow::onCheckSteam);

    m_tmrCheckCookieData = new QTimer(this);
    m_tmrCheckCookieData->setInterval(500);
    connect(m_tmrCheckCookieData, &QTimer::timeout, this, &MainWindow::onCheckCookieData);

    m_tmrReadyToGo = new QTimer(this);
    m_tmrReadyToGo->setInterval(1000);
    connect(m_tmrReadyToGo, &QTimer::timeout, this, &MainWindow::onReadyToGo);

    m_tmrCardDropCheck = new QTimer(this);
    m_tmrCardDropCheck->setInterval(1000);
    connect(m_tmrCardDropCheck, &QTimer::timeout, this, &MainWindow::onCardDropCheck);

    m_tmrStartNext = new QTimer(this);
    m_tmrStartNext->setSingleShot(true);
    connect(m_tmrStartNext, &QTimer::timeout, this, &MainWindow::onStartNext);

    m_tmrBadgeReload = new QTimer(this);
    m_tmrBadgeReload->setInterval(1000);
    connect(m_tmrBadgeReload, &QTimer::timeout, this, &MainWindow::onBadgeReload);

    m_tmrStatistics = new QTimer(this);
    m_tmrStatistics->setInterval(60000);
    connect(m_tmrStatistics, &QTimer::timeout, this, &MainWindow::onStatisticsTick);

    m_tmrHealth = new QTimer(this);
    m_tmrHealth->setInterval(10000);
    connect(m_tmrHealth, &QTimer::timeout, this, &MainWindow::onHealthCheck);

    m_tmrStatistics->start();
    m_tmrHealth->start();
}

void MainWindow::setupTray()
{
    // Create the icon even if the notification area reports itself unavailable:
    // at launch (especially when started with Windows) Explorer may not be ready
    // yet, and Qt shows the icon once it is. The important part is that a tray
    // icon object always exists so minimizing can never strand a hidden window.
    m_tray = new QSystemTrayIcon(Theme::appIcon(), this);
    m_tray->setToolTip(QStringLiteral("Copper Steam Idler"));

    auto* menu = new QMenu(this);
    QAction* showAction = menu->addAction(tr("&Show Copper Steam Idler"));
    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    menu->addSeparator();
    menu->addAction(m_actPause);
    menu->addAction(m_actResume);
    menu->addAction(m_actSkip);
    menu->addSeparator();
    menu->addAction(m_actExit);
    m_tray->setContextMenu(menu);

    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    // Keep the tray icon visible for the whole session so it is easy to find.
    m_tray->show();
}

void MainWindow::restoreGeometrySettings()
{
    QSettings s(Settings::dataDirectory() + "/settings.ini", QSettings::IniFormat);
    const QByteArray geo = s.value("windowGeometry").toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
}

void MainWindow::startHidden()
{
    if (m_tray) {
        m_tray->show();
        hide();
    } else {
        showMinimized();
    }
}

// ---------------------------------------------------------------------------
// Status ticks
// ---------------------------------------------------------------------------

void MainWindow::onCheckSteam()
{
    Settings& s = Settings::instance();
    const bool running = ProcessManager::isProcessRunning(QStringLiteral("steam.exe"));
    const bool standalone = IdleBackend::resolveMode() == IdleBackend::Mode::Standalone;

    // In standalone mode the Steam client is not needed at all: the app holds its
    // own connection to Steam's CM network.
    m_isSteamReady = running || s.ignoreClient() || standalone;

    if (standalone) {
        m_lblSteamStatus->setText(tr("No Steam client needed (standalone mode)"));
        m_picSteamStatus->setPixmap(Theme::statusIcon(true).pixmap(16, 16));
    } else if (s.ignoreClient()) {
        m_lblSteamStatus->setText(tr("Steam client status ignored"));
        m_picSteamStatus->setPixmap(Theme::statusIcon(true).pixmap(16, 16));
    } else if (running) {
        m_lblSteamStatus->setText(tr("Steam is running"));
        m_picSteamStatus->setPixmap(Theme::statusIcon(true).pixmap(16, 16));
    } else {
        m_lblSteamStatus->setText(tr("Steam is not running"));
        m_picSteamStatus->setPixmap(Theme::statusIcon(false).pixmap(16, 16));
    }

    m_tmrCheckSteam->setInterval((running || standalone) ? 5000 : 500);

    m_actPause->setEnabled(m_isSteamReady);
    m_actSkip->setEnabled(m_isSteamReady);
}

void MainWindow::onCheckCookieData()
{
    Settings& s = Settings::instance();
    const bool wasConnected = m_isCookieReady;
    const bool connected = s.hasSession();
    m_isCookieReady = connected;

    if (connected) {
        m_lblCookieStatus->setText(tr("Copper Steam Idler is connected to Steam"));
        m_lblCookieStatus->setStyleSheet(QStringLiteral("color: #7ea64b;"));
        m_picCookieStatus->setPixmap(Theme::statusIcon(true).pixmap(16, 16));
    } else {
        m_lblCookieStatus->setText(tr("Copper Steam Idler is not connected to Steam"));
        m_lblCookieStatus->setStyleSheet(QString());
        m_picCookieStatus->setPixmap(Theme::statusIcon(false).pixmap(16, 16));
    }

    m_lnkSignIn->setVisible(!connected);
    m_lnkSignOut->setVisible(connected);

    if (!connected)
        m_signedAsName.clear();

    if (connected && s.showUsername()) {
        if (m_signedAsName.isEmpty())
            m_signedAsName = SteamProfile::signedAs();
        m_lblSignedOnAs->setText(m_signedAsName);
        m_lblSignedOnAs->show();
    } else if (!s.showUsername()) {
        m_lblSignedOnAs->hide();
    }

    // A fresh sign-in should resume the startup pipeline, which may already have
    // stopped because it had no session to work with.
    if (!wasConnected && connected && !m_idlingStarted)
        m_tmrReadyToGo->start();
}

void MainWindow::onReadyToGo()
{
    if (!m_isSteamReady || !m_isCookieReady)
        return;
    if (m_idlingStarted || m_currentBadge)
        return;

    m_tmrReadyToGo->stop();

    const QString profile = Settings::instance().myProfileURL();
    if (profile.isEmpty()) {
        resetClientStatus();
        return;
    }

    if (HttpClient::instance().hasValidSession()) {
        m_loginFailures = 0;
        if (loadBadges()) {
            startIdle();
        } else if (!m_tmrBadgeReload->isActive()) {
            // loadBadges() could not even start a retry (for example the badge
            // page came back empty outside the retry path): try again shortly
            // instead of silently stalling after the timer above was stopped.
            m_tmrReadyToGo->start();
        }
    } else if (HttpClient::instance().isRateLimited()) {
        m_lblCurrentStatus->setText(tr("Steam is rate limiting requests, waiting..."));
        m_tmrReadyToGo->start();
    } else {
        // Give the token refresh a couple of tries, then ask the user to
        // sign in again instead of silently failing forever.
        if (++m_loginFailures >= 3) {
            m_loginFailures = 0;
            Logger::log(QStringLiteral("Steam session could not be validated; signing out so the "
                                       "user can sign in again."));
            HttpClient::instance().clearCookies();
            onCheckCookieData();
            return;
        }
        HttpClient::instance().refreshLoginToken();
        m_tmrReadyToGo->start();
    }
}

void MainWindow::onStatisticsTick()
{
    if (m_idlingStarted && !m_paused)
        m_statistics.increaseMinutesIdled();
}

void MainWindow::onHealthCheck()
{
    if (!m_idlingStarted || m_paused)
        return;

    // Restart helpers that exited unexpectedly so a game is never silently
    // dropped from the idle session.
    if (m_currentBadge) {
        if (!m_currentBadge->inIdle())
            m_currentBadge->idle();
    } else if (IdleBackend::instance().idlingCount() == 0 && !canIdleBadges().isEmpty()) {
        updateIdleProcesses();
    }
}

// ---------------------------------------------------------------------------
// Badge loading
// ---------------------------------------------------------------------------

void MainWindow::clearBadges()
{
    qDeleteAll(m_allBadges);
    m_allBadges.clear();
    m_currentBadge = nullptr;
    m_libraryLoaded = false;
}

bool MainWindow::checkIfResponseIsNullWithRetryCount(const QString& html)
{
    if (!html.isEmpty())
        return true;

    ++m_retryCount;
    m_badgeReloadCount = 1;
    m_lblDrops->setText(tr("Badge page didn't load, will retry in %1 seconds").arg(10));
    m_lblDrops->show();
    showSpinner(m_picReadingPage, true);
    m_tmrBadgeReload->start();

    if (m_retryCount >= 18)
        resetClientStatus();
    return false;
}

void MainWindow::resetRetryCountAndUpdateApplicationState()
{
    m_retryCount = 0;
    m_badgeReloadCount = 0;
    sortBadges();
    showSpinner(m_picReadingPage, false);
    updateStateInfo();
}

void MainWindow::sortBadges()
{
    const QString sort = Settings::instance().sort();
    if (sort == QLatin1String("mostcards")) {
        std::sort(m_allBadges.begin(), m_allBadges.end(),
                  [](Badge* a, Badge* b) { return a->remainingCard() > b->remainingCard(); });
    } else if (sort == QLatin1String("leastcards")) {
        std::sort(m_allBadges.begin(), m_allBadges.end(),
                  [](Badge* a, Badge* b) { return a->remainingCard() < b->remainingCard(); });
    }
}

bool MainWindow::loadLibraryBadges()
{
    const QString id = SteamProfile::steamId();
    if (id.isEmpty())
        return false;

    const QUrl url(QStringLiteral(
        "https://steamcommunity.com/profiles/%1/games?tab=all&xml=1").arg(id));
    bool ok = false;
    const QString xml = HttpClient::instance().getSync(url, 30000, &ok);
    if (!ok || xml.isEmpty())
        return false;

    const QStringList blacklist = Settings::instance().blacklist();
    for (const SteamHtml::OwnedGame& game : SteamHtml::parseOwnedGames(xml)) {
        if (blacklist.contains(game.appId))
            continue;
        const int appId = game.appId.toInt();
        Badge* badge = badgeForAppId(appId);
        if (!badge) {
            badge = new Badge(appId, game.name);
            m_allBadges.append(badge);
        } else if (badge->name().isEmpty() || badge->name().startsWith(QLatin1String("App ID:"))) {
            badge->setName(game.name);
        }
        badge->setAlwaysIdle(true);
    }
    m_libraryLoaded = true;
    return true;
}

bool MainWindow::loadBadges()
{
    Settings& s = Settings::instance();
    const QString selection = s.selectionMode();
    const QString profile = s.myProfileURL();
    if (profile.isEmpty())
        return false;

    clearBadges();

    // Always read the badge page for card counts and playtime, unless the user
    // only wants to idle a fixed set and we have no session.
    const QString firstHtml = HttpClient::instance().getSync(
        QUrl(profile + QStringLiteral("/badges/?p=1")), 20000);
    if (!checkIfResponseIsNullWithRetryCount(firstHtml))
        return false;

    const int pages = SteamHtml::totalBadgePages(firstHtml);
    processBadgesOnPage(firstHtml);
    for (int p = 2; p <= pages; ++p) {
        const QString html = HttpClient::instance().getSync(
            QUrl(profile + QStringLiteral("/badges/?p=%1").arg(p)), 20000);
        if (!html.isEmpty())
            processBadgesOnPage(html);
    }

    if (selection == QLatin1String("picked")) {
        for (const QString& id : s.whitelist()) {
            bool valid = false;
            const int appId = id.toInt(&valid);
            if (!valid)
                continue;
            Badge* badge = badgeForAppId(appId);
            if (!badge) {
                badge = new Badge(appId, tr("App ID: %1").arg(appId));
                m_allBadges.append(badge);
            }
            badge->setAlwaysIdle(true);
        }
    } else if (selection == QLatin1String("library")) {
        if (!loadLibraryBadges())
            Logger::log(QStringLiteral("Failed to load the owned games list."));
    }

    resetRetryCountAndUpdateApplicationState();
    return true;
}

void MainWindow::processBadgesOnPage(const QString& html)
{
    const QStringList blacklist = Settings::instance().blacklist();
    const QList<SteamHtml::BadgeRow> rows = SteamHtml::parseBadgeRows(html);

    for (const SteamHtml::BadgeRow& row : rows) {
        if (row.appId.isEmpty() || row.border)
            continue;
        if (blacklist.contains(row.appId))
            continue;
        if (row.appId == QLatin1String("368020") || row.appId == QLatin1String("335590"))
            continue;

        const int appId = row.appId.toInt();
        Badge* badge = badgeForAppId(appId);
        if (!badge) {
            badge = new Badge(appId, row.name);
            m_allBadges.append(badge);
        } else if (!row.name.isEmpty()) {
            badge->setName(row.name);
        }
        badge->updateStats(row.cards, row.hours);
    }
}

void MainWindow::onBadgeReload()
{
    if (m_badgeReloadCount >= 10) {
        m_tmrBadgeReload->stop();
        if (loadBadges() && !m_idlingStarted)
            startIdle();
        return;
    }
    ++m_badgeReloadCount;
    m_lblDrops->setText(
        tr("Badge page didn't load, will retry in %1 seconds").arg(10 - m_badgeReloadCount));
}

// ---------------------------------------------------------------------------
// Idle state machine
// ---------------------------------------------------------------------------

bool MainWindow::badgeMeetsGoal(const Badge* badge) const
{
    if (!badge)
        return true;
    const double target = Settings::instance().targetHours();
    if (target <= 0.0)
        return badge->remainingCard() == 0;
    return badge->remainingCard() == 0 && badge->hoursPlayed() >= target;
}

QList<Badge*> MainWindow::canIdleBadges() const
{
    QList<Badge*> result;
    const bool onlyPlayed = Settings::instance().idleOnlyPlayed();
    const double target = Settings::instance().targetHours();

    for (Badge* badge : m_allBadges) {
        if (badge->alwaysIdle()) {
            if (target > 0.0 && badge->hoursPlayed() >= target)
                continue;
            result.append(badge);
            continue;
        }
        if (badge->remainingCard() != 0) {
            if (onlyPlayed && badge->hoursPlayed() <= 0.0)
                continue;
            result.append(badge);
            continue;
        }
        if (target > 0.0 && badge->hoursPlayed() < target)
            result.append(badge);
    }
    return result;
}

bool MainWindow::hasCardDrops() const
{
    for (Badge* badge : m_allBadges) {
        if (!badge->alwaysIdle() && badge->remainingCard() > 0)
            return true;
    }
    return false;
}

int MainWindow::cardsRemaining() const
{
    int total = 0;
    for (Badge* badge : canIdleBadges())
        total += qMax(0, badge->remainingCard());
    return total;
}

int MainWindow::gamesRemaining() const
{
    return canIdleBadges().size();
}

Badge* MainWindow::badgeForAppId(int appId) const
{
    for (Badge* badge : m_allBadges) {
        if (badge->appId() == appId)
            return badge;
    }
    return nullptr;
}

void MainWindow::startIdle()
{
    Settings& s = Settings::instance();
    IdleBackend& backend = IdleBackend::instance();
    const IdleBackend::Mode mode = IdleBackend::resolveMode();
    backend.setMode(mode);
    backend.killRogueHelpers();

    if (!s.hasSession()) {
        resetClientStatus();
        return;
    }

    // Standalone mode has to be logged on before it can report any game as
    // played, otherwise the user would see "in game" while nothing happens.
    if (mode == IdleBackend::Mode::Standalone && !backend.standaloneReady()) {
        m_lblCurrentStatus->setText(tr("Connecting to Steam..."));
        if (backend.startStandalone())
            return; // resumed when standaloneStateChanged(true) arrives

        // No access token: fall back to the helper processes if Steam is running,
        // otherwise say why nothing can start.
        if (ProcessManager::isProcessRunning(QStringLiteral("steam.exe"))) {
            Logger::log(QStringLiteral("Standalone farming unavailable (%1); using the Steam client")
                            .arg(backend.lastError()));
            backend.setMode(IdleBackend::Mode::Client);
        } else {
            m_lblCurrentStatus->setText(
                tr("Could not connect to Steam: %1").arg(backend.lastError()));
            return;
        }
    }

    if (!m_isSteamReady && backend.mode() == IdleBackend::Mode::Client) {
        resetClientStatus();
        return;
    }
    if (m_badgeReloadCount != 0)
        return;

    const QList<Badge*> badges = canIdleBadges();
    if (badges.isEmpty()) {
        idleComplete();
        return;
    }

    m_statistics.resetSession();
    m_statistics.setRemainingCards(cardsRemaining());
    m_paused = false;
    m_idlingStarted = true;

    if (s.onlyOneGameIdle()) {
        startSoloIdle(badges.first());
    } else if (s.oneThenMany()) {
        Badge* twoHour = nullptr;
        for (Badge* badge : badges) {
            if (badge->hoursPlayed() >= 2.0) {
                twoHour = badge;
                break;
            }
        }
        if (twoHour)
            startSoloIdle(twoHour);
        else
            startMultipleIdle();
    } else if (s.fastMode()) {
        startMultipleIdleFastMode();
    } else {
        QList<Badge*> multi;
        for (Badge* badge : badges) {
            if (badge->hoursPlayed() < 2.0)
                multi.append(badge);
        }
        if (multi.size() >= 2)
            startMultipleIdle();
        else
            startSoloIdle(badges.first());
    }

    updateStateInfo();
}

void MainWindow::showInterruptiveButtons()
{
    m_btnResume->hide();
    m_actResume->setEnabled(false);
    m_btnPause->show();
    m_actPause->setEnabled(true);

    const bool canSkip = !Settings::instance().fastMode();
    m_btnSkip->setVisible(canSkip);
    m_actSkip->setEnabled(canSkip);
}

void MainWindow::scheduleNextDropCheck(int seconds)
{
    m_timeLeft = seconds;
    m_lblTimer->setText(QStringLiteral("%1:%2")
                            .arg(qMax(0, seconds) / 60, 2, 10, QLatin1Char('0'))
                            .arg(qMax(0, seconds) % 60, 2, 10, QLatin1Char('0')));
    m_footer->show();
}

void MainWindow::startSoloIdle(Badge* badge)
{
    if (!badge)
        return;

    m_currentBadge = badge;
    badge->idle();

    m_lblGameName->setText(QStringLiteral("<a href=\"game\">%1</a>").arg(badge->name().toHtmlEscaped()));
    m_lblGameName->show();
    m_lblCurrentStatus->setText(tr("Currently in-game"));
    m_lblCurrentStatus->show();
    m_lblHoursPlayed->setText(tr("%1 hrs on record").arg(badge->hoursPlayed()));
    m_lblHoursPlayed->show();

    if (badge->alwaysIdle()) {
        m_lblCurrentRemaining->setText(tr("Idling this game"));
    } else {
        m_lblCurrentRemaining->setText(QStringLiteral("<a href=\"force\">%1 %2</a>")
                                           .arg(badge->remainingCard())
                                           .arg(tr("card drops remaining")));
    }
    m_lblCurrentRemaining->show();

    updateHeaderImage(badge->stringId());
    m_picApp->show();
    m_gamesState->hide();

    scheduleNextDropCheck(badge->remainingCard() == 1 ? 300 : 900);

    m_pbIdle->setRange(0, qMax(1, cardsRemaining()));
    m_pbIdle->setValue(qMax(0, m_pbIdle->maximum() - cardsRemaining()));
    showSpinner(m_picIdleStatus, true);

    showInterruptiveButtons();
    m_tmrCardDropCheck->start();
}

void MainWindow::startMultipleIdle()
{
    m_currentBadge = nullptr;
    m_lblCurrentStatus->setText(tr("Currently in-game"));
    m_lblCurrentStatus->show();
    m_lblGameName->hide();
    m_lblHoursPlayed->hide();
    m_lblCurrentRemaining->hide();

    m_picApp->hide();
    m_gamesState->show();
    showSpinner(m_picIdleStatus, true);

    refreshGamesStateListView();
    updateIdleProcesses();
    showInterruptiveButtons();

    if (IdleBackend::instance().idlingCount() > 0) {
        scheduleNextDropCheck(int(Settings::instance().simultaneousMinutes()) * 60);
        m_tmrCardDropCheck->start();
    }
}

void MainWindow::updateIdleProcesses()
{
    const QList<Badge*> badges = canIdleBadges();
    const bool fast = Settings::instance().fastMode();
    IdleBackend& pm = IdleBackend::instance();

    for (Badge* badge : badges) {
        if (fast || badge->alwaysIdle()) {
            if (pm.idlingCount() <= m_maxSimultaneous)
                badge->idle();
        } else if (badge->hoursPlayed() >= 2.0) {
            if (badge->inIdle())
                badge->stopIdle();
        } else if (pm.idlingCount() <= m_maxSimultaneous) {
            badge->idle();
        }
    }

    refreshGamesStateListView();

    bool anyIdling = false;
    for (Badge* badge : badges) {
        if (badge->inIdle()) {
            anyIdling = true;
            break;
        }
    }
    if (!anyIdling)
        nextIdle();

    updateStateInfo();
}

void MainWindow::startMultipleIdleFastMode()
{
    m_currentBadge = nullptr;
    startMultipleIdle();
    scheduleNextDropCheck(int(Settings::instance().simultaneousMinutes()) * 60);
}

void MainWindow::startSoloIdleFastMode()
{
    IdleBackend::instance().stopAll();
    showSpinner(m_picIdleStatus, true);
    m_lblCurrentStatus->setText(tr("Currently in-game"));
    m_lblCurrentStatus->show();
    m_gamesState->hide();
    m_picApp->show();
    m_tmrCardDropCheck->stop();
    m_fastQueue = canIdleBadges();
    m_fastIndex = 0;
    QTimer::singleShot(5000, this, &MainWindow::fastSoloStep);
}

void MainWindow::fastSoloStep()
{
    if (m_paused)
        return;
    if (m_fastIndex >= m_fastQueue.size() || m_fastIndex >= m_maxSimultaneous) {
        startMultipleIdleFastMode();
        return;
    }
    Badge* badge = m_fastQueue.at(m_fastIndex++);
    startSoloIdle(badge);
    m_tmrCardDropCheck->stop();
    scheduleNextDropCheck(5);
    QTimer::singleShot(5000, this, [this, badge]() {
        if (!m_paused)
            badge->stopIdle();
        fastSoloStep();
    });
}

void MainWindow::nextIdle()
{
    stopIdle();

    Settings& s = Settings::instance();
    const bool standalone = IdleBackend::instance().mode() == IdleBackend::Mode::Standalone;
    if (!s.hasSession() || (!m_isSteamReady && !standalone)) {
        resetClientStatus();
        return;
    }

    if (!canIdleBadges().isEmpty()) {
        m_lblCurrentStatus->setText(tr("Loading next game..."));
        m_tmrStartNext->start(boundedDelay(3000, 6000));
    } else {
        idleComplete();
    }
}

void MainWindow::stopIdle()
{
    m_tmrCardDropCheck->stop();

    IdleBackend::instance().stopAll();

    m_currentBadge = nullptr;
    m_idlingStarted = false;

    m_lblGameName->hide();
    m_lblCurrentStatus->setText(tr("Not in game"));
    m_lblHoursPlayed->hide();
    m_lblCurrentRemaining->hide();
    m_picApp->hide();
    m_gamesState->hide();
    m_footer->hide();
    showSpinner(m_picIdleStatus, false);

    m_btnPause->hide();
    m_btnResume->hide();
    m_btnSkip->hide();
}

void MainWindow::idleComplete()
{
    m_lblCurrentStatus->setText(tr("Idling complete, no more card drops available"));
    m_lblGameName->hide();
    m_lblCurrentRemaining->hide();
    m_btnPause->hide();
    m_btnSkip->show();
    performDoneAction();
}

void MainWindow::performDoneAction()
{
    Settings& s = Settings::instance();
    const int action = s.doneAction();
    if (action == 0)
        return;

    switch (action) {
    case 2:
        PowerManager::sleep();
        return;
    case 3:
        PowerManager::hibernate();
        return;
    default:
        break;
    }

    // Shutdown, with a cancellation window.
    QProcess::startDetached(QStringLiteral("shutdown"),
                            {QStringLiteral("/s"), QStringLiteral("/c"),
                             tr("Copper Steam Idler: idling complete"), QStringLiteral("/t"),
                             QStringLiteral("300")});
    const auto choice = QMessageBox::question(
        this, QStringLiteral("Copper Steam Idler"),
        tr("Idling complete. Windows will shut down in 5 minutes.\nCancel?"));
    if (choice == QMessageBox::No)
        QProcess::startDetached(QStringLiteral("shutdown"), {QStringLiteral("/a")});
}

void MainWindow::updateStateInfo()
{
    const int games = gamesRemaining();
    const int idling = IdleBackend::instance().idlingCount();

    m_lblIdle->setText(tr("%1 games left to idle, %2 idle now.").arg(games).arg(idling));
    m_lblIdle->setVisible(games > 0);

    const int cards = cardsRemaining();
    if (cards > 0) {
        m_lblDrops->setText(tr("%1 card drops remaining").arg(cards));
        m_lblDrops->show();
    } else if (!m_lblDrops->text().contains(QLatin1String("retry"))) {
        m_lblDrops->hide();
    }
}

void MainWindow::onCardDropCheck()
{
    if (m_timeLeft > 0) {
        --m_timeLeft;
        m_lblTimer->setText(QStringLiteral("%1:%2")
                                .arg(m_timeLeft / 60, 2, 10, QLatin1Char('0'))
                                .arg(m_timeLeft % 60, 2, 10, QLatin1Char('0')));
        m_footer->show();
        return;
    }

    m_tmrCardDropCheck->stop();

    if (m_currentBadge) {
        m_currentBadge->idle();
        checkCardDrops(m_currentBadge);
        if (!m_idlingStarted)
            return; // skip/complete happened
    }

    bool anyInIdle = false;
    for (Badge* badge : m_allBadges) {
        if (badge->inIdle()) {
            anyInIdle = true;
            break;
        }
    }

    if (anyInIdle) {
        showSpinner(m_picReadingPage, true);
        const bool ok = loadBadges();
        showSpinner(m_picReadingPage, false);
        if (ok) {
            m_statistics.checkCardRemaining(cardsRemaining());
            if (Settings::instance().fastMode()) {
                startSoloIdleFastMode();
                return;
            }
            updateIdleProcesses();
            scheduleNextDropCheck(int(Settings::instance().simultaneousMinutes()) * 60);
        }
    }

    if (m_isCookieReady && !canIdleBadges().isEmpty() && m_timeLeft != 0)
        m_tmrCardDropCheck->start();
}

void MainWindow::checkCardDrops(Badge* badge)
{
    if (!badge)
        return;

    const Badge::DropStatus status = badge->refreshDrops(Settings::instance().myProfileURL());

    if (status == Badge::DropStatus::Error) {
        // Do NOT treat a network error as "no drops"; retry shortly.
        ++m_consecutiveErrors;
        if (m_consecutiveErrors >= 5) {
            m_lblCurrentStatus->setText(tr("Steam is not responding, will retry..."));
            scheduleNextDropCheck(60);
            return;
        }
        scheduleNextDropCheck(30);
        return;
    }

    m_consecutiveErrors = 0;

    const bool goalDone = badgeMeetsGoal(badge);
    if (goalDone && !badge->alwaysIdle()) {
        nextIdle();
        return;
    }
    if (badge->alwaysIdle()) {
        // Keep idling whitelist / library entries; only stop when a time goal
        // (when set) is reached.
        if (Settings::instance().targetHours() > 0.0
            && badge->hoursPlayed() >= Settings::instance().targetHours()) {
            nextIdle();
            return;
        }
    }

    scheduleNextDropCheck(badge->remainingCard() == 1 ? 300 : 900);

    if (!badge->alwaysIdle()) {
        m_lblCurrentRemaining->setText(QStringLiteral("<a href=\"force\">%1 %2</a>")
                                           .arg(badge->remainingCard())
                                           .arg(tr("card drops remaining")));
    }
    m_lblHoursPlayed->setText(tr("%1 hrs on record").arg(badge->hoursPlayed()));

    m_pbIdle->setMaximum(qMax(1, m_pbIdle->maximum()));
    m_pbIdle->setValue(qMax(0, m_pbIdle->maximum() - cardsRemaining()));

    updateStateInfo();
}

// ---------------------------------------------------------------------------
// User actions
// ---------------------------------------------------------------------------

void MainWindow::onStartNext()
{
    startIdle();
}

void MainWindow::onPause()
{
    if (!m_idlingStarted)
        return;
    stopIdle();
    m_paused = true;
    m_lblCurrentStatus->setText(tr("Idling paused"));
    m_btnResume->show();
    m_btnPause->hide();
    m_actResume->setEnabled(true);
    m_actPause->setEnabled(false);
}

void MainWindow::onResume()
{
    m_paused = false;
    startIdle();
}

void MainWindow::onSkip()
{
    if (!m_currentBadge)
        return;

    Badge* badge = m_currentBadge;
    m_currentBadge = nullptr;
    stopIdle();

    m_allBadges.removeAll(badge);
    delete badge;

    if (canIdleBadges().isEmpty())
        loadBadges();
    startIdle();
}

void MainWindow::onBlacklistCurrentGame()
{
    if (!m_currentBadge)
        return;

    QStringList blacklist = Settings::instance().blacklist();
    if (!blacklist.contains(m_currentBadge->stringId())) {
        blacklist.append(m_currentBadge->stringId());
        Settings::instance().setBlacklist(blacklist);
        Settings::instance().save();
    }
    onSkip();
}

void MainWindow::onGameNameClicked()
{
    if (m_currentBadge) {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://store.steampowered.com/app/%1").arg(m_currentBadge->stringId())));
    }
}

void MainWindow::onCurrentRemainingClicked()
{
    if (m_timeLeft > 2)
        m_timeLeft = 2;
}

void MainWindow::onQuickLogin()
{
    QString cookie;
    bool manual = false;

#ifdef QUICK_LOGIN_ENABLED
    QuickLoginDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        cookie = dialog.capturedCookie();
#else
    BrowserLoginDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        cookie = dialog.capturedCookie();
    else
        manual = dialog.manualRequested();
#endif

    if (manual) {
        AdvancedDialog advanced(this);
        if (advanced.exec() == QDialog::Accepted)
            m_tmrReadyToGo->start();
        return;
    }

    if (cookie.isEmpty())
        return;

    Settings& s = Settings::instance();
    s.setSteamLoginSecure(cookie);
    const QString id = SteamProfile::steamId();
    if (!id.isEmpty())
        s.setMyProfileURL(QStringLiteral("https://steamcommunity.com/profiles/%1").arg(id));
    HttpClient::instance().refreshCookies();
    onCheckCookieData();

    if (!HttpClient::instance().isLogined(s.myProfileURL())) {
        QMessageBox::warning(this, QStringLiteral("Copper Steam Idler"),
                             tr("Login succeeded but the session could not be validated.\n"
                                "Please try again."));
    } else {
        s.save();
        m_tmrReadyToGo->start();
    }
}

void MainWindow::onSignOut()
{
    resetClientStatus();
}

void MainWindow::onOpenSettings()
{
    Settings& s = Settings::instance();
    const QString oldSort = s.sort();
    const QString oldSelection = s.selectionMode();
    const bool oldOnlyOne = s.onlyOneGameIdle();
    const bool oldOneThenMany = s.oneThenMany();
    const bool oldFast = s.fastMode();
    const bool oldTheme = s.customTheme();
    const int oldDone = s.doneAction();
    const bool oldNoSleep = s.noSleep();

    SettingsDialog dialog(this);
    dialog.exec();

    const bool modeChanged = oldSort != s.sort() || oldSelection != s.selectionMode()
                             || oldOnlyOne != s.onlyOneGameIdle() || oldOneThenMany != s.oneThenMany()
                             || oldFast != s.fastMode();
    const bool themeChanged = oldTheme != s.customTheme();
    const bool noSleepChanged = oldNoSleep != s.noSleep();
    const bool doneChanged = oldDone != s.doneAction();
    (void)doneChanged;

    if (noSleepChanged) {
        if (s.noSleep())
            PowerManager::preventSleep();
        else
            PowerManager::allowSleep();
    }

    if (themeChanged)
        setTheme();

    if (modeChanged) {
        stopIdle();
        clearBadges();
        m_tmrReadyToGo->start();
    }
}

void MainWindow::onOpenBlacklist()
{
    BlacklistDialog dialog(this);
    dialog.exec();

    if (m_currentBadge
        && Settings::instance().blacklist().contains(m_currentBadge->stringId()))
        onSkip();
}

void MainWindow::onOpenWhitelist()
{
    WhitelistDialog dialog(this);
    dialog.exec();

    stopIdle();
    clearBadges();
    m_tmrReadyToGo->start();
}

void MainWindow::onOpenGames()
{
    GamesDialog dialog(this);
    dialog.exec();

    stopIdle();
    clearBadges();
    m_tmrReadyToGo->start();
}

void MainWindow::onOpenStatistics()
{
    StatisticsDialog dialog(m_statistics, this);
    dialog.exec();
}

void MainWindow::onOpenAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::resetClientStatus()
{
    HttpClient::instance().clearCookies();

    stopIdle();
    // Signing out (or losing the session) must also drop the CM connection: its
    // access token came from the session that just went away.
    IdleBackend::instance().stopStandalone();
    clearBadges();

    m_isCookieReady = false;
    m_signedAsName.clear();
    m_lblSignedOnAs->hide();
    m_lblDrops->hide();
    m_lblIdle->hide();
    m_lblCurrentStatus->setText(tr("Not in game"));
    showSpinner(m_picReadingPage, false);
    showSpinner(m_picIdleStatus, false);

    m_tmrCheckSteam->setInterval(500);
    m_tmrCheckCookieData->start();
    m_tmrReadyToGo->start();
}

// ---------------------------------------------------------------------------
// Theme / helpers
// ---------------------------------------------------------------------------

void MainWindow::setTheme()
{
    const bool dark = Settings::instance().customTheme();
    m_isCurrentThemeCustom = dark;
    Theme::applyApplicationTheme(dark);
    Theme::applyWidgetTheme(this, dark);
    applyIcons();
}

void MainWindow::applyIcons()
{
    auto setIcon = [](QAction* action, const QString& name) {
        if (action)
            action->setIcon(Theme::icon(name));
    };
    setIcon(m_actQuickLogin, QStringLiteral("account-login-2x"));
    setIcon(m_actSettings, QStringLiteral("cog-2x"));
    setIcon(m_actBlacklist, QStringLiteral("ban-2x"));
    setIcon(m_actWhitelist, QStringLiteral("check-2x"));
    setIcon(m_actSelectGames, QStringLiteral("grid-three-up-2x"));
    setIcon(m_actDonate, QStringLiteral("eye-2x"));
    setIcon(m_actExit, QStringLiteral("power-standby-2x"));
    setIcon(m_actPause, QStringLiteral("media-pause-2x"));
    setIcon(m_actResume, QStringLiteral("media-play-2x"));
    setIcon(m_actSkip, QStringLiteral("media-step-forward-2x"));
    setIcon(m_actBlacklistCurrent, QStringLiteral("ban-2x"));
    setIcon(m_actWiki, QStringLiteral("info-2x"));
    setIcon(m_actChangelog, QStringLiteral("document-2x"));
    setIcon(m_actGroup, QStringLiteral("globe-2x"));
    setIcon(m_actStatistics, QStringLiteral("bar-chart-2x"));

    if (m_btnPause)
        m_btnPause->setIcon(Theme::icon(QStringLiteral("media-pause")));
    if (m_btnResume)
        m_btnResume->setIcon(Theme::icon(QStringLiteral("media-play")));
    if (m_btnSkip)
        m_btnSkip->setIcon(Theme::icon(QStringLiteral("media-step-forward")));
    if (m_tray)
        m_tray->setIcon(Theme::appIcon());

    if (m_movieReading) {
        m_movieReading->stop();
        delete m_movieReading;
    }
    if (m_movieIdle) {
        m_movieIdle->stop();
        delete m_movieIdle;
    }
    m_movieReading = Theme::spinner(this);
    m_movieIdle = Theme::spinner(this);
    m_picReadingPage->setMovie(m_movieReading);
    m_picIdleStatus->setMovie(m_movieIdle);

    m_isCurrentIconsWhite = Settings::instance().whiteIcons();
}

void MainWindow::showSpinner(QLabel* label, bool visible)
{
    if (!label)
        return;
    QMovie* movie = (label == m_picReadingPage) ? m_movieReading : m_movieIdle;
    if (visible) {
        if (movie && movie->isValid())
            movie->start();
        label->show();
    } else {
        if (movie)
            movie->stop();
        label->hide();
    }
}

void MainWindow::refreshGamesStateListView()
{
    if (!m_gamesState)
        return;
    m_gamesState->clear();
    for (Badge* badge : canIdleBadges()) {
        auto* item = new QTreeWidgetItem(m_gamesState);
        item->setText(0, badge->name());
        item->setText(1, QString::number(badge->hoursPlayed(), 'f', 1));
        item->setTextAlignment(1, Qt::AlignCenter);
    }
}

void MainWindow::updateHeaderImage(const QString& appId)
{
    const QUrl url(QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/header_292x136.jpg")
                       .arg(appId));
    QNetworkReply* reply = HttpClient::instance().get(url);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QPixmap pixmap;
        if (reply->error() == QNetworkReply::NoError)
            pixmap.loadFromData(reply->readAll());
        if (pixmap.isNull()) {
            // Draw a neutral placeholder instead of leaving a broken image
            // (the upstream 404 banner aborted the whole idle loop).
            pixmap = QPixmap(m_picApp->size().expandedTo(QSize(275, 139)));
            pixmap.fill(QColor(QStringLiteral("#2a2a2a")));
            QPainter painter(&pixmap);
            painter.setPen(QColor(QStringLiteral("#8a8a8a")));
            painter.drawText(pixmap.rect(), Qt::AlignCenter, tr("No image available"));
        }
        m_picApp->setPixmap(pixmap.scaled(m_picApp->size(), Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
    });
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
        showNormal();
        raise();
        activateWindow();
        // The tray icon intentionally stays visible: it is the app's permanent home.
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized()
        && Settings::instance().minToTray() && m_tray
        && QSystemTrayIcon::isSystemTrayAvailable()) {
        // Only hide when a visible tray icon is guaranteed to bring the window
        // back; otherwise fall through to a normal taskbar minimize.
        QTimer::singleShot(0, this, [this]() {
            hide();
            if (m_tray)
                m_tray->show();
        });
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings s(Settings::dataDirectory() + "/settings.ini", QSettings::IniFormat);
    s.setValue("windowGeometry", saveGeometry());

    stopIdle();
    if (Settings::instance().noSleep())
        PowerManager::allowSleep();
    if (m_tray)
        m_tray->hide();
    event->accept();
    QCoreApplication::quit();
}
