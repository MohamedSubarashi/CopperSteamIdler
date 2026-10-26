#pragma once

#include <QColor>
#include <QSettings>
#include <QString>
#include <QStringList>

// Thin typed wrapper around QSettings. Keys and defaults mirror the original
// Idle Master Extended user settings so behaviour stays equivalent.
//
// Login uses a single cookie (steamLoginSecure). All other Steam cookies were
// removed because the community site authenticates GET requests with that one
// value and requiring the others was the source of most login failures.
class Settings
{
public:
    static Settings& instance();

    void save() { m_s.sync(); }

    // One-time migration from the old multi-cookie / multi-flag layout.
    void upgrade();

    // Authentication / session ------------------------------------------
    QString steamLoginSecure() const { return m_s.value("steamLoginSecure").toString(); }
    void setSteamLoginSecure(const QString& v) { m_s.setValue("steamLoginSecure", v); }

    QString myProfileURL() const { return m_s.value("myProfileURL").toString(); }
    void setMyProfileURL(const QString& v) { m_s.setValue("myProfileURL", v); }

    bool hasSession() const { return !steamLoginSecure().isEmpty(); }

    // General ------------------------------------------------------------
    bool minToTray() const { return m_s.value("minToTray", false).toBool(); }
    void setMinToTray(bool v) { m_s.setValue("minToTray", v); }

    bool startMinimized() const { return m_s.value("startMinimized", false).toBool(); }
    void setStartMinimized(bool v) { m_s.setValue("startMinimized", v); }

    bool updateNeeded() const { return m_s.value("updateNeeded", true).toBool(); }
    void setUpdateNeeded(bool v) { m_s.setValue("updateNeeded", v); }

    bool ignoreClient() const { return m_s.value("ignoreclient", false).toBool(); }
    void setIgnoreClient(bool v) { m_s.setValue("ignoreclient", v); }

    bool showUsername() const { return m_s.value("showUsername", false).toBool(); }
    void setShowUsername(bool v) { m_s.setValue("showUsername", v); }

    bool noSleep() const { return m_s.value("NoSleep", true).toBool(); }
    void setNoSleep(bool v) { m_s.setValue("NoSleep", v); }

    // Action performed when idling completes:
    //   0 = nothing, 1 = shut down, 2 = sleep, 3 = hibernate
    int doneAction() const
    {
        if (m_s.contains("DoneAction"))
            return m_s.value("DoneAction").toInt();
        // Migrate the old boolean.
        return m_s.value("ShutdownWindowsOnDone", false).toBool() ? 1 : 0;
    }
    void setDoneAction(int v) { m_s.setValue("DoneAction", v); }

    bool idleOnlyPlayed() const { return m_s.value("IdleOnlyPlayed", false).toBool(); }
    void setIdleOnlyPlayed(bool v) { m_s.setValue("IdleOnlyPlayed", v); }

    QString language() const { return m_s.value("language").toString(); }
    void setLanguage(const QString& v) { m_s.setValue("language", v); }

    // Idling behaviour ---------------------------------------------------
    // Backend used to farm cards:
    //   "auto"       = standalone when a saved Steam session exists, else client
    //   "client"     = copper-steam-idle helpers (requires the Steam client)
    //   "standalone" = direct Steam CM session (no Steam client needed)
    QString farmingMethod() const { return m_s.value("farmingMethod", "auto").toString(); }
    void setFarmingMethod(const QString& v) { m_s.setValue("farmingMethod", v); }

    bool fastMode() const { return m_s.value("fastMode", true).toBool(); }
    void setFastMode(bool v) { m_s.setValue("fastMode", v); }

    bool oneThenMany() const { return m_s.value("OneThenMany", true).toBool(); }
    void setOneThenMany(bool v) { m_s.setValue("OneThenMany", v); }

    bool onlyOneGameIdle() const { return m_s.value("OnlyOneGameIdle", false).toBool(); }
    void setOnlyOneGameIdle(bool v) { m_s.setValue("OnlyOneGameIdle", v); }

    // Game selection: "all" = card-eligible games (default), "picked" =
    // whitelist, "library" = every owned game.
    QString selectionMode() const
    {
        if (m_s.contains("SelectionMode"))
            return m_s.value("SelectionMode").toString();
        return m_s.value("IdlingModeWhitelist", false).toBool() ? QStringLiteral("picked")
                                                                : QStringLiteral("all");
    }
    void setSelectionMode(const QString& v) { m_s.setValue("SelectionMode", v); }

    // Backwards-compatible predicate used throughout the idle state machine.
    bool idlingModeWhitelist() const { return selectionMode() == QLatin1String("picked"); }
    void setIdlingModeWhitelist(bool v) { setSelectionMode(v ? "picked" : "all"); }

    bool idleWholeLibrary() const { return selectionMode() == QLatin1String("library"); }

    // Allow simultaneous (fast) idling while in picked/whitelist mode.
    bool whitelistFast() const { return m_s.value("WhitelistFast", false).toBool(); }
    void setWhitelistFast(bool v) { m_s.setValue("WhitelistFast", v); }

    // Minutes to idle a batch of games simultaneously (default 6 minutes,
    // matching the original 360-second check interval).
    uint simultaneousMinutes() const { return m_s.value("SimultaneousMinutes", 6u).toUInt(); }
    void setSimultaneousMinutes(uint v) { m_s.setValue("SimultaneousMinutes", v); }

    // Global time-played goal in hours (0 = disabled). Games are idled until
    // Steam reports at least this many hours, even without card drops.
    double targetHours() const { return m_s.value("TargetHours", 0.0).toDouble(); }
    void setTargetHours(double v) { m_s.setValue("TargetHours", v); }

    QString sort() const { return m_s.value("sort", "default").toString(); }
    void setSort(const QString& v) { m_s.setValue("sort", v); }

    // Appearance ---------------------------------------------------------
    bool customTheme() const { return m_s.value("customTheme", false).toBool(); }
    void setCustomTheme(bool v) { m_s.setValue("customTheme", v); }

    bool whiteIcons() const { return m_s.value("whiteIcons", false).toBool(); }
    void setWhiteIcons(bool v) { m_s.setValue("whiteIcons", v); }

    QColor colorBgd() const { return QColor(m_s.value("colorBgd", "#262626").toString()); }
    void setColorBgd(const QColor& c) { m_s.setValue("colorBgd", c.name()); }

    QColor colorTxt() const { return QColor(m_s.value("colorTxt", "#c4c4c4").toString()); }
    void setColorTxt(const QColor& c) { m_s.setValue("colorTxt", c.name()); }

    // Statistics ---------------------------------------------------------
    uint totalCardIdled() const { return m_s.value("totalCardIdled", 0u).toUInt(); }
    void setTotalCardIdled(uint v) { m_s.setValue("totalCardIdled", v); }

    uint totalMinutesIdled() const { return m_s.value("totalMinutesIdled", 0u).toUInt(); }
    void setTotalMinutesIdled(uint v) { m_s.setValue("totalMinutesIdled", v); }

    // Lists --------------------------------------------------------------
    QStringList blacklist() const { return m_s.value("blacklist").toStringList(); }
    void setBlacklist(const QStringList& v) { m_s.setValue("blacklist", v); }

    QStringList whitelist() const { return m_s.value("whitelist").toStringList(); }
    void setWhitelist(const QStringList& v) { m_s.setValue("whitelist", v); }

    static QString dataDirectory();

private:
    Settings();
    QSettings m_s;
};
