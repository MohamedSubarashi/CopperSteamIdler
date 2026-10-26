#pragma once

#include <QList>
#include <QMainWindow>
#include <QSet>
#include <QSystemTrayIcon>

#include "../core/Statistics.h"

class QAction;
class QLabel;
class QListWidget;
class QMovie;
class QProgressBar;
class QTimer;
class QToolButton;
class QTreeWidget;
class QWidget;
class Badge;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Launch minimized to the tray (called from main()).
    void startHidden();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onCheckSteam();
    void onCheckCookieData();
    void onReadyToGo();
    void onCardDropCheck();
    void onHealthCheck();
    void onStartNext();
    void onBadgeReload();
    void onStatisticsTick();
    void onPause();
    void onResume();
    void onSkip();
    void onQuickLogin();
    void onLinkAccount();
    void onSignOut();
    void onOpenSettings();
    void onOpenBlacklist();
    void onOpenWhitelist();
    void onOpenGames();
    void onOpenStatistics();
    void onOpenAbout();
    void onBlacklistCurrentGame();
    void onGameNameClicked();
    void onCurrentRemainingClicked();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void fastSoloStep();

private:
    void buildUi();
    void buildMenus();
    void setupTimers();
    void setupTray();
    void restoreGeometrySettings();

    bool loadBadges();
    bool loadLibraryBadges();
    void processBadgesOnPage(const QString& html);
    bool checkIfResponseIsNullWithRetryCount(const QString& html);
    void resetRetryCountAndUpdateApplicationState();
    void sortBadges();
    void clearBadges();

    void startIdle();
    void startSoloIdle(Badge* badge);
    void startMultipleIdle();
    void updateIdleProcesses();
    void startMultipleIdleFastMode();
    void startSoloIdleFastMode();
    void nextIdle();
    void stopIdle();
    void idleComplete();
    void performDoneAction();
    void updateStateInfo();
    void checkCardDrops(Badge* badge);
    void showInterruptiveButtons();
    void resetClientStatus();

    void setTheme();
    void applyIcons();
    void refreshGamesStateListView();
    void updateHeaderImage(const QString& appId);
    void showSpinner(QLabel* label, bool visible);
    void scheduleNextDropCheck(int seconds);

    QList<Badge*> canIdleBadges() const;
    bool badgeMeetsGoal(const Badge* badge) const;
    int cardsRemaining() const;
    int gamesRemaining() const;
    Badge* badgeForAppId(int appId) const;
    bool hasCardDrops() const;

    // ---- state -------------------------------------------------------
    QList<Badge*> m_allBadges;
    bool m_isCookieReady = false;
    bool m_isSteamReady = false;
    int m_timeLeft = 900;
    int m_retryCount = 0;
    int m_badgeReloadCount = 0;
    int m_consecutiveErrors = 0;
    Badge* m_currentBadge = nullptr;
    const int m_maxSimultaneous = 30;
    bool m_isCurrentThemeCustom = false;
    bool m_isCurrentIconsWhite = false;
    bool m_paused = false;
    bool m_idlingStarted = false;
    QList<Badge*> m_fastQueue;
    int m_fastIndex = 0;
    int m_loginFailures = 0;
    QString m_signedAsName;
    bool m_libraryLoaded = false;
    Statistics m_statistics;

    // ---- widgets -----------------------------------------------------
    QLabel* m_picSteamStatus = nullptr;
    QLabel* m_lblSteamStatus = nullptr;
    QLabel* m_lblLatestRelease = nullptr;
    QLabel* m_picCookieStatus = nullptr;
    QLabel* m_lblCookieStatus = nullptr;
    QLabel* m_lnkSignIn = nullptr;
    QLabel* m_lnkSignOut = nullptr;
    QLabel* m_lblSignedOnAs = nullptr;
    QLabel* m_picReadingPage = nullptr;
    QLabel* m_lblDrops = nullptr;
    QLabel* m_lblIdle = nullptr;
    QLabel* m_lblCurrentStatus = nullptr;
    QLabel* m_lblGameName = nullptr;
    QToolButton* m_btnPause = nullptr;
    QToolButton* m_btnResume = nullptr;
    QToolButton* m_btnSkip = nullptr;
    QLabel* m_picApp = nullptr;
    QTreeWidget* m_gamesState = nullptr;
    QLabel* m_lblCurrentRemaining = nullptr;
    QLabel* m_lblHoursPlayed = nullptr;
    QLabel* m_picIdleStatus = nullptr;
    QWidget* m_footer = nullptr;
    QProgressBar* m_pbIdle = nullptr;
    QLabel* m_lblTimer = nullptr;

    QMovie* m_movieReading = nullptr;
    QMovie* m_movieIdle = nullptr;

    // ---- actions -----------------------------------------------------
    QAction* m_actQuickLogin = nullptr;
    QAction* m_actLinkAccount = nullptr;
    QAction* m_actSettings = nullptr;
    QAction* m_actBlacklist = nullptr;
    QAction* m_actWhitelist = nullptr;
    QAction* m_actSelectGames = nullptr;
    QAction* m_actDonate = nullptr;
    QAction* m_actExit = nullptr;
    QAction* m_actPause = nullptr;
    QAction* m_actResume = nullptr;
    QAction* m_actSkip = nullptr;
    QAction* m_actBlacklistCurrent = nullptr;
    QAction* m_actWiki = nullptr;
    QAction* m_actChangelog = nullptr;
    QAction* m_actGroup = nullptr;
    QAction* m_actStatistics = nullptr;
    QAction* m_actAbout = nullptr;

    // ---- timers ------------------------------------------------------
    QTimer* m_tmrCheckSteam = nullptr;
    QTimer* m_tmrCheckCookieData = nullptr;
    QTimer* m_tmrReadyToGo = nullptr;
    QTimer* m_tmrCardDropCheck = nullptr;
    QTimer* m_tmrStartNext = nullptr;
    QTimer* m_tmrBadgeReload = nullptr;
    QTimer* m_tmrStatistics = nullptr;
    QTimer* m_tmrHealth = nullptr;

    QSystemTrayIcon* m_tray = nullptr;
};
